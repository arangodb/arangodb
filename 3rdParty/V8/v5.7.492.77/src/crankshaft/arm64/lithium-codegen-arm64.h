// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_ARM64_LITHIUM_CODEGEN_ARM64_H_
#define V8_CRANKSHAFT_ARM64_LITHIUM_CODEGEN_ARM64_H_

#include "src/crankshaft/arm64/lithium-arm64.h"

#include "src/ast/scopes.h"
#include "src/crankshaft/arm64/lithium-gap-resolver-arm64.h"
#include "src/crankshaft/lithium-codegen.h"
#include "src/deoptimizer.h"
#include "src/safepoint-table.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class SafepointGenerator;
class BranchGenerator;

class LCodeGen: public LCodeGenBase {
 public:
  LCodeGen(LChunk* chunk, MacroAssembler* assembler, CompilationInfo* info)
      : LCodeGenBase(chunk, assembler, info),
        jump_table_(4, info->zone()),
        scope_(info->scope()),
        deferred_(8, info->zone()),
        frame_is_built_(false),
        safepoints_(info->zone()),
        resolver_(this),
        expected_safepoint_kind_(Safepoint::kSimple),
        pushed_arguments_(0) {
    PopulateDeoptimizationLiteralsWithInlinedFunctions();
  }

  // Simple accessors.
  Scope* scope() const { return scope_; }

  int LookupDestination(int block_id) const {
    return chunk()->LookupDestination(block_id);
  }

  bool IsNextEmittedBlock(int block_id) const {
    return LookupDestination(block_id) == GetNextEmittedBlock();
  }

  bool NeedsEagerFrame() const {
    return HasAllocatedStackSlots() || info()->is_non_deferred_calling() ||
           !info()->IsStub() || info()->requires_frame();
  }
  bool NeedsDeferredFrame() const {
    return !NeedsEagerFrame() && info()->is_deferred_calling();
  }

  LinkRegisterStatus GetLinkRegisterState() const {
    return frame_is_built_ ? kLRHasBeenSaved : kLRHasNotBeenSaved;
  }

  // Try to generate code for the entire chunk, but it may fail if the
  // chunk contains constructs we cannot handle. Returns true if the
  // code generation attempt succeeded.
  bool GenerateCode();

  // Finish the code by setting stack height, safepoint, and bailout
  // information on it.
  void FinishCode(Handle<Code> code);

  enum IntegerSignedness { SIGNED_INT32, UNSIGNED_INT32 };
  // Support for converting LOperands to assembler types.
  Register ToRegister(LOperand* op) const;
  Register ToRegister32(LOperand* op) const;
  Operand ToOperand(LOperand* op);
  Operand ToOperand32(LOperand* op);
  enum StackMode { kMustUseFramePointer, kCanUseStackPointer };
  MemOperand ToMemOperand(LOperand* op,
                          StackMode stack_mode = kCanUseStackPointer) const;
  Handle<Object> ToHandle(LConstantOperand* op) const;

  template <class LI>
  Operand ToShiftedRightOperand32(LOperand* right, LI* shift_info);

  int JSShiftAmountFromLConstant(LOperand* constant) {
    return ToInteger32(LConstantOperand::cast(constant)) & 0x1f;
  }

  // TODO(jbramley): Examine these helpers and check that they make sense.
  // IsInteger32Constant returns true for smi constants, for example.
  bool IsInteger32Constant(LConstantOperand* op) const;
  bool IsSmi(LConstantOperand* op) const;

  int32_t ToInteger32(LConstantOperand* op) const;
  Smi* ToSmi(LConstantOperand* op) const;
  double ToDouble(LConstantOperand* op) const;
  DoubleRegister ToDoubleRegister(LOperand* op) const;

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) void Do##type(L##type* node);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  // Return a double scratch register which can be used locally
  // when generating code for a lithium instruction.
  DoubleRegister double_scratch() { return crankshaft_fp_scratch; }

  // Deferred code support.
  void DoDeferredNumberTagD(LNumberTagD* instr);
  void DoDeferredStackCheck(LStackCheck* instr);
  void DoDeferredMaybeGrowElements(LMaybeGrowElements* instr);
  void DoDeferredStringCharCodeAt(LStringCharCodeAt* instr);
  void DoDeferredStringCharFromCode(LStringCharFromCode* instr);
  void DoDeferredMathAbsTagged(LMathAbsTagged* instr,
                               Label* exit,
                               Label* allocation_entry);

  void DoDeferredNumberTagU(LInstruction* instr,
                            LOperand* value,
                            LOperand* temp1,
                            LOperand* temp2);
  void DoDeferredTaggedToI(LTaggedToI* instr,
                           LOperand* value,
                           LOperand* temp1,
                           LOperand* temp2);
  void DoDeferredAllocate(LAllocate* instr);
  void DoDeferredInstanceMigration(LCheckMaps* instr, Register object);
  void DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                   Register result,
                                   Register object,
                                   Register index);

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block);
  void DoGap(LGap* instr);

  // Generic version of EmitBranch. It contains some code to avoid emitting a
  // branch on the next emitted basic block where we could just fall-through.
  // You shouldn't use that directly but rather consider one of the helper like
  // LCodeGen::EmitBranch, LCodeGen::EmitCompareAndBranch...
  template<class InstrType>
  void EmitBranchGeneric(InstrType instr,
                         const BranchGenerator& branch);

  template<class InstrType>
  void EmitBranch(InstrType instr, Condition condition);

  template<class InstrType>
  void EmitCompareAndBranch(InstrType instr,
                            Condition condition,
                            const Register& lhs,
                            const Operand& rhs);

  template<class InstrType>
  void EmitTestAndBranch(InstrType instr,
                         Condition condition,
                         const Register& value,
                         uint64_t mask);

  template<class InstrType>
  void EmitBranchIfNonZeroNumber(InstrType instr,
                                 const FPRegister& value,
                                 const FPRegister& scratch);

  template<class InstrType>
  void EmitBranchIfHeapNumber(InstrType instr,
                              const Register& value);

  template<class InstrType>
  void EmitBranchIfRoot(InstrType instr,
                        const Register& value,
                        Heap::RootListIndex index);

  // Emits optimized code to deep-copy the contents of statically known object
  // graphs (e.g. object literal boilerplate). Expects a pointer to the
  // allocated destination object in the result register, and a pointer to the
  // source object in the source register.
  void EmitDeepCopy(Handle<JSObject> object,
                    Register result,
                    Register source,
                    Register scratch,
                    int* offset,
                    AllocationSiteMode mode);

  template <class T>
  void EmitVectorLoadICRegisters(T* instr);

  // Emits optimized code for %_IsString(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsString(Register input, Register temp1, Label* is_not_string,
                         SmiCheck check_needed);

  MemOperand BuildSeqStringOperand(Register string,
                                   Register temp,
                                   LOperand* index,
                                   String::Encoding encoding);
  void DeoptimizeBranch(LInstruction* instr, DeoptimizeReason deopt_reason,
                        BranchType branch_type, Register reg = NoReg,
                        int bit = -1,
                        Deoptimizer::BailoutType* override_bailout_type = NULL);
  void Deoptimize(LInstruction* instr, DeoptimizeReason deopt_reason,
                  Deoptimizer::BailoutType* override_bailout_type = NULL);
  void DeoptimizeIf(Condition cond, LInstruction* instr,
                    DeoptimizeReason deopt_reason);
  void DeoptimizeIfZero(Register rt, LInstruction* instr,
                        DeoptimizeReason deopt_reason);
  void DeoptimizeIfNotZero(Register rt, LInstruction* instr,
                           DeoptimizeReason deopt_reason);
  void DeoptimizeIfNegative(Register rt, LInstruction* instr,
                            DeoptimizeReason deopt_reason);
  void DeoptimizeIfSmi(Register rt, LInstruction* instr,
                       DeoptimizeReason deopt_reason);
  void DeoptimizeIfNotSmi(Register rt, LInstruction* instr,
                          DeoptimizeReason deopt_reason);
  void DeoptimizeIfRoot(Register rt, Heap::RootListIndex index,
                        LInstruction* instr, DeoptimizeReason deopt_reason);
  void DeoptimizeIfNotRoot(Register rt, Heap::RootListIndex index,
                           LInstruction* instr, DeoptimizeReason deopt_reason);
  void DeoptimizeIfNotHeapNumber(Register object, LInstruction* instr);
  void DeoptimizeIfMinusZero(DoubleRegister input, LInstruction* instr,
                             DeoptimizeReason deopt_reason);
  void DeoptimizeIfBitSet(Register rt, int bit, LInstruction* instr,
                          DeoptimizeReason deopt_reason);
  void DeoptimizeIfBitClear(Register rt, int bit, LInstruction* instr,
                            DeoptimizeReason deopt_reason);

  MemOperand PrepareKeyedExternalArrayOperand(Register key,
                                              Register base,
                                              Register scratch,
                                              bool key_is_smi,
                                              bool key_is_constant,
                                              int constant_key,
                                              ElementsKind elements_kind,
                                              int base_offset);
  MemOperand PrepareKeyedArrayOperand(Register base,
                                      Register elements,
                                      Register key,
                                      bool key_is_tagged,
                                      ElementsKind elements_kind,
                                      Representation representation,
                                      int base_offset);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);

  bool HasAllocatedStackSlots() const {
    return chunk()->HasAllocatedStackSlots();
  }
  int GetStackSlotCount() const { return chunk()->GetSpillSlotCount(); }
  int GetTotalFrameSlotCount() const {
    return chunk()->GetTotalFrameSlotCount();
  }

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

  // Emit frame translation commands for an environment.
  void WriteTranslation(LEnvironment* environment, Translation* translation);

  void AddToTranslation(LEnvironment* environment,
                        Translation* translation,
                        LOperand* op,
                        bool is_tagged,
                        bool is_uint32,
                        int* object_index_pointer,
                        int* dematerialized_index_pointer);

  void SaveCallerDoubles();
  void RestoreCallerDoubles();

  // Code generation steps.  Returns true if code generation should continue.
  void GenerateBodyInstructionPre(LInstruction* instr) override;
  bool GeneratePrologue();
  bool GenerateDeferredCode();
  bool GenerateJumpTable();
  bool GenerateSafepointTable();

  // Generates the custom OSR entrypoint and sets the osr_pc_offset.
  void GenerateOsrPrologue();

  enum SafepointMode {
    RECORD_SIMPLE_SAFEPOINT,
    RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS
  };

  void CallCode(Handle<Code> code,
                RelocInfo::Mode mode,
                LInstruction* instr);

  void CallCodeGeneric(Handle<Code> code,
                       RelocInfo::Mode mode,
                       LInstruction* instr,
                       SafepointMode safepoint_mode);

  void CallRuntime(const Runtime::Function* function,
                   int num_arguments,
                   LInstruction* instr,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  void CallRuntime(Runtime::FunctionId id,
                   int num_arguments,
                   LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, num_arguments, instr);
  }

  void CallRuntime(Runtime::FunctionId id, LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, function->nargs, instr);
  }

  void LoadContextFromDeferred(LOperand* context);
  void CallRuntimeFromDeferred(Runtime::FunctionId id,
                               int argc,
                               LInstruction* instr,
                               LOperand* context);

  void PrepareForTailCall(const ParameterCount& actual, Register scratch1,
                          Register scratch2, Register scratch3);

  // Generate a direct call to a known function.  Expects the function
  // to be in x1.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count, int arity,
                         bool is_tail_call, LInstruction* instr);

  // Support for recording safepoint information.
  void RecordSafepoint(LPointerMap* pointers,
                       Safepoint::Kind kind,
                       int arguments,
                       Safepoint::DeoptMode mode);
  void RecordSafepoint(LPointerMap* pointers, Safepoint::DeoptMode mode);
  void RecordSafepoint(Safepoint::DeoptMode mode);
  void RecordSafepointWithRegisters(LPointerMap* pointers,
                                    int arguments,
                                    Safepoint::DeoptMode mode);
  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void EnsureSpaceForLazyDeopt(int space_needed) override;

  ZoneList<Deoptimizer::JumpTableEntry*> jump_table_;
  Scope* const scope_;
  ZoneList<LDeferredCode*> deferred_;
  bool frame_is_built_;

  // Builder that keeps track of safepoints in the code. The table itself is
  // emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  // The number of arguments pushed onto the stack, either by this block or by a
  // predecessor.
  int pushed_arguments_;

  void RecordPushedArgumentsDelta(int delta) {
    pushed_arguments_ += delta;
    DCHECK(pushed_arguments_ >= 0);
  }

  int old_position_;

  class PushSafepointRegistersScope BASE_EMBEDDED {
   public:
    explicit PushSafepointRegistersScope(LCodeGen* codegen);

    ~PushSafepointRegistersScope();

   private:
    LCodeGen* codegen_;
  };

  friend class LDeferredCode;
  friend class SafepointGenerator;
  DISALLOW_COPY_AND_ASSIGN(LCodeGen);
};


class LDeferredCode: public ZoneObject {
 public:
  explicit LDeferredCode(LCodeGen* codegen)
      : codegen_(codegen),
        external_exit_(NULL),
        instruction_index_(codegen->current_instruction_) {
    codegen->AddDeferredCode(this);
  }

  virtual ~LDeferredCode() { }
  virtual void Generate() = 0;
  virtual LInstruction* instr() = 0;

  void SetExit(Label* exit) { external_exit_ = exit; }
  Label* entry() { return &entry_; }
  Label* exit() { return (external_exit_ != NULL) ? external_exit_ : &exit_; }
  int instruction_index() const { return instruction_index_; }

 protected:
  LCodeGen* codegen() const { return codegen_; }
  MacroAssembler* masm() const { return codegen_->masm(); }

 private:
  LCodeGen* codegen_;
  Label entry_;
  Label exit_;
  Label* external_exit_;
  int instruction_index_;
};


// This is the abstract class used by EmitBranchGeneric.
// It is used to emit code for conditional branching. The Emit() function
// emits code to branch when the condition holds and EmitInverted() emits
// the branch when the inverted condition is verified.
//
// For actual examples of condition see the concrete implementation in
// lithium-codegen-arm64.cc (e.g. BranchOnCondition, CompareAndBranch).
class BranchGenerator BASE_EMBEDDED {
 public:
  explicit BranchGenerator(LCodeGen* codegen)
    : codegen_(codegen) { }

  virtual ~BranchGenerator() { }

  virtual void Emit(Label* label) const = 0;
  virtual void EmitInverted(Label* label) const = 0;

 protected:
  MacroAssembler* masm() const { return codegen_->masm(); }

  LCodeGen* codegen_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_ARM64_LITHIUM_CODEGEN_ARM64_H_
