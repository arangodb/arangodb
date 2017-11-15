// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/mips64/lithium-codegen-mips64.h"

#include "src/builtins/builtins-constructor.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/crankshaft/hydrogen-osr.h"
#include "src/crankshaft/mips64/lithium-gap-resolver-mips64.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {


class SafepointGenerator final : public CallWrapper {
 public:
  SafepointGenerator(LCodeGen* codegen,
                     LPointerMap* pointers,
                     Safepoint::DeoptMode mode)
      : codegen_(codegen),
        pointers_(pointers),
        deopt_mode_(mode) { }
  virtual ~SafepointGenerator() {}

  void BeforeCall(int call_size) const override {}

  void AfterCall() const override {
    codegen_->RecordSafepoint(pointers_, deopt_mode_);
  }

 private:
  LCodeGen* codegen_;
  LPointerMap* pointers_;
  Safepoint::DeoptMode deopt_mode_;
};

LCodeGen::PushSafepointRegistersScope::PushSafepointRegistersScope(
    LCodeGen* codegen)
    : codegen_(codegen) {
  DCHECK(codegen_->info()->is_calling());
  DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
  codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;

  StoreRegistersStateStub stub(codegen_->isolate());
  codegen_->masm_->push(ra);
  codegen_->masm_->CallStub(&stub);
}

LCodeGen::PushSafepointRegistersScope::~PushSafepointRegistersScope() {
  DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kWithRegisters);
  RestoreRegistersStateStub stub(codegen_->isolate());
  codegen_->masm_->push(ra);
  codegen_->masm_->CallStub(&stub);
  codegen_->expected_safepoint_kind_ = Safepoint::kSimple;
}

#define __ masm()->

bool LCodeGen::GenerateCode() {
  LPhase phase("Z_Code generation", chunk());
  DCHECK(is_unused());
  status_ = GENERATING;

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // NONE indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in GeneratePrologue).
  FrameScope frame_scope(masm_, StackFrame::NONE);

  return GeneratePrologue() && GenerateBody() && GenerateDeferredCode() &&
         GenerateJumpTable() && GenerateSafepointTable();
}


void LCodeGen::FinishCode(Handle<Code> code) {
  DCHECK(is_done());
  code->set_stack_slots(GetTotalFrameSlotCount());
  code->set_safepoint_table_offset(safepoints_.GetCodeOffset());
  PopulateDeoptimizationData(code);
}


void LCodeGen::SaveCallerDoubles() {
  DCHECK(info()->saves_caller_doubles());
  DCHECK(NeedsEagerFrame());
  Comment(";;; Save clobbered callee double registers");
  int count = 0;
  BitVector* doubles = chunk()->allocated_double_registers();
  BitVector::Iterator save_iterator(doubles);
  while (!save_iterator.Done()) {
    __ sdc1(DoubleRegister::from_code(save_iterator.Current()),
            MemOperand(sp, count * kDoubleSize));
    save_iterator.Advance();
    count++;
  }
}


void LCodeGen::RestoreCallerDoubles() {
  DCHECK(info()->saves_caller_doubles());
  DCHECK(NeedsEagerFrame());
  Comment(";;; Restore clobbered callee double registers");
  BitVector* doubles = chunk()->allocated_double_registers();
  BitVector::Iterator save_iterator(doubles);
  int count = 0;
  while (!save_iterator.Done()) {
    __ ldc1(DoubleRegister::from_code(save_iterator.Current()),
            MemOperand(sp, count * kDoubleSize));
    save_iterator.Advance();
    count++;
  }
}


bool LCodeGen::GeneratePrologue() {
  DCHECK(is_generating());

  if (info()->IsOptimizing()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm_);

    // a1: Callee's JS function.
    // cp: Callee's context.
    // fp: Caller's frame pointer.
    // lr: Caller's pc.
  }

  info()->set_prologue_offset(masm_->pc_offset());
  if (NeedsEagerFrame()) {
    if (info()->IsStub()) {
      __ StubPrologue(StackFrame::STUB);
    } else {
      __ Prologue(info()->GeneratePreagedPrologue());
    }
    frame_is_built_ = true;
  }

  // Reserve space for the stack slots needed by the code.
  int slots = GetStackSlotCount();
  if (slots > 0) {
    if (FLAG_debug_code) {
      __ Dsubu(sp,  sp, Operand(slots * kPointerSize));
      __ Push(a0, a1);
      __ Daddu(a0, sp, Operand(slots *  kPointerSize));
      __ li(a1, Operand(kSlotsZapValue));
      Label loop;
      __ bind(&loop);
      __ Dsubu(a0, a0, Operand(kPointerSize));
      __ sd(a1, MemOperand(a0, 2 * kPointerSize));
      __ Branch(&loop, ne, a0, Operand(sp));
      __ Pop(a0, a1);
    } else {
      __ Dsubu(sp, sp, Operand(slots * kPointerSize));
    }
  }

  if (info()->saves_caller_doubles()) {
    SaveCallerDoubles();
  }
  return !is_aborted();
}


void LCodeGen::DoPrologue(LPrologue* instr) {
  Comment(";;; Prologue begin");

  // Possibly allocate a local context.
  if (info()->scope()->NeedsContext()) {
    Comment(";;; Allocate local context");
    bool need_write_barrier = true;
    // Argument to NewContext is the function, which is in a1.
    int slots = info()->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    Safepoint::DeoptMode deopt_mode = Safepoint::kNoLazyDeopt;
    if (info()->scope()->is_script_scope()) {
      __ push(a1);
      __ Push(info()->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      deopt_mode = Safepoint::kLazyDeopt;
    } else {
      if (slots <=
          ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
        Callable callable = CodeFactory::FastNewFunctionContext(
            isolate(), info()->scope()->scope_type());
        __ li(FastNewFunctionContextDescriptor::SlotsRegister(),
              Operand(slots));
        __ Call(callable.code(), RelocInfo::CODE_TARGET);
        // Result of the FastNewFunctionContext builtin is always in new space.
        need_write_barrier = false;
      } else {
        __ push(a1);
        __ Push(Smi::FromInt(info()->scope()->scope_type()));
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
    }
    RecordSafepoint(deopt_mode);

    // Context is returned in both v0. It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mov(cp, v0);
    __ sd(v0, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info()->scope()->num_parameters();
    int first_parameter = info()->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var = (i == -1) ? info()->scope()->receiver()
                                : info()->scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ ld(a0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ sd(a0, target);
        // Update the write barrier. This clobbers a3 and a0.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(
              cp, target.offset(), a0, a3, GetRAState(), kSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, a0, &done);
          __ Abort(kExpectedNewSpaceObject);
          __ bind(&done);
        }
      }
    }
    Comment(";;; End allocate local context");
  }

  Comment(";;; Prologue end");
}


void LCodeGen::GenerateOsrPrologue() {
  // Generate the OSR entry prologue at the first unknown OSR value, or if there
  // are none, at the OSR entrypoint instruction.
  if (osr_pc_offset_ >= 0) return;

  osr_pc_offset_ = masm()->pc_offset();

  // Adjust the frame size, subsuming the unoptimized frame into the
  // optimized frame.
  int slots = GetStackSlotCount() - graph()->osr()->UnoptimizedFrameSlots();
  DCHECK(slots >= 0);
  __ Dsubu(sp, sp, Operand(slots * kPointerSize));
}


void LCodeGen::GenerateBodyInstructionPre(LInstruction* instr) {
  if (instr->IsCall()) {
    EnsureSpaceForLazyDeopt(Deoptimizer::patch_size());
  }
  if (!instr->IsLazyBailout() && !instr->IsGap()) {
    safepoints_.BumpLastLazySafepointIndex();
  }
}


bool LCodeGen::GenerateDeferredCode() {
  DCHECK(is_generating());
  if (deferred_.length() > 0) {
    for (int i = 0; !is_aborted() && i < deferred_.length(); i++) {
      LDeferredCode* code = deferred_[i];

      HValue* value =
          instructions_->at(code->instruction_index())->hydrogen_value();
      RecordAndWritePosition(value->position());

      Comment(";;; <@%d,#%d> "
              "-------------------- Deferred %s --------------------",
              code->instruction_index(),
              code->instr()->hydrogen_value()->id(),
              code->instr()->Mnemonic());
      __ bind(code->entry());
      if (NeedsDeferredFrame()) {
        Comment(";;; Build frame");
        DCHECK(!frame_is_built_);
        DCHECK(info()->IsStub());
        frame_is_built_ = true;
        __ li(scratch0(), Operand(Smi::FromInt(StackFrame::STUB)));
        __ PushCommonFrame(scratch0());
        Comment(";;; Deferred code");
      }
      code->Generate();
      if (NeedsDeferredFrame()) {
        Comment(";;; Destroy frame");
        DCHECK(frame_is_built_);
        __ PopCommonFrame(scratch0());
        frame_is_built_ = false;
      }
      __ jmp(code->exit());
    }
  }
  // Deferred code is the last part of the instruction sequence. Mark
  // the generated code as done unless we bailed out.
  if (!is_aborted()) status_ = DONE;
  return !is_aborted();
}


bool LCodeGen::GenerateJumpTable() {
  if (jump_table_.length() > 0) {
    Comment(";;; -------------------- Jump table --------------------");
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    Label table_start, call_deopt_entry;

    __ bind(&table_start);
    Label needs_frame;
    Address base = jump_table_[0]->address;
    for (int i = 0; i < jump_table_.length(); i++) {
      Deoptimizer::JumpTableEntry* table_entry = jump_table_[i];
      __ bind(&table_entry->label);
      Address entry = table_entry->address;
      DeoptComment(table_entry->deopt_info);

      // Second-level deopt table entries are contiguous and small, so instead
      // of loading the full, absolute address of each one, load the base
      // address and add an immediate offset.
      if (is_int16(entry - base)) {
        if (table_entry->needs_frame) {
          DCHECK(!info()->saves_caller_doubles());
          Comment(";;; call deopt with frame");
          __ PushCommonFrame();
          __ BranchAndLink(&needs_frame, USE_DELAY_SLOT);
          __ li(t9, Operand(entry - base));
        } else {
          __ BranchAndLink(&call_deopt_entry, USE_DELAY_SLOT);
          __ li(t9, Operand(entry - base));
        }

      } else {
        __ li(t9, Operand(entry - base));
        if (table_entry->needs_frame) {
          DCHECK(!info()->saves_caller_doubles());
          Comment(";;; call deopt with frame");
          __ PushCommonFrame();
          __ BranchAndLink(&needs_frame);
        } else {
          __ BranchAndLink(&call_deopt_entry);
        }
      }
    }
    if (needs_frame.is_linked()) {
      __ bind(&needs_frame);
      // This variant of deopt can only be used with stubs. Since we don't
      // have a function pointer to install in the stack frame that we're
      // building, install a special marker there instead.
      __ li(at, Operand(Smi::FromInt(StackFrame::STUB)));
      __ push(at);
      DCHECK(info()->IsStub());
    }

    Comment(";;; call deopt");
    __ bind(&call_deopt_entry);

    if (info()->saves_caller_doubles()) {
      DCHECK(info()->IsStub());
      RestoreCallerDoubles();
    }

    __ li(at,
          Operand(reinterpret_cast<int64_t>(base), RelocInfo::RUNTIME_ENTRY));
    __ Daddu(t9, t9, Operand(at));
    __ Jump(t9);
  }
  // The deoptimization jump table is the last part of the instruction
  // sequence. Mark the generated code as done unless we bailed out.
  if (!is_aborted()) status_ = DONE;
  return !is_aborted();
}


bool LCodeGen::GenerateSafepointTable() {
  DCHECK(is_done());
  safepoints_.Emit(masm(), GetTotalFrameSlotCount());
  return !is_aborted();
}


Register LCodeGen::ToRegister(int index) const {
  return Register::from_code(index);
}


DoubleRegister LCodeGen::ToDoubleRegister(int index) const {
  return DoubleRegister::from_code(index);
}


Register LCodeGen::ToRegister(LOperand* op) const {
  DCHECK(op->IsRegister());
  return ToRegister(op->index());
}


Register LCodeGen::EmitLoadRegister(LOperand* op, Register scratch) {
  if (op->IsRegister()) {
    return ToRegister(op->index());
  } else if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk_->LookupConstant(const_op);
    Handle<Object> literal = constant->handle(isolate());
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      AllowDeferredHandleDereference get_number;
      DCHECK(literal->IsNumber());
      __ li(scratch, Operand(static_cast<int32_t>(literal->Number())));
    } else if (r.IsSmi()) {
      DCHECK(constant->HasSmiValue());
      __ li(scratch, Operand(Smi::FromInt(constant->Integer32Value())));
    } else if (r.IsDouble()) {
      Abort(kEmitLoadRegisterUnsupportedDoubleImmediate);
    } else {
      DCHECK(r.IsSmiOrTagged());
      __ li(scratch, literal);
    }
    return scratch;
  } else if (op->IsStackSlot()) {
    __ ld(scratch, ToMemOperand(op));
    return scratch;
  }
  UNREACHABLE();
  return scratch;
}


DoubleRegister LCodeGen::ToDoubleRegister(LOperand* op) const {
  DCHECK(op->IsDoubleRegister());
  return ToDoubleRegister(op->index());
}


DoubleRegister LCodeGen::EmitLoadDoubleRegister(LOperand* op,
                                                FloatRegister flt_scratch,
                                                DoubleRegister dbl_scratch) {
  if (op->IsDoubleRegister()) {
    return ToDoubleRegister(op->index());
  } else if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk_->LookupConstant(const_op);
    Handle<Object> literal = constant->handle(isolate());
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      DCHECK(literal->IsNumber());
      __ li(at, Operand(static_cast<int32_t>(literal->Number())));
      __ mtc1(at, flt_scratch);
      __ cvt_d_w(dbl_scratch, flt_scratch);
      return dbl_scratch;
    } else if (r.IsDouble()) {
      Abort(kUnsupportedDoubleImmediate);
    } else if (r.IsTagged()) {
      Abort(kUnsupportedTaggedImmediate);
    }
  } else if (op->IsStackSlot()) {
    MemOperand mem_op = ToMemOperand(op);
    __ ldc1(dbl_scratch, mem_op);
    return dbl_scratch;
  }
  UNREACHABLE();
  return dbl_scratch;
}


Handle<Object> LCodeGen::ToHandle(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  DCHECK(chunk_->LookupLiteralRepresentation(op).IsSmiOrTagged());
  return constant->handle(isolate());
}


bool LCodeGen::IsInteger32(LConstantOperand* op) const {
  return chunk_->LookupLiteralRepresentation(op).IsSmiOrInteger32();
}


bool LCodeGen::IsSmi(LConstantOperand* op) const {
  return chunk_->LookupLiteralRepresentation(op).IsSmi();
}


int32_t LCodeGen::ToInteger32(LConstantOperand* op) const {
  // return ToRepresentation(op, Representation::Integer32());
  HConstant* constant = chunk_->LookupConstant(op);
  return constant->Integer32Value();
}


int64_t LCodeGen::ToRepresentation_donotuse(LConstantOperand* op,
                                            const Representation& r) const {
  HConstant* constant = chunk_->LookupConstant(op);
  int32_t value = constant->Integer32Value();
  if (r.IsInteger32()) return value;
  DCHECK(r.IsSmiOrTagged());
  return reinterpret_cast<int64_t>(Smi::FromInt(value));
}


Smi* LCodeGen::ToSmi(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  return Smi::FromInt(constant->Integer32Value());
}


double LCodeGen::ToDouble(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  DCHECK(constant->HasDoubleValue());
  return constant->DoubleValue();
}


Operand LCodeGen::ToOperand(LOperand* op) {
  if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk()->LookupConstant(const_op);
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsSmi()) {
      DCHECK(constant->HasSmiValue());
      return Operand(Smi::FromInt(constant->Integer32Value()));
    } else if (r.IsInteger32()) {
      DCHECK(constant->HasInteger32Value());
      return Operand(constant->Integer32Value());
    } else if (r.IsDouble()) {
      Abort(kToOperandUnsupportedDoubleImmediate);
    }
    DCHECK(r.IsTagged());
    return Operand(constant->handle(isolate()));
  } else if (op->IsRegister()) {
    return Operand(ToRegister(op));
  } else if (op->IsDoubleRegister()) {
    Abort(kToOperandIsDoubleRegisterUnimplemented);
    return Operand((int64_t)0);
  }
  // Stack slots not implemented, use ToMemOperand instead.
  UNREACHABLE();
  return Operand((int64_t)0);
}


static int ArgumentsOffsetWithoutFrame(int index) {
  DCHECK(index < 0);
  return -(index + 1) * kPointerSize;
}


MemOperand LCodeGen::ToMemOperand(LOperand* op) const {
  DCHECK(!op->IsRegister());
  DCHECK(!op->IsDoubleRegister());
  DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
  if (NeedsEagerFrame()) {
    return MemOperand(fp, FrameSlotToFPOffset(op->index()));
  } else {
    // Retrieve parameter without eager stack-frame relative to the
    // stack-pointer.
    return MemOperand(sp, ArgumentsOffsetWithoutFrame(op->index()));
  }
}


MemOperand LCodeGen::ToHighMemOperand(LOperand* op) const {
  DCHECK(op->IsDoubleStackSlot());
  if (NeedsEagerFrame()) {
    // return MemOperand(fp, FrameSlotToFPOffset(op->index()) + kPointerSize);
    return MemOperand(fp, FrameSlotToFPOffset(op->index()) + kIntSize);
  } else {
    // Retrieve parameter without eager stack-frame relative to the
    // stack-pointer.
    // return MemOperand(
    //    sp, ArgumentsOffsetWithoutFrame(op->index()) + kPointerSize);
    return MemOperand(
        sp, ArgumentsOffsetWithoutFrame(op->index()) + kIntSize);
  }
}


void LCodeGen::WriteTranslation(LEnvironment* environment,
                                Translation* translation) {
  if (environment == NULL) return;

  // The translation includes one command per value in the environment.
  int translation_size = environment->translation_size();

  WriteTranslation(environment->outer(), translation);
  WriteTranslationFrame(environment, translation);

  int object_index = 0;
  int dematerialized_index = 0;
  for (int i = 0; i < translation_size; ++i) {
    LOperand* value = environment->values()->at(i);
    AddToTranslation(
        environment, translation, value, environment->HasTaggedValueAt(i),
        environment->HasUint32ValueAt(i), &object_index, &dematerialized_index);
  }
}


void LCodeGen::AddToTranslation(LEnvironment* environment,
                                Translation* translation,
                                LOperand* op,
                                bool is_tagged,
                                bool is_uint32,
                                int* object_index_pointer,
                                int* dematerialized_index_pointer) {
  if (op == LEnvironment::materialization_marker()) {
    int object_index = (*object_index_pointer)++;
    if (environment->ObjectIsDuplicateAt(object_index)) {
      int dupe_of = environment->ObjectDuplicateOfAt(object_index);
      translation->DuplicateObject(dupe_of);
      return;
    }
    int object_length = environment->ObjectLengthAt(object_index);
    if (environment->ObjectIsArgumentsAt(object_index)) {
      translation->BeginArgumentsObject(object_length);
    } else {
      translation->BeginCapturedObject(object_length);
    }
    int dematerialized_index = *dematerialized_index_pointer;
    int env_offset = environment->translation_size() + dematerialized_index;
    *dematerialized_index_pointer += object_length;
    for (int i = 0; i < object_length; ++i) {
      LOperand* value = environment->values()->at(env_offset + i);
      AddToTranslation(environment,
                       translation,
                       value,
                       environment->HasTaggedValueAt(env_offset + i),
                       environment->HasUint32ValueAt(env_offset + i),
                       object_index_pointer,
                       dematerialized_index_pointer);
    }
    return;
  }

  if (op->IsStackSlot()) {
    int index = op->index();
    if (is_tagged) {
      translation->StoreStackSlot(index);
    } else if (is_uint32) {
      translation->StoreUint32StackSlot(index);
    } else {
      translation->StoreInt32StackSlot(index);
    }
  } else if (op->IsDoubleStackSlot()) {
    int index = op->index();
    translation->StoreDoubleStackSlot(index);
  } else if (op->IsRegister()) {
    Register reg = ToRegister(op);
    if (is_tagged) {
      translation->StoreRegister(reg);
    } else if (is_uint32) {
      translation->StoreUint32Register(reg);
    } else {
      translation->StoreInt32Register(reg);
    }
  } else if (op->IsDoubleRegister()) {
    DoubleRegister reg = ToDoubleRegister(op);
    translation->StoreDoubleRegister(reg);
  } else if (op->IsConstantOperand()) {
    HConstant* constant = chunk()->LookupConstant(LConstantOperand::cast(op));
    int src_index = DefineDeoptimizationLiteral(constant->handle(isolate()));
    translation->StoreLiteral(src_index);
  } else {
    UNREACHABLE();
  }
}


void LCodeGen::CallCode(Handle<Code> code,
                        RelocInfo::Mode mode,
                        LInstruction* instr) {
  CallCodeGeneric(code, mode, instr, RECORD_SIMPLE_SAFEPOINT);
}


void LCodeGen::CallCodeGeneric(Handle<Code> code,
                               RelocInfo::Mode mode,
                               LInstruction* instr,
                               SafepointMode safepoint_mode) {
  DCHECK(instr != NULL);
  __ Call(code, mode);
  RecordSafepointWithLazyDeopt(instr, safepoint_mode);
}


void LCodeGen::CallRuntime(const Runtime::Function* function,
                           int num_arguments,
                           LInstruction* instr,
                           SaveFPRegsMode save_doubles) {
  DCHECK(instr != NULL);

  __ CallRuntime(function, num_arguments, save_doubles);

  RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
}


void LCodeGen::LoadContextFromDeferred(LOperand* context) {
  if (context->IsRegister()) {
    __ Move(cp, ToRegister(context));
  } else if (context->IsStackSlot()) {
    __ ld(cp, ToMemOperand(context));
  } else if (context->IsConstantOperand()) {
    HConstant* constant =
        chunk_->LookupConstant(LConstantOperand::cast(context));
    __ li(cp, Handle<Object>::cast(constant->handle(isolate())));
  } else {
    UNREACHABLE();
  }
}


void LCodeGen::CallRuntimeFromDeferred(Runtime::FunctionId id,
                                       int argc,
                                       LInstruction* instr,
                                       LOperand* context) {
  LoadContextFromDeferred(context);
  __ CallRuntimeSaveDoubles(id);
  RecordSafepointWithRegisters(
      instr->pointer_map(), argc, Safepoint::kNoLazyDeopt);
}


void LCodeGen::RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                                    Safepoint::DeoptMode mode) {
  environment->set_has_been_used();
  if (!environment->HasBeenRegistered()) {
    // Physical stack frame layout:
    // -x ............. -4  0 ..................................... y
    // [incoming arguments] [spill slots] [pushed outgoing arguments]

    // Layout of the environment:
    // 0 ..................................................... size-1
    // [parameters] [locals] [expression stack including arguments]

    // Layout of the translation:
    // 0 ........................................................ size - 1 + 4
    // [expression stack including arguments] [locals] [4 words] [parameters]
    // |>------------  translation_size ------------<|

    int frame_count = 0;
    int jsframe_count = 0;
    for (LEnvironment* e = environment; e != NULL; e = e->outer()) {
      ++frame_count;
      if (e->frame_type() == JS_FUNCTION) {
        ++jsframe_count;
      }
    }
    Translation translation(&translations_, frame_count, jsframe_count, zone());
    WriteTranslation(environment, &translation);
    int deoptimization_index = deoptimizations_.length();
    int pc_offset = masm()->pc_offset();
    environment->Register(deoptimization_index,
                          translation.index(),
                          (mode == Safepoint::kLazyDeopt) ? pc_offset : -1);
    deoptimizations_.Add(environment, zone());
  }
}

void LCodeGen::DeoptimizeIf(Condition condition, LInstruction* instr,
                            DeoptimizeReason deopt_reason,
                            Deoptimizer::BailoutType bailout_type,
                            Register src1, const Operand& src2) {
  LEnvironment* environment = instr->environment();
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);
  DCHECK(environment->HasBeenRegistered());
  int id = environment->deoptimization_index();
  Address entry =
      Deoptimizer::GetDeoptimizationEntry(isolate(), id, bailout_type);
  if (entry == NULL) {
    Abort(kBailoutWasNotPrepared);
    return;
  }

  if (FLAG_deopt_every_n_times != 0 && !info()->IsStub()) {
    Register scratch = scratch0();
    ExternalReference count = ExternalReference::stress_deopt_count(isolate());
    Label no_deopt;
    __ Push(a1, scratch);
    __ li(scratch, Operand(count));
    __ lw(a1, MemOperand(scratch));
    __ Subu(a1, a1, Operand(1));
    __ Branch(&no_deopt, ne, a1, Operand(zero_reg));
    __ li(a1, Operand(FLAG_deopt_every_n_times));
    __ sw(a1, MemOperand(scratch));
    __ Pop(a1, scratch);

    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
    __ bind(&no_deopt);
    __ sw(a1, MemOperand(scratch));
    __ Pop(a1, scratch);
  }

  if (info()->ShouldTrapOnDeopt()) {
    Label skip;
    if (condition != al) {
      __ Branch(&skip, NegateCondition(condition), src1, src2);
    }
    __ stop("trap_on_deopt");
    __ bind(&skip);
  }

  Deoptimizer::DeoptInfo deopt_info = MakeDeoptInfo(instr, deopt_reason, id);

  DCHECK(info()->IsStub() || frame_is_built_);
  // Go through jump table if we need to handle condition, build frame, or
  // restore caller doubles.
  if (condition == al && frame_is_built_ &&
      !info()->saves_caller_doubles()) {
    DeoptComment(deopt_info);
    __ Call(entry, RelocInfo::RUNTIME_ENTRY, condition, src1, src2);
  } else {
    Deoptimizer::JumpTableEntry* table_entry =
        new (zone()) Deoptimizer::JumpTableEntry(
            entry, deopt_info, bailout_type, !frame_is_built_);
    // We often have several deopts to the same entry, reuse the last
    // jump entry if this is the case.
    if (FLAG_trace_deopt || isolate()->is_profiling() ||
        jump_table_.is_empty() ||
        !table_entry->IsEquivalentTo(*jump_table_.last())) {
      jump_table_.Add(table_entry, zone());
    }
    __ Branch(&jump_table_.last()->label, condition, src1, src2);
  }
}

void LCodeGen::DeoptimizeIf(Condition condition, LInstruction* instr,
                            DeoptimizeReason deopt_reason, Register src1,
                            const Operand& src2) {
  Deoptimizer::BailoutType bailout_type = info()->IsStub()
      ? Deoptimizer::LAZY
      : Deoptimizer::EAGER;
  DeoptimizeIf(condition, instr, deopt_reason, bailout_type, src1, src2);
}


void LCodeGen::RecordSafepointWithLazyDeopt(
    LInstruction* instr, SafepointMode safepoint_mode) {
  if (safepoint_mode == RECORD_SIMPLE_SAFEPOINT) {
    RecordSafepoint(instr->pointer_map(), Safepoint::kLazyDeopt);
  } else {
    DCHECK(safepoint_mode == RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 0, Safepoint::kLazyDeopt);
  }
}


void LCodeGen::RecordSafepoint(
    LPointerMap* pointers,
    Safepoint::Kind kind,
    int arguments,
    Safepoint::DeoptMode deopt_mode) {
  DCHECK(expected_safepoint_kind_ == kind);

  const ZoneList<LOperand*>* operands = pointers->GetNormalizedOperands();
  Safepoint safepoint = safepoints_.DefineSafepoint(masm(),
      kind, arguments, deopt_mode);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index(), zone());
    } else if (pointer->IsRegister() && (kind & Safepoint::kWithRegisters)) {
      safepoint.DefinePointerRegister(ToRegister(pointer), zone());
    }
  }
}


void LCodeGen::RecordSafepoint(LPointerMap* pointers,
                               Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(pointers, Safepoint::kSimple, 0, deopt_mode);
}


void LCodeGen::RecordSafepoint(Safepoint::DeoptMode deopt_mode) {
  LPointerMap empty_pointers(zone());
  RecordSafepoint(&empty_pointers, deopt_mode);
}


void LCodeGen::RecordSafepointWithRegisters(LPointerMap* pointers,
                                            int arguments,
                                            Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(
      pointers, Safepoint::kWithRegisters, arguments, deopt_mode);
}


static const char* LabelType(LLabel* label) {
  if (label->is_loop_header()) return " (loop header)";
  if (label->is_osr_entry()) return " (OSR entry)";
  return "";
}


void LCodeGen::DoLabel(LLabel* label) {
  Comment(";;; <@%d,#%d> -------------------- B%d%s --------------------",
          current_instruction_,
          label->hydrogen_value()->id(),
          label->block_id(),
          LabelType(label));
  __ bind(label->label());
  current_block_ = label->block_id();
  DoGap(label);
}


void LCodeGen::DoParallelMove(LParallelMove* move) {
  resolver_.Resolve(move);
}


void LCodeGen::DoGap(LGap* gap) {
  for (int i = LGap::FIRST_INNER_POSITION;
       i <= LGap::LAST_INNER_POSITION;
       i++) {
    LGap::InnerPosition inner_pos = static_cast<LGap::InnerPosition>(i);
    LParallelMove* move = gap->GetParallelMove(inner_pos);
    if (move != NULL) DoParallelMove(move);
  }
}


void LCodeGen::DoInstructionGap(LInstructionGap* instr) {
  DoGap(instr);
}


void LCodeGen::DoParameter(LParameter* instr) {
  // Nothing to do.
}


void LCodeGen::DoUnknownOSRValue(LUnknownOSRValue* instr) {
  GenerateOsrPrologue();
}


void LCodeGen::DoModByPowerOf2I(LModByPowerOf2I* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  DCHECK(dividend.is(ToRegister(instr->result())));

  // Theoretically, a variation of the branch-free code for integer division by
  // a power of 2 (calculating the remainder via an additional multiplication
  // (which gets simplified to an 'and') and subtraction) should be faster, and
  // this is exactly what GCC and clang emit. Nevertheless, benchmarks seem to
  // indicate that positive dividends are heavily favored, so the branching
  // version performs better.
  HMod* hmod = instr->hydrogen();
  int32_t mask = divisor < 0 ? -(divisor + 1) : (divisor - 1);
  Label dividend_is_not_negative, done;

  if (hmod->CheckFlag(HValue::kLeftCanBeNegative)) {
    __ Branch(&dividend_is_not_negative, ge, dividend, Operand(zero_reg));
    // Note: The code below even works when right contains kMinInt.
    __ dsubu(dividend, zero_reg, dividend);
    __ And(dividend, dividend, Operand(mask));
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, dividend,
                   Operand(zero_reg));
    }
    __ Branch(USE_DELAY_SLOT, &done);
    __ dsubu(dividend, zero_reg, dividend);
  }

  __ bind(&dividend_is_not_negative);
  __ And(dividend, dividend, Operand(mask));
  __ bind(&done);
}


void LCodeGen::DoModByConstI(LModByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, DeoptimizeReason::kDivisionByZero);
    return;
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  __ Dmul(result, result, Operand(Abs(divisor)));
  __ Dsubu(result, dividend, Operand(result));

  // Check for negative zero.
  HMod* hmod = instr->hydrogen();
  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label remainder_not_zero;
    __ Branch(&remainder_not_zero, ne, result, Operand(zero_reg));
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero, dividend,
                 Operand(zero_reg));
    __ bind(&remainder_not_zero);
  }
}


void LCodeGen::DoModI(LModI* instr) {
  HMod* hmod = instr->hydrogen();
  const Register left_reg = ToRegister(instr->left());
  const Register right_reg = ToRegister(instr->right());
  const Register result_reg = ToRegister(instr->result());

  // div runs in the background while we check for special cases.
  __ Dmod(result_reg, left_reg, right_reg);

  Label done;
  // Check for x % 0, we have to deopt in this case because we can't return a
  // NaN.
  if (hmod->CheckFlag(HValue::kCanBeDivByZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero, right_reg,
                 Operand(zero_reg));
  }

  // Check for kMinInt % -1, div will return kMinInt, which is not what we
  // want. We have to deopt if we care about -0, because we can't return that.
  if (hmod->CheckFlag(HValue::kCanOverflow)) {
    Label no_overflow_possible;
    __ Branch(&no_overflow_possible, ne, left_reg, Operand(kMinInt));
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, right_reg,
                   Operand(-1));
    } else {
      __ Branch(&no_overflow_possible, ne, right_reg, Operand(-1));
      __ Branch(USE_DELAY_SLOT, &done);
      __ mov(result_reg, zero_reg);
    }
    __ bind(&no_overflow_possible);
  }

  // If we care about -0, test if the dividend is <0 and the result is 0.
  __ Branch(&done, ge, left_reg, Operand(zero_reg));

  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, result_reg,
                 Operand(zero_reg));
  }
  __ bind(&done);
}


void LCodeGen::DoDivByPowerOf2I(LDivByPowerOf2I* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(divisor == kMinInt || base::bits::IsPowerOfTwo32(Abs(divisor)));
  DCHECK(!result.is(dividend));

  // Check for (0 / -x) that will produce negative zero.
  HDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, dividend,
                 Operand(zero_reg));
  }
  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) && divisor == -1) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow, dividend,
                 Operand(kMinInt));
  }
  // Deoptimize if remainder will not be 0.
  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32) &&
      divisor != 1 && divisor != -1) {
    int32_t mask = divisor < 0 ? -(divisor + 1) : (divisor - 1);
    __ And(at, dividend, Operand(mask));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision, at,
                 Operand(zero_reg));
  }

  if (divisor == -1) {  // Nice shortcut, not needed for correctness.
    __ Dsubu(result, zero_reg, dividend);
    return;
  }
  uint16_t shift = WhichPowerOf2Abs(divisor);
  if (shift == 0) {
    __ Move(result, dividend);
  } else if (shift == 1) {
    __ dsrl32(result, dividend, 31);
    __ Daddu(result, dividend, Operand(result));
  } else {
    __ dsra32(result, dividend, 31);
    __ dsrl32(result, result, 32 - shift);
    __ Daddu(result, dividend, Operand(result));
  }
  if (shift > 0) __ dsra(result, result, shift);
  if (divisor < 0) __ Dsubu(result, zero_reg, result);
}


void LCodeGen::DoDivByConstI(LDivByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, DeoptimizeReason::kDivisionByZero);
    return;
  }

  // Check for (0 / -x) that will produce negative zero.
  HDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, dividend,
                 Operand(zero_reg));
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ Subu(result, zero_reg, result);

  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    __ Dmul(scratch0(), result, Operand(divisor));
    __ Dsubu(scratch0(), scratch0(), dividend);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision, scratch0(),
                 Operand(zero_reg));
  }
}


// TODO(svenpanne) Refactor this to avoid code duplication with DoFlooringDivI.
void LCodeGen::DoDivI(LDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  Register dividend = ToRegister(instr->dividend());
  Register divisor = ToRegister(instr->divisor());
  const Register result = ToRegister(instr->result());

  // On MIPS div is asynchronous - it will run in the background while we
  // check for special cases.
  __ Div(result, dividend, divisor);

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero, divisor,
                 Operand(zero_reg));
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label left_not_zero;
    __ Branch(&left_not_zero, ne, dividend, Operand(zero_reg));
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero, divisor,
                 Operand(zero_reg));
    __ bind(&left_not_zero);
  }

  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) &&
      !hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
    Label left_not_min_int;
    __ Branch(&left_not_min_int, ne, dividend, Operand(kMinInt));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow, divisor, Operand(-1));
    __ bind(&left_not_min_int);
  }

  if (!hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
    // Calculate remainder.
    Register remainder = ToRegister(instr->temp());
    if (kArchVariant != kMips64r6) {
      __ mfhi(remainder);
    } else {
      __ dmod(remainder, dividend, divisor);
    }
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision, remainder,
                 Operand(zero_reg));
  }
}


void LCodeGen::DoMultiplyAddD(LMultiplyAddD* instr) {
  DoubleRegister addend = ToDoubleRegister(instr->addend());
  DoubleRegister multiplier = ToDoubleRegister(instr->multiplier());
  DoubleRegister multiplicand = ToDoubleRegister(instr->multiplicand());

  // This is computed in-place.
  DCHECK(addend.is(ToDoubleRegister(instr->result())));

  __ Madd_d(addend, addend, multiplier, multiplicand, double_scratch0());
}


void LCodeGen::DoFlooringDivByPowerOf2I(LFlooringDivByPowerOf2I* instr) {
  Register dividend = ToRegister(instr->dividend());
  Register result = ToRegister(instr->result());
  int32_t divisor = instr->divisor();
  Register scratch = result.is(dividend) ? scratch0() : dividend;
  DCHECK(!result.is(dividend) || !scratch.is(dividend));

  // If the divisor is 1, return the dividend.
  if (divisor == 0) {
    __ Move(result, dividend);
    return;
  }

  // If the divisor is positive, things are easy: There can be no deopts and we
  // can simply do an arithmetic right shift.
  uint16_t shift = WhichPowerOf2Abs(divisor);
  if (divisor > 1) {
    __ dsra(result, dividend, shift);
    return;
  }

  // If the divisor is negative, we have to negate and handle edge cases.
  // Dividend can be the same register as result so save the value of it
  // for checking overflow.
  __ Move(scratch, dividend);

  __ Dsubu(result, zero_reg, dividend);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, result,
                 Operand(zero_reg));
  }

  __ Xor(scratch, scratch, result);
  // Dividing by -1 is basically negation, unless we overflow.
  if (divisor == -1) {
    if (instr->hydrogen()->CheckFlag(HValue::kLeftCanBeMinInt)) {
      DeoptimizeIf(gt, instr, DeoptimizeReason::kOverflow, result,
                   Operand(kMaxInt));
    }
    return;
  }

  // If the negation could not overflow, simply shifting is OK.
  if (!instr->hydrogen()->CheckFlag(HValue::kLeftCanBeMinInt)) {
    __ dsra(result, result, shift);
    return;
  }

  Label no_overflow, done;
  __ Branch(&no_overflow, lt, scratch, Operand(zero_reg));
  __ li(result, Operand(kMinInt / divisor), CONSTANT_SIZE);
  __ Branch(&done);
  __ bind(&no_overflow);
  __ dsra(result, result, shift);
  __ bind(&done);
}


void LCodeGen::DoFlooringDivByConstI(LFlooringDivByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, DeoptimizeReason::kDivisionByZero);
    return;
  }

  // Check for (0 / -x) that will produce negative zero.
  HMathFloorOfDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, dividend,
                 Operand(zero_reg));
  }

  // Easy case: We need no dynamic check for the dividend and the flooring
  // division is the same as the truncating division.
  if ((divisor > 0 && !hdiv->CheckFlag(HValue::kLeftCanBeNegative)) ||
      (divisor < 0 && !hdiv->CheckFlag(HValue::kLeftCanBePositive))) {
    __ TruncatingDiv(result, dividend, Abs(divisor));
    if (divisor < 0) __ Dsubu(result, zero_reg, result);
    return;
  }

  // In the general case we may need to adjust before and after the truncating
  // division to get a flooring division.
  Register temp = ToRegister(instr->temp());
  DCHECK(!temp.is(dividend) && !temp.is(result));
  Label needs_adjustment, done;
  __ Branch(&needs_adjustment, divisor > 0 ? lt : gt,
            dividend, Operand(zero_reg));
  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ Dsubu(result, zero_reg, result);
  __ jmp(&done);
  __ bind(&needs_adjustment);
  __ Daddu(temp, dividend, Operand(divisor > 0 ? 1 : -1));
  __ TruncatingDiv(result, temp, Abs(divisor));
  if (divisor < 0) __ Dsubu(result, zero_reg, result);
  __ Dsubu(result, result, Operand(1));
  __ bind(&done);
}


// TODO(svenpanne) Refactor this to avoid code duplication with DoDivI.
void LCodeGen::DoFlooringDivI(LFlooringDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  Register dividend = ToRegister(instr->dividend());
  Register divisor = ToRegister(instr->divisor());
  const Register result = ToRegister(instr->result());

  // On MIPS div is asynchronous - it will run in the background while we
  // check for special cases.
  __ Ddiv(result, dividend, divisor);

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero, divisor,
                 Operand(zero_reg));
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label left_not_zero;
    __ Branch(&left_not_zero, ne, dividend, Operand(zero_reg));
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero, divisor,
                 Operand(zero_reg));
    __ bind(&left_not_zero);
  }

  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) &&
      !hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
    Label left_not_min_int;
    __ Branch(&left_not_min_int, ne, dividend, Operand(kMinInt));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow, divisor, Operand(-1));
    __ bind(&left_not_min_int);
  }

  // We performed a truncating division. Correct the result if necessary.
  Label done;
  Register remainder = scratch0();
  if (kArchVariant != kMips64r6) {
    __ mfhi(remainder);
  } else {
    __ dmod(remainder, dividend, divisor);
  }
  __ Branch(&done, eq, remainder, Operand(zero_reg), USE_DELAY_SLOT);
  __ Xor(remainder, remainder, Operand(divisor));
  __ Branch(&done, ge, remainder, Operand(zero_reg));
  __ Dsubu(result, result, Operand(1));
  __ bind(&done);
}


void LCodeGen::DoMulS(LMulS* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());
  // Note that result may alias left.
  Register left = ToRegister(instr->left());
  LOperand* right_op = instr->right();

  bool bailout_on_minus_zero =
    instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero);
  bool overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (right_op->IsConstantOperand()) {
    int32_t constant = ToInteger32(LConstantOperand::cast(right_op));

    if (bailout_on_minus_zero && (constant < 0)) {
      // The case of a null constant will be handled separately.
      // If constant is negative and left is null, the result should be -0.
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, left,
                   Operand(zero_reg));
    }

    switch (constant) {
      case -1:
        if (overflow) {
          Label no_overflow;
          __ DsubBranchNoOvf(result, zero_reg, Operand(left), &no_overflow);
          DeoptimizeIf(al, instr);
          __ bind(&no_overflow);
        } else {
          __ Dsubu(result, zero_reg, left);
        }
        break;
      case 0:
        if (bailout_on_minus_zero) {
          // If left is strictly negative and the constant is null, the
          // result is -0. Deoptimize if required, otherwise return 0.
          DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero, left,
                       Operand(zero_reg));
        }
        __ mov(result, zero_reg);
        break;
      case 1:
        // Nothing to do.
        __ Move(result, left);
        break;
      default:
        // Multiplying by powers of two and powers of two plus or minus
        // one can be done faster with shifted operands.
        // For other constants we emit standard code.
        int32_t mask = constant >> 31;
        uint32_t constant_abs = (constant + mask) ^ mask;

        if (base::bits::IsPowerOfTwo32(constant_abs)) {
          int32_t shift = WhichPowerOf2(constant_abs);
          __ dsll(result, left, shift);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ Dsubu(result, zero_reg, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs - 1)) {
          int32_t shift = WhichPowerOf2(constant_abs - 1);
          __ Dlsa(result, left, left, shift);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ Dsubu(result, zero_reg, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs + 1)) {
          int32_t shift = WhichPowerOf2(constant_abs + 1);
          __ dsll(scratch, left, shift);
          __ Dsubu(result, scratch, left);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ Dsubu(result, zero_reg, result);
        } else {
          // Generate standard code.
          __ li(at, constant);
          __ Dmul(result, left, at);
        }
    }
  } else {
    DCHECK(right_op->IsRegister());
    Register right = ToRegister(right_op);

    if (overflow) {
      // hi:lo = left * right.
      __ Dmulh(result, left, right);
      __ dsra32(scratch, result, 0);
      __ sra(at, result, 31);
      __ SmiTag(result);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow, scratch,
                   Operand(at));
    } else {
      __ SmiUntag(result, left);
      __ dmul(result, result, right);
    }

    if (bailout_on_minus_zero) {
      Label done;
      __ Xor(at, left, right);
      __ Branch(&done, ge, at, Operand(zero_reg));
      // Bail out if the result is minus zero.
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, result,
                   Operand(zero_reg));
      __ bind(&done);
    }
  }
}


void LCodeGen::DoMulI(LMulI* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());
  // Note that result may alias left.
  Register left = ToRegister(instr->left());
  LOperand* right_op = instr->right();

  bool bailout_on_minus_zero =
      instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero);
  bool overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (right_op->IsConstantOperand()) {
    int32_t constant = ToInteger32(LConstantOperand::cast(right_op));

    if (bailout_on_minus_zero && (constant < 0)) {
      // The case of a null constant will be handled separately.
      // If constant is negative and left is null, the result should be -0.
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, left,
                   Operand(zero_reg));
    }

    switch (constant) {
      case -1:
        if (overflow) {
          Label no_overflow;
          __ SubBranchNoOvf(result, zero_reg, Operand(left), &no_overflow);
          DeoptimizeIf(al, instr);
          __ bind(&no_overflow);
        } else {
          __ Subu(result, zero_reg, left);
        }
        break;
      case 0:
        if (bailout_on_minus_zero) {
          // If left is strictly negative and the constant is null, the
          // result is -0. Deoptimize if required, otherwise return 0.
          DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero, left,
                       Operand(zero_reg));
        }
        __ mov(result, zero_reg);
        break;
      case 1:
        // Nothing to do.
        __ Move(result, left);
        break;
      default:
        // Multiplying by powers of two and powers of two plus or minus
        // one can be done faster with shifted operands.
        // For other constants we emit standard code.
        int32_t mask = constant >> 31;
        uint32_t constant_abs = (constant + mask) ^ mask;

        if (base::bits::IsPowerOfTwo32(constant_abs)) {
          int32_t shift = WhichPowerOf2(constant_abs);
          __ sll(result, left, shift);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ Subu(result, zero_reg, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs - 1)) {
          int32_t shift = WhichPowerOf2(constant_abs - 1);
          __ Lsa(result, left, left, shift);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ Subu(result, zero_reg, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs + 1)) {
          int32_t shift = WhichPowerOf2(constant_abs + 1);
          __ sll(scratch, left, shift);
          __ Subu(result, scratch, left);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ Subu(result, zero_reg, result);
        } else {
          // Generate standard code.
          __ li(at, constant);
          __ Mul(result, left, at);
        }
    }

  } else {
    DCHECK(right_op->IsRegister());
    Register right = ToRegister(right_op);

    if (overflow) {
      // hi:lo = left * right.
      __ Dmul(result, left, right);
      __ dsra32(scratch, result, 0);
      __ sra(at, result, 31);

      DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow, scratch,
                   Operand(at));
    } else {
      __ mul(result, left, right);
    }

    if (bailout_on_minus_zero) {
      Label done;
      __ Xor(at, left, right);
      __ Branch(&done, ge, at, Operand(zero_reg));
      // Bail out if the result is minus zero.
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, result,
                   Operand(zero_reg));
      __ bind(&done);
    }
  }
}


void LCodeGen::DoBitI(LBitI* instr) {
  LOperand* left_op = instr->left();
  LOperand* right_op = instr->right();
  DCHECK(left_op->IsRegister());
  Register left = ToRegister(left_op);
  Register result = ToRegister(instr->result());
  Operand right(no_reg);

  if (right_op->IsStackSlot()) {
    right = Operand(EmitLoadRegister(right_op, at));
  } else {
    DCHECK(right_op->IsRegister() || right_op->IsConstantOperand());
    right = ToOperand(right_op);
  }

  switch (instr->op()) {
    case Token::BIT_AND:
      __ And(result, left, right);
      break;
    case Token::BIT_OR:
      __ Or(result, left, right);
      break;
    case Token::BIT_XOR:
      if (right_op->IsConstantOperand() && right.immediate() == int32_t(~0)) {
        __ Nor(result, zero_reg, left);
      } else {
        __ Xor(result, left, right);
      }
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoShiftI(LShiftI* instr) {
  // Both 'left' and 'right' are "used at start" (see LCodeGen::DoShift), so
  // result may alias either of them.
  LOperand* right_op = instr->right();
  Register left = ToRegister(instr->left());
  Register result = ToRegister(instr->result());

  if (right_op->IsRegister()) {
    // No need to mask the right operand on MIPS, it is built into the variable
    // shift instructions.
    switch (instr->op()) {
      case Token::ROR:
        __ Ror(result, left, Operand(ToRegister(right_op)));
        break;
      case Token::SAR:
        __ srav(result, left, ToRegister(right_op));
        break;
      case Token::SHR:
        __ srlv(result, left, ToRegister(right_op));
        if (instr->can_deopt()) {
           // TODO(yy): (-1) >>> 0. anything else?
           DeoptimizeIf(lt, instr, DeoptimizeReason::kNegativeValue, result,
                        Operand(zero_reg));
           DeoptimizeIf(gt, instr, DeoptimizeReason::kNegativeValue, result,
                        Operand(kMaxInt));
        }
        break;
      case Token::SHL:
        __ sllv(result, left, ToRegister(right_op));
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    // Mask the right_op operand.
    int value = ToInteger32(LConstantOperand::cast(right_op));
    uint8_t shift_count = static_cast<uint8_t>(value & 0x1F);
    switch (instr->op()) {
      case Token::ROR:
        if (shift_count != 0) {
          __ Ror(result, left, Operand(shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SAR:
        if (shift_count != 0) {
          __ sra(result, left, shift_count);
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SHR:
        if (shift_count != 0) {
          __ srl(result, left, shift_count);
        } else {
          if (instr->can_deopt()) {
            __ And(at, left, Operand(0x80000000));
            DeoptimizeIf(ne, instr, DeoptimizeReason::kNegativeValue, at,
                         Operand(zero_reg));
          }
          __ Move(result, left);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
          if (instr->hydrogen_value()->representation().IsSmi()) {
            __ dsll(result, left, shift_count);
          } else {
            __ sll(result, left, shift_count);
          }
        } else {
          __ Move(result, left);
        }
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoSubS(LSubS* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (!can_overflow) {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ Dsubu(ToRegister(result), ToRegister(left), ToOperand(right));
  } else {  // can_overflow.
    Register scratch = scratch0();
    Label no_overflow_label;
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ DsubBranchNoOvf(ToRegister(result), ToRegister(left), ToOperand(right),
                       &no_overflow_label, scratch);
    DeoptimizeIf(al, instr);
    __ bind(&no_overflow_label);
  }
}


void LCodeGen::DoSubI(LSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (!can_overflow) {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ Subu(ToRegister(result), ToRegister(left), ToOperand(right));
  } else {  // can_overflow.
    Register scratch = scratch0();
    Label no_overflow_label;
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ SubBranchNoOvf(ToRegister(result), ToRegister(left), ToOperand(right),
                      &no_overflow_label, scratch);
    DeoptimizeIf(al, instr);
    __ bind(&no_overflow_label);
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  __ li(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantS(LConstantS* instr) {
  __ li(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  DCHECK(instr->result()->IsDoubleRegister());
  DoubleRegister result = ToDoubleRegister(instr->result());
  double v = instr->value();
  __ Move(result, v);
}


void LCodeGen::DoConstantE(LConstantE* instr) {
  __ li(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantT(LConstantT* instr) {
  Handle<Object> object = instr->value(isolate());
  AllowDeferredHandleDereference smi_check;
  __ li(ToRegister(instr->result()), object);
}


MemOperand LCodeGen::BuildSeqStringOperand(Register string,
                                           LOperand* index,
                                           String::Encoding encoding) {
  if (index->IsConstantOperand()) {
    int offset = ToInteger32(LConstantOperand::cast(index));
    if (encoding == String::TWO_BYTE_ENCODING) {
      offset *= kUC16Size;
    }
    STATIC_ASSERT(kCharSize == 1);
    return FieldMemOperand(string, SeqString::kHeaderSize + offset);
  }
  Register scratch = scratch0();
  DCHECK(!scratch.is(string));
  DCHECK(!scratch.is(ToRegister(index)));
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ Daddu(scratch, string, ToRegister(index));
  } else {
    STATIC_ASSERT(kUC16Size == 2);
    __ dsll(scratch, ToRegister(index), 1);
    __ Daddu(scratch, string, scratch);
  }
  return FieldMemOperand(scratch, SeqString::kHeaderSize);
}


void LCodeGen::DoSeqStringGetChar(LSeqStringGetChar* instr) {
  String::Encoding encoding = instr->hydrogen()->encoding();
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());

  if (FLAG_debug_code) {
    Register scratch = scratch0();
    __ ld(scratch, FieldMemOperand(string, HeapObject::kMapOffset));
    __ lbu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

    __ And(scratch, scratch,
           Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ Dsubu(at, scratch, Operand(encoding == String::ONE_BYTE_ENCODING
                                ? one_byte_seq_type : two_byte_seq_type));
    __ Check(eq, kUnexpectedStringType, at, Operand(zero_reg));
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ lbu(result, operand);
  } else {
    __ lhu(result, operand);
  }
}


void LCodeGen::DoSeqStringSetChar(LSeqStringSetChar* instr) {
  String::Encoding encoding = instr->hydrogen()->encoding();
  Register string = ToRegister(instr->string());
  Register value = ToRegister(instr->value());

  if (FLAG_debug_code) {
    Register scratch = scratch0();
    Register index = ToRegister(instr->index());
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    int encoding_mask =
        instr->hydrogen()->encoding() == String::ONE_BYTE_ENCODING
        ? one_byte_seq_type : two_byte_seq_type;
    __ EmitSeqStringSetCharCheck(string, index, value, scratch, encoding_mask);
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ sb(value, operand);
  } else {
    __ sh(value, operand);
  }
}


void LCodeGen::DoAddE(LAddE* instr) {
  LOperand* result = instr->result();
  LOperand* left = instr->left();
  LOperand* right = instr->right();

  DCHECK(!instr->hydrogen()->CheckFlag(HValue::kCanOverflow));
  DCHECK(right->IsRegister() || right->IsConstantOperand());
  __ Daddu(ToRegister(result), ToRegister(left), ToOperand(right));
}


void LCodeGen::DoAddS(LAddS* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (!can_overflow) {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ Daddu(ToRegister(result), ToRegister(left), ToOperand(right));
  } else {  // can_overflow.
    Label no_overflow_label;
    Register scratch = scratch1();
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ DaddBranchNoOvf(ToRegister(result), ToRegister(left), ToOperand(right),
                       &no_overflow_label, scratch);
    DeoptimizeIf(al, instr);
    __ bind(&no_overflow_label);
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (!can_overflow) {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ Addu(ToRegister(result), ToRegister(left), ToOperand(right));
  } else {  // can_overflow.
    Label no_overflow_label;
    Register scratch = scratch1();
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ AddBranchNoOvf(ToRegister(result), ToRegister(left), ToOperand(right),
                      &no_overflow_label, scratch);
    DeoptimizeIf(al, instr);
    __ bind(&no_overflow_label);
  }
}


void LCodeGen::DoMathMinMax(LMathMinMax* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  HMathMinMax::Operation operation = instr->hydrogen()->operation();
  Register scratch = scratch1();
  if (instr->hydrogen()->representation().IsSmiOrInteger32()) {
    Condition condition = (operation == HMathMinMax::kMathMin) ? le : ge;
    Register left_reg = ToRegister(left);
    Register right_reg = EmitLoadRegister(right, scratch0());
    Register result_reg = ToRegister(instr->result());
    Label return_right, done;
    __ Slt(scratch, left_reg, Operand(right_reg));
    if (condition == ge) {
     __  Movz(result_reg, left_reg, scratch);
     __  Movn(result_reg, right_reg, scratch);
    } else {
     DCHECK(condition == le);
     __  Movn(result_reg, left_reg, scratch);
     __  Movz(result_reg, right_reg, scratch);
    }
  } else {
    DCHECK(instr->hydrogen()->representation().IsDouble());
    FPURegister left_reg = ToDoubleRegister(left);
    FPURegister right_reg = ToDoubleRegister(right);
    FPURegister result_reg = ToDoubleRegister(instr->result());
    Label nan, done;
    if (operation == HMathMinMax::kMathMax) {
      __ Float64Max(result_reg, left_reg, right_reg, &nan);
    } else {
      DCHECK(operation == HMathMinMax::kMathMin);
      __ Float64Min(result_reg, left_reg, right_reg, &nan);
    }
    __ Branch(&done);

    __ bind(&nan);
    __ add_d(result_reg, left_reg, right_reg);

    __ bind(&done);
  }
}


void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  DoubleRegister left = ToDoubleRegister(instr->left());
  DoubleRegister right = ToDoubleRegister(instr->right());
  DoubleRegister result = ToDoubleRegister(instr->result());
  switch (instr->op()) {
    case Token::ADD:
      __ add_d(result, left, right);
      break;
    case Token::SUB:
      __ sub_d(result, left, right);
      break;
    case Token::MUL:
      __ mul_d(result, left, right);
      break;
    case Token::DIV:
      __ div_d(result, left, right);
      break;
    case Token::MOD: {
      // Save a0-a3 on the stack.
      RegList saved_regs = a0.bit() | a1.bit() | a2.bit() | a3.bit();
      __ MultiPush(saved_regs);

      __ PrepareCallCFunction(0, 2, scratch0());
      __ MovToFloatParameters(left, right);
      __ CallCFunction(
          ExternalReference::mod_two_doubles_operation(isolate()),
          0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(result);

      // Restore saved register.
      __ MultiPop(saved_regs);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(a1));
  DCHECK(ToRegister(instr->right()).is(a0));
  DCHECK(ToRegister(instr->result()).is(v0));

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), instr->op()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
  // Other arch use a nop here, to signal that there is no inlined
  // patchable code. Mips does not need the nop, since our marker
  // instruction (andi zero_reg) will never be used in normal code.
}


template<class InstrType>
void LCodeGen::EmitBranch(InstrType instr,
                          Condition condition,
                          Register src1,
                          const Operand& src2) {
  int left_block = instr->TrueDestination(chunk_);
  int right_block = instr->FalseDestination(chunk_);

  int next_block = GetNextEmittedBlock();
  if (right_block == left_block || condition == al) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ Branch(chunk_->GetAssemblyLabel(right_block),
              NegateCondition(condition), src1, src2);
  } else if (right_block == next_block) {
    __ Branch(chunk_->GetAssemblyLabel(left_block), condition, src1, src2);
  } else {
    __ Branch(chunk_->GetAssemblyLabel(left_block), condition, src1, src2);
    __ Branch(chunk_->GetAssemblyLabel(right_block));
  }
}


template<class InstrType>
void LCodeGen::EmitBranchF(InstrType instr,
                           Condition condition,
                           FPURegister src1,
                           FPURegister src2) {
  int right_block = instr->FalseDestination(chunk_);
  int left_block = instr->TrueDestination(chunk_);

  int next_block = GetNextEmittedBlock();
  if (right_block == left_block) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ BranchF(chunk_->GetAssemblyLabel(right_block), NULL,
               NegateFpuCondition(condition), src1, src2);
  } else if (right_block == next_block) {
    __ BranchF(chunk_->GetAssemblyLabel(left_block), NULL,
               condition, src1, src2);
  } else {
    __ BranchF(chunk_->GetAssemblyLabel(left_block), NULL,
               condition, src1, src2);
    __ Branch(chunk_->GetAssemblyLabel(right_block));
  }
}


template <class InstrType>
void LCodeGen::EmitTrueBranch(InstrType instr, Condition condition,
                              Register src1, const Operand& src2) {
  int true_block = instr->TrueDestination(chunk_);
  __ Branch(chunk_->GetAssemblyLabel(true_block), condition, src1, src2);
}


template <class InstrType>
void LCodeGen::EmitFalseBranch(InstrType instr, Condition condition,
                               Register src1, const Operand& src2) {
  int false_block = instr->FalseDestination(chunk_);
  __ Branch(chunk_->GetAssemblyLabel(false_block), condition, src1, src2);
}


template<class InstrType>
void LCodeGen::EmitFalseBranchF(InstrType instr,
                                Condition condition,
                                FPURegister src1,
                                FPURegister src2) {
  int false_block = instr->FalseDestination(chunk_);
  __ BranchF(chunk_->GetAssemblyLabel(false_block), NULL,
             condition, src1, src2);
}


void LCodeGen::DoDebugBreak(LDebugBreak* instr) {
  __ stop("LDebugBreak");
}


void LCodeGen::DoBranch(LBranch* instr) {
  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsInteger32() || r.IsSmi()) {
    DCHECK(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    EmitBranch(instr, ne, reg, Operand(zero_reg));
  } else if (r.IsDouble()) {
    DCHECK(!info()->IsStub());
    DoubleRegister reg = ToDoubleRegister(instr->value());
    // Test the double value. Zero and NaN are false.
    EmitBranchF(instr, ogl, reg, kDoubleRegZero);
  } else {
    DCHECK(r.IsTagged());
    Register reg = ToRegister(instr->value());
    HType type = instr->hydrogen()->value()->type();
    if (type.IsBoolean()) {
      DCHECK(!info()->IsStub());
      __ LoadRoot(at, Heap::kTrueValueRootIndex);
      EmitBranch(instr, eq, reg, Operand(at));
    } else if (type.IsSmi()) {
      DCHECK(!info()->IsStub());
      EmitBranch(instr, ne, reg, Operand(zero_reg));
    } else if (type.IsJSArray()) {
      DCHECK(!info()->IsStub());
      EmitBranch(instr, al, zero_reg, Operand(zero_reg));
    } else if (type.IsHeapNumber()) {
      DCHECK(!info()->IsStub());
      DoubleRegister dbl_scratch = double_scratch0();
      __ ldc1(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
      // Test the double value. Zero and NaN are false.
      EmitBranchF(instr, ogl, dbl_scratch, kDoubleRegZero);
    } else if (type.IsString()) {
      DCHECK(!info()->IsStub());
      __ ld(at, FieldMemOperand(reg, String::kLengthOffset));
      EmitBranch(instr, ne, at, Operand(zero_reg));
    } else {
      ToBooleanHints expected = instr->hydrogen()->expected_input_types();
      // Avoid deopts in the case where we've never executed this path before.
      if (expected == ToBooleanHint::kNone) expected = ToBooleanHint::kAny;

      if (expected & ToBooleanHint::kUndefined) {
        // undefined -> false.
        __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
        __ Branch(instr->FalseLabel(chunk_), eq, reg, Operand(at));
      }
      if (expected & ToBooleanHint::kBoolean) {
        // Boolean -> its value.
        __ LoadRoot(at, Heap::kTrueValueRootIndex);
        __ Branch(instr->TrueLabel(chunk_), eq, reg, Operand(at));
        __ LoadRoot(at, Heap::kFalseValueRootIndex);
        __ Branch(instr->FalseLabel(chunk_), eq, reg, Operand(at));
      }
      if (expected & ToBooleanHint::kNull) {
        // 'null' -> false.
        __ LoadRoot(at, Heap::kNullValueRootIndex);
        __ Branch(instr->FalseLabel(chunk_), eq, reg, Operand(at));
      }

      if (expected & ToBooleanHint::kSmallInteger) {
        // Smis: 0 -> false, all other -> true.
        __ Branch(instr->FalseLabel(chunk_), eq, reg, Operand(zero_reg));
        __ JumpIfSmi(reg, instr->TrueLabel(chunk_));
      } else if (expected & ToBooleanHint::kNeedsMap) {
        // If we need a map later and have a Smi -> deopt.
        __ SmiTst(reg, at);
        DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, at, Operand(zero_reg));
      }

      const Register map = scratch0();
      if (expected & ToBooleanHint::kNeedsMap) {
        __ ld(map, FieldMemOperand(reg, HeapObject::kMapOffset));
        if (expected & ToBooleanHint::kCanBeUndetectable) {
          // Undetectable -> false.
          __ lbu(at, FieldMemOperand(map, Map::kBitFieldOffset));
          __ And(at, at, Operand(1 << Map::kIsUndetectable));
          __ Branch(instr->FalseLabel(chunk_), ne, at, Operand(zero_reg));
        }
      }

      if (expected & ToBooleanHint::kReceiver) {
        // spec object -> true.
        __ lbu(at, FieldMemOperand(map, Map::kInstanceTypeOffset));
        __ Branch(instr->TrueLabel(chunk_),
                  ge, at, Operand(FIRST_JS_RECEIVER_TYPE));
      }

      if (expected & ToBooleanHint::kString) {
        // String value -> false iff empty.
        Label not_string;
        __ lbu(at, FieldMemOperand(map, Map::kInstanceTypeOffset));
        __ Branch(&not_string, ge , at, Operand(FIRST_NONSTRING_TYPE));
        __ ld(at, FieldMemOperand(reg, String::kLengthOffset));
        __ Branch(instr->TrueLabel(chunk_), ne, at, Operand(zero_reg));
        __ Branch(instr->FalseLabel(chunk_));
        __ bind(&not_string);
      }

      if (expected & ToBooleanHint::kSymbol) {
        // Symbol value -> true.
        const Register scratch = scratch1();
        __ lbu(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
        __ Branch(instr->TrueLabel(chunk_), eq, scratch, Operand(SYMBOL_TYPE));
      }

      if (expected & ToBooleanHint::kSimdValue) {
        // SIMD value -> true.
        const Register scratch = scratch1();
        __ lbu(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
        __ Branch(instr->TrueLabel(chunk_), eq, scratch,
                  Operand(SIMD128_VALUE_TYPE));
      }

      if (expected & ToBooleanHint::kHeapNumber) {
        // heap number -> false iff +0, -0, or NaN.
        DoubleRegister dbl_scratch = double_scratch0();
        Label not_heap_number;
        __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
        __ Branch(&not_heap_number, ne, map, Operand(at));
        __ ldc1(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
        __ BranchF(instr->TrueLabel(chunk_), instr->FalseLabel(chunk_),
                   ne, dbl_scratch, kDoubleRegZero);
        // Falls through if dbl_scratch == 0.
        __ Branch(instr->FalseLabel(chunk_));
        __ bind(&not_heap_number);
      }

      if (expected != ToBooleanHint::kAny) {
        // We've seen something for the first time -> deopt.
        // This can only happen if we are not generic already.
        DeoptimizeIf(al, instr, DeoptimizeReason::kUnexpectedObject, zero_reg,
                     Operand(zero_reg));
      }
    }
  }
}


void LCodeGen::EmitGoto(int block) {
  if (!IsNextEmittedBlock(block)) {
    __ jmp(chunk_->GetAssemblyLabel(LookupDestination(block)));
  }
}


void LCodeGen::DoGoto(LGoto* instr) {
  EmitGoto(instr->block_id());
}


Condition LCodeGen::TokenToCondition(Token::Value op, bool is_unsigned) {
  Condition cond = kNoCondition;
  switch (op) {
    case Token::EQ:
    case Token::EQ_STRICT:
      cond = eq;
      break;
    case Token::NE:
    case Token::NE_STRICT:
      cond = ne;
      break;
    case Token::LT:
      cond = is_unsigned ? lo : lt;
      break;
    case Token::GT:
      cond = is_unsigned ? hi : gt;
      break;
    case Token::LTE:
      cond = is_unsigned ? ls : le;
      break;
    case Token::GTE:
      cond = is_unsigned ? hs : ge;
      break;
    case Token::IN:
    case Token::INSTANCEOF:
    default:
      UNREACHABLE();
  }
  return cond;
}


void LCodeGen::DoCompareNumericAndBranch(LCompareNumericAndBranch* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  bool is_unsigned =
      instr->hydrogen()->left()->CheckFlag(HInstruction::kUint32) ||
      instr->hydrogen()->right()->CheckFlag(HInstruction::kUint32);
  Condition cond = TokenToCondition(instr->op(), is_unsigned);

  if (left->IsConstantOperand() && right->IsConstantOperand()) {
    // We can statically evaluate the comparison.
    double left_val = ToDouble(LConstantOperand::cast(left));
    double right_val = ToDouble(LConstantOperand::cast(right));
    int next_block = Token::EvalComparison(instr->op(), left_val, right_val)
                         ? instr->TrueDestination(chunk_)
                         : instr->FalseDestination(chunk_);
    EmitGoto(next_block);
  } else {
    if (instr->is_double()) {
      // Compare left and right as doubles and load the
      // resulting flags into the normal status register.
      FPURegister left_reg = ToDoubleRegister(left);
      FPURegister right_reg = ToDoubleRegister(right);

      // If a NaN is involved, i.e. the result is unordered,
      // jump to false block label.
      __ BranchF(NULL, instr->FalseLabel(chunk_), eq,
                 left_reg, right_reg);

      EmitBranchF(instr, cond, left_reg, right_reg);
    } else {
      Register cmp_left;
      Operand cmp_right = Operand((int64_t)0);
      if (right->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(right));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          cmp_left = ToRegister(left);
          cmp_right = Operand(Smi::FromInt(value));
        } else {
          cmp_left = ToRegister(left);
          cmp_right = Operand(value);
        }
      } else if (left->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(left));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          cmp_left = ToRegister(right);
          cmp_right = Operand(Smi::FromInt(value));
        } else {
          cmp_left = ToRegister(right);
          cmp_right = Operand(value);
        }
        // We commuted the operands, so commute the condition.
        cond = CommuteCondition(cond);
      } else {
        cmp_left = ToRegister(left);
        cmp_right = Operand(ToRegister(right));
      }

      EmitBranch(instr, cond, cmp_left, cmp_right);
    }
  }
}


void LCodeGen::DoCmpObjectEqAndBranch(LCmpObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());

  EmitBranch(instr, eq, left, Operand(right));
}


void LCodeGen::DoCmpHoleAndBranch(LCmpHoleAndBranch* instr) {
  if (instr->hydrogen()->representation().IsTagged()) {
    Register input_reg = ToRegister(instr->object());
    __ li(at, Operand(factory()->the_hole_value()));
    EmitBranch(instr, eq, input_reg, Operand(at));
    return;
  }

  DoubleRegister input_reg = ToDoubleRegister(instr->object());
  EmitFalseBranchF(instr, eq, input_reg, input_reg);

  Register scratch = scratch0();
  __ FmoveHigh(scratch, input_reg);
  EmitBranch(instr, eq, scratch,
             Operand(static_cast<int32_t>(kHoleNanUpper32)));
}


Condition LCodeGen::EmitIsString(Register input,
                                 Register temp1,
                                 Label* is_not_string,
                                 SmiCheck check_needed = INLINE_SMI_CHECK) {
  if (check_needed == INLINE_SMI_CHECK) {
    __ JumpIfSmi(input, is_not_string);
  }
  __ GetObjectType(input, temp1, temp1);

  return lt;
}


void LCodeGen::DoIsStringAndBranch(LIsStringAndBranch* instr) {
  Register reg = ToRegister(instr->value());
  Register temp1 = ToRegister(instr->temp());

  SmiCheck check_needed =
      instr->hydrogen()->value()->type().IsHeapObject()
          ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
  Condition true_cond =
      EmitIsString(reg, temp1, instr->FalseLabel(chunk_), check_needed);

  EmitBranch(instr, true_cond, temp1,
             Operand(FIRST_NONSTRING_TYPE));
}


void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  Register input_reg = EmitLoadRegister(instr->value(), at);
  __ And(at, input_reg, kSmiTagMask);
  EmitBranch(instr, eq, at, Operand(zero_reg));
}


void LCodeGen::DoIsUndetectableAndBranch(LIsUndetectableAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }
  __ ld(temp, FieldMemOperand(input, HeapObject::kMapOffset));
  __ lbu(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  __ And(at, temp, Operand(1 << Map::kIsUndetectable));
  EmitBranch(instr, ne, at, Operand(zero_reg));
}


static Condition ComputeCompareCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}


void LCodeGen::DoStringCompareAndBranch(LStringCompareAndBranch* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(a1));
  DCHECK(ToRegister(instr->right()).is(a0));

  Handle<Code> code = CodeFactory::StringCompare(isolate(), instr->op()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
  __ LoadRoot(at, Heap::kTrueValueRootIndex);
  EmitBranch(instr, eq, v0, Operand(at));
}


static InstanceType TestType(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == FIRST_TYPE) return to;
  DCHECK(from == to || to == LAST_TYPE);
  return from;
}


static Condition BranchCondition(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == to) return eq;
  if (to == LAST_TYPE) return hs;
  if (from == FIRST_TYPE) return ls;
  UNREACHABLE();
  return eq;
}


void LCodeGen::DoHasInstanceTypeAndBranch(LHasInstanceTypeAndBranch* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->value());

  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }

  __ GetObjectType(input, scratch, scratch);
  EmitBranch(instr,
             BranchCondition(instr->hydrogen()),
             scratch,
             Operand(TestType(instr->hydrogen())));
}

// Branches to a label or falls through with the answer in flags.  Trashes
// the temp registers, but not the input.
void LCodeGen::EmitClassOfTest(Label* is_true,
                               Label* is_false,
                               Handle<String>class_name,
                               Register input,
                               Register temp,
                               Register temp2) {
  DCHECK(!input.is(temp));
  DCHECK(!input.is(temp2));
  DCHECK(!temp.is(temp2));

  __ JumpIfSmi(input, is_false);

  __ GetObjectType(input, temp, temp2);
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  if (String::Equals(isolate()->factory()->Function_string(), class_name)) {
    __ Branch(is_true, hs, temp2, Operand(FIRST_FUNCTION_TYPE));
  } else {
    __ Branch(is_false, hs, temp2, Operand(FIRST_FUNCTION_TYPE));
  }

  // Now we are in the FIRST-LAST_NONCALLABLE_SPEC_OBJECT_TYPE range.
  // Check if the constructor in the map is a function.
  Register instance_type = scratch1();
  DCHECK(!instance_type.is(temp));
  __ GetMapConstructor(temp, temp, temp2, instance_type);

  // Objects with a non-function constructor have class 'Object'.
  if (String::Equals(class_name, isolate()->factory()->Object_string())) {
    __ Branch(is_true, ne, instance_type, Operand(JS_FUNCTION_TYPE));
  } else {
    __ Branch(is_false, ne, instance_type, Operand(JS_FUNCTION_TYPE));
  }

  // temp now contains the constructor function. Grab the
  // instance class name from there.
  __ ld(temp, FieldMemOperand(temp, JSFunction::kSharedFunctionInfoOffset));
  __ ld(temp, FieldMemOperand(temp,
                               SharedFunctionInfo::kInstanceClassNameOffset));
  // The class name we are testing against is internalized since it's a literal.
  // The name in the constructor is internalized because of the way the context
  // is booted.  This routine isn't expected to work for random API-created
  // classes and it doesn't have to because you can't access it with natives
  // syntax.  Since both sides are internalized it is sufficient to use an
  // identity comparison.

  // End with the address of this class_name instance in temp register.
  // On MIPS, the caller must do the comparison with Handle<String>class_name.
}


void LCodeGen::DoClassOfTestAndBranch(LClassOfTestAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = scratch0();
  Register temp2 = ToRegister(instr->temp());
  Handle<String> class_name = instr->hydrogen()->class_name();

  EmitClassOfTest(instr->TrueLabel(chunk_), instr->FalseLabel(chunk_),
                  class_name, input, temp, temp2);

  EmitBranch(instr, eq, temp, Operand(class_name));
}


void LCodeGen::DoCmpMapAndBranch(LCmpMapAndBranch* instr) {
  Register reg = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  __ ld(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  EmitBranch(instr, eq, temp, Operand(instr->map()));
}


void LCodeGen::DoHasInPrototypeChainAndBranch(
    LHasInPrototypeChainAndBranch* instr) {
  Register const object = ToRegister(instr->object());
  Register const object_map = scratch0();
  Register const object_instance_type = scratch1();
  Register const object_prototype = object_map;
  Register const prototype = ToRegister(instr->prototype());

  // The {object} must be a spec object.  It's sufficient to know that {object}
  // is not a smi, since all other non-spec objects have {null} prototypes and
  // will be ruled out below.
  if (instr->hydrogen()->ObjectNeedsSmiCheck()) {
    __ SmiTst(object, at);
    EmitFalseBranch(instr, eq, at, Operand(zero_reg));
  }

  // Loop through the {object}s prototype chain looking for the {prototype}.
  __ ld(object_map, FieldMemOperand(object, HeapObject::kMapOffset));
  Label loop;
  __ bind(&loop);

  // Deoptimize if the object needs to be access checked.
  __ lbu(object_instance_type,
         FieldMemOperand(object_map, Map::kBitFieldOffset));
  __ And(object_instance_type, object_instance_type,
         Operand(1 << Map::kIsAccessCheckNeeded));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kAccessCheck, object_instance_type,
               Operand(zero_reg));
  __ lbu(object_instance_type,
         FieldMemOperand(object_map, Map::kInstanceTypeOffset));
  DeoptimizeIf(eq, instr, DeoptimizeReason::kProxy, object_instance_type,
               Operand(JS_PROXY_TYPE));

  __ ld(object_prototype, FieldMemOperand(object_map, Map::kPrototypeOffset));
  __ LoadRoot(at, Heap::kNullValueRootIndex);
  EmitFalseBranch(instr, eq, object_prototype, Operand(at));
  EmitTrueBranch(instr, eq, object_prototype, Operand(prototype));
  __ Branch(&loop, USE_DELAY_SLOT);
  __ ld(object_map, FieldMemOperand(object_prototype,
                                    HeapObject::kMapOffset));  // In delay slot.
}


void LCodeGen::DoCmpT(LCmpT* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  Token::Value op = instr->op();

  Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  // On MIPS there is no need for a "no inlined smi code" marker (nop).

  Condition condition = ComputeCompareCondition(op);
  // A minor optimization that relies on LoadRoot always emitting one
  // instruction.
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm());
  Label done, check;
  __ Branch(USE_DELAY_SLOT, &done, condition, v0, Operand(zero_reg));
  __ bind(&check);
  __ LoadRoot(ToRegister(instr->result()), Heap::kTrueValueRootIndex);
  DCHECK_EQ(1, masm()->InstructionsGeneratedSince(&check));
  __ LoadRoot(ToRegister(instr->result()), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace && info()->IsOptimizing()) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in v0. We're leaving the code
    // managed by the register allocator and tearing down the frame, it's
    // safe to write to the context register.
    __ push(v0);
    __ ld(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kTraceExit);
  }
  if (info()->saves_caller_doubles()) {
    RestoreCallerDoubles();
  }
  if (NeedsEagerFrame()) {
    __ mov(sp, fp);
    __ Pop(ra, fp);
  }
  if (instr->has_constant_parameter_count()) {
    int parameter_count = ToInteger32(instr->constant_parameter_count());
    int32_t sp_delta = (parameter_count + 1) * kPointerSize;
    if (sp_delta != 0) {
      __ Daddu(sp, sp, Operand(sp_delta));
    }
  } else {
    DCHECK(info()->IsStub());  // Functions would need to drop one more value.
    Register reg = ToRegister(instr->parameter_count());
    // The argument count parameter is a smi
    __ SmiUntag(reg);
    __ Dlsa(sp, sp, reg, kPointerSizeLog2);
  }

  __ Jump(ra);
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());

  __ ld(result, ContextMemOperand(context, instr->slot_index()));
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);

    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole, result, Operand(at));
    } else {
      Label is_not_hole;
      __ Branch(&is_not_hole, ne, result, Operand(at));
      __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
      __ bind(&is_not_hole);
    }
  }
}


void LCodeGen::DoStoreContextSlot(LStoreContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register value = ToRegister(instr->value());
  Register scratch = scratch0();
  MemOperand target = ContextMemOperand(context, instr->slot_index());

  Label skip_assignment;

  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ ld(scratch, target);
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);

    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole, scratch, Operand(at));
    } else {
      __ Branch(&skip_assignment, ne, scratch, Operand(at));
    }
  }

  __ sd(value, target);
  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed =
        instr->hydrogen()->value()->type().IsHeapObject()
            ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
    __ RecordWriteContextSlot(context,
                              target.offset(),
                              value,
                              scratch0(),
                              GetRAState(),
                              kSaveFPRegs,
                              EMIT_REMEMBERED_SET,
                              check_needed);
  }

  __ bind(&skip_assignment);
}


void LCodeGen::DoLoadNamedField(LLoadNamedField* instr) {
  HObjectAccess access = instr->hydrogen()->access();
  int offset = access.offset();
  Register object = ToRegister(instr->object());
  if (access.IsExternalMemory()) {
    Register result = ToRegister(instr->result());
    MemOperand operand = MemOperand(object, offset);
    __ Load(result, operand, access.representation());
    return;
  }

  if (instr->hydrogen()->representation().IsDouble()) {
    DoubleRegister result = ToDoubleRegister(instr->result());
    __ ldc1(result, FieldMemOperand(object, offset));
    return;
  }

  Register result = ToRegister(instr->result());
  if (!access.IsInobject()) {
    __ ld(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    object = result;
  }

  Representation representation = access.representation();
  if (representation.IsSmi() && SmiValuesAre32Bits() &&
      instr->hydrogen()->representation().IsInteger32()) {
    if (FLAG_debug_code) {
      // Verify this is really an Smi.
      Register scratch = scratch0();
      __ Load(scratch, FieldMemOperand(object, offset), representation);
      __ AssertSmi(scratch);
    }

    // Read int value directly from upper half of the smi.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 32);
    offset = SmiWordOffset(offset);
    representation = Representation::Integer32();
  }
  __ Load(result, FieldMemOperand(object, offset), representation);
}


void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register scratch = scratch0();
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());

  // Get the prototype or initial map from the function.
  __ ld(result,
         FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kHole, result, Operand(at));

  // If the function does not have an initial map, we're done.
  Label done;
  __ GetObjectType(result, scratch, scratch);
  __ Branch(&done, ne, scratch, Operand(MAP_TYPE));

  // Get the prototype from the initial map.
  __ ld(result, FieldMemOperand(result, Map::kPrototypeOffset));

  // All done.
  __ bind(&done);
}


void LCodeGen::DoLoadRoot(LLoadRoot* instr) {
  Register result = ToRegister(instr->result());
  __ LoadRoot(result, instr->index());
}


void LCodeGen::DoAccessArgumentsAt(LAccessArgumentsAt* instr) {
  Register arguments = ToRegister(instr->arguments());
  Register result = ToRegister(instr->result());
  // There are two words between the frame pointer and the last argument.
  // Subtracting from length accounts for one of them add one more.
  if (instr->length()->IsConstantOperand()) {
    int const_length = ToInteger32(LConstantOperand::cast(instr->length()));
    if (instr->index()->IsConstantOperand()) {
      int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
      int index = (const_length - const_index) + 1;
      __ ld(result, MemOperand(arguments, index * kPointerSize));
    } else {
      Register index = ToRegister(instr->index());
      __ li(at, Operand(const_length + 1));
      __ Dsubu(result, at, index);
      __ Dlsa(at, arguments, result, kPointerSizeLog2);
      __ ld(result, MemOperand(at));
    }
  } else if (instr->index()->IsConstantOperand()) {
    Register length = ToRegister(instr->length());
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    int loc = const_index - 1;
    if (loc != 0) {
      __ Dsubu(result, length, Operand(loc));
      __ Dlsa(at, arguments, result, kPointerSizeLog2);
      __ ld(result, MemOperand(at));
    } else {
      __ Dlsa(at, arguments, length, kPointerSizeLog2);
      __ ld(result, MemOperand(at));
    }
  } else {
    Register length = ToRegister(instr->length());
    Register index = ToRegister(instr->index());
    __ Dsubu(result, length, index);
    __ Daddu(result, result, 1);
    __ Dlsa(at, arguments, result, kPointerSizeLog2);
    __ ld(result, MemOperand(at));
  }
}


void LCodeGen::DoLoadKeyedExternalArray(LLoadKeyed* instr) {
  Register external_pointer = ToRegister(instr->elements());
  Register key = no_reg;
  ElementsKind elements_kind = instr->elements_kind();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }
  int element_size_shift = ElementsKindToShiftSize(elements_kind);
  int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
      ? (element_size_shift - (kSmiTagSize + kSmiShiftSize))
      : element_size_shift;
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    FPURegister result = ToDoubleRegister(instr->result());
    if (key_is_constant) {
      __ Daddu(scratch0(), external_pointer,
          constant_key << element_size_shift);
    } else {
      if (shift_size < 0) {
         if (shift_size == -32) {
           __ dsra32(scratch0(), key, 0);
         } else {
           __ dsra(scratch0(), key, -shift_size);
         }
      } else {
        __ dsll(scratch0(), key, shift_size);
      }
      __ Daddu(scratch0(), scratch0(), external_pointer);
    }
    if (elements_kind == FLOAT32_ELEMENTS) {
      __ lwc1(result, MemOperand(scratch0(), base_offset));
      __ cvt_d_s(result, result);
    } else  {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ ldc1(result, MemOperand(scratch0(), base_offset));
    }
  } else {
    Register result = ToRegister(instr->result());
    MemOperand mem_operand = PrepareKeyedOperand(
        key, external_pointer, key_is_constant, constant_key,
        element_size_shift, shift_size, base_offset);
    switch (elements_kind) {
      case INT8_ELEMENTS:
        __ lb(result, mem_operand);
        break;
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
        __ lbu(result, mem_operand);
        break;
      case INT16_ELEMENTS:
        __ lh(result, mem_operand);
        break;
      case UINT16_ELEMENTS:
        __ lhu(result, mem_operand);
        break;
      case INT32_ELEMENTS:
        __ lw(result, mem_operand);
        break;
      case UINT32_ELEMENTS:
        __ lw(result, mem_operand);
        if (!instr->hydrogen()->CheckFlag(HInstruction::kUint32)) {
          DeoptimizeIf(Ugreater_equal, instr, DeoptimizeReason::kNegativeValue,
                       result, Operand(0x80000000));
        }
        break;
      case FLOAT32_ELEMENTS:
      case FLOAT64_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      case FAST_STRING_WRAPPER_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
      case NO_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr) {
  Register elements = ToRegister(instr->elements());
  bool key_is_constant = instr->key()->IsConstantOperand();
  Register key = no_reg;
  DoubleRegister result = ToDoubleRegister(instr->result());
  Register scratch = scratch0();

  int element_size_shift = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);

  int base_offset = instr->base_offset();
  if (key_is_constant) {
    int constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
    base_offset += constant_key * kDoubleSize;
  }
  __ Daddu(scratch, elements, Operand(base_offset));

  if (!key_is_constant) {
    key = ToRegister(instr->key());
    int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
        ? (element_size_shift - (kSmiTagSize + kSmiShiftSize))
        : element_size_shift;
    if (shift_size > 0) {
      __ dsll(at, key, shift_size);
    } else if (shift_size == -32) {
      __ dsra32(at, key, 0);
    } else {
      __ dsra(at, key, -shift_size);
    }
    __ Daddu(scratch, scratch, at);
  }

  __ ldc1(result, MemOperand(scratch));

  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ FmoveHigh(scratch, result);
    DeoptimizeIf(eq, instr, DeoptimizeReason::kHole, scratch,
                 Operand(static_cast<int32_t>(kHoleNanUpper32)));
  }
}


void LCodeGen::DoLoadKeyedFixedArray(LLoadKeyed* instr) {
  HLoadKeyed* hinstr = instr->hydrogen();
  Register elements = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  Register store_base = scratch;
  int offset = instr->base_offset();

  if (instr->key()->IsConstantOperand()) {
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset += ToInteger32(const_operand) * kPointerSize;
    store_base = elements;
  } else {
    Register key = ToRegister(instr->key());
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (instr->hydrogen()->key()->representation().IsSmi()) {
    __ SmiScale(scratch, key, kPointerSizeLog2);
    __ daddu(scratch, elements, scratch);
    } else {
      __ Dlsa(scratch, elements, key, kPointerSizeLog2);
    }
  }

  Representation representation = hinstr->representation();
  if (representation.IsInteger32() && SmiValuesAre32Bits() &&
      hinstr->elements_kind() == FAST_SMI_ELEMENTS) {
    DCHECK(!hinstr->RequiresHoleCheck());
    if (FLAG_debug_code) {
      Register temp = scratch1();
      __ Load(temp, MemOperand(store_base, offset), Representation::Smi());
      __ AssertSmi(temp);
    }

    // Read int value directly from upper half of the smi.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 32);
    offset = SmiWordOffset(offset);
  }

  __ Load(result, MemOperand(store_base, offset), representation);

  // Check for the hole value.
  if (hinstr->RequiresHoleCheck()) {
    if (IsFastSmiElementsKind(instr->hydrogen()->elements_kind())) {
      __ SmiTst(result, scratch);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi, scratch,
                   Operand(zero_reg));
    } else {
      __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole, result,
                   Operand(scratch));
    }
  } else if (instr->hydrogen()->hole_mode() == CONVERT_HOLE_TO_UNDEFINED) {
    DCHECK(instr->hydrogen()->elements_kind() == FAST_HOLEY_ELEMENTS);
    Label done;
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    __ Branch(&done, ne, result, Operand(scratch));
    if (info()->IsStub()) {
      // A stub can safely convert the hole to undefined only if the array
      // protector cell contains (Smi) Isolate::kProtectorValid. Otherwise
      // it needs to bail out.
      __ LoadRoot(result, Heap::kArrayProtectorRootIndex);
      // The comparison only needs LS bits of value, which is a smi.
      __ ld(result, FieldMemOperand(result, PropertyCell::kValueOffset));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kHole, result,
                   Operand(Smi::FromInt(Isolate::kProtectorValid)));
    }
    __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
    __ bind(&done);
  }
}


void LCodeGen::DoLoadKeyed(LLoadKeyed* instr) {
  if (instr->is_fixed_typed_array()) {
    DoLoadKeyedExternalArray(instr);
  } else if (instr->hydrogen()->representation().IsDouble()) {
    DoLoadKeyedFixedDoubleArray(instr);
  } else {
    DoLoadKeyedFixedArray(instr);
  }
}


MemOperand LCodeGen::PrepareKeyedOperand(Register key,
                                         Register base,
                                         bool key_is_constant,
                                         int constant_key,
                                         int element_size,
                                         int shift_size,
                                         int base_offset) {
  if (key_is_constant) {
    return MemOperand(base, (constant_key << element_size) + base_offset);
  }

  if (base_offset == 0) {
    if (shift_size >= 0) {
      __ dsll(scratch0(), key, shift_size);
      __ Daddu(scratch0(), base, scratch0());
      return MemOperand(scratch0());
    } else {
      if (shift_size == -32) {
        __ dsra32(scratch0(), key, 0);
      } else {
        __ dsra(scratch0(), key, -shift_size);
      }
      __ Daddu(scratch0(), base, scratch0());
      return MemOperand(scratch0());
    }
  }

  if (shift_size >= 0) {
    __ dsll(scratch0(), key, shift_size);
    __ Daddu(scratch0(), base, scratch0());
    return MemOperand(scratch0(), base_offset);
  } else {
    if (shift_size == -32) {
       __ dsra32(scratch0(), key, 0);
    } else {
      __ dsra(scratch0(), key, -shift_size);
    }
    __ Daddu(scratch0(), base, scratch0());
    return MemOperand(scratch0(), base_offset);
  }
}


void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register scratch = scratch0();
  Register temp = scratch1();
  Register result = ToRegister(instr->result());

  if (instr->hydrogen()->from_inlined()) {
    __ Dsubu(result, sp, 2 * kPointerSize);
  } else if (instr->hydrogen()->arguments_adaptor()) {
    // Check if the calling frame is an arguments adaptor frame.
    Label done, adapted;
    __ ld(scratch, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ ld(result,
          MemOperand(scratch, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ Xor(temp, result, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

    // Result is the frame pointer for the frame if not adapted and for the real
    // frame below the adaptor frame if adapted.
    __ Movn(result, fp, temp);  // Move only if temp is not equal to zero (ne).
    __ Movz(result, scratch, temp);  // Move only if temp is equal to zero (eq).
  } else {
    __ mov(result, fp);
  }
}


void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Register elem = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());

  Label done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ Daddu(result, zero_reg, Operand(scope()->num_parameters()));
  __ Branch(&done, eq, fp, Operand(elem));

  // Arguments adaptor frame present. Get argument length from there.
  __ ld(result, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ld(result,
        MemOperand(result, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(result);

  // Argument length is in result register.
  __ bind(&done);
}


void LCodeGen::DoWrapReceiver(LWrapReceiver* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // If the receiver is null or undefined, we have to pass the global
  // object as a receiver to normal functions. Values have to be
  // passed unchanged to builtins and strict-mode functions.
  Label global_object, result_in_receiver;

  if (!instr->hydrogen()->known_function()) {
    // Do not transform the receiver to object for strict mode functions.
    __ ld(scratch,
           FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));

    // Do not transform the receiver to object for builtins.
    int32_t strict_mode_function_mask =
        1 <<  SharedFunctionInfo::kStrictModeBitWithinByte;
    int32_t native_mask = 1 << SharedFunctionInfo::kNativeBitWithinByte;

    __ lbu(at,
           FieldMemOperand(scratch, SharedFunctionInfo::kStrictModeByteOffset));
    __ And(at, at, Operand(strict_mode_function_mask));
    __ Branch(&result_in_receiver, ne, at, Operand(zero_reg));
    __ lbu(at,
           FieldMemOperand(scratch, SharedFunctionInfo::kNativeByteOffset));
    __ And(at, at, Operand(native_mask));
    __ Branch(&result_in_receiver, ne, at, Operand(zero_reg));
  }

  // Normal function. Replace undefined or null with global receiver.
  __ LoadRoot(scratch, Heap::kNullValueRootIndex);
  __ Branch(&global_object, eq, receiver, Operand(scratch));
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ Branch(&global_object, eq, receiver, Operand(scratch));

  // Deoptimize if the receiver is not a JS object.
  __ SmiTst(receiver, scratch);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, scratch, Operand(zero_reg));

  __ GetObjectType(receiver, scratch, scratch);
  DeoptimizeIf(lt, instr, DeoptimizeReason::kNotAJavaScriptObject, scratch,
               Operand(FIRST_JS_RECEIVER_TYPE));
  __ Branch(&result_in_receiver);

  __ bind(&global_object);
  __ ld(result, FieldMemOperand(function, JSFunction::kContextOffset));
  __ ld(result, ContextMemOperand(result, Context::NATIVE_CONTEXT_INDEX));
  __ ld(result, ContextMemOperand(result, Context::GLOBAL_PROXY_INDEX));

  if (result.is(receiver)) {
    __ bind(&result_in_receiver);
  } else {
    Label result_ok;
    __ Branch(&result_ok);
    __ bind(&result_in_receiver);
    __ mov(result, receiver);
    __ bind(&result_ok);
  }
}


void LCodeGen::DoApplyArguments(LApplyArguments* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  DCHECK(receiver.is(a0));  // Used for parameter count.
  DCHECK(function.is(a1));  // Required by InvokeFunction.
  DCHECK(ToRegister(instr->result()).is(v0));

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  DeoptimizeIf(hi, instr, DeoptimizeReason::kTooManyArguments, length,
               Operand(kArgumentsLimit));

  // Push the receiver and use the register to keep the original
  // number of arguments.
  __ push(receiver);
  __ Move(receiver, length);
  // The arguments are at a one pointer size offset from elements.
  __ Daddu(elements, elements, Operand(1 * kPointerSize));

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label invoke, loop;
  // length is a small non-negative integer, due to the test above.
  __ Branch(USE_DELAY_SLOT, &invoke, eq, length, Operand(zero_reg));
  __ dsll(scratch, length, kPointerSizeLog2);
  __ bind(&loop);
  __ Daddu(scratch, elements, scratch);
  __ ld(scratch, MemOperand(scratch));
  __ push(scratch);
  __ Dsubu(length, length, Operand(1));
  __ Branch(USE_DELAY_SLOT, &loop, ne, length, Operand(zero_reg));
  __ dsll(scratch, length, kPointerSizeLog2);

  __ bind(&invoke);

  InvokeFlag flag = CALL_FUNCTION;
  if (instr->hydrogen()->tail_call_mode() == TailCallMode::kAllow) {
    DCHECK(!info()->saves_caller_doubles());
    // TODO(ishell): drop current frame before pushing arguments to the stack.
    flag = JUMP_FUNCTION;
    ParameterCount actual(a0);
    // It is safe to use t0, t1 and t2 as scratch registers here given that
    // we are not going to return to caller function anyway.
    PrepareForTailCall(actual, t0, t1, t2);
  }

  DCHECK(instr->HasPointerMap());
  LPointerMap* pointers = instr->pointer_map();
  SafepointGenerator safepoint_generator(this, pointers, Safepoint::kLazyDeopt);
  // The number of arguments is stored in receiver which is a0, as expected
  // by InvokeFunction.
  ParameterCount actual(receiver);
  __ InvokeFunction(function, no_reg, actual, flag, safepoint_generator);
}


void LCodeGen::DoPushArgument(LPushArgument* instr) {
  LOperand* argument = instr->value();
  if (argument->IsDoubleRegister() || argument->IsDoubleStackSlot()) {
    Abort(kDoPushArgumentNotImplementedForDoubleType);
  } else {
    Register argument_reg = EmitLoadRegister(argument, at);
    __ push(argument_reg);
  }
}


void LCodeGen::DoDrop(LDrop* instr) {
  __ Drop(instr->count());
}


void LCodeGen::DoThisFunction(LThisFunction* instr) {
  Register result = ToRegister(instr->result());
  __ ld(result, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
}


void LCodeGen::DoContext(LContext* instr) {
  // If there is a non-return use, the context must be moved to a register.
  Register result = ToRegister(instr->result());
  if (info()->IsOptimizing()) {
    __ ld(result, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // If there is no frame, the context must be in cp.
    DCHECK(result.is(cp));
  }
}


void LCodeGen::DoDeclareGlobals(LDeclareGlobals* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  __ li(scratch0(), instr->hydrogen()->declarations());
  __ li(scratch1(), Operand(Smi::FromInt(instr->hydrogen()->flags())));
  __ Push(scratch0(), scratch1());
  __ li(scratch0(), instr->hydrogen()->feedback_vector());
  __ Push(scratch0());
  CallRuntime(Runtime::kDeclareGlobals, instr);
}

void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int formal_parameter_count, int arity,
                                 bool is_tail_call, LInstruction* instr) {
  bool dont_adapt_arguments =
      formal_parameter_count == SharedFunctionInfo::kDontAdaptArgumentsSentinel;
  bool can_invoke_directly =
      dont_adapt_arguments || formal_parameter_count == arity;

  Register function_reg = a1;
  LPointerMap* pointers = instr->pointer_map();

  if (can_invoke_directly) {
    // Change context.
    __ ld(cp, FieldMemOperand(function_reg, JSFunction::kContextOffset));

    // Always initialize new target and number of actual arguments.
    __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
    __ li(a0, Operand(arity));

    bool is_self_call = function.is_identical_to(info()->closure());

    // Invoke function.
    if (is_self_call) {
      Handle<Code> self(reinterpret_cast<Code**>(__ CodeObject().location()));
      if (is_tail_call) {
        __ Jump(self, RelocInfo::CODE_TARGET);
      } else {
        __ Call(self, RelocInfo::CODE_TARGET);
      }
    } else {
      __ ld(at, FieldMemOperand(function_reg, JSFunction::kCodeEntryOffset));
      if (is_tail_call) {
        __ Jump(at);
      } else {
        __ Call(at);
      }
    }

    if (!is_tail_call) {
      // Set up deoptimization.
      RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
    }
  } else {
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount actual(arity);
    ParameterCount expected(formal_parameter_count);
    InvokeFlag flag = is_tail_call ? JUMP_FUNCTION : CALL_FUNCTION;
    __ InvokeFunction(function_reg, expected, actual, flag, generator);
  }
}


void LCodeGen::DoDeferredMathAbsTaggedHeapNumber(LMathAbs* instr) {
  DCHECK(instr->context() != NULL);
  DCHECK(ToRegister(instr->context()).is(cp));
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // Deoptimize if not a heap number.
  __ ld(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber, scratch,
               Operand(at));

  Label done;
  Register exponent = scratch0();
  scratch = no_reg;
  __ lwu(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive, just
  // return it.
  __ Move(result, input);
  __ And(at, exponent, Operand(HeapNumber::kSignMask));
  __ Branch(&done, eq, at, Operand(zero_reg));

  // Input is negative. Reverse its sign.
  // Preserve the value of all registers.
  {
    PushSafepointRegistersScope scope(this);

    // Registers were saved at the safepoint, so we can use
    // many scratch registers.
    Register tmp1 = input.is(a1) ? a0 : a1;
    Register tmp2 = input.is(a2) ? a0 : a2;
    Register tmp3 = input.is(a3) ? a0 : a3;
    Register tmp4 = input.is(a4) ? a0 : a4;

    // exponent: floating point exponent value.

    Label allocated, slow;
    __ LoadRoot(tmp4, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(tmp1, tmp2, tmp3, tmp4, &slow);
    __ Branch(&allocated);

    // Slow case: Call the runtime system to do the number allocation.
    __ bind(&slow);

    CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr,
                            instr->context());
    // Set the pointer to the new heap number in tmp.
    if (!tmp1.is(v0))
      __ mov(tmp1, v0);
    // Restore input_reg after call to runtime.
    __ LoadFromSafepointRegisterSlot(input, input);
    __ lwu(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));

    __ bind(&allocated);
    // exponent: floating point exponent value.
    // tmp1: allocated heap number.
    __ And(exponent, exponent, Operand(~HeapNumber::kSignMask));
    __ sw(exponent, FieldMemOperand(tmp1, HeapNumber::kExponentOffset));
    __ lwu(tmp2, FieldMemOperand(input, HeapNumber::kMantissaOffset));
    __ sw(tmp2, FieldMemOperand(tmp1, HeapNumber::kMantissaOffset));

    __ StoreToSafepointRegisterSlot(tmp1, result);
  }

  __ bind(&done);
}


void LCodeGen::EmitIntegerMathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
  Label done;
  __ Branch(USE_DELAY_SLOT, &done, ge, input, Operand(zero_reg));
  __ mov(result, input);
  __ subu(result, zero_reg, input);
  // Overflow if result is still negative, i.e. 0x80000000.
  DeoptimizeIf(lt, instr, DeoptimizeReason::kOverflow, result,
               Operand(zero_reg));
  __ bind(&done);
}


void LCodeGen::EmitSmiMathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
  Label done;
  __ Branch(USE_DELAY_SLOT, &done, ge, input, Operand(zero_reg));
  __ mov(result, input);
  __ dsubu(result, zero_reg, input);
  // Overflow if result is still negative, i.e. 0x80000000 00000000.
  DeoptimizeIf(lt, instr, DeoptimizeReason::kOverflow, result,
               Operand(zero_reg));
  __ bind(&done);
}


void LCodeGen::DoMathAbs(LMathAbs* instr) {
  // Class for deferred case.
  class DeferredMathAbsTaggedHeapNumber final : public LDeferredCode {
   public:
    DeferredMathAbsTaggedHeapNumber(LCodeGen* codegen, LMathAbs* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override {
      codegen()->DoDeferredMathAbsTaggedHeapNumber(instr_);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LMathAbs* instr_;
  };

  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsDouble()) {
    FPURegister input = ToDoubleRegister(instr->value());
    FPURegister result = ToDoubleRegister(instr->result());
    __ abs_d(result, input);
  } else if (r.IsInteger32()) {
    EmitIntegerMathAbs(instr);
  } else if (r.IsSmi()) {
    EmitSmiMathAbs(instr);
  } else {
    // Representation is tagged.
    DeferredMathAbsTaggedHeapNumber* deferred =
        new(zone()) DeferredMathAbsTaggedHeapNumber(this, instr);
    Register input = ToRegister(instr->value());
    // Smi check.
    __ JumpIfNotSmi(input, deferred->entry());
    // If smi, handle it directly.
    EmitSmiMathAbs(instr);
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoMathFloor(LMathFloor* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register scratch1 = scratch0();
  Register except_flag = ToRegister(instr->temp());

  __ EmitFPUTruncate(kRoundToMinusInf,
                     result,
                     input,
                     scratch1,
                     double_scratch0(),
                     except_flag);

  // Deopt if the operation did not succeed.
  DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN, except_flag,
               Operand(zero_reg));

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    Label done;
    __ Branch(&done, ne, result, Operand(zero_reg));
    __ mfhc1(scratch1, input);  // Get exponent/sign bits.
    __ And(scratch1, scratch1, Operand(HeapNumber::kSignMask));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero, scratch1,
                 Operand(zero_reg));
    __ bind(&done);
  }
}


void LCodeGen::DoMathRound(LMathRound* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  DoubleRegister double_scratch1 = ToDoubleRegister(instr->temp());
  Register scratch = scratch0();
  Label done, check_sign_on_zero;

  // Extract exponent bits.
  __ mfhc1(result, input);
  __ Ext(scratch,
         result,
         HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);

  // If the number is in ]-0.5, +0.5[, the result is +/- 0.
  Label skip1;
  __ Branch(&skip1, gt, scratch, Operand(HeapNumber::kExponentBias - 2));
  __ mov(result, zero_reg);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ Branch(&check_sign_on_zero);
  } else {
    __ Branch(&done);
  }
  __ bind(&skip1);

  // The following conversion will not work with numbers
  // outside of ]-2^32, 2^32[.
  DeoptimizeIf(ge, instr, DeoptimizeReason::kOverflow, scratch,
               Operand(HeapNumber::kExponentBias + 32));

  // Save the original sign for later comparison.
  __ And(scratch, result, Operand(HeapNumber::kSignMask));

  __ Move(double_scratch0(), 0.5);
  __ add_d(double_scratch0(), input, double_scratch0());

  // Check sign of the result: if the sign changed, the input
  // value was in ]0.5, 0[ and the result should be -0.
  __ mfhc1(result, double_scratch0());
  // mfhc1 sign-extends, clear the upper bits.
  __ dsll32(result, result, 0);
  __ dsrl32(result, result, 0);
  __ Xor(result, result, Operand(scratch));
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // ARM uses 'mi' here, which is 'lt'
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero, result,
                 Operand(zero_reg));
  } else {
    Label skip2;
    // ARM uses 'mi' here, which is 'lt'
    // Negating it results in 'ge'
    __ Branch(&skip2, ge, result, Operand(zero_reg));
    __ mov(result, zero_reg);
    __ Branch(&done);
    __ bind(&skip2);
  }

  Register except_flag = scratch;
  __ EmitFPUTruncate(kRoundToMinusInf,
                     result,
                     double_scratch0(),
                     at,
                     double_scratch1,
                     except_flag);

  DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN, except_flag,
               Operand(zero_reg));

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    __ Branch(&done, ne, result, Operand(zero_reg));
    __ bind(&check_sign_on_zero);
    __ mfhc1(scratch, input);  // Get exponent/sign bits.
    __ And(scratch, scratch, Operand(HeapNumber::kSignMask));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero, scratch,
                 Operand(zero_reg));
  }
  __ bind(&done);
}


void LCodeGen::DoMathFround(LMathFround* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  DoubleRegister result = ToDoubleRegister(instr->result());
  __ cvt_s_d(result, input);
  __ cvt_d_s(result, result);
}


void LCodeGen::DoMathSqrt(LMathSqrt* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  DoubleRegister result = ToDoubleRegister(instr->result());
  __ sqrt_d(result, input);
}


void LCodeGen::DoMathPowHalf(LMathPowHalf* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  DoubleRegister result = ToDoubleRegister(instr->result());
  DoubleRegister temp = ToDoubleRegister(instr->temp());

  DCHECK(!input.is(result));

  // Note that according to ECMA-262 15.8.2.13:
  // Math.pow(-Infinity, 0.5) == Infinity
  // Math.sqrt(-Infinity) == NaN
  Label done;
  __ Move(temp, static_cast<double>(-V8_INFINITY));
  // Set up Infinity.
  __ Neg_d(result, temp);
  // result is overwritten if the branch is not taken.
  __ BranchF(&done, NULL, eq, temp, input);

  // Add +0 to convert -0 to +0.
  __ add_d(result, input, kDoubleRegZero);
  __ sqrt_d(result, result);
  __ bind(&done);
}


void LCodeGen::DoPower(LPower* instr) {
  Representation exponent_type = instr->hydrogen()->right()->representation();
  // Having marked this as a call, we can use any registers.
  // Just make sure that the input/output registers are the expected ones.
  Register tagged_exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(!instr->right()->IsDoubleRegister() ||
         ToDoubleRegister(instr->right()).is(f4));
  DCHECK(!instr->right()->IsRegister() ||
         ToRegister(instr->right()).is(tagged_exponent));
  DCHECK(ToDoubleRegister(instr->left()).is(f2));
  DCHECK(ToDoubleRegister(instr->result()).is(f0));

  if (exponent_type.IsSmi()) {
    MathPowStub stub(isolate(), MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsTagged()) {
    Label no_deopt;
    __ JumpIfSmi(tagged_exponent, &no_deopt);
    DCHECK(!a7.is(tagged_exponent));
    __ lw(a7, FieldMemOperand(tagged_exponent, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber, a7, Operand(at));
    __ bind(&no_deopt);
    MathPowStub stub(isolate(), MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsInteger32()) {
    MathPowStub stub(isolate(), MathPowStub::INTEGER);
    __ CallStub(&stub);
  } else {
    DCHECK(exponent_type.IsDouble());
    MathPowStub stub(isolate(), MathPowStub::DOUBLE);
    __ CallStub(&stub);
  }
}

void LCodeGen::DoMathCos(LMathCos* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_cos_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}

void LCodeGen::DoMathSin(LMathSin* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_sin_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}

void LCodeGen::DoMathExp(LMathExp* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_exp_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}


void LCodeGen::DoMathLog(LMathLog* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_log_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}


void LCodeGen::DoMathClz32(LMathClz32* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  __ Clz(result, input);
}

void LCodeGen::PrepareForTailCall(const ParameterCount& actual,
                                  Register scratch1, Register scratch2,
                                  Register scratch3) {
#if DEBUG
  if (actual.is_reg()) {
    DCHECK(!AreAliased(actual.reg(), scratch1, scratch2, scratch3));
  } else {
    DCHECK(!AreAliased(scratch1, scratch2, scratch3));
  }
#endif
  if (FLAG_code_comments) {
    if (actual.is_reg()) {
      Comment(";;; PrepareForTailCall, actual: %s {",
              RegisterConfiguration::Crankshaft()->GetGeneralRegisterName(
                  actual.reg().code()));
    } else {
      Comment(";;; PrepareForTailCall, actual: %d {", actual.immediate());
    }
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ ld(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ld(scratch3, MemOperand(scratch2, StandardFrameConstants::kContextOffset));
  __ Branch(&no_arguments_adaptor, ne, scratch3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ ld(caller_args_count_reg,
        MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ Branch(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ li(caller_args_count_reg, Operand(info()->literal()->parameter_count()));

  __ bind(&formal_parameter_count_loaded);
  __ PrepareForTailCall(actual, caller_args_count_reg, scratch2, scratch3);

  Comment(";;; }");
}

void LCodeGen::DoInvokeFunction(LInvokeFunction* instr) {
  HInvokeFunction* hinstr = instr->hydrogen();
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->function()).is(a1));
  DCHECK(instr->HasPointerMap());

  bool is_tail_call = hinstr->tail_call_mode() == TailCallMode::kAllow;

  if (is_tail_call) {
    DCHECK(!info()->saves_caller_doubles());
    ParameterCount actual(instr->arity());
    // It is safe to use t0, t1 and t2 as scratch registers here given that
    // we are not going to return to caller function anyway.
    PrepareForTailCall(actual, t0, t1, t2);
  }

  Handle<JSFunction> known_function = hinstr->known_function();
  if (known_function.is_null()) {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount actual(instr->arity());
    InvokeFlag flag = is_tail_call ? JUMP_FUNCTION : CALL_FUNCTION;
    __ InvokeFunction(a1, no_reg, actual, flag, generator);
  } else {
    CallKnownFunction(known_function, hinstr->formal_parameter_count(),
                      instr->arity(), is_tail_call, instr);
  }
}


void LCodeGen::DoCallWithDescriptor(LCallWithDescriptor* instr) {
  DCHECK(ToRegister(instr->result()).is(v0));

  if (instr->hydrogen()->IsTailCall()) {
    if (NeedsEagerFrame()) __ LeaveFrame(StackFrame::INTERNAL);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      __ Jump(code, RelocInfo::CODE_TARGET);
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      __ Daddu(target, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ Jump(target);
    }
  } else {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      generator.BeforeCall(__ CallSize(code, RelocInfo::CODE_TARGET));
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      generator.BeforeCall(__ CallSize(target));
      __ Daddu(target, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ Call(target);
    }
    generator.AfterCall();
  }
}


void LCodeGen::DoCallNewArray(LCallNewArray* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->constructor()).is(a1));
  DCHECK(ToRegister(instr->result()).is(v0));

  __ li(a0, Operand(instr->arity()));
  __ li(a2, instr->hydrogen()->site());

  ElementsKind kind = instr->hydrogen()->elements_kind();
  AllocationSiteOverrideMode override_mode =
      (AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE)
          ? DISABLE_ALLOCATION_SITES
          : DONT_OVERRIDE;

  if (instr->arity() == 0) {
    ArrayNoArgumentConstructorStub stub(isolate(), kind, override_mode);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  } else if (instr->arity() == 1) {
    Label done;
    if (IsFastPackedElementsKind(kind)) {
      Label packed_case;
      // We might need a change here,
      // look at the first argument.
      __ ld(a5, MemOperand(sp, 0));
      __ Branch(&packed_case, eq, a5, Operand(zero_reg));

      ElementsKind holey_kind = GetHoleyElementsKind(kind);
      ArraySingleArgumentConstructorStub stub(isolate(),
                                              holey_kind,
                                              override_mode);
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      __ jmp(&done);
      __ bind(&packed_case);
    }

    ArraySingleArgumentConstructorStub stub(isolate(), kind, override_mode);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
    __ bind(&done);
  } else {
    ArrayNArgumentsConstructorStub stub(isolate());
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  }
}


void LCodeGen::DoCallRuntime(LCallRuntime* instr) {
  CallRuntime(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoStoreCodeEntry(LStoreCodeEntry* instr) {
  Register function = ToRegister(instr->function());
  Register code_object = ToRegister(instr->code_object());
  __ Daddu(code_object, code_object,
          Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sd(code_object,
        FieldMemOperand(function, JSFunction::kCodeEntryOffset));
}


void LCodeGen::DoInnerAllocatedObject(LInnerAllocatedObject* instr) {
  Register result = ToRegister(instr->result());
  Register base = ToRegister(instr->base_object());
  if (instr->offset()->IsConstantOperand()) {
    LConstantOperand* offset = LConstantOperand::cast(instr->offset());
    __ Daddu(result, base, Operand(ToInteger32(offset)));
  } else {
    Register offset = ToRegister(instr->offset());
    __ Daddu(result, base, offset);
  }
}


void LCodeGen::DoStoreNamedField(LStoreNamedField* instr) {
  Representation representation = instr->representation();

  Register object = ToRegister(instr->object());
  Register scratch2 = scratch1();
  Register scratch1 = scratch0();

  HObjectAccess access = instr->hydrogen()->access();
  int offset = access.offset();
  if (access.IsExternalMemory()) {
    Register value = ToRegister(instr->value());
    MemOperand operand = MemOperand(object, offset);
    __ Store(value, operand, representation);
    return;
  }

  __ AssertNotSmi(object);

  DCHECK(!representation.IsSmi() ||
         !instr->value()->IsConstantOperand() ||
         IsSmi(LConstantOperand::cast(instr->value())));
  if (!FLAG_unbox_double_fields && representation.IsDouble()) {
    DCHECK(access.IsInobject());
    DCHECK(!instr->hydrogen()->has_transition());
    DCHECK(!instr->hydrogen()->NeedsWriteBarrier());
    DoubleRegister value = ToDoubleRegister(instr->value());
    __ sdc1(value, FieldMemOperand(object, offset));
    return;
  }

  if (instr->hydrogen()->has_transition()) {
    Handle<Map> transition = instr->hydrogen()->transition_map();
    AddDeprecationDependency(transition);
    __ li(scratch1, Operand(transition));
    __ sd(scratch1, FieldMemOperand(object, HeapObject::kMapOffset));
    if (instr->hydrogen()->NeedsWriteBarrierForMap()) {
      Register temp = ToRegister(instr->temp());
      // Update the write barrier for the map field.
      __ RecordWriteForMap(object,
                           scratch1,
                           temp,
                           GetRAState(),
                           kSaveFPRegs);
    }
  }

  // Do the store.
  Register destination = object;
  if (!access.IsInobject()) {
       destination = scratch1;
    __ ld(destination, FieldMemOperand(object, JSObject::kPropertiesOffset));
  }

  if (representation.IsSmi() && SmiValuesAre32Bits() &&
      instr->hydrogen()->value()->representation().IsInteger32()) {
    DCHECK(instr->hydrogen()->store_mode() == STORE_TO_INITIALIZED_ENTRY);
    if (FLAG_debug_code) {
      __ Load(scratch2, FieldMemOperand(destination, offset), representation);
      __ AssertSmi(scratch2);
    }
    // Store int value directly to upper half of the smi.
    offset = SmiWordOffset(offset);
    representation = Representation::Integer32();
  }
  MemOperand operand = FieldMemOperand(destination, offset);

  if (FLAG_unbox_double_fields && representation.IsDouble()) {
    DCHECK(access.IsInobject());
    DoubleRegister value = ToDoubleRegister(instr->value());
    __ sdc1(value, operand);
  } else {
    DCHECK(instr->value()->IsRegister());
    Register value = ToRegister(instr->value());
    __ Store(value, operand, representation);
  }

  if (instr->hydrogen()->NeedsWriteBarrier()) {
    // Update the write barrier for the object for in-object properties.
    Register value = ToRegister(instr->value());
    __ RecordWriteField(destination,
                        offset,
                        value,
                        scratch2,
                        GetRAState(),
                        kSaveFPRegs,
                        EMIT_REMEMBERED_SET,
                        instr->hydrogen()->SmiCheckForWriteBarrier(),
                        instr->hydrogen()->PointersToHereCheckForValue());
  }
}


void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  Condition cc = instr->hydrogen()->allow_equality() ? hi : hs;
  Operand operand((int64_t)0);
  Register reg;
  if (instr->index()->IsConstantOperand()) {
    operand = ToOperand(instr->index());
    reg = ToRegister(instr->length());
    cc = CommuteCondition(cc);
  } else {
    reg = ToRegister(instr->index());
    operand = ToOperand(instr->length());
  }
  if (FLAG_debug_code && instr->hydrogen()->skip_check()) {
    Label done;
    __ Branch(&done, NegateCondition(cc), reg, operand);
    __ stop("eliminated bounds check failed");
    __ bind(&done);
  } else {
    DeoptimizeIf(cc, instr, DeoptimizeReason::kOutOfBounds, reg, operand);
  }
}


void LCodeGen::DoStoreKeyedExternalArray(LStoreKeyed* instr) {
  Register external_pointer = ToRegister(instr->elements());
  Register key = no_reg;
  ElementsKind elements_kind = instr->elements_kind();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }
  int element_size_shift = ElementsKindToShiftSize(elements_kind);
  int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
      ? (element_size_shift - (kSmiTagSize + kSmiShiftSize))
      : element_size_shift;
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    Register address = scratch0();
    FPURegister value(ToDoubleRegister(instr->value()));
    if (key_is_constant) {
      if (constant_key != 0) {
        __ Daddu(address, external_pointer,
                Operand(constant_key << element_size_shift));
      } else {
        address = external_pointer;
      }
    } else {
      if (shift_size < 0) {
        if (shift_size == -32) {
          __ dsra32(address, key, 0);
        } else {
          __ dsra(address, key, -shift_size);
        }
      } else {
        __ dsll(address, key, shift_size);
      }
      __ Daddu(address, external_pointer, address);
    }

    if (elements_kind == FLOAT32_ELEMENTS) {
      __ cvt_s_d(double_scratch0(), value);
      __ swc1(double_scratch0(), MemOperand(address, base_offset));
    } else {  // Storing doubles, not floats.
      __ sdc1(value, MemOperand(address, base_offset));
    }
  } else {
    Register value(ToRegister(instr->value()));
    MemOperand mem_operand = PrepareKeyedOperand(
        key, external_pointer, key_is_constant, constant_key,
        element_size_shift, shift_size,
        base_offset);
    switch (elements_kind) {
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
      case INT8_ELEMENTS:
        __ sb(value, mem_operand);
        break;
      case INT16_ELEMENTS:
      case UINT16_ELEMENTS:
        __ sh(value, mem_operand);
        break;
      case INT32_ELEMENTS:
      case UINT32_ELEMENTS:
        __ sw(value, mem_operand);
        break;
      case FLOAT32_ELEMENTS:
      case FLOAT64_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      case FAST_STRING_WRAPPER_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
      case NO_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr) {
  DoubleRegister value = ToDoubleRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  DoubleRegister double_scratch = double_scratch0();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int base_offset = instr->base_offset();
  Label not_nan, done;

  // Calculate the effective address of the slot in the array to store the
  // double value.
  int element_size_shift = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  if (key_is_constant) {
    int constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
    __ Daddu(scratch, elements,
             Operand((constant_key << element_size_shift) + base_offset));
  } else {
    int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
        ? (element_size_shift - (kSmiTagSize + kSmiShiftSize))
        : element_size_shift;
    __ Daddu(scratch, elements, Operand(base_offset));
    DCHECK((shift_size == 3) || (shift_size == -29));
    if (shift_size == 3) {
      __ dsll(at, ToRegister(instr->key()), 3);
    } else if (shift_size == -29) {
      __ dsra(at, ToRegister(instr->key()), 29);
    }
    __ Daddu(scratch, scratch, at);
  }

  if (instr->NeedsCanonicalization()) {
    __ FPUCanonicalizeNaN(double_scratch, value);
    __ sdc1(double_scratch, MemOperand(scratch, 0));
  } else {
    __ sdc1(value, MemOperand(scratch, 0));
  }
}


void LCodeGen::DoStoreKeyedFixedArray(LStoreKeyed* instr) {
  Register value = ToRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = instr->key()->IsRegister() ? ToRegister(instr->key())
      : no_reg;
  Register scratch = scratch0();
  Register store_base = scratch;
  int offset = instr->base_offset();

  // Do the store.
  if (instr->key()->IsConstantOperand()) {
    DCHECK(!instr->hydrogen()->NeedsWriteBarrier());
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset += ToInteger32(const_operand) * kPointerSize;
    store_base = elements;
  } else {
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (instr->hydrogen()->key()->representation().IsSmi()) {
      __ SmiScale(scratch, key, kPointerSizeLog2);
      __ daddu(store_base, elements, scratch);
    } else {
      __ Dlsa(store_base, elements, key, kPointerSizeLog2);
    }
  }

  Representation representation = instr->hydrogen()->value()->representation();
  if (representation.IsInteger32() && SmiValuesAre32Bits()) {
    DCHECK(instr->hydrogen()->store_mode() == STORE_TO_INITIALIZED_ENTRY);
    DCHECK(instr->hydrogen()->elements_kind() == FAST_SMI_ELEMENTS);
    if (FLAG_debug_code) {
      Register temp = scratch1();
      __ Load(temp, MemOperand(store_base, offset), Representation::Smi());
      __ AssertSmi(temp);
    }

    // Store int value directly to upper half of the smi.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 32);
    offset = SmiWordOffset(offset);
    representation = Representation::Integer32();
  }

  __ Store(value, MemOperand(store_base, offset), representation);

  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed =
        instr->hydrogen()->value()->type().IsHeapObject()
            ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
    // Compute address of modified element and store it into key register.
    __ Daddu(key, store_base, Operand(offset));
    __ RecordWrite(elements,
                   key,
                   value,
                   GetRAState(),
                   kSaveFPRegs,
                   EMIT_REMEMBERED_SET,
                   check_needed,
                   instr->hydrogen()->PointersToHereCheckForValue());
  }
}


void LCodeGen::DoStoreKeyed(LStoreKeyed* instr) {
  // By cases: external, fast double
  if (instr->is_fixed_typed_array()) {
    DoStoreKeyedExternalArray(instr);
  } else if (instr->hydrogen()->value()->representation().IsDouble()) {
    DoStoreKeyedFixedDoubleArray(instr);
  } else {
    DoStoreKeyedFixedArray(instr);
  }
}


void LCodeGen::DoMaybeGrowElements(LMaybeGrowElements* instr) {
  class DeferredMaybeGrowElements final : public LDeferredCode {
   public:
    DeferredMaybeGrowElements(LCodeGen* codegen, LMaybeGrowElements* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredMaybeGrowElements(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LMaybeGrowElements* instr_;
  };

  Register result = v0;
  DeferredMaybeGrowElements* deferred =
      new (zone()) DeferredMaybeGrowElements(this, instr);
  LOperand* key = instr->key();
  LOperand* current_capacity = instr->current_capacity();

  DCHECK(instr->hydrogen()->key()->representation().IsInteger32());
  DCHECK(instr->hydrogen()->current_capacity()->representation().IsInteger32());
  DCHECK(key->IsConstantOperand() || key->IsRegister());
  DCHECK(current_capacity->IsConstantOperand() ||
         current_capacity->IsRegister());

  if (key->IsConstantOperand() && current_capacity->IsConstantOperand()) {
    int32_t constant_key = ToInteger32(LConstantOperand::cast(key));
    int32_t constant_capacity =
        ToInteger32(LConstantOperand::cast(current_capacity));
    if (constant_key >= constant_capacity) {
      // Deferred case.
      __ jmp(deferred->entry());
    }
  } else if (key->IsConstantOperand()) {
    int32_t constant_key = ToInteger32(LConstantOperand::cast(key));
    __ Branch(deferred->entry(), le, ToRegister(current_capacity),
              Operand(constant_key));
  } else if (current_capacity->IsConstantOperand()) {
    int32_t constant_capacity =
        ToInteger32(LConstantOperand::cast(current_capacity));
    __ Branch(deferred->entry(), ge, ToRegister(key),
              Operand(constant_capacity));
  } else {
    __ Branch(deferred->entry(), ge, ToRegister(key),
              Operand(ToRegister(current_capacity)));
  }

  if (instr->elements()->IsRegister()) {
    __ mov(result, ToRegister(instr->elements()));
  } else {
    __ ld(result, ToMemOperand(instr->elements()));
  }

  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredMaybeGrowElements(LMaybeGrowElements* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register result = v0;
  __ mov(result, zero_reg);

  // We have to call a stub.
  {
    PushSafepointRegistersScope scope(this);
    if (instr->object()->IsRegister()) {
      __ mov(result, ToRegister(instr->object()));
    } else {
      __ ld(result, ToMemOperand(instr->object()));
    }

    LOperand* key = instr->key();
    if (key->IsConstantOperand()) {
      __ li(a3, Operand(ToSmi(LConstantOperand::cast(key))));
    } else {
      __ mov(a3, ToRegister(key));
      __ SmiTag(a3);
    }

    GrowArrayElementsStub stub(isolate(), instr->hydrogen()->kind());
    __ mov(a0, result);
    __ CallStub(&stub);
    RecordSafepointWithLazyDeopt(
        instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    __ StoreToSafepointRegisterSlot(result, result);
  }

  // Deopt on smi, which means the elements array changed to dictionary mode.
  __ SmiTst(result, at);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, at, Operand(zero_reg));
}


void LCodeGen::DoTransitionElementsKind(LTransitionElementsKind* instr) {
  Register object_reg = ToRegister(instr->object());
  Register scratch = scratch0();

  Handle<Map> from_map = instr->original_map();
  Handle<Map> to_map = instr->transitioned_map();
  ElementsKind from_kind = instr->from_kind();
  ElementsKind to_kind = instr->to_kind();

  Label not_applicable;
  __ ld(scratch, FieldMemOperand(object_reg, HeapObject::kMapOffset));
  __ Branch(&not_applicable, ne, scratch, Operand(from_map));

  if (IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Register new_map_reg = ToRegister(instr->new_map_temp());
    __ li(new_map_reg, Operand(to_map));
    __ sd(new_map_reg, FieldMemOperand(object_reg, HeapObject::kMapOffset));
    // Write barrier.
    __ RecordWriteForMap(object_reg,
                         new_map_reg,
                         scratch,
                         GetRAState(),
                         kDontSaveFPRegs);
  } else {
    DCHECK(object_reg.is(a0));
    DCHECK(ToRegister(instr->context()).is(cp));
    PushSafepointRegistersScope scope(this);
    __ li(a1, Operand(to_map));
    TransitionElementsKindStub stub(isolate(), from_kind, to_kind);
    __ CallStub(&stub);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 0, Safepoint::kLazyDeopt);
  }
  __ bind(&not_applicable);
}


void LCodeGen::DoTrapAllocationMemento(LTrapAllocationMemento* instr) {
  Register object = ToRegister(instr->object());
  Register temp = ToRegister(instr->temp());
  Label no_memento_found;
  __ TestJSArrayForAllocationMemento(object, temp, &no_memento_found);
  DeoptimizeIf(al, instr, DeoptimizeReason::kMementoFound);
  __ bind(&no_memento_found);
}


void LCodeGen::DoStringAdd(LStringAdd* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(a1));
  DCHECK(ToRegister(instr->right()).is(a0));
  StringAddStub stub(isolate(),
                     instr->hydrogen()->flags(),
                     instr->hydrogen()->pretenure_flag());
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoStringCharCodeAt(LStringCharCodeAt* instr) {
  class DeferredStringCharCodeAt final : public LDeferredCode {
   public:
    DeferredStringCharCodeAt(LCodeGen* codegen, LStringCharCodeAt* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override { codegen()->DoDeferredStringCharCodeAt(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LStringCharCodeAt* instr_;
  };

  DeferredStringCharCodeAt* deferred =
      new(zone()) DeferredStringCharCodeAt(this, instr);
  StringCharLoadGenerator::Generate(masm(),
                                    ToRegister(instr->string()),
                                    ToRegister(instr->index()),
                                    ToRegister(instr->result()),
                                    deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharCodeAt(LStringCharCodeAt* instr) {
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, zero_reg);

  PushSafepointRegistersScope scope(this);
  __ push(string);
  // Push the index as a smi. This is safe because of the checks in
  // DoStringCharCodeAt above.
  if (instr->index()->IsConstantOperand()) {
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    __ Daddu(scratch, zero_reg, Operand(Smi::FromInt(const_index)));
    __ push(scratch);
  } else {
    Register index = ToRegister(instr->index());
    __ SmiTag(index);
    __ push(index);
  }
  CallRuntimeFromDeferred(Runtime::kStringCharCodeAtRT, 2, instr,
                          instr->context());
  __ AssertSmi(v0);
  __ SmiUntag(v0);
  __ StoreToSafepointRegisterSlot(v0, result);
}


void LCodeGen::DoStringCharFromCode(LStringCharFromCode* instr) {
  class DeferredStringCharFromCode final : public LDeferredCode {
   public:
    DeferredStringCharFromCode(LCodeGen* codegen, LStringCharFromCode* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override {
      codegen()->DoDeferredStringCharFromCode(instr_);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LStringCharFromCode* instr_;
  };

  DeferredStringCharFromCode* deferred =
      new(zone()) DeferredStringCharFromCode(this, instr);

  DCHECK(instr->hydrogen()->value()->representation().IsInteger32());
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  DCHECK(!char_code.is(result));

  __ Branch(deferred->entry(), hi,
            char_code, Operand(String::kMaxOneByteCharCode));
  __ LoadRoot(result, Heap::kSingleCharacterStringCacheRootIndex);
  __ Dlsa(result, result, char_code, kPointerSizeLog2);
  __ ld(result, FieldMemOperand(result, FixedArray::kHeaderSize));
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ Branch(deferred->entry(), eq, result, Operand(scratch));
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharFromCode(LStringCharFromCode* instr) {
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, zero_reg);

  PushSafepointRegistersScope scope(this);
  __ SmiTag(char_code);
  __ push(char_code);
  CallRuntimeFromDeferred(Runtime::kStringCharFromCode, 1, instr,
                          instr->context());
  __ StoreToSafepointRegisterSlot(v0, result);
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  DCHECK(output->IsDoubleRegister());
  FPURegister single_scratch = double_scratch0().low();
  if (input->IsStackSlot()) {
    Register scratch = scratch0();
    __ ld(scratch, ToMemOperand(input));
    __ mtc1(scratch, single_scratch);
  } else {
    __ mtc1(ToRegister(input), single_scratch);
  }
  __ cvt_d_w(ToDoubleRegister(output), single_scratch);
}


void LCodeGen::DoUint32ToDouble(LUint32ToDouble* instr) {
  LOperand* input = instr->value();
  LOperand* output = instr->result();

  FPURegister dbl_scratch = double_scratch0();
  __ mtc1(ToRegister(input), dbl_scratch);
  __ Cvt_d_uw(ToDoubleRegister(output), dbl_scratch);
}


void LCodeGen::DoNumberTagU(LNumberTagU* instr) {
  class DeferredNumberTagU final : public LDeferredCode {
   public:
    DeferredNumberTagU(LCodeGen* codegen, LNumberTagU* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override {
      codegen()->DoDeferredNumberTagIU(instr_,
                                       instr_->value(),
                                       instr_->temp1(),
                                       instr_->temp2(),
                                       UNSIGNED_INT32);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LNumberTagU* instr_;
  };

  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());

  DeferredNumberTagU* deferred = new(zone()) DeferredNumberTagU(this, instr);
  __ Branch(deferred->entry(), hi, input, Operand(Smi::kMaxValue));
  __ SmiTag(result, input);
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredNumberTagIU(LInstruction* instr,
                                     LOperand* value,
                                     LOperand* temp1,
                                     LOperand* temp2,
                                     IntegerSignedness signedness) {
  Label done, slow;
  Register src = ToRegister(value);
  Register dst = ToRegister(instr->result());
  Register tmp1 = scratch0();
  Register tmp2 = ToRegister(temp1);
  Register tmp3 = ToRegister(temp2);
  DoubleRegister dbl_scratch = double_scratch0();

  if (signedness == SIGNED_INT32) {
    // There was overflow, so bits 30 and 31 of the original integer
    // disagree. Try to allocate a heap number in new space and store
    // the value in there. If that fails, call the runtime system.
    if (dst.is(src)) {
      __ SmiUntag(src, dst);
      __ Xor(src, src, Operand(0x80000000));
    }
    __ mtc1(src, dbl_scratch);
    __ cvt_d_w(dbl_scratch, dbl_scratch);
  } else {
    __ mtc1(src, dbl_scratch);
    __ Cvt_d_uw(dbl_scratch, dbl_scratch);
  }

  if (FLAG_inline_new) {
    __ LoadRoot(tmp3, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(dst, tmp1, tmp2, tmp3, &slow);
    __ Branch(&done);
  }

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);
  {
    // TODO(3095996): Put a valid pointer value in the stack slot where the
    // result register is stored, as this register is in the pointer map, but
    // contains an integer value.
    __ mov(dst, zero_reg);
    // Preserve the value of all registers.
    PushSafepointRegistersScope scope(this);
    // Reset the context register.
    if (!dst.is(cp)) {
      __ mov(cp, zero_reg);
    }
    __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 0, Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(v0, dst);
  }

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ sdc1(dbl_scratch, FieldMemOperand(dst, HeapNumber::kValueOffset));
}


void LCodeGen::DoNumberTagD(LNumberTagD* instr) {
  class DeferredNumberTagD final : public LDeferredCode {
   public:
    DeferredNumberTagD(LCodeGen* codegen, LNumberTagD* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override { codegen()->DoDeferredNumberTagD(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LNumberTagD* instr_;
  };

  DoubleRegister input_reg = ToDoubleRegister(instr->value());
  Register scratch = scratch0();
  Register reg = ToRegister(instr->result());
  Register temp1 = ToRegister(instr->temp());
  Register temp2 = ToRegister(instr->temp2());

  DeferredNumberTagD* deferred = new(zone()) DeferredNumberTagD(this, instr);
  if (FLAG_inline_new) {
    __ LoadRoot(scratch, Heap::kHeapNumberMapRootIndex);
    // We want the untagged address first for performance
    __ AllocateHeapNumber(reg, temp1, temp2, scratch, deferred->entry());
  } else {
    __ Branch(deferred->entry());
  }
  __ bind(deferred->exit());
  __ sdc1(input_reg, FieldMemOperand(reg, HeapNumber::kValueOffset));
}


void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ mov(reg, zero_reg);

  PushSafepointRegistersScope scope(this);
  // Reset the context register.
  if (!reg.is(cp)) {
    __ mov(cp, zero_reg);
  }
  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(v0, reg);
}


void LCodeGen::DoSmiTag(LSmiTag* instr) {
  HChange* hchange = instr->hydrogen();
  Register input = ToRegister(instr->value());
  Register output = ToRegister(instr->result());
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      hchange->value()->CheckFlag(HValue::kUint32)) {
    __ And(at, input, Operand(0x80000000));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow, at, Operand(zero_reg));
  }
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      !hchange->value()->CheckFlag(HValue::kUint32)) {
    __ SmiTagCheckOverflow(output, input, at);
    DeoptimizeIf(lt, instr, DeoptimizeReason::kOverflow, at, Operand(zero_reg));
  } else {
    __ SmiTag(output, input);
  }
}


void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  if (instr->needs_check()) {
    STATIC_ASSERT(kHeapObjectTag == 1);
    // If the input is a HeapObject, value of scratch won't be zero.
    __ And(scratch, input, Operand(kHeapObjectTag));
    __ SmiUntag(result, input);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi, scratch,
                 Operand(zero_reg));
  } else {
    __ SmiUntag(result, input);
  }
}


void LCodeGen::EmitNumberUntagD(LNumberUntagD* instr, Register input_reg,
                                DoubleRegister result_reg,
                                NumberUntagDMode mode) {
  bool can_convert_undefined_to_nan = instr->truncating();
  bool deoptimize_on_minus_zero = instr->hydrogen()->deoptimize_on_minus_zero();

  Register scratch = scratch0();
  Label convert, load_smi, done;
  if (mode == NUMBER_CANDIDATE_IS_ANY_TAGGED) {
    // Smi check.
    __ UntagAndJumpIfSmi(scratch, input_reg, &load_smi);
    // Heap number map check.
    __ ld(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    if (can_convert_undefined_to_nan) {
      __ Branch(&convert, ne, scratch, Operand(at));
    } else {
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber, scratch,
                   Operand(at));
    }
    // Load heap number.
    __ ldc1(result_reg, FieldMemOperand(input_reg, HeapNumber::kValueOffset));
    if (deoptimize_on_minus_zero) {
      __ mfc1(at, result_reg);
      __ Branch(&done, ne, at, Operand(zero_reg));
      __ mfhc1(scratch, result_reg);  // Get exponent/sign bits.
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, scratch,
                   Operand(HeapNumber::kSignMask));
    }
    __ Branch(&done);
    if (can_convert_undefined_to_nan) {
      __ bind(&convert);
      // Convert undefined (and hole) to NaN.
      __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumberUndefined,
                   input_reg, Operand(at));
      __ LoadRoot(scratch, Heap::kNanValueRootIndex);
      __ ldc1(result_reg, FieldMemOperand(scratch, HeapNumber::kValueOffset));
      __ Branch(&done);
    }
  } else {
    __ SmiUntag(scratch, input_reg);
    DCHECK(mode == NUMBER_CANDIDATE_IS_SMI);
  }
  // Smi to double register conversion
  __ bind(&load_smi);
  // scratch: untagged value of input_reg
  __ mtc1(scratch, result_reg);
  __ cvt_d_w(result_reg, result_reg);
  __ bind(&done);
}


void LCodeGen::DoDeferredTaggedToI(LTaggedToI* instr) {
  Register input_reg = ToRegister(instr->value());
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->temp());
  DoubleRegister double_scratch = double_scratch0();
  DoubleRegister double_scratch2 = ToDoubleRegister(instr->temp2());

  DCHECK(!scratch1.is(input_reg) && !scratch1.is(scratch2));
  DCHECK(!scratch2.is(input_reg) && !scratch2.is(scratch1));

  Label done;

  // The input is a tagged HeapObject.
  // Heap number map check.
  __ ld(scratch1, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
  // This 'at' value and scratch1 map value are used for tests in both clauses
  // of the if.

  if (instr->truncating()) {
    Label truncate;
    __ Branch(USE_DELAY_SLOT, &truncate, eq, scratch1, Operand(at));
    __ mov(scratch2, input_reg);  // In delay slot.
    __ lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotANumberOrOddball, scratch1,
                 Operand(ODDBALL_TYPE));
    __ bind(&truncate);
    __ TruncateHeapNumberToI(input_reg, scratch2);
  } else {
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber, scratch1,
                 Operand(at));

    // Load the double value.
    __ ldc1(double_scratch,
            FieldMemOperand(input_reg, HeapNumber::kValueOffset));

    Register except_flag = scratch2;
    __ EmitFPUTruncate(kRoundToZero,
                       input_reg,
                       double_scratch,
                       scratch1,
                       double_scratch2,
                       except_flag,
                       kCheckForInexactConversion);

    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN, except_flag,
                 Operand(zero_reg));

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ Branch(&done, ne, input_reg, Operand(zero_reg));

      __ mfhc1(scratch1, double_scratch);  // Get exponent/sign bits.
      __ And(scratch1, scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero, scratch1,
                   Operand(zero_reg));
    }
  }
  __ bind(&done);
}


void LCodeGen::DoTaggedToI(LTaggedToI* instr) {
  class DeferredTaggedToI final : public LDeferredCode {
   public:
    DeferredTaggedToI(LCodeGen* codegen, LTaggedToI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override { codegen()->DoDeferredTaggedToI(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LTaggedToI* instr_;
  };

  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  DCHECK(input->Equals(instr->result()));

  Register input_reg = ToRegister(input);

  if (instr->hydrogen()->value()->representation().IsSmi()) {
    __ SmiUntag(input_reg);
  } else {
    DeferredTaggedToI* deferred = new(zone()) DeferredTaggedToI(this, instr);

    // Let the deferred code handle the HeapObject case.
    __ JumpIfNotSmi(input_reg, deferred->entry());

    // Smi to int32 conversion.
    __ SmiUntag(input_reg);
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoNumberUntagD(LNumberUntagD* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  LOperand* result = instr->result();
  DCHECK(result->IsDoubleRegister());

  Register input_reg = ToRegister(input);
  DoubleRegister result_reg = ToDoubleRegister(result);

  HValue* value = instr->hydrogen()->value();
  NumberUntagDMode mode = value->representation().IsSmi()
      ? NUMBER_CANDIDATE_IS_SMI : NUMBER_CANDIDATE_IS_ANY_TAGGED;

  EmitNumberUntagD(instr, input_reg, result_reg, mode);
}


void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DoubleRegister double_input = ToDoubleRegister(instr->value());

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    Register except_flag = LCodeGen::scratch1();

    __ EmitFPUTruncate(kRoundToMinusInf,
                       result_reg,
                       double_input,
                       scratch1,
                       double_scratch0(),
                       except_flag,
                       kCheckForInexactConversion);

    // Deopt if the operation did not succeed (except_flag != 0).
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN, except_flag,
                 Operand(zero_reg));

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ Branch(&done, ne, result_reg, Operand(zero_reg));
      __ mfhc1(scratch1, double_input);  // Get exponent/sign bits.
      __ And(scratch1, scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero, scratch1,
                   Operand(zero_reg));
      __ bind(&done);
    }
  }
}


void LCodeGen::DoDoubleToSmi(LDoubleToSmi* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = LCodeGen::scratch0();
  DoubleRegister double_input = ToDoubleRegister(instr->value());

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    Register except_flag = LCodeGen::scratch1();

    __ EmitFPUTruncate(kRoundToMinusInf,
                       result_reg,
                       double_input,
                       scratch1,
                       double_scratch0(),
                       except_flag,
                       kCheckForInexactConversion);

    // Deopt if the operation did not succeed (except_flag != 0).
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN, except_flag,
                 Operand(zero_reg));

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ Branch(&done, ne, result_reg, Operand(zero_reg));
      __ mfhc1(scratch1, double_input);  // Get exponent/sign bits.
      __ And(scratch1, scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero, scratch1,
                   Operand(zero_reg));
      __ bind(&done);
    }
  }
  __ SmiTag(result_reg, result_reg);
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->value();
  __ SmiTst(ToRegister(input), at);
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi, at, Operand(zero_reg));
}


void LCodeGen::DoCheckNonSmi(LCheckNonSmi* instr) {
  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    LOperand* input = instr->value();
    __ SmiTst(ToRegister(input), at);
    DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, at, Operand(zero_reg));
  }
}


void LCodeGen::DoCheckArrayBufferNotNeutered(
    LCheckArrayBufferNotNeutered* instr) {
  Register view = ToRegister(instr->view());
  Register scratch = scratch0();

  __ ld(scratch, FieldMemOperand(view, JSArrayBufferView::kBufferOffset));
  __ lw(scratch, FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
  __ And(at, scratch, 1 << JSArrayBuffer::WasNeutered::kShift);
  DeoptimizeIf(ne, instr, DeoptimizeReason::kOutOfBounds, at,
               Operand(zero_reg));
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ GetObjectType(input, scratch, scratch);

  if (instr->hydrogen()->is_interval_check()) {
    InstanceType first;
    InstanceType last;
    instr->hydrogen()->GetCheckInterval(&first, &last);

    // If there is only one type in the interval check for equality.
    if (first == last) {
      DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongInstanceType, scratch,
                   Operand(first));
    } else {
      DeoptimizeIf(lo, instr, DeoptimizeReason::kWrongInstanceType, scratch,
                   Operand(first));
      // Omit check for the last type.
      if (last != LAST_TYPE) {
        DeoptimizeIf(hi, instr, DeoptimizeReason::kWrongInstanceType, scratch,
                     Operand(last));
      }
    }
  } else {
    uint8_t mask;
    uint8_t tag;
    instr->hydrogen()->GetCheckMaskAndTag(&mask, &tag);

    if (base::bits::IsPowerOfTwo32(mask)) {
      DCHECK(tag == 0 || base::bits::IsPowerOfTwo32(tag));
      __ And(at, scratch, mask);
      DeoptimizeIf(tag == 0 ? ne : eq, instr,
                   DeoptimizeReason::kWrongInstanceType, at, Operand(zero_reg));
    } else {
      __ And(scratch, scratch, Operand(mask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongInstanceType, scratch,
                   Operand(tag));
    }
  }
}


void LCodeGen::DoCheckValue(LCheckValue* instr) {
  Register reg = ToRegister(instr->value());
  Handle<HeapObject> object = instr->hydrogen()->object().handle();
  AllowDeferredHandleDereference smi_check;
  if (isolate()->heap()->InNewSpace(*object)) {
    Register reg = ToRegister(instr->value());
    Handle<Cell> cell = isolate()->factory()->NewCell(object);
    __ li(at, Operand(cell));
    __ ld(at, FieldMemOperand(at, Cell::kValueOffset));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kValueMismatch, reg, Operand(at));
  } else {
    DeoptimizeIf(ne, instr, DeoptimizeReason::kValueMismatch, reg,
                 Operand(object));
  }
}


void LCodeGen::DoDeferredInstanceMigration(LCheckMaps* instr, Register object) {
  {
    PushSafepointRegistersScope scope(this);
    __ push(object);
    __ mov(cp, zero_reg);
    __ CallRuntimeSaveDoubles(Runtime::kTryMigrateInstance);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 1, Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(v0, scratch0());
  }
  __ SmiTst(scratch0(), at);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kInstanceMigrationFailed, at,
               Operand(zero_reg));
}


void LCodeGen::DoCheckMaps(LCheckMaps* instr) {
  class DeferredCheckMaps final : public LDeferredCode {
   public:
    DeferredCheckMaps(LCodeGen* codegen, LCheckMaps* instr, Register object)
        : LDeferredCode(codegen), instr_(instr), object_(object) {
      SetExit(check_maps());
    }
    void Generate() override {
      codegen()->DoDeferredInstanceMigration(instr_, object_);
    }
    Label* check_maps() { return &check_maps_; }
    LInstruction* instr() override { return instr_; }

   private:
    LCheckMaps* instr_;
    Label check_maps_;
    Register object_;
  };

  if (instr->hydrogen()->IsStabilityCheck()) {
    const UniqueSet<Map>* maps = instr->hydrogen()->maps();
    for (int i = 0; i < maps->size(); ++i) {
      AddStabilityDependency(maps->at(i).handle());
    }
    return;
  }

  Register map_reg = scratch0();
  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  Register reg = ToRegister(input);
  __ ld(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));

  DeferredCheckMaps* deferred = NULL;
  if (instr->hydrogen()->HasMigrationTarget()) {
    deferred = new(zone()) DeferredCheckMaps(this, instr, reg);
    __ bind(deferred->check_maps());
  }

  const UniqueSet<Map>* maps = instr->hydrogen()->maps();
  Label success;
  for (int i = 0; i < maps->size() - 1; i++) {
    Handle<Map> map = maps->at(i).handle();
    __ CompareMapAndBranch(map_reg, map, &success, eq, &success);
  }
  Handle<Map> map = maps->at(maps->size() - 1).handle();
  // Do the CompareMap() directly within the Branch() and DeoptimizeIf().
  if (instr->hydrogen()->HasMigrationTarget()) {
    __ Branch(deferred->entry(), ne, map_reg, Operand(map));
  } else {
    DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongMap, map_reg, Operand(map));
  }

  __ bind(&success);
}


void LCodeGen::DoClampDToUint8(LClampDToUint8* instr) {
  DoubleRegister value_reg = ToDoubleRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  DoubleRegister temp_reg = ToDoubleRegister(instr->temp());
  __ ClampDoubleToUint8(result_reg, value_reg, temp_reg);
}


void LCodeGen::DoClampIToUint8(LClampIToUint8* instr) {
  Register unclamped_reg = ToRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  __ ClampUint8(result_reg, unclamped_reg);
}


void LCodeGen::DoClampTToUint8(LClampTToUint8* instr) {
  Register scratch = scratch0();
  Register input_reg = ToRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  DoubleRegister temp_reg = ToDoubleRegister(instr->temp());
  Label is_smi, done, heap_number;

  // Both smi and heap number cases are handled.
  __ UntagAndJumpIfSmi(scratch, input_reg, &is_smi);

  // Check for heap number
  __ ld(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ Branch(&heap_number, eq, scratch, Operand(factory()->heap_number_map()));

  // Check for undefined. Undefined is converted to zero for clamping
  // conversions.
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumberUndefined, input_reg,
               Operand(factory()->undefined_value()));
  __ mov(result_reg, zero_reg);
  __ jmp(&done);

  // Heap number
  __ bind(&heap_number);
  __ ldc1(double_scratch0(), FieldMemOperand(input_reg,
                                             HeapNumber::kValueOffset));
  __ ClampDoubleToUint8(result_reg, double_scratch0(), temp_reg);
  __ jmp(&done);

  __ bind(&is_smi);
  __ ClampUint8(result_reg, scratch);

  __ bind(&done);
}


void LCodeGen::DoAllocate(LAllocate* instr) {
  class DeferredAllocate final : public LDeferredCode {
   public:
    DeferredAllocate(LCodeGen* codegen, LAllocate* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override { codegen()->DoDeferredAllocate(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LAllocate* instr_;
  };

  DeferredAllocate* deferred =
      new(zone()) DeferredAllocate(this, instr);

  Register result = ToRegister(instr->result());
  Register scratch = ToRegister(instr->temp1());
  Register scratch2 = ToRegister(instr->temp2());

  // Allocate memory for the object.
  AllocationFlags flags = NO_ALLOCATION_FLAGS;
  if (instr->hydrogen()->MustAllocateDoubleAligned()) {
    flags = static_cast<AllocationFlags>(flags | DOUBLE_ALIGNMENT);
  }
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE);
  }

  if (instr->hydrogen()->IsAllocationFoldingDominator()) {
    flags = static_cast<AllocationFlags>(flags | ALLOCATION_FOLDING_DOMINATOR);
  }
  DCHECK(!instr->hydrogen()->IsAllocationFolded());

  if (instr->size()->IsConstantOperand()) {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    CHECK(size <= kMaxRegularHeapObjectSize);
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  } else {
    Register size = ToRegister(instr->size());
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  }

  __ bind(deferred->exit());

  if (instr->hydrogen()->MustPrefillWithFiller()) {
    STATIC_ASSERT(kHeapObjectTag == 1);
    if (instr->size()->IsConstantOperand()) {
      int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
      __ li(scratch, Operand(size - kHeapObjectTag));
    } else {
      __ Dsubu(scratch, ToRegister(instr->size()), Operand(kHeapObjectTag));
    }
    __ li(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    Label loop;
    __ bind(&loop);
    __ Dsubu(scratch, scratch, Operand(kPointerSize));
    __ Daddu(at, result, Operand(scratch));
    __ sd(scratch2, MemOperand(at));
    __ Branch(&loop, ge, scratch, Operand(zero_reg));
  }
}


void LCodeGen::DoDeferredAllocate(LAllocate* instr) {
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, zero_reg);

  PushSafepointRegistersScope scope(this);
  if (instr->size()->IsRegister()) {
    Register size = ToRegister(instr->size());
    DCHECK(!size.is(result));
    __ SmiTag(size);
    __ push(size);
  } else {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    if (size >= 0 && size <= Smi::kMaxValue) {
      __ li(v0, Operand(Smi::FromInt(size)));
      __ Push(v0);
    } else {
      // We should never get here at runtime => abort
      __ stop("invalid allocation size");
      return;
    }
  }

  int flags = AllocateDoubleAlignFlag::encode(
      instr->hydrogen()->MustAllocateDoubleAligned());
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = AllocateTargetSpace::update(flags, OLD_SPACE);
  } else {
    flags = AllocateTargetSpace::update(flags, NEW_SPACE);
  }
  __ li(v0, Operand(Smi::FromInt(flags)));
  __ Push(v0);

  CallRuntimeFromDeferred(
      Runtime::kAllocateInTargetSpace, 2, instr, instr->context());
  __ StoreToSafepointRegisterSlot(v0, result);

  if (instr->hydrogen()->IsAllocationFoldingDominator()) {
    AllocationFlags allocation_flags = NO_ALLOCATION_FLAGS;
    if (instr->hydrogen()->IsOldSpaceAllocation()) {
      DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
      allocation_flags = static_cast<AllocationFlags>(flags | PRETENURE);
    }
    // If the allocation folding dominator allocate triggered a GC, allocation
    // happend in the runtime. We have to reset the top pointer to virtually
    // undo the allocation.
    ExternalReference allocation_top =
        AllocationUtils::GetAllocationTopReference(isolate(), allocation_flags);
    Register top_address = scratch0();
    __ Dsubu(v0, v0, Operand(kHeapObjectTag));
    __ li(top_address, Operand(allocation_top));
    __ sd(v0, MemOperand(top_address));
    __ Daddu(v0, v0, Operand(kHeapObjectTag));
  }
}

void LCodeGen::DoFastAllocate(LFastAllocate* instr) {
  DCHECK(instr->hydrogen()->IsAllocationFolded());
  DCHECK(!instr->hydrogen()->IsAllocationFoldingDominator());
  Register result = ToRegister(instr->result());
  Register scratch1 = ToRegister(instr->temp1());
  Register scratch2 = ToRegister(instr->temp2());

  AllocationFlags flags = ALLOCATION_FOLDED;
  if (instr->hydrogen()->MustAllocateDoubleAligned()) {
    flags = static_cast<AllocationFlags>(flags | DOUBLE_ALIGNMENT);
  }
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE);
  }
  if (instr->size()->IsConstantOperand()) {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    CHECK(size <= kMaxRegularHeapObjectSize);
    __ FastAllocate(size, result, scratch1, scratch2, flags);
  } else {
    Register size = ToRegister(instr->size());
    __ FastAllocate(size, result, scratch1, scratch2, flags);
  }
}


void LCodeGen::DoTypeof(LTypeof* instr) {
  DCHECK(ToRegister(instr->value()).is(a3));
  DCHECK(ToRegister(instr->result()).is(v0));
  Label end, do_call;
  Register value_register = ToRegister(instr->value());
  __ JumpIfNotSmi(value_register, &do_call);
  __ li(v0, Operand(isolate()->factory()->number_string()));
  __ jmp(&end);
  __ bind(&do_call);
  Callable callable = CodeFactory::Typeof(isolate());
  CallCode(callable.code(), RelocInfo::CODE_TARGET, instr);
  __ bind(&end);
}


void LCodeGen::DoTypeofIsAndBranch(LTypeofIsAndBranch* instr) {
  Register input = ToRegister(instr->value());

  Register cmp1 = no_reg;
  Operand cmp2 = Operand(no_reg);

  Condition final_branch_condition = EmitTypeofIs(instr->TrueLabel(chunk_),
                                                  instr->FalseLabel(chunk_),
                                                  input,
                                                  instr->type_literal(),
                                                  &cmp1,
                                                  &cmp2);

  DCHECK(cmp1.is_valid());
  DCHECK(!cmp2.is_reg() || cmp2.rm().is_valid());

  if (final_branch_condition != kNoCondition) {
    EmitBranch(instr, final_branch_condition, cmp1, cmp2);
  }
}


Condition LCodeGen::EmitTypeofIs(Label* true_label,
                                 Label* false_label,
                                 Register input,
                                 Handle<String> type_name,
                                 Register* cmp1,
                                 Operand* cmp2) {
  // This function utilizes the delay slot heavily. This is used to load
  // values that are always usable without depending on the type of the input
  // register.
  Condition final_branch_condition = kNoCondition;
  Register scratch = scratch0();
  Factory* factory = isolate()->factory();
  if (String::Equals(type_name, factory->number_string())) {
    __ JumpIfSmi(input, true_label);
    __ ld(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    *cmp1 = input;
    *cmp2 = Operand(at);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->string_string())) {
    __ JumpIfSmi(input, false_label);
    __ GetObjectType(input, input, scratch);
    *cmp1 = scratch;
    *cmp2 = Operand(FIRST_NONSTRING_TYPE);
    final_branch_condition = lt;

  } else if (String::Equals(type_name, factory->symbol_string())) {
    __ JumpIfSmi(input, false_label);
    __ GetObjectType(input, input, scratch);
    *cmp1 = scratch;
    *cmp2 = Operand(SYMBOL_TYPE);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->boolean_string())) {
    __ LoadRoot(at, Heap::kTrueValueRootIndex);
    __ Branch(USE_DELAY_SLOT, true_label, eq, at, Operand(input));
    __ LoadRoot(at, Heap::kFalseValueRootIndex);
    *cmp1 = at;
    *cmp2 = Operand(input);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->undefined_string())) {
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    __ Branch(USE_DELAY_SLOT, false_label, eq, at, Operand(input));
    // The first instruction of JumpIfSmi is an And - it is safe in the delay
    // slot.
    __ JumpIfSmi(input, false_label);
    // Check for undetectable objects => true.
    __ ld(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ lbu(at, FieldMemOperand(input, Map::kBitFieldOffset));
    __ And(at, at, 1 << Map::kIsUndetectable);
    *cmp1 = at;
    *cmp2 = Operand(zero_reg);
    final_branch_condition = ne;

  } else if (String::Equals(type_name, factory->function_string())) {
    __ JumpIfSmi(input, false_label);
    __ ld(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ lbu(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ And(scratch, scratch,
           Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    *cmp1 = scratch;
    *cmp2 = Operand(1 << Map::kIsCallable);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->object_string())) {
    __ JumpIfSmi(input, false_label);
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    __ Branch(USE_DELAY_SLOT, true_label, eq, at, Operand(input));
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ GetObjectType(input, scratch, scratch1());
    __ Branch(false_label, lt, scratch1(), Operand(FIRST_JS_RECEIVER_TYPE));
    // Check for callable or undetectable objects => false.
    __ lbu(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ And(at, scratch,
           Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    *cmp1 = at;
    *cmp2 = Operand(zero_reg);
    final_branch_condition = eq;

// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)        \
  } else if (String::Equals(type_name, factory->type##_string())) {  \
    __ JumpIfSmi(input, false_label);                                \
    __ ld(input, FieldMemOperand(input, HeapObject::kMapOffset));    \
    __ LoadRoot(at, Heap::k##Type##MapRootIndex);                    \
    *cmp1 = input;                                                   \
    *cmp2 = Operand(at);                                             \
    final_branch_condition = eq;
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on


  } else {
    *cmp1 = at;
    *cmp2 = Operand(zero_reg);  // Set to valid regs, to avoid caller assertion.
    __ Branch(false_label);
  }

  return final_branch_condition;
}


void LCodeGen::EnsureSpaceForLazyDeopt(int space_needed) {
  if (info()->ShouldEnsureSpaceForLazyDeopt()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    int current_pc = masm()->pc_offset();
    if (current_pc < last_lazy_deopt_pc_ + space_needed) {
      int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
      DCHECK_EQ(0, padding_size % Assembler::kInstrSize);
      while (padding_size > 0) {
        __ nop();
        padding_size -= Assembler::kInstrSize;
      }
    }
  }
  last_lazy_deopt_pc_ = masm()->pc_offset();
}


void LCodeGen::DoLazyBailout(LLazyBailout* instr) {
  last_lazy_deopt_pc_ = masm()->pc_offset();
  DCHECK(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}


void LCodeGen::DoDeoptimize(LDeoptimize* instr) {
  Deoptimizer::BailoutType type = instr->hydrogen()->type();
  // TODO(danno): Stubs expect all deopts to be lazy for historical reasons (the
  // needed return address), even though the implementation of LAZY and EAGER is
  // now identical. When LAZY is eventually completely folded into EAGER, remove
  // the special case below.
  if (info()->IsStub() && type == Deoptimizer::EAGER) {
    type = Deoptimizer::LAZY;
  }

  DeoptimizeIf(al, instr, instr->hydrogen()->reason(), type, zero_reg,
               Operand(zero_reg));
}


void LCodeGen::DoDummy(LDummy* instr) {
  // Nothing to see here, move on!
}


void LCodeGen::DoDummyUse(LDummyUse* instr) {
  // Nothing to see here, move on!
}


void LCodeGen::DoDeferredStackCheck(LStackCheck* instr) {
  PushSafepointRegistersScope scope(this);
  LoadContextFromDeferred(instr->context());
  __ CallRuntimeSaveDoubles(Runtime::kStackGuard);
  RecordSafepointWithLazyDeopt(
      instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
  DCHECK(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}


void LCodeGen::DoStackCheck(LStackCheck* instr) {
  class DeferredStackCheck final : public LDeferredCode {
   public:
    DeferredStackCheck(LCodeGen* codegen, LStackCheck* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override { codegen()->DoDeferredStackCheck(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LStackCheck* instr_;
  };

  DCHECK(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  // There is no LLazyBailout instruction for stack-checks. We have to
  // prepare for lazy deoptimization explicitly here.
  if (instr->hydrogen()->is_function_entry()) {
    // Perform stack overflow check.
    Label done;
    __ LoadRoot(at, Heap::kStackLimitRootIndex);
    __ Branch(&done, hs, sp, Operand(at));
    DCHECK(instr->context()->IsRegister());
    DCHECK(ToRegister(instr->context()).is(cp));
    CallCode(isolate()->builtins()->StackCheck(),
             RelocInfo::CODE_TARGET,
             instr);
    __ bind(&done);
  } else {
    DCHECK(instr->hydrogen()->is_backwards_branch());
    // Perform stack overflow check if this goto needs it before jumping.
    DeferredStackCheck* deferred_stack_check =
        new(zone()) DeferredStackCheck(this, instr);
    __ LoadRoot(at, Heap::kStackLimitRootIndex);
    __ Branch(deferred_stack_check->entry(), lo, sp, Operand(at));
    EnsureSpaceForLazyDeopt(Deoptimizer::patch_size());
    __ bind(instr->done_label());
    deferred_stack_check->SetExit(instr->done_label());
    RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
    // Don't record a deoptimization index for the safepoint here.
    // This will be done explicitly when emitting call and the safepoint in
    // the deferred code.
  }
}


void LCodeGen::DoOsrEntry(LOsrEntry* instr) {
  // This is a pseudo-instruction that ensures that the environment here is
  // properly registered for deoptimization and records the assembler's PC
  // offset.
  LEnvironment* environment = instr->environment();

  // If the environment were already registered, we would have no way of
  // backpatching it with the spill slot operands.
  DCHECK(!environment->HasBeenRegistered());
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);

  GenerateOsrPrologue();
}


void LCodeGen::DoForInPrepareMap(LForInPrepareMap* instr) {
  Register result = ToRegister(instr->result());
  Register object = ToRegister(instr->object());

  Label use_cache, call_runtime;
  DCHECK(object.is(a0));
  __ CheckEnumCache(&call_runtime);

  __ ld(result, FieldMemOperand(object, HeapObject::kMapOffset));
  __ Branch(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(object);
  CallRuntime(Runtime::kForInEnumerate, instr);
  __ bind(&use_cache);
}


void LCodeGen::DoForInCacheArray(LForInCacheArray* instr) {
  Register map = ToRegister(instr->map());
  Register result = ToRegister(instr->result());
  Label load_cache, done;
  __ EnumLength(result, map);
  __ Branch(&load_cache, ne, result, Operand(Smi::kZero));
  __ li(result, Operand(isolate()->factory()->empty_fixed_array()));
  __ jmp(&done);

  __ bind(&load_cache);
  __ LoadInstanceDescriptors(map, result);
  __ ld(result,
        FieldMemOperand(result, DescriptorArray::kEnumCacheOffset));
  __ ld(result,
        FieldMemOperand(result, FixedArray::SizeFor(instr->idx())));
  DeoptimizeIf(eq, instr, DeoptimizeReason::kNoCache, result,
               Operand(zero_reg));

  __ bind(&done);
}


void LCodeGen::DoCheckMapValue(LCheckMapValue* instr) {
  Register object = ToRegister(instr->value());
  Register map = ToRegister(instr->map());
  __ ld(scratch0(), FieldMemOperand(object, HeapObject::kMapOffset));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongMap, map,
               Operand(scratch0()));
}


void LCodeGen::DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                           Register result,
                                           Register object,
                                           Register index) {
  PushSafepointRegistersScope scope(this);
  __ Push(object, index);
  __ mov(cp, zero_reg);
  __ CallRuntimeSaveDoubles(Runtime::kLoadMutableDouble);
  RecordSafepointWithRegisters(
     instr->pointer_map(), 2, Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(v0, result);
}


void LCodeGen::DoLoadFieldByIndex(LLoadFieldByIndex* instr) {
  class DeferredLoadMutableDouble final : public LDeferredCode {
   public:
    DeferredLoadMutableDouble(LCodeGen* codegen,
                              LLoadFieldByIndex* instr,
                              Register result,
                              Register object,
                              Register index)
        : LDeferredCode(codegen),
          instr_(instr),
          result_(result),
          object_(object),
          index_(index) {
    }
    void Generate() override {
      codegen()->DoDeferredLoadMutableDouble(instr_, result_, object_, index_);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LLoadFieldByIndex* instr_;
    Register result_;
    Register object_;
    Register index_;
  };

  Register object = ToRegister(instr->object());
  Register index = ToRegister(instr->index());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  DeferredLoadMutableDouble* deferred;
  deferred = new(zone()) DeferredLoadMutableDouble(
      this, instr, result, object, index);

  Label out_of_object, done;

  __ And(scratch, index, Operand(Smi::FromInt(1)));
  __ Branch(deferred->entry(), ne, scratch, Operand(zero_reg));
  __ dsra(index, index, 1);

  __ Branch(USE_DELAY_SLOT, &out_of_object, lt, index, Operand(zero_reg));
  __ SmiScale(scratch, index, kPointerSizeLog2);  // In delay slot.
  __ Daddu(scratch, object, scratch);
  __ ld(result, FieldMemOperand(scratch, JSObject::kHeaderSize));

  __ Branch(&done);

  __ bind(&out_of_object);
  __ ld(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
  // Index is equal to negated out of object property index plus 1.
  __ Dsubu(scratch, result, scratch);
  __ ld(result, FieldMemOperand(scratch,
                                FixedArray::kHeaderSize - kPointerSize));
  __ bind(deferred->exit());
  __ bind(&done);
}

#undef __

}  // namespace internal
}  // namespace v8
