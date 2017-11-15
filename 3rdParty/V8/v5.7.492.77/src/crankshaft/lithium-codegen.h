// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_LITHIUM_CODEGEN_H_
#define V8_CRANKSHAFT_LITHIUM_CODEGEN_H_

#include "src/bailout-reason.h"
#include "src/deoptimizer.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class HGraph;
class LChunk;
class LEnvironment;
class LInstruction;
class LPlatformChunk;

class LCodeGenBase BASE_EMBEDDED {
 public:
  LCodeGenBase(LChunk* chunk,
               MacroAssembler* assembler,
               CompilationInfo* info);
  virtual ~LCodeGenBase() {}

  // Simple accessors.
  MacroAssembler* masm() const { return masm_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const;
  Factory* factory() const { return isolate()->factory(); }
  Heap* heap() const { return isolate()->heap(); }
  Zone* zone() const { return zone_; }
  LPlatformChunk* chunk() const { return chunk_; }
  HGraph* graph() const;
  SourcePositionTableBuilder* source_position_table_builder() {
    return &source_position_table_builder_;
  }

  void PRINTF_FORMAT(2, 3) Comment(const char* format, ...);
  void DeoptComment(const Deoptimizer::DeoptInfo& deopt_info);
  static Deoptimizer::DeoptInfo MakeDeoptInfo(LInstruction* instr,
                                              DeoptimizeReason deopt_reason,
                                              int deopt_id);

  bool GenerateBody();
  virtual void GenerateBodyInstructionPre(LInstruction* instr) {}
  virtual void GenerateBodyInstructionPost(LInstruction* instr) {}

  virtual void EnsureSpaceForLazyDeopt(int space_needed) = 0;
  void RecordAndWritePosition(SourcePosition position);

  int GetNextEmittedBlock() const;

  void WriteTranslationFrame(LEnvironment* environment,
                             Translation* translation);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  void PopulateDeoptimizationData(Handle<Code> code);
  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  // Check that an environment assigned via AssignEnvironment is actually being
  // used. Redundant assignments keep things alive longer than necessary, and
  // consequently lead to worse code, so it's important to minimize this.
  void CheckEnvironmentUsage();

 protected:
  enum Status {
    UNUSED,
    GENERATING,
    DONE,
    ABORTED
  };

  LPlatformChunk* const chunk_;
  MacroAssembler* const masm_;
  CompilationInfo* const info_;
  Zone* zone_;
  Status status_;
  int current_block_;
  int current_instruction_;
  const ZoneList<LInstruction*>* instructions_;
  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  TranslationBuffer translations_;
  int inlined_function_count_;
  int last_lazy_deopt_pc_;
  int osr_pc_offset_;
  SourcePositionTableBuilder source_position_table_builder_;

  bool is_unused() const { return status_ == UNUSED; }
  bool is_generating() const { return status_ == GENERATING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  void Abort(BailoutReason reason);
  void Retry(BailoutReason reason);

  // Methods for code dependencies.
  void AddDeprecationDependency(Handle<Map> map);
  void AddStabilityDependency(Handle<Map> map);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_LITHIUM_CODEGEN_H_
