/* -*- mesa-c++  -*-
 *
 * Copyright (c) 2019 Collabora LTD
 *
 * Author: Gert Wollny <gert.wollny@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef sfn_shader_from_nir_h
#define sfn_shader_from_nir_h


#include "gallium/drivers/r600/r600_shader.h"

#include "compiler/nir/nir.h"
#include "compiler/nir_types.h"

#include "sfn_instruction_export.h"
#include "sfn_alu_defines.h"
#include "sfn_valuepool.h"
#include "sfn_debug.h"
#include "sfn_instruction_cf.h"
#include "sfn_emittexinstruction.h"
#include "sfn_emitaluinstruction.h"
#include "sfn_emitssboinstruction.h"

#include <vector>
#include <set>
#include <stack>

struct nir_instr;

namespace r600 {

extern SfnLog sfn_log;

class ShaderFromNirProcessor : public ValuePool {
public:
   ShaderFromNirProcessor(pipe_shader_type ptype, r600_pipe_shader_selector& sel,
                          r600_shader& sh_info, int scratch_size);
   virtual ~ShaderFromNirProcessor();

   void emit_instruction(Instruction *ir);

   PValue from_nir_with_fetch_constant(const nir_src& src, unsigned component);
   GPRVector *vec_from_nir_with_fetch_constant(const nir_src& src, unsigned mask,
                                               const GPRVector::Swizzle& swizzle);

   bool emit_instruction(EAluOp opcode, PValue dest,
                         std::vector<PValue> src0,
                         const std::set<AluModifiers>& m_flags);
   void emit_export_instruction(WriteoutInstruction *ir);

   void split_constants(nir_alu_instr* instr);
   void load_uniform(const nir_alu_src& src);

   void remap_registers();

   const nir_variable *get_deref_location(const nir_src& src) const;
protected:

   void set_var_address(nir_deref_instr *instr);
   void set_input(unsigned pos, PValue var);
   void set_output(unsigned pos, PValue var);

   void evaluate_spi_sid(r600_shader_io &io);

   r600_shader& sh_info() {return m_sh_info;}

   bool scan_instruction(nir_instr *instr);

   virtual bool scan_sysvalue_access(nir_instr *instr) = 0;

   bool emit_if_start(int if_id, nir_if *if_stmt);
   bool emit_else_start(int if_id);
   bool emit_ifelse_end(int if_id);

   bool emit_loop_start(int loop_id);
   bool emit_loop_end(int loop_id);
   bool emit_jump_instruction(nir_jump_instr *instr);

   const GPRVector *output_register(unsigned location) const;

   bool load_preloaded_value(const nir_dest& dest, int chan, PValue value,
                             bool as_last = true);
   void add_param_output_reg(int loc, const GPRVector *gpr);
   void inc_atomic_file_count();
   std::bitset<8> m_sv_values;

   enum ESlots {
      es_face,
      es_instanceid,
      es_pos,
      es_sample_mask_in,
      es_sample_id,
      es_vertexid,
   };

private:
   virtual bool allocate_reserved_registers() = 0;

   bool emit_alu_instruction(nir_instr *instr);
   bool emit_deref_instruction(nir_deref_instr* instr);
   bool emit_intrinsic_instruction(nir_intrinsic_instr* instr);
   virtual bool emit_intrinsic_instruction_override(nir_intrinsic_instr* instr);
   bool emit_tex_instruction(nir_instr* instr);
   bool emit_discard_if(nir_intrinsic_instr* instr);
   bool emit_load_ubo(nir_intrinsic_instr* instr);
   bool emit_ssbo_atomic_add(nir_intrinsic_instr* instr);
   bool load_uniform_indirect(nir_intrinsic_instr* instr, PValue addr, int offest, int bufid);

   /* Code creating functions */
   bool emit_load_input_deref(const nir_variable *var, nir_intrinsic_instr* instr);
   bool emit_load_function_temp(const nir_variable *var, nir_intrinsic_instr *instr);
   AluInstruction *emit_load_literal(const nir_load_const_instr *literal, const nir_src& src, unsigned writemask);

   bool emit_store_deref(nir_intrinsic_instr* instr);

   bool reserve_uniform(nir_intrinsic_instr* instr);
   bool process_uniforms(nir_variable *uniform);
   bool process_inputs(nir_variable *input);
   bool process_outputs(nir_variable *output);

   void add_array_deref(nir_deref_instr* instr);

   virtual void emit_shader_start();
   virtual bool emit_deref_instruction_override(nir_deref_instr* instr);
   virtual bool do_process_inputs(nir_variable *input) = 0;
   virtual bool do_process_outputs(nir_variable *output) = 0;
   virtual bool do_emit_load_deref(const nir_variable *in_var, nir_intrinsic_instr* instr) = 0;
   virtual bool do_emit_store_deref(const nir_variable *out_var, nir_intrinsic_instr* instr) = 0;

   bool emit_store_scratch(nir_intrinsic_instr* instr);
   bool emit_load_scratch(nir_intrinsic_instr* instr);
   virtual void do_finalize() = 0;

   void finalize();
   friend class ShaderFromNir;

   std::set<nir_variable*> m_arrays;

   std::map<unsigned, PValue> m_inputs;
   std::map<unsigned, PValue> m_outputs;

   std::map<unsigned, nir_variable*> m_var_derefs;
   std::map<const nir_variable *, nir_variable_mode> m_var_mode;

   std::map<unsigned, const glsl_type*>  m_uniform_type_map;
   std::map<int, IfElseInstruction *> m_if_block_start_map;
   std::map<int, LoopBeginInstruction *> m_loop_begin_block_map;

   pipe_shader_type m_processor_type;

   std::vector<PInstruction> m_output;
   std::vector<PInstruction> m_export_output;
   r600_shader& m_sh_info;

   EmitTexInstruction m_tex_instr;
   EmitAluInstruction m_alu_instr;
   EmitSSBOInstruction m_ssbo_instr;
   OutputRegisterMap m_output_register_map;

   IfElseInstruction *m_pending_else;
   int m_scratch_size;
   int m_next_hwatomic_loc;

   r600_pipe_shader_selector& m_sel;
};

}

#endif
