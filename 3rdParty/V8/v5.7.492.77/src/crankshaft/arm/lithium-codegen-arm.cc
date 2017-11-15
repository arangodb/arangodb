// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/arm/lithium-codegen-arm.h"

#include "src/base/bits.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/crankshaft/arm/lithium-gap-resolver-arm.h"
#include "src/crankshaft/hydrogen-osr.h"
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
    __ vstr(DoubleRegister::from_code(save_iterator.Current()),
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
    __ vldr(DoubleRegister::from_code(save_iterator.Current()),
            MemOperand(sp, count * kDoubleSize));
    save_iterator.Advance();
    count++;
  }
}


bool LCodeGen::GeneratePrologue() {
  DCHECK(is_generating());

  if (info()->IsOptimizing()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm_);

    // r1: Callee's JS function.
    // cp: Callee's context.
    // pp: Callee's constant pool pointer (if enabled)
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
      __ sub(sp,  sp, Operand(slots * kPointerSize));
      __ push(r0);
      __ push(r1);
      __ add(r0, sp, Operand(slots *  kPointerSize));
      __ mov(r1, Operand(kSlotsZapValue));
      Label loop;
      __ bind(&loop);
      __ sub(r0, r0, Operand(kPointerSize));
      __ str(r1, MemOperand(r0, 2 * kPointerSize));
      __ cmp(r0, sp);
      __ b(ne, &loop);
      __ pop(r1);
      __ pop(r0);
    } else {
      __ sub(sp,  sp, Operand(slots * kPointerSize));
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
    // Argument to NewContext is the function, which is in r1.
    int slots = info()->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    Safepoint::DeoptMode deopt_mode = Safepoint::kNoLazyDeopt;
    if (info()->scope()->is_script_scope()) {
      __ push(r1);
      __ Push(info()->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      deopt_mode = Safepoint::kLazyDeopt;
    } else {
      if (slots <=
          ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
        Callable callable = CodeFactory::FastNewFunctionContext(
            isolate(), info()->scope()->scope_type());
        __ mov(FastNewFunctionContextDescriptor::SlotsRegister(),
               Operand(slots));
        __ Call(callable.code(), RelocInfo::CODE_TARGET);
        // Result of the FastNewFunctionContext builtin is always in new space.
        need_write_barrier = false;
      } else {
        __ push(r1);
        __ Push(Smi::FromInt(info()->scope()->scope_type()));
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
    }
    RecordSafepoint(deopt_mode);

    // Context is returned in both r0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ mov(cp, r0);
    __ str(r0, MemOperand(fp, StandardFrameConstants::kContextOffset));
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
        __ ldr(r0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ str(r0, target);
        // Update the write barrier. This clobbers r3 and r0.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(
              cp,
              target.offset(),
              r0,
              r3,
              GetLinkRegisterState(),
              kSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r0, &done);
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
  __ sub(sp, sp, Operand(slots * kPointerSize));
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
        __ Move(scratch0(), Smi::FromInt(StackFrame::STUB));
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

  // Force constant pool emission at the end of the deferred code to make
  // sure that no constant pools are emitted after.
  masm()->CheckConstPool(true, false);

  return !is_aborted();
}


bool LCodeGen::GenerateJumpTable() {
  // Check that the jump table is accessible from everywhere in the function
  // code, i.e. that offsets to the table can be encoded in the 24bit signed
  // immediate of a branch instruction.
  // To simplify we consider the code size from the first instruction to the
  // end of the jump table. We also don't consider the pc load delta.
  // Each entry in the jump table generates one instruction and inlines one
  // 32bit data after it.
  if (!is_int24((masm()->pc_offset() / Assembler::kInstrSize) +
                jump_table_.length() * 7)) {
    Abort(kGeneratedCodeIsTooLarge);
  }

  if (jump_table_.length() > 0) {
    Label needs_frame, call_deopt_entry;

    Comment(";;; -------------------- Jump table --------------------");
    Address base = jump_table_[0].address;

    Register entry_offset = scratch0();

    int length = jump_table_.length();
    for (int i = 0; i < length; i++) {
      Deoptimizer::JumpTableEntry* table_entry = &jump_table_[i];
      __ bind(&table_entry->label);

      DCHECK_EQ(jump_table_[0].bailout_type, table_entry->bailout_type);
      Address entry = table_entry->address;
      DeoptComment(table_entry->deopt_info);

      // Second-level deopt table entries are contiguous and small, so instead
      // of loading the full, absolute address of each one, load an immediate
      // offset which will be added to the base address later.
      __ mov(entry_offset, Operand(entry - base));

      if (table_entry->needs_frame) {
        DCHECK(!info()->saves_caller_doubles());
        Comment(";;; call deopt with frame");
        __ PushCommonFrame();
        __ bl(&needs_frame);
      } else {
        __ bl(&call_deopt_entry);
      }
      masm()->CheckConstPool(false, false);
    }

    if (needs_frame.is_linked()) {
      __ bind(&needs_frame);
      // This variant of deopt can only be used with stubs. Since we don't
      // have a function pointer to install in the stack frame that we're
      // building, install a special marker there instead.
      __ mov(ip, Operand(Smi::FromInt(StackFrame::STUB)));
      __ push(ip);
      DCHECK(info()->IsStub());
    }

    Comment(";;; call deopt");
    __ bind(&call_deopt_entry);

    if (info()->saves_caller_doubles()) {
      DCHECK(info()->IsStub());
      RestoreCallerDoubles();
    }

    // Add the base address to the offset previously loaded in entry_offset.
    __ add(entry_offset, entry_offset,
           Operand(ExternalReference::ForDeoptEntry(base)));
    __ bx(entry_offset);
  }

  // Force constant pool emission at the end of the deopt jump table to make
  // sure that no constant pools are emitted after.
  masm()->CheckConstPool(true, false);

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


Register LCodeGen::ToRegister(int code) const {
  return Register::from_code(code);
}


DwVfpRegister LCodeGen::ToDoubleRegister(int code) const {
  return DwVfpRegister::from_code(code);
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
      __ mov(scratch, Operand(static_cast<int32_t>(literal->Number())));
    } else if (r.IsDouble()) {
      Abort(kEmitLoadRegisterUnsupportedDoubleImmediate);
    } else {
      DCHECK(r.IsSmiOrTagged());
      __ Move(scratch, literal);
    }
    return scratch;
  } else if (op->IsStackSlot()) {
    __ ldr(scratch, ToMemOperand(op));
    return scratch;
  }
  UNREACHABLE();
  return scratch;
}


DwVfpRegister LCodeGen::ToDoubleRegister(LOperand* op) const {
  DCHECK(op->IsDoubleRegister());
  return ToDoubleRegister(op->index());
}


DwVfpRegister LCodeGen::EmitLoadDoubleRegister(LOperand* op,
                                               SwVfpRegister flt_scratch,
                                               DwVfpRegister dbl_scratch) {
  if (op->IsDoubleRegister()) {
    return ToDoubleRegister(op->index());
  } else if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk_->LookupConstant(const_op);
    Handle<Object> literal = constant->handle(isolate());
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      DCHECK(literal->IsNumber());
      __ mov(ip, Operand(static_cast<int32_t>(literal->Number())));
      __ vmov(flt_scratch, ip);
      __ vcvt_f64_s32(dbl_scratch, flt_scratch);
      return dbl_scratch;
    } else if (r.IsDouble()) {
      Abort(kUnsupportedDoubleImmediate);
    } else if (r.IsTagged()) {
      Abort(kUnsupportedTaggedImmediate);
    }
  } else if (op->IsStackSlot()) {
    // TODO(regis): Why is vldr not taking a MemOperand?
    // __ vldr(dbl_scratch, ToMemOperand(op));
    MemOperand mem_op = ToMemOperand(op);
    __ vldr(dbl_scratch, mem_op.rn(), mem_op.offset());
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
  return ToRepresentation(op, Representation::Integer32());
}


int32_t LCodeGen::ToRepresentation(LConstantOperand* op,
                                   const Representation& r) const {
  HConstant* constant = chunk_->LookupConstant(op);
  int32_t value = constant->Integer32Value();
  if (r.IsInteger32()) return value;
  DCHECK(r.IsSmiOrTagged());
  return reinterpret_cast<int32_t>(Smi::FromInt(value));
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
    return Operand::Zero();
  }
  // Stack slots not implemented, use ToMemOperand instead.
  UNREACHABLE();
  return Operand::Zero();
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
    return MemOperand(fp, FrameSlotToFPOffset(op->index()) + kPointerSize);
  } else {
    // Retrieve parameter without eager stack-frame relative to the
    // stack-pointer.
    return MemOperand(
        sp, ArgumentsOffsetWithoutFrame(op->index()) + kPointerSize);
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


int LCodeGen::CallCodeSize(Handle<Code> code, RelocInfo::Mode mode) {
  int size = masm()->CallSize(code, mode);
  if (code->kind() == Code::BINARY_OP_IC ||
      code->kind() == Code::COMPARE_IC) {
    size += Assembler::kInstrSize;  // extra nop() added in CallCodeGeneric.
  }
  return size;
}


void LCodeGen::CallCode(Handle<Code> code,
                        RelocInfo::Mode mode,
                        LInstruction* instr,
                        TargetAddressStorageMode storage_mode) {
  CallCodeGeneric(code, mode, instr, RECORD_SIMPLE_SAFEPOINT, storage_mode);
}


void LCodeGen::CallCodeGeneric(Handle<Code> code,
                               RelocInfo::Mode mode,
                               LInstruction* instr,
                               SafepointMode safepoint_mode,
                               TargetAddressStorageMode storage_mode) {
  DCHECK(instr != NULL);
  // Block literal pool emission to ensure nop indicating no inlined smi code
  // is in the correct position.
  Assembler::BlockConstPoolScope block_const_pool(masm());
  __ Call(code, mode, TypeFeedbackId::None(), al, storage_mode);
  RecordSafepointWithLazyDeopt(instr, safepoint_mode);

  // Signal that we don't inline smi code before these stubs in the
  // optimizing code generator.
  if (code->kind() == Code::BINARY_OP_IC ||
      code->kind() == Code::COMPARE_IC) {
    __ nop();
  }
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
    __ ldr(cp, ToMemOperand(context));
  } else if (context->IsConstantOperand()) {
    HConstant* constant =
        chunk_->LookupConstant(LConstantOperand::cast(context));
    __ Move(cp, Handle<Object>::cast(constant->handle(isolate())));
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
                            Deoptimizer::BailoutType bailout_type) {
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

    // Store the condition on the stack if necessary
    if (condition != al) {
      __ mov(scratch, Operand::Zero(), LeaveCC, NegateCondition(condition));
      __ mov(scratch, Operand(1), LeaveCC, condition);
      __ push(scratch);
    }

    __ push(r1);
    __ mov(scratch, Operand(count));
    __ ldr(r1, MemOperand(scratch));
    __ sub(r1, r1, Operand(1), SetCC);
    __ mov(r1, Operand(FLAG_deopt_every_n_times), LeaveCC, eq);
    __ str(r1, MemOperand(scratch));
    __ pop(r1);

    if (condition != al) {
      // Clean up the stack before the deoptimizer call
      __ pop(scratch);
    }

    __ Call(entry, RelocInfo::RUNTIME_ENTRY, eq);

    // 'Restore' the condition in a slightly hacky way. (It would be better
    // to use 'msr' and 'mrs' instructions here, but they are not supported by
    // our ARM simulator).
    if (condition != al) {
      condition = ne;
      __ cmp(scratch, Operand::Zero());
    }
  }

  if (info()->ShouldTrapOnDeopt()) {
    __ stop("trap_on_deopt", condition);
  }

  Deoptimizer::DeoptInfo deopt_info = MakeDeoptInfo(instr, deopt_reason, id);

  DCHECK(info()->IsStub() || frame_is_built_);
  // Go through jump table if we need to handle condition, build frame, or
  // restore caller doubles.
  if (condition == al && frame_is_built_ &&
      !info()->saves_caller_doubles()) {
    DeoptComment(deopt_info);
    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
  } else {
    Deoptimizer::JumpTableEntry table_entry(entry, deopt_info, bailout_type,
                                            !frame_is_built_);
    // We often have several deopts to the same entry, reuse the last
    // jump entry if this is the case.
    if (FLAG_trace_deopt || isolate()->is_profiling() ||
        jump_table_.is_empty() ||
        !table_entry.IsEquivalentTo(jump_table_.last())) {
      jump_table_.Add(table_entry, zone());
    }
    __ b(condition, &jump_table_.last().label);
  }
}

void LCodeGen::DeoptimizeIf(Condition condition, LInstruction* instr,
                            DeoptimizeReason deopt_reason) {
  Deoptimizer::BailoutType bailout_type = info()->IsStub()
      ? Deoptimizer::LAZY
      : Deoptimizer::EAGER;
  DeoptimizeIf(condition, instr, deopt_reason, bailout_type);
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
    __ cmp(dividend, Operand::Zero());
    __ b(pl, &dividend_is_not_negative);
    // Note that this is correct even for kMinInt operands.
    __ rsb(dividend, dividend, Operand::Zero());
    __ and_(dividend, dividend, Operand(mask));
    __ rsb(dividend, dividend, Operand::Zero(), SetCC);
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    }
    __ b(&done);
  }

  __ bind(&dividend_is_not_negative);
  __ and_(dividend, dividend, Operand(mask));
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
  __ mov(ip, Operand(Abs(divisor)));
  __ smull(result, ip, result, ip);
  __ sub(result, dividend, result, SetCC);

  // Check for negative zero.
  HMod* hmod = instr->hydrogen();
  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label remainder_not_zero;
    __ b(ne, &remainder_not_zero);
    __ cmp(dividend, Operand::Zero());
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
    __ bind(&remainder_not_zero);
  }
}


void LCodeGen::DoModI(LModI* instr) {
  HMod* hmod = instr->hydrogen();
  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm(), SUDIV);

    Register left_reg = ToRegister(instr->left());
    Register right_reg = ToRegister(instr->right());
    Register result_reg = ToRegister(instr->result());

    Label done;
    // Check for x % 0, sdiv might signal an exception. We have to deopt in this
    // case because we can't return a NaN.
    if (hmod->CheckFlag(HValue::kCanBeDivByZero)) {
      __ cmp(right_reg, Operand::Zero());
      DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
    }

    // Check for kMinInt % -1, sdiv will return kMinInt, which is not what we
    // want. We have to deopt if we care about -0, because we can't return that.
    if (hmod->CheckFlag(HValue::kCanOverflow)) {
      Label no_overflow_possible;
      __ cmp(left_reg, Operand(kMinInt));
      __ b(ne, &no_overflow_possible);
      __ cmp(right_reg, Operand(-1));
      if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
        DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
      } else {
        __ b(ne, &no_overflow_possible);
        __ mov(result_reg, Operand::Zero());
        __ jmp(&done);
      }
      __ bind(&no_overflow_possible);
    }

    // For 'r3 = r1 % r2' we can have the following ARM code:
    //   sdiv r3, r1, r2
    //   mls r3, r3, r2, r1

    __ sdiv(result_reg, left_reg, right_reg);
    __ Mls(result_reg, result_reg, right_reg, left_reg);

    // If we care about -0, test if the dividend is <0 and the result is 0.
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmp(result_reg, Operand::Zero());
      __ b(ne, &done);
      __ cmp(left_reg, Operand::Zero());
      DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
    }
    __ bind(&done);

  } else {
    // General case, without any SDIV support.
    Register left_reg = ToRegister(instr->left());
    Register right_reg = ToRegister(instr->right());
    Register result_reg = ToRegister(instr->result());
    Register scratch = scratch0();
    DCHECK(!scratch.is(left_reg));
    DCHECK(!scratch.is(right_reg));
    DCHECK(!scratch.is(result_reg));
    DwVfpRegister dividend = ToDoubleRegister(instr->temp());
    DwVfpRegister divisor = ToDoubleRegister(instr->temp2());
    DCHECK(!divisor.is(dividend));
    LowDwVfpRegister quotient = double_scratch0();
    DCHECK(!quotient.is(dividend));
    DCHECK(!quotient.is(divisor));

    Label done;
    // Check for x % 0, we have to deopt in this case because we can't return a
    // NaN.
    if (hmod->CheckFlag(HValue::kCanBeDivByZero)) {
      __ cmp(right_reg, Operand::Zero());
      DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
    }

    __ Move(result_reg, left_reg);
    // Load the arguments in VFP registers. The divisor value is preloaded
    // before. Be careful that 'right_reg' is only live on entry.
    // TODO(svenpanne) The last comments seems to be wrong nowadays.
    __ vmov(double_scratch0().low(), left_reg);
    __ vcvt_f64_s32(dividend, double_scratch0().low());
    __ vmov(double_scratch0().low(), right_reg);
    __ vcvt_f64_s32(divisor, double_scratch0().low());

    // We do not care about the sign of the divisor. Note that we still handle
    // the kMinInt % -1 case correctly, though.
    __ vabs(divisor, divisor);
    // Compute the quotient and round it to a 32bit integer.
    __ vdiv(quotient, dividend, divisor);
    __ vcvt_s32_f64(quotient.low(), quotient);
    __ vcvt_f64_s32(quotient, quotient.low());

    // Compute the remainder in result.
    __ vmul(double_scratch0(), divisor, quotient);
    __ vcvt_s32_f64(double_scratch0().low(), double_scratch0());
    __ vmov(scratch, double_scratch0().low());
    __ sub(result_reg, left_reg, scratch, SetCC);

    // If we care about -0, test if the dividend is <0 and the result is 0.
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ b(ne, &done);
      __ cmp(left_reg, Operand::Zero());
      DeoptimizeIf(mi, instr, DeoptimizeReason::kMinusZero);
    }
    __ bind(&done);
  }
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
    __ cmp(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }
  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) && divisor == -1) {
    __ cmp(dividend, Operand(kMinInt));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
  }
  // Deoptimize if remainder will not be 0.
  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32) &&
      divisor != 1 && divisor != -1) {
    int32_t mask = divisor < 0 ? -(divisor + 1) : (divisor - 1);
    __ tst(dividend, Operand(mask));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision);
  }

  if (divisor == -1) {  // Nice shortcut, not needed for correctness.
    __ rsb(result, dividend, Operand(0));
    return;
  }
  int32_t shift = WhichPowerOf2Abs(divisor);
  if (shift == 0) {
    __ mov(result, dividend);
  } else if (shift == 1) {
    __ add(result, dividend, Operand(dividend, LSR, 31));
  } else {
    __ mov(result, Operand(dividend, ASR, 31));
    __ add(result, dividend, Operand(result, LSR, 32 - shift));
  }
  if (shift > 0) __ mov(result, Operand(result, ASR, shift));
  if (divisor < 0) __ rsb(result, result, Operand(0));
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
    __ cmp(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ rsb(result, result, Operand::Zero());

  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    __ mov(ip, Operand(divisor));
    __ smull(scratch0(), ip, result, ip);
    __ sub(scratch0(), scratch0(), dividend, SetCC);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision);
  }
}


// TODO(svenpanne) Refactor this to avoid code duplication with DoFlooringDivI.
void LCodeGen::DoDivI(LDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  Register dividend = ToRegister(instr->dividend());
  Register divisor = ToRegister(instr->divisor());
  Register result = ToRegister(instr->result());

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmp(divisor, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label positive;
    if (!instr->hydrogen_value()->CheckFlag(HValue::kCanBeDivByZero)) {
      // Do the test only if it hadn't be done above.
      __ cmp(divisor, Operand::Zero());
    }
    __ b(pl, &positive);
    __ cmp(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    __ bind(&positive);
  }

  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) &&
      (!CpuFeatures::IsSupported(SUDIV) ||
       !hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32))) {
    // We don't need to check for overflow when truncating with sdiv
    // support because, on ARM, sdiv kMinInt, -1 -> kMinInt.
    __ cmp(dividend, Operand(kMinInt));
    __ cmp(divisor, Operand(-1), eq);
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
  }

  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm(), SUDIV);
    __ sdiv(result, dividend, divisor);
  } else {
    DoubleRegister vleft = ToDoubleRegister(instr->temp());
    DoubleRegister vright = double_scratch0();
    __ vmov(double_scratch0().low(), dividend);
    __ vcvt_f64_s32(vleft, double_scratch0().low());
    __ vmov(double_scratch0().low(), divisor);
    __ vcvt_f64_s32(vright, double_scratch0().low());
    __ vdiv(vleft, vleft, vright);  // vleft now contains the result.
    __ vcvt_s32_f64(double_scratch0().low(), vleft);
    __ vmov(result, double_scratch0().low());
  }

  if (!hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
    // Compute remainder and deopt if it's not zero.
    Register remainder = scratch0();
    __ Mls(remainder, result, divisor, dividend);
    __ cmp(remainder, Operand::Zero());
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision);
  }
}


void LCodeGen::DoMultiplyAddD(LMultiplyAddD* instr) {
  DwVfpRegister addend = ToDoubleRegister(instr->addend());
  DwVfpRegister multiplier = ToDoubleRegister(instr->multiplier());
  DwVfpRegister multiplicand = ToDoubleRegister(instr->multiplicand());

  // This is computed in-place.
  DCHECK(addend.is(ToDoubleRegister(instr->result())));

  __ vmla(addend, multiplier, multiplicand);
}


void LCodeGen::DoMultiplySubD(LMultiplySubD* instr) {
  DwVfpRegister minuend = ToDoubleRegister(instr->minuend());
  DwVfpRegister multiplier = ToDoubleRegister(instr->multiplier());
  DwVfpRegister multiplicand = ToDoubleRegister(instr->multiplicand());

  // This is computed in-place.
  DCHECK(minuend.is(ToDoubleRegister(instr->result())));

  __ vmls(minuend, multiplier, multiplicand);
}


void LCodeGen::DoFlooringDivByPowerOf2I(LFlooringDivByPowerOf2I* instr) {
  Register dividend = ToRegister(instr->dividend());
  Register result = ToRegister(instr->result());
  int32_t divisor = instr->divisor();

  // If the divisor is 1, return the dividend.
  if (divisor == 1) {
    __ Move(result, dividend);
    return;
  }

  // If the divisor is positive, things are easy: There can be no deopts and we
  // can simply do an arithmetic right shift.
  int32_t shift = WhichPowerOf2Abs(divisor);
  if (divisor > 1) {
    __ mov(result, Operand(dividend, ASR, shift));
    return;
  }

  // If the divisor is negative, we have to negate and handle edge cases.
  __ rsb(result, dividend, Operand::Zero(), SetCC);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }

  // Dividing by -1 is basically negation, unless we overflow.
  if (divisor == -1) {
    if (instr->hydrogen()->CheckFlag(HValue::kLeftCanBeMinInt)) {
      DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
    }
    return;
  }

  // If the negation could not overflow, simply shifting is OK.
  if (!instr->hydrogen()->CheckFlag(HValue::kLeftCanBeMinInt)) {
    __ mov(result, Operand(result, ASR, shift));
    return;
  }

  __ mov(result, Operand(kMinInt / divisor), LeaveCC, vs);
  __ mov(result, Operand(result, ASR, shift), LeaveCC, vc);
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
    __ cmp(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }

  // Easy case: We need no dynamic check for the dividend and the flooring
  // division is the same as the truncating division.
  if ((divisor > 0 && !hdiv->CheckFlag(HValue::kLeftCanBeNegative)) ||
      (divisor < 0 && !hdiv->CheckFlag(HValue::kLeftCanBePositive))) {
    __ TruncatingDiv(result, dividend, Abs(divisor));
    if (divisor < 0) __ rsb(result, result, Operand::Zero());
    return;
  }

  // In the general case we may need to adjust before and after the truncating
  // division to get a flooring division.
  Register temp = ToRegister(instr->temp());
  DCHECK(!temp.is(dividend) && !temp.is(result));
  Label needs_adjustment, done;
  __ cmp(dividend, Operand::Zero());
  __ b(divisor > 0 ? lt : gt, &needs_adjustment);
  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ rsb(result, result, Operand::Zero());
  __ jmp(&done);
  __ bind(&needs_adjustment);
  __ add(temp, dividend, Operand(divisor > 0 ? 1 : -1));
  __ TruncatingDiv(result, temp, Abs(divisor));
  if (divisor < 0) __ rsb(result, result, Operand::Zero());
  __ sub(result, result, Operand(1));
  __ bind(&done);
}


// TODO(svenpanne) Refactor this to avoid code duplication with DoDivI.
void LCodeGen::DoFlooringDivI(LFlooringDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  Register left = ToRegister(instr->dividend());
  Register right = ToRegister(instr->divisor());
  Register result = ToRegister(instr->result());

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmp(right, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label positive;
    if (!instr->hydrogen_value()->CheckFlag(HValue::kCanBeDivByZero)) {
      // Do the test only if it hadn't be done above.
      __ cmp(right, Operand::Zero());
    }
    __ b(pl, &positive);
    __ cmp(left, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    __ bind(&positive);
  }

  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) &&
      (!CpuFeatures::IsSupported(SUDIV) ||
       !hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32))) {
    // We don't need to check for overflow when truncating with sdiv
    // support because, on ARM, sdiv kMinInt, -1 -> kMinInt.
    __ cmp(left, Operand(kMinInt));
    __ cmp(right, Operand(-1), eq);
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
  }

  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm(), SUDIV);
    __ sdiv(result, left, right);
  } else {
    DoubleRegister vleft = ToDoubleRegister(instr->temp());
    DoubleRegister vright = double_scratch0();
    __ vmov(double_scratch0().low(), left);
    __ vcvt_f64_s32(vleft, double_scratch0().low());
    __ vmov(double_scratch0().low(), right);
    __ vcvt_f64_s32(vright, double_scratch0().low());
    __ vdiv(vleft, vleft, vright);  // vleft now contains the result.
    __ vcvt_s32_f64(double_scratch0().low(), vleft);
    __ vmov(result, double_scratch0().low());
  }

  Label done;
  Register remainder = scratch0();
  __ Mls(remainder, result, right, left);
  __ cmp(remainder, Operand::Zero());
  __ b(eq, &done);
  __ eor(remainder, remainder, Operand(right));
  __ add(result, result, Operand(remainder, ASR, 31));
  __ bind(&done);
}


void LCodeGen::DoMulI(LMulI* instr) {
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
      __ cmp(left, Operand::Zero());
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    }

    switch (constant) {
      case -1:
        if (overflow) {
          __ rsb(result, left, Operand::Zero(), SetCC);
          DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
        } else {
          __ rsb(result, left, Operand::Zero());
        }
        break;
      case 0:
        if (bailout_on_minus_zero) {
          // If left is strictly negative and the constant is null, the
          // result is -0. Deoptimize if required, otherwise return 0.
          __ cmp(left, Operand::Zero());
          DeoptimizeIf(mi, instr, DeoptimizeReason::kMinusZero);
        }
        __ mov(result, Operand::Zero());
        break;
      case 1:
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
          __ mov(result, Operand(left, LSL, shift));
          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand::Zero());
        } else if (base::bits::IsPowerOfTwo32(constant_abs - 1)) {
          int32_t shift = WhichPowerOf2(constant_abs - 1);
          __ add(result, left, Operand(left, LSL, shift));
          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand::Zero());
        } else if (base::bits::IsPowerOfTwo32(constant_abs + 1)) {
          int32_t shift = WhichPowerOf2(constant_abs + 1);
          __ rsb(result, left, Operand(left, LSL, shift));
          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand::Zero());
        } else {
          // Generate standard code.
          __ mov(ip, Operand(constant));
          __ mul(result, left, ip);
        }
    }

  } else {
    DCHECK(right_op->IsRegister());
    Register right = ToRegister(right_op);

    if (overflow) {
      Register scratch = scratch0();
      // scratch:result = left * right.
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ smull(result, scratch, result, right);
      } else {
        __ smull(result, scratch, left, right);
      }
      __ cmp(scratch, Operand(result, ASR, 31));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow);
    } else {
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ mul(result, result, right);
      } else {
        __ mul(result, left, right);
      }
    }

    if (bailout_on_minus_zero) {
      Label done;
      __ teq(left, Operand(right));
      __ b(pl, &done);
      // Bail out if the result is minus zero.
      __ cmp(result, Operand::Zero());
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
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
    right = Operand(EmitLoadRegister(right_op, ip));
  } else {
    DCHECK(right_op->IsRegister() || right_op->IsConstantOperand());
    right = ToOperand(right_op);
  }

  switch (instr->op()) {
    case Token::BIT_AND:
      __ and_(result, left, right);
      break;
    case Token::BIT_OR:
      __ orr(result, left, right);
      break;
    case Token::BIT_XOR:
      if (right_op->IsConstantOperand() && right.immediate() == int32_t(~0)) {
        __ mvn(result, Operand(left));
      } else {
        __ eor(result, left, right);
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
  Register scratch = scratch0();
  if (right_op->IsRegister()) {
    // Mask the right_op operand.
    __ and_(scratch, ToRegister(right_op), Operand(0x1F));
    switch (instr->op()) {
      case Token::ROR:
        __ mov(result, Operand(left, ROR, scratch));
        break;
      case Token::SAR:
        __ mov(result, Operand(left, ASR, scratch));
        break;
      case Token::SHR:
        if (instr->can_deopt()) {
          __ mov(result, Operand(left, LSR, scratch), SetCC);
          DeoptimizeIf(mi, instr, DeoptimizeReason::kNegativeValue);
        } else {
          __ mov(result, Operand(left, LSR, scratch));
        }
        break;
      case Token::SHL:
        __ mov(result, Operand(left, LSL, scratch));
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
          __ mov(result, Operand(left, ROR, shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SAR:
        if (shift_count != 0) {
          __ mov(result, Operand(left, ASR, shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SHR:
        if (shift_count != 0) {
          __ mov(result, Operand(left, LSR, shift_count));
        } else {
          if (instr->can_deopt()) {
            __ tst(left, Operand(0x80000000));
            DeoptimizeIf(ne, instr, DeoptimizeReason::kNegativeValue);
          }
          __ Move(result, left);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
          if (instr->hydrogen_value()->representation().IsSmi() &&
              instr->can_deopt()) {
            if (shift_count != 1) {
              __ mov(result, Operand(left, LSL, shift_count - 1));
              __ SmiTag(result, result, SetCC);
            } else {
              __ SmiTag(result, left, SetCC);
            }
            DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
          } else {
            __ mov(result, Operand(left, LSL, shift_count));
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


void LCodeGen::DoSubI(LSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ sub(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ sub(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
  }
}


void LCodeGen::DoRSubI(LRSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ rsb(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ rsb(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantS(LConstantS* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  DCHECK(instr->result()->IsDoubleRegister());
  DwVfpRegister result = ToDoubleRegister(instr->result());
#if V8_HOST_ARCH_IA32
  // Need some crappy work-around for x87 sNaN -> qNaN breakage in simulator
  // builds.
  uint64_t bits = instr->bits();
  if ((bits & V8_UINT64_C(0x7FF8000000000000)) ==
      V8_UINT64_C(0x7FF0000000000000)) {
    uint32_t lo = static_cast<uint32_t>(bits);
    uint32_t hi = static_cast<uint32_t>(bits >> 32);
    __ mov(ip, Operand(lo));
    __ mov(scratch0(), Operand(hi));
    __ vmov(result, ip, scratch0());
    return;
  }
#endif
  double v = instr->value();
  __ Vmov(result, v, scratch0());
}


void LCodeGen::DoConstantE(LConstantE* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantT(LConstantT* instr) {
  Handle<Object> object = instr->value(isolate());
  AllowDeferredHandleDereference smi_check;
  __ Move(ToRegister(instr->result()), object);
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
    __ add(scratch, string, Operand(ToRegister(index)));
  } else {
    STATIC_ASSERT(kUC16Size == 2);
    __ add(scratch, string, Operand(ToRegister(index), LSL, 1));
  }
  return FieldMemOperand(scratch, SeqString::kHeaderSize);
}


void LCodeGen::DoSeqStringGetChar(LSeqStringGetChar* instr) {
  String::Encoding encoding = instr->hydrogen()->encoding();
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());

  if (FLAG_debug_code) {
    Register scratch = scratch0();
    __ ldr(scratch, FieldMemOperand(string, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

    __ and_(scratch, scratch,
            Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ cmp(scratch, Operand(encoding == String::ONE_BYTE_ENCODING
                            ? one_byte_seq_type : two_byte_seq_type));
    __ Check(eq, kUnexpectedStringType);
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ ldrb(result, operand);
  } else {
    __ ldrh(result, operand);
  }
}


void LCodeGen::DoSeqStringSetChar(LSeqStringSetChar* instr) {
  String::Encoding encoding = instr->hydrogen()->encoding();
  Register string = ToRegister(instr->string());
  Register value = ToRegister(instr->value());

  if (FLAG_debug_code) {
    Register index = ToRegister(instr->index());
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    int encoding_mask =
        instr->hydrogen()->encoding() == String::ONE_BYTE_ENCODING
        ? one_byte_seq_type : two_byte_seq_type;
    __ EmitSeqStringSetCharCheck(string, index, value, encoding_mask);
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ strb(value, operand);
  } else {
    __ strh(value, operand);
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ add(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    DCHECK(right->IsRegister() || right->IsConstantOperand());
    __ add(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
  }
}


void LCodeGen::DoMathMinMax(LMathMinMax* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  HMathMinMax::Operation operation = instr->hydrogen()->operation();
  if (instr->hydrogen()->representation().IsSmiOrInteger32()) {
    Condition condition = (operation == HMathMinMax::kMathMin) ? le : ge;
    Register left_reg = ToRegister(left);
    Operand right_op = (right->IsRegister() || right->IsConstantOperand())
        ? ToOperand(right)
        : Operand(EmitLoadRegister(right, ip));
    Register result_reg = ToRegister(instr->result());
    __ cmp(left_reg, right_op);
    __ Move(result_reg, left_reg, condition);
    __ mov(result_reg, right_op, LeaveCC, NegateCondition(condition));
  } else {
    DCHECK(instr->hydrogen()->representation().IsDouble());
    DwVfpRegister left_reg = ToDoubleRegister(left);
    DwVfpRegister right_reg = ToDoubleRegister(right);
    DwVfpRegister result_reg = ToDoubleRegister(instr->result());
    Label result_is_nan, return_left, return_right, check_zero, done;
    __ VFPCompareAndSetFlags(left_reg, right_reg);
    if (operation == HMathMinMax::kMathMin) {
      __ b(mi, &return_left);
      __ b(gt, &return_right);
    } else {
      __ b(mi, &return_right);
      __ b(gt, &return_left);
    }
    __ b(vs, &result_is_nan);
    // Left equals right => check for -0.
    __ VFPCompareAndSetFlags(left_reg, 0.0);
    if (left_reg.is(result_reg) || right_reg.is(result_reg)) {
      __ b(ne, &done);  // left == right != 0.
    } else {
      __ b(ne, &return_left);  // left == right != 0.
    }
    // At this point, both left and right are either 0 or -0.
    if (operation == HMathMinMax::kMathMin) {
      // We could use a single 'vorr' instruction here if we had NEON support.
      // The algorithm is: -((-L) + (-R)), which in case of L and R being
      // different registers is most efficiently expressed as -((-L) - R).
      __ vneg(left_reg, left_reg);
      if (left_reg.is(right_reg)) {
        __ vadd(result_reg, left_reg, right_reg);
      } else {
        __ vsub(result_reg, left_reg, right_reg);
      }
      __ vneg(result_reg, result_reg);
    } else {
      // Since we operate on +0 and/or -0, vadd and vand have the same effect;
      // the decision for vadd is easy because vand is a NEON instruction.
      __ vadd(result_reg, left_reg, right_reg);
    }
    __ b(&done);

    __ bind(&result_is_nan);
    __ vadd(result_reg, left_reg, right_reg);
    __ b(&done);

    __ bind(&return_right);
    __ Move(result_reg, right_reg);
    if (!left_reg.is(result_reg)) {
      __ b(&done);
    }

    __ bind(&return_left);
    __ Move(result_reg, left_reg);

    __ bind(&done);
  }
}


void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  DwVfpRegister left = ToDoubleRegister(instr->left());
  DwVfpRegister right = ToDoubleRegister(instr->right());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  switch (instr->op()) {
    case Token::ADD:
      __ vadd(result, left, right);
      break;
    case Token::SUB:
      __ vsub(result, left, right);
      break;
    case Token::MUL:
      __ vmul(result, left, right);
      break;
    case Token::DIV:
      __ vdiv(result, left, right);
      break;
    case Token::MOD: {
      __ PrepareCallCFunction(0, 2, scratch0());
      __ MovToFloatParameters(left, right);
      __ CallCFunction(
          ExternalReference::mod_two_doubles_operation(isolate()),
          0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(result);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(r1));
  DCHECK(ToRegister(instr->right()).is(r0));
  DCHECK(ToRegister(instr->result()).is(r0));

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), instr->op()).code();
  // Block literal pool emission to ensure nop indicating no inlined smi code
  // is in the correct position.
  Assembler::BlockConstPoolScope block_const_pool(masm());
  CallCode(code, RelocInfo::CODE_TARGET, instr);
}


template<class InstrType>
void LCodeGen::EmitBranch(InstrType instr, Condition condition) {
  int left_block = instr->TrueDestination(chunk_);
  int right_block = instr->FalseDestination(chunk_);

  int next_block = GetNextEmittedBlock();

  if (right_block == left_block || condition == al) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ b(NegateCondition(condition), chunk_->GetAssemblyLabel(right_block));
  } else if (right_block == next_block) {
    __ b(condition, chunk_->GetAssemblyLabel(left_block));
  } else {
    __ b(condition, chunk_->GetAssemblyLabel(left_block));
    __ b(chunk_->GetAssemblyLabel(right_block));
  }
}


template <class InstrType>
void LCodeGen::EmitTrueBranch(InstrType instr, Condition condition) {
  int true_block = instr->TrueDestination(chunk_);
  __ b(condition, chunk_->GetAssemblyLabel(true_block));
}


template <class InstrType>
void LCodeGen::EmitFalseBranch(InstrType instr, Condition condition) {
  int false_block = instr->FalseDestination(chunk_);
  __ b(condition, chunk_->GetAssemblyLabel(false_block));
}


void LCodeGen::DoDebugBreak(LDebugBreak* instr) {
  __ stop("LBreak");
}


void LCodeGen::DoBranch(LBranch* instr) {
  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsInteger32() || r.IsSmi()) {
    DCHECK(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    __ cmp(reg, Operand::Zero());
    EmitBranch(instr, ne);
  } else if (r.IsDouble()) {
    DCHECK(!info()->IsStub());
    DwVfpRegister reg = ToDoubleRegister(instr->value());
    // Test the double value. Zero and NaN are false.
    __ VFPCompareAndSetFlags(reg, 0.0);
    __ cmp(r0, r0, vs);  // If NaN, set the Z flag. (NaN -> false)
    EmitBranch(instr, ne);
  } else {
    DCHECK(r.IsTagged());
    Register reg = ToRegister(instr->value());
    HType type = instr->hydrogen()->value()->type();
    if (type.IsBoolean()) {
      DCHECK(!info()->IsStub());
      __ CompareRoot(reg, Heap::kTrueValueRootIndex);
      EmitBranch(instr, eq);
    } else if (type.IsSmi()) {
      DCHECK(!info()->IsStub());
      __ cmp(reg, Operand::Zero());
      EmitBranch(instr, ne);
    } else if (type.IsJSArray()) {
      DCHECK(!info()->IsStub());
      EmitBranch(instr, al);
    } else if (type.IsHeapNumber()) {
      DCHECK(!info()->IsStub());
      DwVfpRegister dbl_scratch = double_scratch0();
      __ vldr(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
      // Test the double value. Zero and NaN are false.
      __ VFPCompareAndSetFlags(dbl_scratch, 0.0);
      __ cmp(r0, r0, vs);  // If NaN, set the Z flag. (NaN)
      EmitBranch(instr, ne);
    } else if (type.IsString()) {
      DCHECK(!info()->IsStub());
      __ ldr(ip, FieldMemOperand(reg, String::kLengthOffset));
      __ cmp(ip, Operand::Zero());
      EmitBranch(instr, ne);
    } else {
      ToBooleanHints expected = instr->hydrogen()->expected_input_types();
      // Avoid deopts in the case where we've never executed this path before.
      if (expected == ToBooleanHint::kNone) expected = ToBooleanHint::kAny;

      if (expected & ToBooleanHint::kUndefined) {
        // undefined -> false.
        __ CompareRoot(reg, Heap::kUndefinedValueRootIndex);
        __ b(eq, instr->FalseLabel(chunk_));
      }
      if (expected & ToBooleanHint::kBoolean) {
        // Boolean -> its value.
        __ CompareRoot(reg, Heap::kTrueValueRootIndex);
        __ b(eq, instr->TrueLabel(chunk_));
        __ CompareRoot(reg, Heap::kFalseValueRootIndex);
        __ b(eq, instr->FalseLabel(chunk_));
      }
      if (expected & ToBooleanHint::kNull) {
        // 'null' -> false.
        __ CompareRoot(reg, Heap::kNullValueRootIndex);
        __ b(eq, instr->FalseLabel(chunk_));
      }

      if (expected & ToBooleanHint::kSmallInteger) {
        // Smis: 0 -> false, all other -> true.
        __ cmp(reg, Operand::Zero());
        __ b(eq, instr->FalseLabel(chunk_));
        __ JumpIfSmi(reg, instr->TrueLabel(chunk_));
      } else if (expected & ToBooleanHint::kNeedsMap) {
        // If we need a map later and have a Smi -> deopt.
        __ SmiTst(reg);
        DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi);
      }

      const Register map = scratch0();
      if (expected & ToBooleanHint::kNeedsMap) {
        __ ldr(map, FieldMemOperand(reg, HeapObject::kMapOffset));

        if (expected & ToBooleanHint::kCanBeUndetectable) {
          // Undetectable -> false.
          __ ldrb(ip, FieldMemOperand(map, Map::kBitFieldOffset));
          __ tst(ip, Operand(1 << Map::kIsUndetectable));
          __ b(ne, instr->FalseLabel(chunk_));
        }
      }

      if (expected & ToBooleanHint::kReceiver) {
        // spec object -> true.
        __ CompareInstanceType(map, ip, FIRST_JS_RECEIVER_TYPE);
        __ b(ge, instr->TrueLabel(chunk_));
      }

      if (expected & ToBooleanHint::kString) {
        // String value -> false iff empty.
        Label not_string;
        __ CompareInstanceType(map, ip, FIRST_NONSTRING_TYPE);
        __ b(ge, &not_string);
        __ ldr(ip, FieldMemOperand(reg, String::kLengthOffset));
        __ cmp(ip, Operand::Zero());
        __ b(ne, instr->TrueLabel(chunk_));
        __ b(instr->FalseLabel(chunk_));
        __ bind(&not_string);
      }

      if (expected & ToBooleanHint::kSymbol) {
        // Symbol value -> true.
        __ CompareInstanceType(map, ip, SYMBOL_TYPE);
        __ b(eq, instr->TrueLabel(chunk_));
      }

      if (expected & ToBooleanHint::kSimdValue) {
        // SIMD value -> true.
        __ CompareInstanceType(map, ip, SIMD128_VALUE_TYPE);
        __ b(eq, instr->TrueLabel(chunk_));
      }

      if (expected & ToBooleanHint::kHeapNumber) {
        // heap number -> false iff +0, -0, or NaN.
        DwVfpRegister dbl_scratch = double_scratch0();
        Label not_heap_number;
        __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
        __ b(ne, &not_heap_number);
        __ vldr(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
        __ VFPCompareAndSetFlags(dbl_scratch, 0.0);
        __ cmp(r0, r0, vs);  // NaN -> false.
        __ b(eq, instr->FalseLabel(chunk_));  // +0, -0 -> false.
        __ b(instr->TrueLabel(chunk_));
        __ bind(&not_heap_number);
      }

      if (expected != ToBooleanHint::kAny) {
        // We've seen something for the first time -> deopt.
        // This can only happen if we are not generic already.
        DeoptimizeIf(al, instr, DeoptimizeReason::kUnexpectedObject);
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
      // Compare left and right operands as doubles and load the
      // resulting flags into the normal status register.
      __ VFPCompareAndSetFlags(ToDoubleRegister(left), ToDoubleRegister(right));
      // If a NaN is involved, i.e. the result is unordered (V set),
      // jump to false block label.
      __ b(vs, instr->FalseLabel(chunk_));
    } else {
      if (right->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(right));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          __ cmp(ToRegister(left), Operand(Smi::FromInt(value)));
        } else {
          __ cmp(ToRegister(left), Operand(value));
        }
      } else if (left->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(left));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          __ cmp(ToRegister(right), Operand(Smi::FromInt(value)));
        } else {
          __ cmp(ToRegister(right), Operand(value));
        }
        // We commuted the operands, so commute the condition.
        cond = CommuteCondition(cond);
      } else {
        __ cmp(ToRegister(left), ToRegister(right));
      }
    }
    EmitBranch(instr, cond);
  }
}


void LCodeGen::DoCmpObjectEqAndBranch(LCmpObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());

  __ cmp(left, Operand(right));
  EmitBranch(instr, eq);
}


void LCodeGen::DoCmpHoleAndBranch(LCmpHoleAndBranch* instr) {
  if (instr->hydrogen()->representation().IsTagged()) {
    Register input_reg = ToRegister(instr->object());
    __ mov(ip, Operand(factory()->the_hole_value()));
    __ cmp(input_reg, ip);
    EmitBranch(instr, eq);
    return;
  }

  DwVfpRegister input_reg = ToDoubleRegister(instr->object());
  __ VFPCompareAndSetFlags(input_reg, input_reg);
  EmitFalseBranch(instr, vc);

  Register scratch = scratch0();
  __ VmovHigh(scratch, input_reg);
  __ cmp(scratch, Operand(kHoleNanUpper32));
  EmitBranch(instr, eq);
}


Condition LCodeGen::EmitIsString(Register input,
                                 Register temp1,
                                 Label* is_not_string,
                                 SmiCheck check_needed = INLINE_SMI_CHECK) {
  if (check_needed == INLINE_SMI_CHECK) {
    __ JumpIfSmi(input, is_not_string);
  }
  __ CompareObjectType(input, temp1, temp1, FIRST_NONSTRING_TYPE);

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

  EmitBranch(instr, true_cond);
}


void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  Register input_reg = EmitLoadRegister(instr->value(), ip);
  __ SmiTst(input_reg);
  EmitBranch(instr, eq);
}


void LCodeGen::DoIsUndetectableAndBranch(LIsUndetectableAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }
  __ ldr(temp, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  __ tst(temp, Operand(1 << Map::kIsUndetectable));
  EmitBranch(instr, ne);
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
  DCHECK(ToRegister(instr->left()).is(r1));
  DCHECK(ToRegister(instr->right()).is(r0));

  Handle<Code> code = CodeFactory::StringCompare(isolate(), instr->op()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
  __ CompareRoot(r0, Heap::kTrueValueRootIndex);
  EmitBranch(instr, eq);
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

  __ CompareObjectType(input, scratch, scratch, TestType(instr->hydrogen()));
  EmitBranch(instr, BranchCondition(instr->hydrogen()));
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

  __ CompareObjectType(input, temp, temp2, FIRST_FUNCTION_TYPE);
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  if (String::Equals(isolate()->factory()->Function_string(), class_name)) {
    __ b(hs, is_true);
  } else {
    __ b(hs, is_false);
  }

  // Check if the constructor in the map is a function.
  Register instance_type = ip;
  __ GetMapConstructor(temp, temp, temp2, instance_type);

  // Objects with a non-function constructor have class 'Object'.
  __ cmp(instance_type, Operand(JS_FUNCTION_TYPE));
  if (String::Equals(isolate()->factory()->Object_string(), class_name)) {
    __ b(ne, is_true);
  } else {
    __ b(ne, is_false);
  }

  // temp now contains the constructor function. Grab the
  // instance class name from there.
  __ ldr(temp, FieldMemOperand(temp, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(temp, FieldMemOperand(temp,
                               SharedFunctionInfo::kInstanceClassNameOffset));
  // The class name we are testing against is internalized since it's a literal.
  // The name in the constructor is internalized because of the way the context
  // is booted.  This routine isn't expected to work for random API-created
  // classes and it doesn't have to because you can't access it with natives
  // syntax.  Since both sides are internalized it is sufficient to use an
  // identity comparison.
  __ cmp(temp, Operand(class_name));
  // End with the answer in flags.
}


void LCodeGen::DoClassOfTestAndBranch(LClassOfTestAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = scratch0();
  Register temp2 = ToRegister(instr->temp());
  Handle<String> class_name = instr->hydrogen()->class_name();

  EmitClassOfTest(instr->TrueLabel(chunk_), instr->FalseLabel(chunk_),
      class_name, input, temp, temp2);

  EmitBranch(instr, eq);
}


void LCodeGen::DoCmpMapAndBranch(LCmpMapAndBranch* instr) {
  Register reg = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  __ ldr(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ cmp(temp, Operand(instr->map()));
  EmitBranch(instr, eq);
}


void LCodeGen::DoHasInPrototypeChainAndBranch(
    LHasInPrototypeChainAndBranch* instr) {
  Register const object = ToRegister(instr->object());
  Register const object_map = scratch0();
  Register const object_instance_type = ip;
  Register const object_prototype = object_map;
  Register const prototype = ToRegister(instr->prototype());

  // The {object} must be a spec object.  It's sufficient to know that {object}
  // is not a smi, since all other non-spec objects have {null} prototypes and
  // will be ruled out below.
  if (instr->hydrogen()->ObjectNeedsSmiCheck()) {
    __ SmiTst(object);
    EmitFalseBranch(instr, eq);
  }

  // Loop through the {object}s prototype chain looking for the {prototype}.
  __ ldr(object_map, FieldMemOperand(object, HeapObject::kMapOffset));
  Label loop;
  __ bind(&loop);

  // Deoptimize if the object needs to be access checked.
  __ ldrb(object_instance_type,
          FieldMemOperand(object_map, Map::kBitFieldOffset));
  __ tst(object_instance_type, Operand(1 << Map::kIsAccessCheckNeeded));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kAccessCheck);
  // Deoptimize for proxies.
  __ CompareInstanceType(object_map, object_instance_type, JS_PROXY_TYPE);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kProxy);

  __ ldr(object_prototype, FieldMemOperand(object_map, Map::kPrototypeOffset));
  __ CompareRoot(object_prototype, Heap::kNullValueRootIndex);
  EmitFalseBranch(instr, eq);
  __ cmp(object_prototype, prototype);
  EmitTrueBranch(instr, eq);
  __ ldr(object_map, FieldMemOperand(object_prototype, HeapObject::kMapOffset));
  __ b(&loop);
}


void LCodeGen::DoCmpT(LCmpT* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  Token::Value op = instr->op();

  Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  // This instruction also signals no smi code inlined.
  __ cmp(r0, Operand::Zero());

  Condition condition = ComputeCompareCondition(op);
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kTrueValueRootIndex,
              condition);
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kFalseValueRootIndex,
              NegateCondition(condition));
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace && info()->IsOptimizing()) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in r0.  We're leaving the code
    // managed by the register allocator and tearing down the frame, it's
    // safe to write to the context register.
    __ push(r0);
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kTraceExit);
  }
  if (info()->saves_caller_doubles()) {
    RestoreCallerDoubles();
  }
  if (NeedsEagerFrame()) {
    masm_->LeaveFrame(StackFrame::JAVA_SCRIPT);
  }
  { ConstantPoolUnavailableScope constant_pool_unavailable(masm());
    if (instr->has_constant_parameter_count()) {
      int parameter_count = ToInteger32(instr->constant_parameter_count());
      int32_t sp_delta = (parameter_count + 1) * kPointerSize;
      if (sp_delta != 0) {
        __ add(sp, sp, Operand(sp_delta));
      }
    } else {
      DCHECK(info()->IsStub());  // Functions would need to drop one more value.
      Register reg = ToRegister(instr->parameter_count());
      // The argument count parameter is a smi
      __ SmiUntag(reg);
      __ add(sp, sp, Operand(reg, LSL, kPointerSizeLog2));
    }

    __ Jump(lr);
  }
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextMemOperand(context, instr->slot_index()));
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(result, ip);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
    } else {
      __ mov(result, Operand(factory()->undefined_value()), LeaveCC, eq);
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
    __ ldr(scratch, target);
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(scratch, ip);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
    } else {
      __ b(ne, &skip_assignment);
    }
  }

  __ str(value, target);
  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed =
        instr->hydrogen()->value()->type().IsHeapObject()
            ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
    __ RecordWriteContextSlot(context,
                              target.offset(),
                              value,
                              scratch,
                              GetLinkRegisterState(),
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
    DwVfpRegister result = ToDoubleRegister(instr->result());
    __ vldr(result, FieldMemOperand(object, offset));
    return;
  }

  Register result = ToRegister(instr->result());
  if (!access.IsInobject()) {
    __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    object = result;
  }
  MemOperand operand = FieldMemOperand(object, offset);
  __ Load(result, operand, access.representation());
}


void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register scratch = scratch0();
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());

  // Get the prototype or initial map from the function.
  __ ldr(result,
         FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(result, ip);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);

  // If the function does not have an initial map, we're done.
  Label done;
  __ CompareObjectType(result, scratch, scratch, MAP_TYPE);
  __ b(ne, &done);

  // Get the prototype from the initial map.
  __ ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));

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
      __ ldr(result, MemOperand(arguments, index * kPointerSize));
    } else {
      Register index = ToRegister(instr->index());
      __ rsb(result, index, Operand(const_length + 1));
      __ ldr(result, MemOperand(arguments, result, LSL, kPointerSizeLog2));
    }
  } else if (instr->index()->IsConstantOperand()) {
      Register length = ToRegister(instr->length());
      int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
      int loc = const_index - 1;
      if (loc != 0) {
        __ sub(result, length, Operand(loc));
        __ ldr(result, MemOperand(arguments, result, LSL, kPointerSizeLog2));
      } else {
        __ ldr(result, MemOperand(arguments, length, LSL, kPointerSizeLog2));
      }
    } else {
    Register length = ToRegister(instr->length());
    Register index = ToRegister(instr->index());
    __ sub(result, length, index);
    __ add(result, result, Operand(1));
    __ ldr(result, MemOperand(arguments, result, LSL, kPointerSizeLog2));
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
      ? (element_size_shift - kSmiTagSize) : element_size_shift;
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    DwVfpRegister result = ToDoubleRegister(instr->result());
    Operand operand = key_is_constant
        ? Operand(constant_key << element_size_shift)
        : Operand(key, LSL, shift_size);
    __ add(scratch0(), external_pointer, operand);
    if (elements_kind == FLOAT32_ELEMENTS) {
      __ vldr(double_scratch0().low(), scratch0(), base_offset);
      __ vcvt_f64_f32(result, double_scratch0().low());
    } else  {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ vldr(result, scratch0(), base_offset);
    }
  } else {
    Register result = ToRegister(instr->result());
    MemOperand mem_operand = PrepareKeyedOperand(
        key, external_pointer, key_is_constant, constant_key,
        element_size_shift, shift_size, base_offset);
    switch (elements_kind) {
      case INT8_ELEMENTS:
        __ ldrsb(result, mem_operand);
        break;
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
        __ ldrb(result, mem_operand);
        break;
      case INT16_ELEMENTS:
        __ ldrsh(result, mem_operand);
        break;
      case UINT16_ELEMENTS:
        __ ldrh(result, mem_operand);
        break;
      case INT32_ELEMENTS:
        __ ldr(result, mem_operand);
        break;
      case UINT32_ELEMENTS:
        __ ldr(result, mem_operand);
        if (!instr->hydrogen()->CheckFlag(HInstruction::kUint32)) {
          __ cmp(result, Operand(0x80000000));
          DeoptimizeIf(cs, instr, DeoptimizeReason::kNegativeValue);
        }
        break;
      case FLOAT32_ELEMENTS:
      case FLOAT64_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
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
  DwVfpRegister result = ToDoubleRegister(instr->result());
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
  __ add(scratch, elements, Operand(base_offset));

  if (!key_is_constant) {
    key = ToRegister(instr->key());
    int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
        ? (element_size_shift - kSmiTagSize) : element_size_shift;
    __ add(scratch, scratch, Operand(key, LSL, shift_size));
  }

  __ vldr(result, scratch, 0);

  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ ldr(scratch, MemOperand(scratch, sizeof(kHoleNanLower32)));
    __ cmp(scratch, Operand(kHoleNanUpper32));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
  }
}


void LCodeGen::DoLoadKeyedFixedArray(LLoadKeyed* instr) {
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
      __ add(scratch, elements, Operand::PointerOffsetFromSmiKey(key));
    } else {
      __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
    }
  }
  __ ldr(result, MemOperand(store_base, offset));

  // Check for the hole value.
  if (instr->hydrogen()->RequiresHoleCheck()) {
    if (IsFastSmiElementsKind(instr->hydrogen()->elements_kind())) {
      __ SmiTst(result);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi);
    } else {
      __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
      __ cmp(result, scratch);
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
    }
  } else if (instr->hydrogen()->hole_mode() == CONVERT_HOLE_TO_UNDEFINED) {
    DCHECK(instr->hydrogen()->elements_kind() == FAST_HOLEY_ELEMENTS);
    Label done;
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    __ cmp(result, scratch);
    __ b(ne, &done);
    if (info()->IsStub()) {
      // A stub can safely convert the hole to undefined only if the array
      // protector cell contains (Smi) Isolate::kProtectorValid. Otherwise
      // it needs to bail out.
      __ LoadRoot(result, Heap::kArrayProtectorRootIndex);
      __ ldr(result, FieldMemOperand(result, PropertyCell::kValueOffset));
      __ cmp(result, Operand(Smi::FromInt(Isolate::kProtectorValid)));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kHole);
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
      return MemOperand(base, key, LSL, shift_size);
    } else {
      DCHECK_EQ(-1, shift_size);
      return MemOperand(base, key, LSR, 1);
    }
  }

  if (shift_size >= 0) {
    __ add(scratch0(), base, Operand(key, LSL, shift_size));
    return MemOperand(scratch0(), base_offset);
  } else {
    DCHECK_EQ(-1, shift_size);
    __ add(scratch0(), base, Operand(key, ASR, 1));
    return MemOperand(scratch0(), base_offset);
  }
}


void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());

  if (instr->hydrogen()->from_inlined()) {
    __ sub(result, sp, Operand(2 * kPointerSize));
  } else if (instr->hydrogen()->arguments_adaptor()) {
    // Check if the calling frame is an arguments adaptor frame.
    Label done, adapted;
    __ ldr(scratch, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ ldr(result, MemOperand(scratch,
                              CommonFrameConstants::kContextOrFrameTypeOffset));
    __ cmp(result, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

    // Result is the frame pointer for the frame if not adapted and for the real
    // frame below the adaptor frame if adapted.
    __ mov(result, fp, LeaveCC, ne);
    __ mov(result, scratch, LeaveCC, eq);
  } else {
    __ mov(result, fp);
  }
}


void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Register elem = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());

  Label done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ cmp(fp, elem);
  __ mov(result, Operand(scope()->num_parameters()));
  __ b(eq, &done);

  // Arguments adaptor frame present. Get argument length from there.
  __ ldr(result, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(result,
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
    // Do not transform the receiver to object for strict mode
    // functions.
    __ ldr(scratch,
           FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(scratch,
           FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    int mask = 1 << (SharedFunctionInfo::kStrictModeFunction + kSmiTagSize);
    __ tst(scratch, Operand(mask));
    __ b(ne, &result_in_receiver);

    // Do not transform the receiver to object for builtins.
    __ tst(scratch, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
    __ b(ne, &result_in_receiver);
  }

  // Normal function. Replace undefined or null with global receiver.
  __ LoadRoot(scratch, Heap::kNullValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_object);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_object);

  // Deoptimize if the receiver is not a JS object.
  __ SmiTst(receiver);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi);
  __ CompareObjectType(receiver, scratch, scratch, FIRST_JS_RECEIVER_TYPE);
  DeoptimizeIf(lt, instr, DeoptimizeReason::kNotAJavaScriptObject);

  __ b(&result_in_receiver);
  __ bind(&global_object);
  __ ldr(result, FieldMemOperand(function, JSFunction::kContextOffset));
  __ ldr(result, ContextMemOperand(result, Context::NATIVE_CONTEXT_INDEX));
  __ ldr(result, ContextMemOperand(result, Context::GLOBAL_PROXY_INDEX));

  if (result.is(receiver)) {
    __ bind(&result_in_receiver);
  } else {
    Label result_ok;
    __ b(&result_ok);
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
  DCHECK(receiver.is(r0));  // Used for parameter count.
  DCHECK(function.is(r1));  // Required by InvokeFunction.
  DCHECK(ToRegister(instr->result()).is(r0));

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  __ cmp(length, Operand(kArgumentsLimit));
  DeoptimizeIf(hi, instr, DeoptimizeReason::kTooManyArguments);

  // Push the receiver and use the register to keep the original
  // number of arguments.
  __ push(receiver);
  __ mov(receiver, length);
  // The arguments are at a one pointer size offset from elements.
  __ add(elements, elements, Operand(1 * kPointerSize));

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label invoke, loop;
  // length is a small non-negative integer, due to the test above.
  __ cmp(length, Operand::Zero());
  __ b(eq, &invoke);
  __ bind(&loop);
  __ ldr(scratch, MemOperand(elements, length, LSL, 2));
  __ push(scratch);
  __ sub(length, length, Operand(1), SetCC);
  __ b(ne, &loop);

  __ bind(&invoke);

  InvokeFlag flag = CALL_FUNCTION;
  if (instr->hydrogen()->tail_call_mode() == TailCallMode::kAllow) {
    DCHECK(!info()->saves_caller_doubles());
    // TODO(ishell): drop current frame before pushing arguments to the stack.
    flag = JUMP_FUNCTION;
    ParameterCount actual(r0);
    // It is safe to use r3, r4 and r5 as scratch registers here given that
    // 1) we are not going to return to caller function anyway,
    // 2) r3 (new.target) will be initialized below.
    PrepareForTailCall(actual, r3, r4, r5);
  }

  DCHECK(instr->HasPointerMap());
  LPointerMap* pointers = instr->pointer_map();
  SafepointGenerator safepoint_generator(this, pointers, Safepoint::kLazyDeopt);
  // The number of arguments is stored in receiver which is r0, as expected
  // by InvokeFunction.
  ParameterCount actual(receiver);
  __ InvokeFunction(function, no_reg, actual, flag, safepoint_generator);
}


void LCodeGen::DoPushArgument(LPushArgument* instr) {
  LOperand* argument = instr->value();
  if (argument->IsDoubleRegister() || argument->IsDoubleStackSlot()) {
    Abort(kDoPushArgumentNotImplementedForDoubleType);
  } else {
    Register argument_reg = EmitLoadRegister(argument, ip);
    __ push(argument_reg);
  }
}


void LCodeGen::DoDrop(LDrop* instr) {
  __ Drop(instr->count());
}


void LCodeGen::DoThisFunction(LThisFunction* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
}


void LCodeGen::DoContext(LContext* instr) {
  // If there is a non-return use, the context must be moved to a register.
  Register result = ToRegister(instr->result());
  if (info()->IsOptimizing()) {
    __ ldr(result, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // If there is no frame, the context must be in cp.
    DCHECK(result.is(cp));
  }
}


void LCodeGen::DoDeclareGlobals(LDeclareGlobals* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  __ Move(scratch0(), instr->hydrogen()->declarations());
  __ push(scratch0());
  __ mov(scratch0(), Operand(Smi::FromInt(instr->hydrogen()->flags())));
  __ push(scratch0());
  __ Move(scratch0(), instr->hydrogen()->feedback_vector());
  __ push(scratch0());
  CallRuntime(Runtime::kDeclareGlobals, instr);
}

void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int formal_parameter_count, int arity,
                                 bool is_tail_call, LInstruction* instr) {
  bool dont_adapt_arguments =
      formal_parameter_count == SharedFunctionInfo::kDontAdaptArgumentsSentinel;
  bool can_invoke_directly =
      dont_adapt_arguments || formal_parameter_count == arity;

  Register function_reg = r1;

  LPointerMap* pointers = instr->pointer_map();

  if (can_invoke_directly) {
    // Change context.
    __ ldr(cp, FieldMemOperand(function_reg, JSFunction::kContextOffset));

    // Always initialize new target and number of actual arguments.
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ mov(r0, Operand(arity));

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
      __ ldr(ip, FieldMemOperand(function_reg, JSFunction::kCodeEntryOffset));
      if (is_tail_call) {
        __ Jump(ip);
      } else {
        __ Call(ip);
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
  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, Operand(ip));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);

  Label done;
  Register exponent = scratch0();
  scratch = no_reg;
  __ ldr(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive, just
  // return it.
  __ tst(exponent, Operand(HeapNumber::kSignMask));
  // Move the input to the result if necessary.
  __ Move(result, input);
  __ b(eq, &done);

  // Input is negative. Reverse its sign.
  // Preserve the value of all registers.
  {
    PushSafepointRegistersScope scope(this);

    // Registers were saved at the safepoint, so we can use
    // many scratch registers.
    Register tmp1 = input.is(r1) ? r0 : r1;
    Register tmp2 = input.is(r2) ? r0 : r2;
    Register tmp3 = input.is(r3) ? r0 : r3;
    Register tmp4 = input.is(r4) ? r0 : r4;

    // exponent: floating point exponent value.

    Label allocated, slow;
    __ LoadRoot(tmp4, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(tmp1, tmp2, tmp3, tmp4, &slow);
    __ b(&allocated);

    // Slow case: Call the runtime system to do the number allocation.
    __ bind(&slow);

    CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr,
                            instr->context());
    // Set the pointer to the new heap number in tmp.
    if (!tmp1.is(r0)) __ mov(tmp1, Operand(r0));
    // Restore input_reg after call to runtime.
    __ LoadFromSafepointRegisterSlot(input, input);
    __ ldr(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));

    __ bind(&allocated);
    // exponent: floating point exponent value.
    // tmp1: allocated heap number.
    __ bic(exponent, exponent, Operand(HeapNumber::kSignMask));
    __ str(exponent, FieldMemOperand(tmp1, HeapNumber::kExponentOffset));
    __ ldr(tmp2, FieldMemOperand(input, HeapNumber::kMantissaOffset));
    __ str(tmp2, FieldMemOperand(tmp1, HeapNumber::kMantissaOffset));

    __ StoreToSafepointRegisterSlot(tmp1, result);
  }

  __ bind(&done);
}


void LCodeGen::EmitIntegerMathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  __ cmp(input, Operand::Zero());
  __ Move(result, input, pl);
  // We can make rsb conditional because the previous cmp instruction
  // will clear the V (overflow) flag and rsb won't set this flag
  // if input is positive.
  __ rsb(result, input, Operand::Zero(), SetCC, mi);
  // Deoptimize on overflow.
  DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
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
    DwVfpRegister input = ToDoubleRegister(instr->value());
    DwVfpRegister result = ToDoubleRegister(instr->result());
    __ vabs(result, input);
  } else if (r.IsSmiOrInteger32()) {
    EmitIntegerMathAbs(instr);
  } else {
    // Representation is tagged.
    DeferredMathAbsTaggedHeapNumber* deferred =
        new(zone()) DeferredMathAbsTaggedHeapNumber(this, instr);
    Register input = ToRegister(instr->value());
    // Smi check.
    __ JumpIfNotSmi(input, deferred->entry());
    // If smi, handle it directly.
    EmitIntegerMathAbs(instr);
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoMathFloor(LMathFloor* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register input_high = scratch0();
  Label done, exact;

  __ TryInt32Floor(result, input, input_high, double_scratch0(), &done, &exact);
  DeoptimizeIf(al, instr, DeoptimizeReason::kLostPrecisionOrNaN);

  __ bind(&exact);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    __ cmp(result, Operand::Zero());
    __ b(ne, &done);
    __ cmp(input_high, Operand::Zero());
    DeoptimizeIf(mi, instr, DeoptimizeReason::kMinusZero);
  }
  __ bind(&done);
}


void LCodeGen::DoMathRound(LMathRound* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  DwVfpRegister double_scratch1 = ToDoubleRegister(instr->temp());
  DwVfpRegister input_plus_dot_five = double_scratch1;
  Register input_high = scratch0();
  DwVfpRegister dot_five = double_scratch0();
  Label convert, done;

  __ Vmov(dot_five, 0.5, scratch0());
  __ vabs(double_scratch1, input);
  __ VFPCompareAndSetFlags(double_scratch1, dot_five);
  // If input is in [-0.5, -0], the result is -0.
  // If input is in [+0, +0.5[, the result is +0.
  // If the input is +0.5, the result is 1.
  __ b(hi, &convert);  // Out of [-0.5, +0.5].
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ VmovHigh(input_high, input);
    __ cmp(input_high, Operand::Zero());
    // [-0.5, -0].
    DeoptimizeIf(mi, instr, DeoptimizeReason::kMinusZero);
  }
  __ VFPCompareAndSetFlags(input, dot_five);
  __ mov(result, Operand(1), LeaveCC, eq);  // +0.5.
  // Remaining cases: [+0, +0.5[ or [-0.5, +0.5[, depending on
  // flag kBailoutOnMinusZero.
  __ mov(result, Operand::Zero(), LeaveCC, ne);
  __ b(&done);

  __ bind(&convert);
  __ vadd(input_plus_dot_five, input, dot_five);
  // Reuse dot_five (double_scratch0) as we no longer need this value.
  __ TryInt32Floor(result, input_plus_dot_five, input_high, double_scratch0(),
                   &done, &done);
  DeoptimizeIf(al, instr, DeoptimizeReason::kLostPrecisionOrNaN);
  __ bind(&done);
}


void LCodeGen::DoMathFround(LMathFround* instr) {
  DwVfpRegister input_reg = ToDoubleRegister(instr->value());
  DwVfpRegister output_reg = ToDoubleRegister(instr->result());
  LowDwVfpRegister scratch = double_scratch0();
  __ vcvt_f32_f64(scratch.low(), input_reg);
  __ vcvt_f64_f32(output_reg, scratch.low());
}


void LCodeGen::DoMathSqrt(LMathSqrt* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  __ vsqrt(result, input);
}


void LCodeGen::DoMathPowHalf(LMathPowHalf* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  DwVfpRegister temp = double_scratch0();

  // Note that according to ECMA-262 15.8.2.13:
  // Math.pow(-Infinity, 0.5) == Infinity
  // Math.sqrt(-Infinity) == NaN
  Label done;
  __ vmov(temp, -V8_INFINITY, scratch0());
  __ VFPCompareAndSetFlags(input, temp);
  __ vneg(result, temp, eq);
  __ b(&done, eq);

  // Add +0 to convert -0 to +0.
  __ vadd(result, input, kDoubleRegZero);
  __ vsqrt(result, result);
  __ bind(&done);
}


void LCodeGen::DoPower(LPower* instr) {
  Representation exponent_type = instr->hydrogen()->right()->representation();
  // Having marked this as a call, we can use any registers.
  // Just make sure that the input/output registers are the expected ones.
  Register tagged_exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(!instr->right()->IsDoubleRegister() ||
         ToDoubleRegister(instr->right()).is(d1));
  DCHECK(!instr->right()->IsRegister() ||
         ToRegister(instr->right()).is(tagged_exponent));
  DCHECK(ToDoubleRegister(instr->left()).is(d0));
  DCHECK(ToDoubleRegister(instr->result()).is(d2));

  if (exponent_type.IsSmi()) {
    MathPowStub stub(isolate(), MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsTagged()) {
    Label no_deopt;
    __ JumpIfSmi(tagged_exponent, &no_deopt);
    DCHECK(!r6.is(tagged_exponent));
    __ ldr(r6, FieldMemOperand(tagged_exponent, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r6, Operand(ip));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);
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
  __ clz(result, input);
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
  __ ldr(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(scratch3,
         MemOperand(scratch2, StandardFrameConstants::kContextOffset));
  __ cmp(scratch3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &no_arguments_adaptor);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ ldr(caller_args_count_reg,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ b(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ mov(caller_args_count_reg, Operand(info()->literal()->parameter_count()));

  __ bind(&formal_parameter_count_loaded);
  __ PrepareForTailCall(actual, caller_args_count_reg, scratch2, scratch3);

  Comment(";;; }");
}

void LCodeGen::DoInvokeFunction(LInvokeFunction* instr) {
  HInvokeFunction* hinstr = instr->hydrogen();
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->function()).is(r1));
  DCHECK(instr->HasPointerMap());

  bool is_tail_call = hinstr->tail_call_mode() == TailCallMode::kAllow;

  if (is_tail_call) {
    DCHECK(!info()->saves_caller_doubles());
    ParameterCount actual(instr->arity());
    // It is safe to use r3, r4 and r5 as scratch registers here given that
    // 1) we are not going to return to caller function anyway,
    // 2) r3 (new.target) will be initialized below.
    PrepareForTailCall(actual, r3, r4, r5);
  }

  Handle<JSFunction> known_function = hinstr->known_function();
  if (known_function.is_null()) {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount actual(instr->arity());
    InvokeFlag flag = is_tail_call ? JUMP_FUNCTION : CALL_FUNCTION;
    __ InvokeFunction(r1, no_reg, actual, flag, generator);
  } else {
    CallKnownFunction(known_function, hinstr->formal_parameter_count(),
                      instr->arity(), is_tail_call, instr);
  }
}


void LCodeGen::DoCallWithDescriptor(LCallWithDescriptor* instr) {
  DCHECK(ToRegister(instr->result()).is(r0));

  if (instr->hydrogen()->IsTailCall()) {
    if (NeedsEagerFrame()) __ LeaveFrame(StackFrame::INTERNAL);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      __ Jump(code, RelocInfo::CODE_TARGET);
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      // Make sure we don't emit any additional entries in the constant pool
      // before the call to ensure that the CallCodeSize() calculated the
      // correct
      // number of instructions for the constant pool load.
      {
        ConstantPoolUnavailableScope constant_pool_unavailable(masm_);
        __ add(target, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      }
      __ Jump(target);
    }
  } else {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      generator.BeforeCall(__ CallSize(code, RelocInfo::CODE_TARGET));
      PlatformInterfaceDescriptor* call_descriptor =
          instr->descriptor().platform_specific_descriptor();
      if (call_descriptor != NULL) {
        __ Call(code, RelocInfo::CODE_TARGET, TypeFeedbackId::None(), al,
                call_descriptor->storage_mode());
      } else {
        __ Call(code, RelocInfo::CODE_TARGET, TypeFeedbackId::None(), al);
      }
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      generator.BeforeCall(__ CallSize(target));
      // Make sure we don't emit any additional entries in the constant pool
      // before the call to ensure that the CallCodeSize() calculated the
      // correct
      // number of instructions for the constant pool load.
      {
        ConstantPoolUnavailableScope constant_pool_unavailable(masm_);
        __ add(target, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      }
      __ Call(target);
    }
    generator.AfterCall();
  }
}


void LCodeGen::DoCallNewArray(LCallNewArray* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->constructor()).is(r1));
  DCHECK(ToRegister(instr->result()).is(r0));

  __ mov(r0, Operand(instr->arity()));
  __ Move(r2, instr->hydrogen()->site());

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
      // We might need a change here
      // look at the first argument
      __ ldr(r5, MemOperand(sp, 0));
      __ cmp(r5, Operand::Zero());
      __ b(eq, &packed_case);

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
  __ add(code_object, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ str(code_object,
         FieldMemOperand(function, JSFunction::kCodeEntryOffset));
}


void LCodeGen::DoInnerAllocatedObject(LInnerAllocatedObject* instr) {
  Register result = ToRegister(instr->result());
  Register base = ToRegister(instr->base_object());
  if (instr->offset()->IsConstantOperand()) {
    LConstantOperand* offset = LConstantOperand::cast(instr->offset());
    __ add(result, base, Operand(ToInteger32(offset)));
  } else {
    Register offset = ToRegister(instr->offset());
    __ add(result, base, offset);
  }
}


void LCodeGen::DoStoreNamedField(LStoreNamedField* instr) {
  Representation representation = instr->representation();

  Register object = ToRegister(instr->object());
  Register scratch = scratch0();
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
  if (representation.IsDouble()) {
    DCHECK(access.IsInobject());
    DCHECK(!instr->hydrogen()->has_transition());
    DCHECK(!instr->hydrogen()->NeedsWriteBarrier());
    DwVfpRegister value = ToDoubleRegister(instr->value());
    __ vstr(value, FieldMemOperand(object, offset));
    return;
  }

  if (instr->hydrogen()->has_transition()) {
    Handle<Map> transition = instr->hydrogen()->transition_map();
    AddDeprecationDependency(transition);
    __ mov(scratch, Operand(transition));
    __ str(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    if (instr->hydrogen()->NeedsWriteBarrierForMap()) {
      Register temp = ToRegister(instr->temp());
      // Update the write barrier for the map field.
      __ RecordWriteForMap(object,
                           scratch,
                           temp,
                           GetLinkRegisterState(),
                           kSaveFPRegs);
    }
  }

  // Do the store.
  Register value = ToRegister(instr->value());
  if (access.IsInobject()) {
    MemOperand operand = FieldMemOperand(object, offset);
    __ Store(value, operand, representation);
    if (instr->hydrogen()->NeedsWriteBarrier()) {
      // Update the write barrier for the object for in-object properties.
      __ RecordWriteField(object,
                          offset,
                          value,
                          scratch,
                          GetLinkRegisterState(),
                          kSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          instr->hydrogen()->SmiCheckForWriteBarrier(),
                          instr->hydrogen()->PointersToHereCheckForValue());
    }
  } else {
    __ ldr(scratch, FieldMemOperand(object, JSObject::kPropertiesOffset));
    MemOperand operand = FieldMemOperand(scratch, offset);
    __ Store(value, operand, representation);
    if (instr->hydrogen()->NeedsWriteBarrier()) {
      // Update the write barrier for the properties array.
      // object is used as a scratch register.
      __ RecordWriteField(scratch,
                          offset,
                          value,
                          object,
                          GetLinkRegisterState(),
                          kSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          instr->hydrogen()->SmiCheckForWriteBarrier(),
                          instr->hydrogen()->PointersToHereCheckForValue());
    }
  }
}


void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  Condition cc = instr->hydrogen()->allow_equality() ? hi : hs;
  if (instr->index()->IsConstantOperand()) {
    Operand index = ToOperand(instr->index());
    Register length = ToRegister(instr->length());
    __ cmp(length, index);
    cc = CommuteCondition(cc);
  } else {
    Register index = ToRegister(instr->index());
    Operand length = ToOperand(instr->length());
    __ cmp(index, length);
  }
  if (FLAG_debug_code && instr->hydrogen()->skip_check()) {
    Label done;
    __ b(NegateCondition(cc), &done);
    __ stop("eliminated bounds check failed");
    __ bind(&done);
  } else {
    DeoptimizeIf(cc, instr, DeoptimizeReason::kOutOfBounds);
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
      ? (element_size_shift - kSmiTagSize) : element_size_shift;
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    Register address = scratch0();
    DwVfpRegister value(ToDoubleRegister(instr->value()));
    if (key_is_constant) {
      if (constant_key != 0) {
        __ add(address, external_pointer,
               Operand(constant_key << element_size_shift));
      } else {
        address = external_pointer;
      }
    } else {
      __ add(address, external_pointer, Operand(key, LSL, shift_size));
    }
    if (elements_kind == FLOAT32_ELEMENTS) {
      __ vcvt_f32_f64(double_scratch0().low(), value);
      __ vstr(double_scratch0().low(), address, base_offset);
    } else {  // Storing doubles, not floats.
      __ vstr(value, address, base_offset);
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
        __ strb(value, mem_operand);
        break;
      case INT16_ELEMENTS:
      case UINT16_ELEMENTS:
        __ strh(value, mem_operand);
        break;
      case INT32_ELEMENTS:
      case UINT32_ELEMENTS:
        __ str(value, mem_operand);
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
  DwVfpRegister value = ToDoubleRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  DwVfpRegister double_scratch = double_scratch0();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int base_offset = instr->base_offset();

  // Calculate the effective address of the slot in the array to store the
  // double value.
  int element_size_shift = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  if (key_is_constant) {
    int constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
    __ add(scratch, elements,
           Operand((constant_key << element_size_shift) + base_offset));
  } else {
    int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
        ? (element_size_shift - kSmiTagSize) : element_size_shift;
    __ add(scratch, elements, Operand(base_offset));
    __ add(scratch, scratch,
           Operand(ToRegister(instr->key()), LSL, shift_size));
  }

  if (instr->NeedsCanonicalization()) {
    // Force a canonical NaN.
    __ VFPCanonicalizeNaN(double_scratch, value);
    __ vstr(double_scratch, scratch, 0);
  } else {
    __ vstr(value, scratch, 0);
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
      __ add(scratch, elements, Operand::PointerOffsetFromSmiKey(key));
    } else {
      __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
    }
  }
  __ str(value, MemOperand(store_base, offset));

  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed =
        instr->hydrogen()->value()->type().IsHeapObject()
            ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
    // Compute address of modified element and store it into key register.
    __ add(key, store_base, Operand(offset));
    __ RecordWrite(elements,
                   key,
                   value,
                   GetLinkRegisterState(),
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

  Register result = r0;
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
    __ cmp(ToRegister(current_capacity), Operand(constant_key));
    __ b(le, deferred->entry());
  } else if (current_capacity->IsConstantOperand()) {
    int32_t constant_capacity =
        ToInteger32(LConstantOperand::cast(current_capacity));
    __ cmp(ToRegister(key), Operand(constant_capacity));
    __ b(ge, deferred->entry());
  } else {
    __ cmp(ToRegister(key), ToRegister(current_capacity));
    __ b(ge, deferred->entry());
  }

  if (instr->elements()->IsRegister()) {
    __ Move(result, ToRegister(instr->elements()));
  } else {
    __ ldr(result, ToMemOperand(instr->elements()));
  }

  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredMaybeGrowElements(LMaybeGrowElements* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register result = r0;
  __ mov(result, Operand::Zero());

  // We have to call a stub.
  {
    PushSafepointRegistersScope scope(this);
    if (instr->object()->IsRegister()) {
      __ Move(result, ToRegister(instr->object()));
    } else {
      __ ldr(result, ToMemOperand(instr->object()));
    }

    LOperand* key = instr->key();
    if (key->IsConstantOperand()) {
      LConstantOperand* constant_key = LConstantOperand::cast(key);
      int32_t int_key = ToInteger32(constant_key);
      if (Smi::IsValid(int_key)) {
        __ mov(r3, Operand(Smi::FromInt(int_key)));
      } else {
        Abort(kArrayIndexConstantValueTooBig);
      }
    } else {
      Label is_smi;
      __ SmiTag(r3, ToRegister(key), SetCC);
      // Deopt if the key is outside Smi range. The stub expects Smi and would
      // bump the elements into dictionary mode (and trigger a deopt) anyways.
      __ b(vc, &is_smi);
      __ PopSafepointRegisters();
      DeoptimizeIf(al, instr, DeoptimizeReason::kOverflow);
      __ bind(&is_smi);
    }

    GrowArrayElementsStub stub(isolate(), instr->hydrogen()->kind());
    __ CallStub(&stub);
    RecordSafepointWithLazyDeopt(
        instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    __ StoreToSafepointRegisterSlot(result, result);
  }

  // Deopt on smi, which means the elements array changed to dictionary mode.
  __ SmiTst(result);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi);
}


void LCodeGen::DoTransitionElementsKind(LTransitionElementsKind* instr) {
  Register object_reg = ToRegister(instr->object());
  Register scratch = scratch0();

  Handle<Map> from_map = instr->original_map();
  Handle<Map> to_map = instr->transitioned_map();
  ElementsKind from_kind = instr->from_kind();
  ElementsKind to_kind = instr->to_kind();

  Label not_applicable;
  __ ldr(scratch, FieldMemOperand(object_reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(from_map));
  __ b(ne, &not_applicable);

  if (IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Register new_map_reg = ToRegister(instr->new_map_temp());
    __ mov(new_map_reg, Operand(to_map));
    __ str(new_map_reg, FieldMemOperand(object_reg, HeapObject::kMapOffset));
    // Write barrier.
    __ RecordWriteForMap(object_reg,
                         new_map_reg,
                         scratch,
                         GetLinkRegisterState(),
                         kDontSaveFPRegs);
  } else {
    DCHECK(ToRegister(instr->context()).is(cp));
    DCHECK(object_reg.is(r0));
    PushSafepointRegistersScope scope(this);
    __ Move(r1, to_map);
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
  DeoptimizeIf(eq, instr, DeoptimizeReason::kMementoFound);
  __ bind(&no_memento_found);
}


void LCodeGen::DoStringAdd(LStringAdd* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(r1));
  DCHECK(ToRegister(instr->right()).is(r0));
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
  __ mov(result, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  __ push(string);
  // Push the index as a smi. This is safe because of the checks in
  // DoStringCharCodeAt above.
  if (instr->index()->IsConstantOperand()) {
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    __ mov(scratch, Operand(Smi::FromInt(const_index)));
    __ push(scratch);
  } else {
    Register index = ToRegister(instr->index());
    __ SmiTag(index);
    __ push(index);
  }
  CallRuntimeFromDeferred(Runtime::kStringCharCodeAtRT, 2, instr,
                          instr->context());
  __ AssertSmi(r0);
  __ SmiUntag(r0);
  __ StoreToSafepointRegisterSlot(r0, result);
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
  DCHECK(!char_code.is(result));

  __ cmp(char_code, Operand(String::kMaxOneByteCharCode));
  __ b(hi, deferred->entry());
  __ LoadRoot(result, Heap::kSingleCharacterStringCacheRootIndex);
  __ add(result, result, Operand(char_code, LSL, kPointerSizeLog2));
  __ ldr(result, FieldMemOperand(result, FixedArray::kHeaderSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(result, ip);
  __ b(eq, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharFromCode(LStringCharFromCode* instr) {
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  __ SmiTag(char_code);
  __ push(char_code);
  CallRuntimeFromDeferred(Runtime::kStringCharFromCode, 1, instr,
                          instr->context());
  __ StoreToSafepointRegisterSlot(r0, result);
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  DCHECK(output->IsDoubleRegister());
  SwVfpRegister single_scratch = double_scratch0().low();
  if (input->IsStackSlot()) {
    Register scratch = scratch0();
    __ ldr(scratch, ToMemOperand(input));
    __ vmov(single_scratch, scratch);
  } else {
    __ vmov(single_scratch, ToRegister(input));
  }
  __ vcvt_f64_s32(ToDoubleRegister(output), single_scratch);
}


void LCodeGen::DoUint32ToDouble(LUint32ToDouble* instr) {
  LOperand* input = instr->value();
  LOperand* output = instr->result();

  SwVfpRegister flt_scratch = double_scratch0().low();
  __ vmov(flt_scratch, ToRegister(input));
  __ vcvt_f64_u32(ToDoubleRegister(output), flt_scratch);
}


void LCodeGen::DoNumberTagI(LNumberTagI* instr) {
  class DeferredNumberTagI final : public LDeferredCode {
   public:
    DeferredNumberTagI(LCodeGen* codegen, LNumberTagI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    void Generate() override {
      codegen()->DoDeferredNumberTagIU(instr_,
                                       instr_->value(),
                                       instr_->temp1(),
                                       instr_->temp2(),
                                       SIGNED_INT32);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LNumberTagI* instr_;
  };

  Register src = ToRegister(instr->value());
  Register dst = ToRegister(instr->result());

  DeferredNumberTagI* deferred = new(zone()) DeferredNumberTagI(this, instr);
  __ SmiTag(dst, src, SetCC);
  __ b(vs, deferred->entry());
  __ bind(deferred->exit());
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
  __ cmp(input, Operand(Smi::kMaxValue));
  __ b(hi, deferred->entry());
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
  LowDwVfpRegister dbl_scratch = double_scratch0();

  if (signedness == SIGNED_INT32) {
    // There was overflow, so bits 30 and 31 of the original integer
    // disagree. Try to allocate a heap number in new space and store
    // the value in there. If that fails, call the runtime system.
    if (dst.is(src)) {
      __ SmiUntag(src, dst);
      __ eor(src, src, Operand(0x80000000));
    }
    __ vmov(dbl_scratch.low(), src);
    __ vcvt_f64_s32(dbl_scratch, dbl_scratch.low());
  } else {
    __ vmov(dbl_scratch.low(), src);
    __ vcvt_f64_u32(dbl_scratch, dbl_scratch.low());
  }

  if (FLAG_inline_new) {
    __ LoadRoot(tmp3, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(dst, tmp1, tmp2, tmp3, &slow);
    __ b(&done);
  }

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);
  {
    // TODO(3095996): Put a valid pointer value in the stack slot where the
    // result register is stored, as this register is in the pointer map, but
    // contains an integer value.
    __ mov(dst, Operand::Zero());

    // Preserve the value of all registers.
    PushSafepointRegistersScope scope(this);
    // Reset the context register.
    if (!dst.is(cp)) {
      __ mov(cp, Operand::Zero());
    }
    __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 0, Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(r0, dst);
  }

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ vstr(dbl_scratch, FieldMemOperand(dst, HeapNumber::kValueOffset));
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

  DwVfpRegister input_reg = ToDoubleRegister(instr->value());
  Register scratch = scratch0();
  Register reg = ToRegister(instr->result());
  Register temp1 = ToRegister(instr->temp());
  Register temp2 = ToRegister(instr->temp2());

  DeferredNumberTagD* deferred = new(zone()) DeferredNumberTagD(this, instr);
  if (FLAG_inline_new) {
    __ LoadRoot(scratch, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(reg, temp1, temp2, scratch, deferred->entry());
  } else {
    __ jmp(deferred->entry());
  }
  __ bind(deferred->exit());
  __ vstr(input_reg, FieldMemOperand(reg, HeapNumber::kValueOffset));
}


void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ mov(reg, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  // Reset the context register.
  if (!reg.is(cp)) {
    __ mov(cp, Operand::Zero());
  }
  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(r0, reg);
}


void LCodeGen::DoSmiTag(LSmiTag* instr) {
  HChange* hchange = instr->hydrogen();
  Register input = ToRegister(instr->value());
  Register output = ToRegister(instr->result());
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      hchange->value()->CheckFlag(HValue::kUint32)) {
    __ tst(input, Operand(0xc0000000));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow);
  }
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      !hchange->value()->CheckFlag(HValue::kUint32)) {
    __ SmiTag(output, input, SetCC);
    DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
  } else {
    __ SmiTag(output, input);
  }
}


void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  if (instr->needs_check()) {
    STATIC_ASSERT(kHeapObjectTag == 1);
    // If the input is a HeapObject, SmiUntag will set the carry flag.
    __ SmiUntag(result, input, SetCC);
    DeoptimizeIf(cs, instr, DeoptimizeReason::kNotASmi);
  } else {
    __ SmiUntag(result, input);
  }
}


void LCodeGen::EmitNumberUntagD(LNumberUntagD* instr, Register input_reg,
                                DwVfpRegister result_reg,
                                NumberUntagDMode mode) {
  bool can_convert_undefined_to_nan = instr->truncating();
  bool deoptimize_on_minus_zero = instr->hydrogen()->deoptimize_on_minus_zero();

  Register scratch = scratch0();
  SwVfpRegister flt_scratch = double_scratch0().low();
  DCHECK(!result_reg.is(double_scratch0()));
  Label convert, load_smi, done;
  if (mode == NUMBER_CANDIDATE_IS_ANY_TAGGED) {
    // Smi check.
    __ UntagAndJumpIfSmi(scratch, input_reg, &load_smi);
    // Heap number map check.
    __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(scratch, Operand(ip));
    if (can_convert_undefined_to_nan) {
      __ b(ne, &convert);
    } else {
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);
    }
    // load heap number
    __ vldr(result_reg, input_reg, HeapNumber::kValueOffset - kHeapObjectTag);
    if (deoptimize_on_minus_zero) {
      __ VmovLow(scratch, result_reg);
      __ cmp(scratch, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch, result_reg);
      __ cmp(scratch, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    }
    __ jmp(&done);
    if (can_convert_undefined_to_nan) {
      __ bind(&convert);
      // Convert undefined (and hole) to NaN.
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      __ cmp(input_reg, Operand(ip));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumberUndefined);
      __ LoadRoot(scratch, Heap::kNanValueRootIndex);
      __ vldr(result_reg, scratch, HeapNumber::kValueOffset - kHeapObjectTag);
      __ jmp(&done);
    }
  } else {
    __ SmiUntag(scratch, input_reg);
    DCHECK(mode == NUMBER_CANDIDATE_IS_SMI);
  }
  // Smi to double register conversion
  __ bind(&load_smi);
  // scratch: untagged value of input_reg
  __ vmov(flt_scratch, scratch);
  __ vcvt_f64_s32(result_reg, flt_scratch);
  __ bind(&done);
}


void LCodeGen::DoDeferredTaggedToI(LTaggedToI* instr) {
  Register input_reg = ToRegister(instr->value());
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->temp());
  LowDwVfpRegister double_scratch = double_scratch0();
  DwVfpRegister double_scratch2 = ToDoubleRegister(instr->temp2());

  DCHECK(!scratch1.is(input_reg) && !scratch1.is(scratch2));
  DCHECK(!scratch2.is(input_reg) && !scratch2.is(scratch1));

  Label done;

  // The input was optimistically untagged; revert it.
  // The carry flag is set when we reach this deferred code as we just executed
  // SmiUntag(heap_object, SetCC)
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ adc(scratch2, input_reg, Operand(input_reg));

  // Heap number map check.
  __ ldr(scratch1, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch1, Operand(ip));

  if (instr->truncating()) {
    Label truncate;
    __ b(eq, &truncate);
    __ CompareInstanceType(scratch1, scratch1, ODDBALL_TYPE);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotANumberOrOddball);
    __ bind(&truncate);
    __ TruncateHeapNumberToI(input_reg, scratch2);
  } else {
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);

    __ sub(ip, scratch2, Operand(kHeapObjectTag));
    __ vldr(double_scratch2, ip, HeapNumber::kValueOffset);
    __ TryDoubleToInt32Exact(input_reg, double_scratch2, double_scratch);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN);

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmp(input_reg, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch1, double_scratch2);
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero);
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

    // Optimistically untag the input.
    // If the input is a HeapObject, SmiUntag will set the carry flag.
    __ SmiUntag(input_reg, SetCC);
    // Branch to deferred code if the input was tagged.
    // The deferred code will take care of restoring the tag.
    __ b(cs, deferred->entry());
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoNumberUntagD(LNumberUntagD* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  LOperand* result = instr->result();
  DCHECK(result->IsDoubleRegister());

  Register input_reg = ToRegister(input);
  DwVfpRegister result_reg = ToDoubleRegister(result);

  HValue* value = instr->hydrogen()->value();
  NumberUntagDMode mode = value->representation().IsSmi()
      ? NUMBER_CANDIDATE_IS_SMI : NUMBER_CANDIDATE_IS_ANY_TAGGED;

  EmitNumberUntagD(instr, input_reg, result_reg, mode);
}


void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DwVfpRegister double_input = ToDoubleRegister(instr->value());
  LowDwVfpRegister double_scratch = double_scratch0();

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    __ TryDoubleToInt32Exact(result_reg, double_input, double_scratch);
    // Deoptimize if the input wasn't a int32 (inside a double).
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ cmp(result_reg, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch1, double_input);
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero);
      __ bind(&done);
    }
  }
}


void LCodeGen::DoDoubleToSmi(LDoubleToSmi* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DwVfpRegister double_input = ToDoubleRegister(instr->value());
  LowDwVfpRegister double_scratch = double_scratch0();

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    __ TryDoubleToInt32Exact(result_reg, double_input, double_scratch);
    // Deoptimize if the input wasn't a int32 (inside a double).
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ cmp(result_reg, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch1, double_input);
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kMinusZero);
      __ bind(&done);
    }
  }
  __ SmiTag(result_reg, SetCC);
  DeoptimizeIf(vs, instr, DeoptimizeReason::kOverflow);
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->value();
  __ SmiTst(ToRegister(input));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi);
}


void LCodeGen::DoCheckNonSmi(LCheckNonSmi* instr) {
  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    LOperand* input = instr->value();
    __ SmiTst(ToRegister(input));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi);
  }
}


void LCodeGen::DoCheckArrayBufferNotNeutered(
    LCheckArrayBufferNotNeutered* instr) {
  Register view = ToRegister(instr->view());
  Register scratch = scratch0();

  __ ldr(scratch, FieldMemOperand(view, JSArrayBufferView::kBufferOffset));
  __ ldr(scratch, FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
  __ tst(scratch, Operand(1 << JSArrayBuffer::WasNeutered::kShift));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kOutOfBounds);
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

  if (instr->hydrogen()->is_interval_check()) {
    InstanceType first;
    InstanceType last;
    instr->hydrogen()->GetCheckInterval(&first, &last);

    __ cmp(scratch, Operand(first));

    // If there is only one type in the interval check for equality.
    if (first == last) {
      DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongInstanceType);
    } else {
      DeoptimizeIf(lo, instr, DeoptimizeReason::kWrongInstanceType);
      // Omit check for the last type.
      if (last != LAST_TYPE) {
        __ cmp(scratch, Operand(last));
        DeoptimizeIf(hi, instr, DeoptimizeReason::kWrongInstanceType);
      }
    }
  } else {
    uint8_t mask;
    uint8_t tag;
    instr->hydrogen()->GetCheckMaskAndTag(&mask, &tag);

    if (base::bits::IsPowerOfTwo32(mask)) {
      DCHECK(tag == 0 || base::bits::IsPowerOfTwo32(tag));
      __ tst(scratch, Operand(mask));
      DeoptimizeIf(tag == 0 ? ne : eq, instr,
                   DeoptimizeReason::kWrongInstanceType);
    } else {
      __ and_(scratch, scratch, Operand(mask));
      __ cmp(scratch, Operand(tag));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongInstanceType);
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
    __ mov(ip, Operand(cell));
    __ ldr(ip, FieldMemOperand(ip, Cell::kValueOffset));
    __ cmp(reg, ip);
  } else {
    __ cmp(reg, Operand(object));
  }
  DeoptimizeIf(ne, instr, DeoptimizeReason::kValueMismatch);
}


void LCodeGen::DoDeferredInstanceMigration(LCheckMaps* instr, Register object) {
  {
    PushSafepointRegistersScope scope(this);
    __ push(object);
    __ mov(cp, Operand::Zero());
    __ CallRuntimeSaveDoubles(Runtime::kTryMigrateInstance);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 1, Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(r0, scratch0());
  }
  __ tst(scratch0(), Operand(kSmiTagMask));
  DeoptimizeIf(eq, instr, DeoptimizeReason::kInstanceMigrationFailed);
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

  __ ldr(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));

  DeferredCheckMaps* deferred = NULL;
  if (instr->hydrogen()->HasMigrationTarget()) {
    deferred = new(zone()) DeferredCheckMaps(this, instr, reg);
    __ bind(deferred->check_maps());
  }

  const UniqueSet<Map>* maps = instr->hydrogen()->maps();
  Label success;
  for (int i = 0; i < maps->size() - 1; i++) {
    Handle<Map> map = maps->at(i).handle();
    __ CompareMap(map_reg, map, &success);
    __ b(eq, &success);
  }

  Handle<Map> map = maps->at(maps->size() - 1).handle();
  __ CompareMap(map_reg, map, &success);
  if (instr->hydrogen()->HasMigrationTarget()) {
    __ b(ne, deferred->entry());
  } else {
    DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongMap);
  }

  __ bind(&success);
}


void LCodeGen::DoClampDToUint8(LClampDToUint8* instr) {
  DwVfpRegister value_reg = ToDoubleRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  __ ClampDoubleToUint8(result_reg, value_reg, double_scratch0());
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
  DwVfpRegister temp_reg = ToDoubleRegister(instr->temp());
  Label is_smi, done, heap_number;

  // Both smi and heap number cases are handled.
  __ UntagAndJumpIfSmi(result_reg, input_reg, &is_smi);

  // Check for heap number
  __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(factory()->heap_number_map()));
  __ b(eq, &heap_number);

  // Check for undefined. Undefined is converted to zero for clamping
  // conversions.
  __ cmp(input_reg, Operand(factory()->undefined_value()));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumberUndefined);
  __ mov(result_reg, Operand::Zero());
  __ jmp(&done);

  // Heap number
  __ bind(&heap_number);
  __ vldr(temp_reg, FieldMemOperand(input_reg, HeapNumber::kValueOffset));
  __ ClampDoubleToUint8(result_reg, temp_reg, double_scratch0());
  __ jmp(&done);

  // smi
  __ bind(&is_smi);
  __ ClampUint8(result_reg, result_reg);

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
      __ mov(scratch, Operand(size - kHeapObjectTag));
    } else {
      __ sub(scratch, ToRegister(instr->size()), Operand(kHeapObjectTag));
    }
    __ mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    Label loop;
    __ bind(&loop);
    __ sub(scratch, scratch, Operand(kPointerSize), SetCC);
    __ str(scratch2, MemOperand(result, scratch));
    __ b(ge, &loop);
  }
}


void LCodeGen::DoDeferredAllocate(LAllocate* instr) {
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand(Smi::kZero));

  PushSafepointRegistersScope scope(this);
  if (instr->size()->IsRegister()) {
    Register size = ToRegister(instr->size());
    DCHECK(!size.is(result));
    __ SmiTag(size);
    __ push(size);
  } else {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    if (size >= 0 && size <= Smi::kMaxValue) {
      __ Push(Smi::FromInt(size));
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
  __ Push(Smi::FromInt(flags));

  CallRuntimeFromDeferred(
      Runtime::kAllocateInTargetSpace, 2, instr, instr->context());
  __ StoreToSafepointRegisterSlot(r0, result);

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
    __ sub(r0, r0, Operand(kHeapObjectTag));
    __ mov(top_address, Operand(allocation_top));
    __ str(r0, MemOperand(top_address));
    __ add(r0, r0, Operand(kHeapObjectTag));
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
  DCHECK(ToRegister(instr->value()).is(r3));
  DCHECK(ToRegister(instr->result()).is(r0));
  Label end, do_call;
  Register value_register = ToRegister(instr->value());
  __ JumpIfNotSmi(value_register, &do_call);
  __ mov(r0, Operand(isolate()->factory()->number_string()));
  __ jmp(&end);
  __ bind(&do_call);
  Callable callable = CodeFactory::Typeof(isolate());
  CallCode(callable.code(), RelocInfo::CODE_TARGET, instr);
  __ bind(&end);
}


void LCodeGen::DoTypeofIsAndBranch(LTypeofIsAndBranch* instr) {
  Register input = ToRegister(instr->value());

  Condition final_branch_condition = EmitTypeofIs(instr->TrueLabel(chunk_),
                                                  instr->FalseLabel(chunk_),
                                                  input,
                                                  instr->type_literal());
  if (final_branch_condition != kNoCondition) {
    EmitBranch(instr, final_branch_condition);
  }
}


Condition LCodeGen::EmitTypeofIs(Label* true_label,
                                 Label* false_label,
                                 Register input,
                                 Handle<String> type_name) {
  Condition final_branch_condition = kNoCondition;
  Register scratch = scratch0();
  Factory* factory = isolate()->factory();
  if (String::Equals(type_name, factory->number_string())) {
    __ JumpIfSmi(input, true_label);
    __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->string_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, scratch, no_reg, FIRST_NONSTRING_TYPE);
    final_branch_condition = lt;

  } else if (String::Equals(type_name, factory->symbol_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, scratch, no_reg, SYMBOL_TYPE);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->boolean_string())) {
    __ CompareRoot(input, Heap::kTrueValueRootIndex);
    __ b(eq, true_label);
    __ CompareRoot(input, Heap::kFalseValueRootIndex);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->undefined_string())) {
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    __ b(eq, false_label);
    __ JumpIfSmi(input, false_label);
    // Check for undetectable objects => true.
    __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tst(scratch, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = ne;

  } else if (String::Equals(type_name, factory->function_string())) {
    __ JumpIfSmi(input, false_label);
    __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ and_(scratch, scratch,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ cmp(scratch, Operand(1 << Map::kIsCallable));
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->object_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    __ b(eq, true_label);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(input, scratch, ip, FIRST_JS_RECEIVER_TYPE);
    __ b(lt, false_label);
    // Check for callable or undetectable objects => false.
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tst(scratch,
           Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    final_branch_condition = eq;

// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)        \
  } else if (String::Equals(type_name, factory->type##_string())) {  \
    __ JumpIfSmi(input, false_label);                                \
    __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset)); \
    __ CompareRoot(scratch, Heap::k##Type##MapRootIndex);            \
    final_branch_condition = eq;
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on

  } else {
    __ b(false_label);
  }

  return final_branch_condition;
}


void LCodeGen::EnsureSpaceForLazyDeopt(int space_needed) {
  if (info()->ShouldEnsureSpaceForLazyDeopt()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    int current_pc = masm()->pc_offset();
    if (current_pc < last_lazy_deopt_pc_ + space_needed) {
      // Block literal pool emission for duration of padding.
      Assembler::BlockConstPoolScope block_const_pool(masm());
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

  DeoptimizeIf(al, instr, instr->hydrogen()->reason(), type);
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
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(hs, &done);
    Handle<Code> stack_check = isolate()->builtins()->StackCheck();
    PredictableCodeSizeScope predictable(masm());
    predictable.ExpectSize(CallCodeSize(stack_check, RelocInfo::CODE_TARGET));
    DCHECK(instr->context()->IsRegister());
    DCHECK(ToRegister(instr->context()).is(cp));
    CallCode(stack_check, RelocInfo::CODE_TARGET, instr);
    __ bind(&done);
  } else {
    DCHECK(instr->hydrogen()->is_backwards_branch());
    // Perform stack overflow check if this goto needs it before jumping.
    DeferredStackCheck* deferred_stack_check =
        new(zone()) DeferredStackCheck(this, instr);
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(lo, deferred_stack_check->entry());
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
  Label use_cache, call_runtime;
  __ CheckEnumCache(&call_runtime);

  __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r0);
  CallRuntime(Runtime::kForInEnumerate, instr);
  __ bind(&use_cache);
}


void LCodeGen::DoForInCacheArray(LForInCacheArray* instr) {
  Register map = ToRegister(instr->map());
  Register result = ToRegister(instr->result());
  Label load_cache, done;
  __ EnumLength(result, map);
  __ cmp(result, Operand(Smi::kZero));
  __ b(ne, &load_cache);
  __ mov(result, Operand(isolate()->factory()->empty_fixed_array()));
  __ jmp(&done);

  __ bind(&load_cache);
  __ LoadInstanceDescriptors(map, result);
  __ ldr(result,
         FieldMemOperand(result, DescriptorArray::kEnumCacheOffset));
  __ ldr(result,
         FieldMemOperand(result, FixedArray::SizeFor(instr->idx())));
  __ cmp(result, Operand::Zero());
  DeoptimizeIf(eq, instr, DeoptimizeReason::kNoCache);

  __ bind(&done);
}


void LCodeGen::DoCheckMapValue(LCheckMapValue* instr) {
  Register object = ToRegister(instr->value());
  Register map = ToRegister(instr->map());
  __ ldr(scratch0(), FieldMemOperand(object, HeapObject::kMapOffset));
  __ cmp(map, scratch0());
  DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongMap);
}


void LCodeGen::DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                           Register result,
                                           Register object,
                                           Register index) {
  PushSafepointRegistersScope scope(this);
  __ Push(object);
  __ Push(index);
  __ mov(cp, Operand::Zero());
  __ CallRuntimeSaveDoubles(Runtime::kLoadMutableDouble);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 2, Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(r0, result);
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

  __ tst(index, Operand(Smi::FromInt(1)));
  __ b(ne, deferred->entry());
  __ mov(index, Operand(index, ASR, 1));

  __ cmp(index, Operand::Zero());
  __ b(lt, &out_of_object);

  __ add(scratch, object, Operand::PointerOffsetFromSmiKey(index));
  __ ldr(result, FieldMemOperand(scratch, JSObject::kHeaderSize));

  __ b(&done);

  __ bind(&out_of_object);
  __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
  // Index is equal to negated out of object property index plus 1.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ sub(scratch, result, Operand::PointerOffsetFromSmiKey(index));
  __ ldr(result, FieldMemOperand(scratch,
                                 FixedArray::kHeaderSize - kPointerSize));
  __ bind(deferred->exit());
  __ bind(&done);
}

#undef __

}  // namespace internal
}  // namespace v8
