// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/address-map.h"
#include "src/assembler-inl.h"
#include "src/base/adapters.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler.h"
#include "src/eh-frame.h"
#include "src/frames.h"
#include "src/lsan.h"
#include "src/macro-assembler-inl.h"
#include "src/optimized-compilation-info.h"
#include "src/string-constants.h"

namespace v8 {
namespace internal {
namespace compiler {

class CodeGenerator::JumpTable final : public ZoneObject {
 public:
  JumpTable(JumpTable* next, Label** targets, size_t target_count)
      : next_(next), targets_(targets), target_count_(target_count) {}

  Label* label() { return &label_; }
  JumpTable* next() const { return next_; }
  Label** targets() const { return targets_; }
  size_t target_count() const { return target_count_; }

 private:
  Label label_;
  JumpTable* const next_;
  Label** const targets_;
  size_t const target_count_;
};

CodeGenerator::CodeGenerator(
    Zone* codegen_zone, Frame* frame, Linkage* linkage,
    InstructionSequence* code, OptimizedCompilationInfo* info, Isolate* isolate,
    base::Optional<OsrHelper> osr_helper, int start_source_position,
    JumpOptimizationInfo* jump_opt, PoisoningMitigationLevel poisoning_level,
    const AssemblerOptions& options, int32_t builtin_index)
    : zone_(codegen_zone),
      isolate_(isolate),
      frame_access_state_(nullptr),
      linkage_(linkage),
      code_(code),
      unwinding_info_writer_(zone()),
      info_(info),
      labels_(zone()->NewArray<Label>(code->InstructionBlockCount())),
      current_block_(RpoNumber::Invalid()),
      start_source_position_(start_source_position),
      current_source_position_(SourcePosition::Unknown()),
      tasm_(isolate, options, nullptr, 0, CodeObjectRequired::kNo),
      resolver_(this),
      safepoints_(zone()),
      handlers_(zone()),
      deoptimization_exits_(zone()),
      deoptimization_states_(zone()),
      deoptimization_literals_(zone()),
      inlined_function_count_(0),
      translations_(zone()),
      handler_table_offset_(0),
      last_lazy_deopt_pc_(0),
      caller_registers_saved_(false),
      jump_tables_(nullptr),
      ools_(nullptr),
      osr_helper_(std::move(osr_helper)),
      osr_pc_offset_(-1),
      optimized_out_literal_id_(-1),
      source_position_table_builder_(
          SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS),
      protected_instructions_(zone()),
      result_(kSuccess),
      poisoning_level_(poisoning_level),
      block_starts_(zone()),
      instr_starts_(zone()) {
  for (int i = 0; i < code->InstructionBlockCount(); ++i) {
    new (&labels_[i]) Label;
  }
  CreateFrameAccessState(frame);
  CHECK_EQ(info->is_osr(), osr_helper_.has_value());
  tasm_.set_jump_optimization_info(jump_opt);
  Code::Kind code_kind = info->code_kind();
  if (code_kind == Code::WASM_FUNCTION ||
      code_kind == Code::WASM_TO_JS_FUNCTION ||
      code_kind == Code::WASM_INTERPRETER_ENTRY ||
      (Builtins::IsBuiltinId(builtin_index) &&
       Builtins::IsWasmRuntimeStub(builtin_index))) {
    tasm_.set_abort_hard(true);
  }
  tasm_.set_builtin_index(builtin_index);
}

bool CodeGenerator::wasm_runtime_exception_support() const {
  DCHECK_NOT_NULL(info_);
  return info_->wasm_runtime_exception_support();
}

void CodeGenerator::AddProtectedInstructionLanding(uint32_t instr_offset,
                                                   uint32_t landing_offset) {
  protected_instructions_.push_back({instr_offset, landing_offset});
}

void CodeGenerator::CreateFrameAccessState(Frame* frame) {
  FinishFrame(frame);
  frame_access_state_ = new (zone()) FrameAccessState(frame);
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, SourcePosition pos) {
  DeoptimizeKind deopt_kind = GetDeoptimizationKind(deoptimization_id);
  DeoptimizeReason deoptimization_reason =
      GetDeoptimizationReason(deoptimization_id);
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      tasm()->isolate(), deoptimization_id, deopt_kind);
  if (deopt_entry == kNullAddress) return kTooManyDeoptimizationBailouts;
  if (info()->is_source_positions_enabled()) {
    tasm()->RecordDeoptReason(deoptimization_reason, pos, deoptimization_id);
  }
  tasm()->CallForDeoptimization(deopt_entry, deoptimization_id,
                                RelocInfo::RUNTIME_ENTRY);
  return kSuccess;
}

void CodeGenerator::AssembleCode() {
  OptimizedCompilationInfo* info = this->info();

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in AssemblePrologue).
  FrameScope frame_scope(tasm(), StackFrame::MANUAL);

  if (info->is_source_positions_enabled()) {
    AssembleSourcePosition(start_source_position());
  }

  // Place function entry hook if requested to do so.
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    ProfileEntryHookStub::MaybeCallEntryHookDelayed(tasm(), zone());
  }

  // Check that {kJavaScriptCallCodeStartRegister} has been set correctly.
  if (FLAG_debug_code & (info->code_kind() == Code::OPTIMIZED_FUNCTION ||
                         info->code_kind() == Code::BYTECODE_HANDLER)) {
    tasm()->RecordComment("-- Prologue: check code start register --");
    AssembleCodeStartRegisterCheck();
  }

  // TODO(jupvfranco): This should be the first thing in the code, otherwise
  // MaybeCallEntryHookDelayed may happen twice (for optimized and deoptimized
  // code). We want to bailout only from JS functions, which are the only ones
  // that are optimized.
  if (info->IsOptimizing()) {
    DCHECK(linkage()->GetIncomingDescriptor()->IsJSFunctionCall());
    tasm()->RecordComment("-- Prologue: check for deoptimization --");
    BailoutIfDeoptimized();
  }

  InitializeSpeculationPoison();

  // Define deoptimization literals for all inlined functions.
  DCHECK_EQ(0u, deoptimization_literals_.size());
  for (OptimizedCompilationInfo::InlinedFunctionHolder& inlined :
       info->inlined_functions()) {
    if (!inlined.shared_info.equals(info->shared_info())) {
      int index = DefineDeoptimizationLiteral(
          DeoptimizationLiteral(inlined.shared_info));
      inlined.RegisterInlinedFunctionId(index);
    }
  }
  inlined_function_count_ = deoptimization_literals_.size();

  unwinding_info_writer_.SetNumberOfInstructionBlocks(
      code()->InstructionBlockCount());

  if (info->trace_turbo_json_enabled()) {
    block_starts_.assign(code()->instruction_blocks().size(), -1);
    instr_starts_.assign(code()->instructions().size(), -1);
  }
  // Assemble all non-deferred blocks, followed by deferred ones.
  for (int deferred = 0; deferred < 2; ++deferred) {
    for (const InstructionBlock* block : code()->instruction_blocks()) {
      if (block->IsDeferred() == (deferred == 0)) {
        continue;
      }

      // Align loop headers on 16-byte boundaries.
      if (block->IsLoopHeader() && !tasm()->jump_optimization_info()) {
        tasm()->Align(16);
      }
      if (info->trace_turbo_json_enabled()) {
        block_starts_[block->rpo_number().ToInt()] = tasm()->pc_offset();
      }
      // Bind a label for a block.
      current_block_ = block->rpo_number();
      unwinding_info_writer_.BeginInstructionBlock(tasm()->pc_offset(), block);
      if (FLAG_code_comments) {
        Vector<char> buffer = Vector<char>::New(200);
        char* buffer_start = buffer.start();
        LSAN_IGNORE_OBJECT(buffer_start);

        int next = SNPrintF(
            buffer, "-- B%d start%s%s%s%s", block->rpo_number().ToInt(),
            block->IsDeferred() ? " (deferred)" : "",
            block->needs_frame() ? "" : " (no frame)",
            block->must_construct_frame() ? " (construct frame)" : "",
            block->must_deconstruct_frame() ? " (deconstruct frame)" : "");

        buffer = buffer.SubVector(next, buffer.length());

        if (block->IsLoopHeader()) {
          next =
              SNPrintF(buffer, " (loop up to %d)", block->loop_end().ToInt());
          buffer = buffer.SubVector(next, buffer.length());
        }
        if (block->loop_header().IsValid()) {
          next =
              SNPrintF(buffer, " (in loop %d)", block->loop_header().ToInt());
          buffer = buffer.SubVector(next, buffer.length());
        }
        SNPrintF(buffer, " --");
        tasm()->RecordComment(buffer_start);
      }

      frame_access_state()->MarkHasFrame(block->needs_frame());

      tasm()->bind(GetLabel(current_block_));

      TryInsertBranchPoisoning(block);

      if (block->must_construct_frame()) {
        AssembleConstructFrame();
        // We need to setup the root register after we assemble the prologue, to
        // avoid clobbering callee saved registers in case of C linkage and
        // using the roots.
        // TODO(mtrofin): investigate how we can avoid doing this repeatedly.
        if (linkage()->GetIncomingDescriptor()->InitializeRootRegister()) {
          tasm()->InitializeRootRegister();
        }
      }

      if (FLAG_enable_embedded_constant_pool && !block->needs_frame()) {
        ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
        result_ = AssembleBlock(block);
      } else {
        result_ = AssembleBlock(block);
      }
      if (result_ != kSuccess) return;
      unwinding_info_writer_.EndInstructionBlock(block);
    }
  }

  // Assemble all out-of-line code.
  if (ools_) {
    tasm()->RecordComment("-- Out of line code --");
    for (OutOfLineCode* ool = ools_; ool; ool = ool->next()) {
      tasm()->bind(ool->entry());
      ool->Generate();
      if (ool->exit()->is_bound()) tasm()->jmp(ool->exit());
    }
  }

  // This nop operation is needed to ensure that the trampoline is not
  // confused with the pc of the call before deoptimization.
  // The test regress/regress-259 is an example of where we need it.
  tasm()->nop();

  // Assemble deoptimization exits.
  int last_updated = 0;
  for (DeoptimizationExit* exit : deoptimization_exits_) {
    tasm()->bind(exit->label());
    int trampoline_pc = tasm()->pc_offset();
    int deoptimization_id = exit->deoptimization_id();
    DeoptimizationState* ds = deoptimization_states_[deoptimization_id];

    if (ds->kind() == DeoptimizeKind::kLazy) {
      last_updated = safepoints()->UpdateDeoptimizationInfo(
          ds->pc_offset(), trampoline_pc, last_updated);
    }
    result_ = AssembleDeoptimizerCall(deoptimization_id, exit->pos());
    if (result_ != kSuccess) return;
  }

  FinishCode();

  // Emit the jump tables.
  if (jump_tables_) {
    tasm()->Align(kPointerSize);
    for (JumpTable* table = jump_tables_; table; table = table->next()) {
      tasm()->bind(table->label());
      AssembleJumpTable(table->targets(), table->target_count());
    }
  }

  // The PerfJitLogger logs code up until here, excluding the safepoint
  // table. Resolve the unwinding info now so it is aware of the same code size
  // as reported by perf.
  unwinding_info_writer_.Finish(tasm()->pc_offset());

  safepoints()->Emit(tasm(), frame()->GetTotalFrameSlotCount());

  // Emit the exception handler table.
  if (!handlers_.empty()) {
    handler_table_offset_ = HandlerTable::EmitReturnTableStart(
        tasm(), static_cast<int>(handlers_.size()));
    for (size_t i = 0; i < handlers_.size(); ++i) {
      HandlerTable::EmitReturnEntry(tasm(), handlers_[i].pc_offset,
                                    handlers_[i].handler->pos());
    }
  }

  result_ = kSuccess;
}

void CodeGenerator::TryInsertBranchPoisoning(const InstructionBlock* block) {
  // See if our predecessor was a basic block terminated by a branch_and_poison
  // instruction. If yes, then perform the masking based on the flags.
  if (block->PredecessorCount() != 1) return;
  RpoNumber pred_rpo = (block->predecessors())[0];
  const InstructionBlock* pred = code()->InstructionBlockAt(pred_rpo);
  if (pred->code_start() == pred->code_end()) return;
  Instruction* instr = code()->InstructionAt(pred->code_end() - 1);
  FlagsMode mode = FlagsModeField::decode(instr->opcode());
  switch (mode) {
    case kFlags_branch_and_poison: {
      BranchInfo branch;
      RpoNumber target = ComputeBranchInfo(&branch, instr);
      if (!target.IsValid()) {
        // Non-trivial branch, add the masking code.
        FlagsCondition condition = branch.condition;
        if (branch.false_label == GetLabel(block->rpo_number())) {
          condition = NegateFlagsCondition(condition);
        }
        AssembleBranchPoisoning(condition, instr);
      }
      break;
    }
    case kFlags_deoptimize_and_poison: {
      UNREACHABLE();
      break;
    }
    default:
      break;
  }
}

void CodeGenerator::AssembleArchBinarySearchSwitchRange(
    Register input, RpoNumber def_block, std::pair<int32_t, Label*>* begin,
    std::pair<int32_t, Label*>* end) {
  if (end - begin < kBinarySearchSwitchMinimalCases) {
    while (begin != end) {
      tasm()->JumpIfEqual(input, begin->first, begin->second);
      ++begin;
    }
    AssembleArchJump(def_block);
    return;
  }
  auto middle = begin + (end - begin) / 2;
  Label less_label;
  tasm()->JumpIfLessThan(input, middle->first, &less_label);
  AssembleArchBinarySearchSwitchRange(input, def_block, middle, end);
  tasm()->bind(&less_label);
  AssembleArchBinarySearchSwitchRange(input, def_block, begin, middle);
}

OwnedVector<byte> CodeGenerator::GetSourcePositionTable() {
  return source_position_table_builder_.ToSourcePositionTableVector();
}

OwnedVector<trap_handler::ProtectedInstructionData>
CodeGenerator::GetProtectedInstructions() {
  return OwnedVector<trap_handler::ProtectedInstructionData>::Of(
      protected_instructions_);
}

MaybeHandle<Code> CodeGenerator::FinalizeCode() {
  if (result_ != kSuccess) {
    tasm()->AbortedCodeGeneration();
    return MaybeHandle<Code>();
  }

  // Allocate the source position table.
  Handle<ByteArray> source_positions =
      source_position_table_builder_.ToSourcePositionTable(isolate());

  // Allocate deoptimization data.
  Handle<DeoptimizationData> deopt_data = GenerateDeoptimizationData();

  // Allocate and install the code.
  CodeDesc desc;
  tasm()->GetCode(isolate(), &desc);
  if (unwinding_info_writer_.eh_frame_writer()) {
    unwinding_info_writer_.eh_frame_writer()->GetEhFrame(&desc);
  }

  MaybeHandle<Code> maybe_code = isolate()->factory()->TryNewCode(
      desc, info()->code_kind(), Handle<Object>(), info()->builtin_index(),
      source_positions, deopt_data, kMovable, info()->stub_key(), true,
      frame()->GetTotalFrameSlotCount(), safepoints()->GetCodeOffset(),
      handler_table_offset_);

  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    tasm()->AbortedCodeGeneration();
    return MaybeHandle<Code>();
  }
  isolate()->counters()->total_compiled_code_size()->Increment(
      code->raw_instruction_size());

  LOG_CODE_EVENT(isolate(),
                 CodeLinePosInfoRecordEvent(code->raw_instruction_start(),
                                            *source_positions));

  return code;
}


bool CodeGenerator::IsNextInAssemblyOrder(RpoNumber block) const {
  return code()
      ->InstructionBlockAt(current_block_)
      ->ao_number()
      .IsNext(code()->InstructionBlockAt(block)->ao_number());
}


void CodeGenerator::RecordSafepoint(ReferenceMap* references,
                                    Safepoint::Kind kind, int arguments,
                                    Safepoint::DeoptMode deopt_mode) {
  Safepoint safepoint =
      safepoints()->DefineSafepoint(tasm(), kind, arguments, deopt_mode);
  int stackSlotToSpillSlotDelta =
      frame()->GetTotalFrameSlotCount() - frame()->GetSpillSlotCount();
  for (const InstructionOperand& operand : references->reference_operands()) {
    if (operand.IsStackSlot()) {
      int index = LocationOperand::cast(operand).index();
      DCHECK_LE(0, index);
      // We might index values in the fixed part of the frame (i.e. the
      // closure pointer or the context pointer); these are not spill slots
      // and therefore don't work with the SafepointTable currently, but
      // we also don't need to worry about them, since the GC has special
      // knowledge about those fields anyway.
      if (index < stackSlotToSpillSlotDelta) continue;
      safepoint.DefinePointerSlot(index);
    } else if (operand.IsRegister() && (kind & Safepoint::kWithRegisters)) {
      Register reg = LocationOperand::cast(operand).GetRegister();
      safepoint.DefinePointerRegister(reg);
    }
  }
}

bool CodeGenerator::IsMaterializableFromRoot(Handle<HeapObject> object,
                                             RootIndex* index_return) {
  const CallDescriptor* incoming_descriptor =
      linkage()->GetIncomingDescriptor();
  if (incoming_descriptor->flags() & CallDescriptor::kCanUseRoots) {
    Heap* heap = isolate()->heap();
    return heap->IsRootHandle(object, index_return) &&
           !heap->RootCanBeWrittenAfterInitialization(*index_return);
  }
  return false;
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleBlock(
    const InstructionBlock* block) {
  for (int i = block->code_start(); i < block->code_end(); ++i) {
    if (info()->trace_turbo_json_enabled()) {
      instr_starts_[i] = tasm()->pc_offset();
    }
    Instruction* instr = code()->InstructionAt(i);
    CodeGenResult result = AssembleInstruction(instr, block);
    if (result != kSuccess) return result;
  }
  return kSuccess;
}

bool CodeGenerator::IsValidPush(InstructionOperand source,
                                CodeGenerator::PushTypeFlags push_type) {
  if (source.IsImmediate() &&
      ((push_type & CodeGenerator::kImmediatePush) != 0)) {
    return true;
  }
  if (source.IsRegister() &&
      ((push_type & CodeGenerator::kRegisterPush) != 0)) {
    return true;
  }
  if (source.IsStackSlot() &&
      ((push_type & CodeGenerator::kStackSlotPush) != 0)) {
    return true;
  }
  return false;
}

void CodeGenerator::GetPushCompatibleMoves(Instruction* instr,
                                           PushTypeFlags push_type,
                                           ZoneVector<MoveOperands*>* pushes) {
  pushes->clear();
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; ++i) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    ParallelMove* parallel_move = instr->GetParallelMove(inner_pos);
    if (parallel_move != nullptr) {
      for (auto move : *parallel_move) {
        InstructionOperand source = move->source();
        InstructionOperand destination = move->destination();
        int first_push_compatible_index =
            V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0;
        // If there are any moves from slots that will be overridden by pushes,
        // then the full gap resolver must be used since optimization with
        // pushes don't participate in the parallel move and might clobber
        // values needed for the gap resolve.
        if (source.IsStackSlot() &&
            LocationOperand::cast(source).index() >=
                first_push_compatible_index) {
          pushes->clear();
          return;
        }
        // TODO(danno): Right now, only consider moves from the FIRST gap for
        // pushes. Theoretically, we could extract pushes for both gaps (there
        // are cases where this happens), but the logic for that would also have
        // to check to make sure that non-memory inputs to the pushes from the
        // LAST gap don't get clobbered in the FIRST gap.
        if (i == Instruction::FIRST_GAP_POSITION) {
          if (destination.IsStackSlot() &&
              LocationOperand::cast(destination).index() >=
                  first_push_compatible_index) {
            int index = LocationOperand::cast(destination).index();
            if (IsValidPush(source, push_type)) {
              if (index >= static_cast<int>(pushes->size())) {
                pushes->resize(index + 1);
              }
              (*pushes)[index] = move;
            }
          }
        }
      }
    }
  }

  // For now, only support a set of continuous pushes at the end of the list.
  size_t push_count_upper_bound = pushes->size();
  size_t push_begin = push_count_upper_bound;
  for (auto move : base::Reversed(*pushes)) {
    if (move == nullptr) break;
    push_begin--;
  }
  size_t push_count = pushes->size() - push_begin;
  std::copy(pushes->begin() + push_begin,
            pushes->begin() + push_begin + push_count, pushes->begin());
  pushes->resize(push_count);
}

CodeGenerator::MoveType::Type CodeGenerator::MoveType::InferMove(
    InstructionOperand* source, InstructionOperand* destination) {
  if (source->IsConstant()) {
    if (destination->IsAnyRegister()) {
      return MoveType::kConstantToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kConstantToStack;
    }
  }
  DCHECK(LocationOperand::cast(source)->IsCompatible(
      LocationOperand::cast(destination)));
  if (source->IsAnyRegister()) {
    if (destination->IsAnyRegister()) {
      return MoveType::kRegisterToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kRegisterToStack;
    }
  } else {
    DCHECK(source->IsAnyStackSlot());
    if (destination->IsAnyRegister()) {
      return MoveType::kStackToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kStackToStack;
    }
  }
}

CodeGenerator::MoveType::Type CodeGenerator::MoveType::InferSwap(
    InstructionOperand* source, InstructionOperand* destination) {
  DCHECK(LocationOperand::cast(source)->IsCompatible(
      LocationOperand::cast(destination)));
  if (source->IsAnyRegister()) {
    if (destination->IsAnyRegister()) {
      return MoveType::kRegisterToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kRegisterToStack;
    }
  } else {
    DCHECK(source->IsAnyStackSlot());
    DCHECK(destination->IsAnyStackSlot());
    return MoveType::kStackToStack;
  }
}

RpoNumber CodeGenerator::ComputeBranchInfo(BranchInfo* branch,
                                           Instruction* instr) {
  // Assemble a branch after this instruction.
  InstructionOperandConverter i(this, instr);
  RpoNumber true_rpo = i.InputRpo(instr->InputCount() - 2);
  RpoNumber false_rpo = i.InputRpo(instr->InputCount() - 1);

  if (true_rpo == false_rpo) {
    return true_rpo;
  }
  FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
  if (IsNextInAssemblyOrder(true_rpo)) {
    // true block is next, can fall through if condition negated.
    std::swap(true_rpo, false_rpo);
    condition = NegateFlagsCondition(condition);
  }
  branch->condition = condition;
  branch->true_label = GetLabel(true_rpo);
  branch->false_label = GetLabel(false_rpo);
  branch->fallthru = IsNextInAssemblyOrder(false_rpo);
  return RpoNumber::Invalid();
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleInstruction(
    Instruction* instr, const InstructionBlock* block) {
  int first_unused_stack_slot;
  FlagsMode mode = FlagsModeField::decode(instr->opcode());
  if (mode != kFlags_trap) {
    AssembleSourcePosition(instr);
  }
  bool adjust_stack =
      GetSlotAboveSPBeforeTailCall(instr, &first_unused_stack_slot);
  if (adjust_stack) AssembleTailCallBeforeGap(instr, first_unused_stack_slot);
  AssembleGaps(instr);
  if (adjust_stack) AssembleTailCallAfterGap(instr, first_unused_stack_slot);
  DCHECK_IMPLIES(
      block->must_deconstruct_frame(),
      instr != code()->InstructionAt(block->last_instruction_index()) ||
          instr->IsRet() || instr->IsJump());
  if (instr->IsJump() && block->must_deconstruct_frame()) {
    AssembleDeconstructFrame();
  }
  // Assemble architecture-specific code for the instruction.
  CodeGenResult result = AssembleArchInstruction(instr);
  if (result != kSuccess) return result;

  FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
  switch (mode) {
    case kFlags_branch:
    case kFlags_branch_and_poison: {
      BranchInfo branch;
      RpoNumber target = ComputeBranchInfo(&branch, instr);
      if (target.IsValid()) {
        // redundant branch.
        if (!IsNextInAssemblyOrder(target)) {
          AssembleArchJump(target);
        }
        return kSuccess;
      }
      // Assemble architecture-specific branch.
      AssembleArchBranch(instr, &branch);
      break;
    }
    case kFlags_deoptimize:
    case kFlags_deoptimize_and_poison: {
      // Assemble a conditional eager deoptimization after this instruction.
      InstructionOperandConverter i(this, instr);
      size_t frame_state_offset = MiscField::decode(instr->opcode());
      DeoptimizationExit* const exit =
          AddDeoptimizationExit(instr, frame_state_offset);
      Label continue_label;
      BranchInfo branch;
      branch.condition = condition;
      branch.true_label = exit->label();
      branch.false_label = &continue_label;
      branch.fallthru = true;
      // Assemble architecture-specific branch.
      AssembleArchDeoptBranch(instr, &branch);
      tasm()->bind(&continue_label);
      if (mode == kFlags_deoptimize_and_poison) {
        AssembleBranchPoisoning(NegateFlagsCondition(branch.condition), instr);
      }
      break;
    }
    case kFlags_set: {
      // Assemble a boolean materialization after this instruction.
      AssembleArchBoolean(instr, condition);
      break;
    }
    case kFlags_trap: {
      AssembleArchTrap(instr, condition);
      break;
    }
    case kFlags_none: {
      break;
    }
  }

  // TODO(jarin) We should thread the flag through rather than set it.
  if (instr->IsCall()) {
    ResetSpeculationPoison();
  }

  return kSuccess;
}

void CodeGenerator::AssembleSourcePosition(Instruction* instr) {
  SourcePosition source_position = SourcePosition::Unknown();
  if (instr->IsNop() && instr->AreMovesRedundant()) return;
  if (!code()->GetSourcePosition(instr, &source_position)) return;
  AssembleSourcePosition(source_position);
}

void CodeGenerator::AssembleSourcePosition(SourcePosition source_position) {
  if (source_position == current_source_position_) return;
  current_source_position_ = source_position;
  if (!source_position.IsKnown()) return;
  source_position_table_builder_.AddPosition(tasm()->pc_offset(),
                                             source_position, false);
  if (FLAG_code_comments) {
    OptimizedCompilationInfo* info = this->info();
    if (info->IsStub()) return;
    std::ostringstream buffer;
    buffer << "-- ";
    // Turbolizer only needs the source position, as it can reconstruct
    // the inlining stack from other information.
    if (info->trace_turbo_json_enabled() || !tasm()->isolate() ||
        tasm()->isolate()->concurrent_recompilation_enabled()) {
      buffer << source_position;
    } else {
      AllowHeapAllocation allocation;
      AllowHandleAllocation handles;
      AllowHandleDereference deref;
      buffer << source_position.InliningStack(info);
    }
    buffer << " --";
    char* str = StrDup(buffer.str().c_str());
    LSAN_IGNORE_OBJECT(str);
    tasm()->RecordComment(str);
  }
}

bool CodeGenerator::GetSlotAboveSPBeforeTailCall(Instruction* instr,
                                                 int* slot) {
  if (instr->IsTailCall()) {
    InstructionOperandConverter g(this, instr);
    *slot = g.InputInt32(instr->InputCount() - 1);
    return true;
  } else {
    return false;
  }
}

StubCallMode CodeGenerator::DetermineStubCallMode() const {
  Code::Kind code_kind = info()->code_kind();
  return (code_kind == Code::WASM_FUNCTION ||
          code_kind == Code::WASM_TO_JS_FUNCTION)
             ? StubCallMode::kCallWasmRuntimeStub
             : StubCallMode::kCallOnHeapBuiltin;
}

void CodeGenerator::AssembleGaps(Instruction* instr) {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    ParallelMove* move = instr->GetParallelMove(inner_pos);
    if (move != nullptr) resolver()->Resolve(move);
  }
}

namespace {

Handle<PodArray<InliningPosition>> CreateInliningPositions(
    OptimizedCompilationInfo* info, Isolate* isolate) {
  const OptimizedCompilationInfo::InlinedFunctionList& inlined_functions =
      info->inlined_functions();
  if (inlined_functions.size() == 0) {
    return Handle<PodArray<InliningPosition>>::cast(
        isolate->factory()->empty_byte_array());
  }
  Handle<PodArray<InliningPosition>> inl_positions =
      PodArray<InliningPosition>::New(
          isolate, static_cast<int>(inlined_functions.size()), TENURED);
  for (size_t i = 0; i < inlined_functions.size(); ++i) {
    inl_positions->set(static_cast<int>(i), inlined_functions[i].position);
  }
  return inl_positions;
}

}  // namespace

Handle<DeoptimizationData> CodeGenerator::GenerateDeoptimizationData() {
  OptimizedCompilationInfo* info = this->info();
  int deopt_count = static_cast<int>(deoptimization_states_.size());
  if (deopt_count == 0 && !info->is_osr()) {
    return DeoptimizationData::Empty(isolate());
  }
  Handle<DeoptimizationData> data =
      DeoptimizationData::New(isolate(), deopt_count, TENURED);

  Handle<ByteArray> translation_array =
      translations_.CreateByteArray(isolate()->factory());

  data->SetTranslationByteArray(*translation_array);
  data->SetInlinedFunctionCount(
      Smi::FromInt(static_cast<int>(inlined_function_count_)));
  data->SetOptimizationId(Smi::FromInt(info->optimization_id()));

  if (info->has_shared_info()) {
    data->SetSharedFunctionInfo(*info->shared_info());
  } else {
    data->SetSharedFunctionInfo(Smi::kZero);
  }

  Handle<FixedArray> literals = isolate()->factory()->NewFixedArray(
      static_cast<int>(deoptimization_literals_.size()), TENURED);
  for (unsigned i = 0; i < deoptimization_literals_.size(); i++) {
    Handle<Object> object = deoptimization_literals_[i].Reify(isolate());
    literals->set(i, *object);
  }
  data->SetLiteralArray(*literals);

  Handle<PodArray<InliningPosition>> inl_pos =
      CreateInliningPositions(info, isolate());
  data->SetInliningPositions(*inl_pos);

  if (info->is_osr()) {
    DCHECK_LE(0, osr_pc_offset_);
    data->SetOsrBytecodeOffset(Smi::FromInt(info_->osr_offset().ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(osr_pc_offset_));
  } else {
    BailoutId osr_offset = BailoutId::None();
    data->SetOsrBytecodeOffset(Smi::FromInt(osr_offset.ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(-1));
  }

  // Populate deoptimization entries.
  for (int i = 0; i < deopt_count; i++) {
    DeoptimizationState* deoptimization_state = deoptimization_states_[i];
    data->SetBytecodeOffset(i, deoptimization_state->bailout_id());
    CHECK(deoptimization_state);
    data->SetTranslationIndex(
        i, Smi::FromInt(deoptimization_state->translation_id()));
    data->SetPc(i, Smi::FromInt(deoptimization_state->pc_offset()));
  }

  return data;
}


Label* CodeGenerator::AddJumpTable(Label** targets, size_t target_count) {
  jump_tables_ = new (zone()) JumpTable(jump_tables_, targets, target_count);
  return jump_tables_->label();
}


void CodeGenerator::RecordCallPosition(Instruction* instr) {
  CallDescriptor::Flags flags(MiscField::decode(instr->opcode()));

  bool needs_frame_state = (flags & CallDescriptor::kNeedsFrameState);

  RecordSafepoint(
      instr->reference_map(), Safepoint::kSimple, 0,
      needs_frame_state ? Safepoint::kLazyDeopt : Safepoint::kNoLazyDeopt);

  if (flags & CallDescriptor::kHasExceptionHandler) {
    InstructionOperandConverter i(this, instr);
    RpoNumber handler_rpo = i.InputRpo(instr->InputCount() - 1);
    handlers_.push_back({GetLabel(handler_rpo), tasm()->pc_offset()});
  }

  if (needs_frame_state) {
    MarkLazyDeoptSite();
    // If the frame state is present, it starts at argument 2 - after
    // the code address and the poison-alias index.
    size_t frame_state_offset = 2;
    FrameStateDescriptor* descriptor =
        GetDeoptimizationEntry(instr, frame_state_offset).descriptor();
    int pc_offset = tasm()->pc_offset();
    int deopt_state_id = BuildTranslation(instr, pc_offset, frame_state_offset,
                                          descriptor->state_combine());

    DeoptimizationExit* const exit = new (zone())
        DeoptimizationExit(deopt_state_id, current_source_position_);
    deoptimization_exits_.push_back(exit);
    safepoints()->RecordLazyDeoptimizationIndex(deopt_state_id);
  }
}

int CodeGenerator::DefineDeoptimizationLiteral(DeoptimizationLiteral literal) {
  int result = static_cast<int>(deoptimization_literals_.size());
  for (unsigned i = 0; i < deoptimization_literals_.size(); ++i) {
    if (deoptimization_literals_[i] == literal) return i;
  }
  deoptimization_literals_.push_back(literal);
  return result;
}

DeoptimizationEntry const& CodeGenerator::GetDeoptimizationEntry(
    Instruction* instr, size_t frame_state_offset) {
  InstructionOperandConverter i(this, instr);
  int const state_id = i.InputInt32(frame_state_offset);
  return code()->GetDeoptimizationEntry(state_id);
}

DeoptimizeKind CodeGenerator::GetDeoptimizationKind(
    int deoptimization_id) const {
  size_t const index = static_cast<size_t>(deoptimization_id);
  DCHECK_LT(index, deoptimization_states_.size());
  return deoptimization_states_[index]->kind();
}

DeoptimizeReason CodeGenerator::GetDeoptimizationReason(
    int deoptimization_id) const {
  size_t const index = static_cast<size_t>(deoptimization_id);
  DCHECK_LT(index, deoptimization_states_.size());
  return deoptimization_states_[index]->reason();
}

void CodeGenerator::TranslateStateValueDescriptor(
    StateValueDescriptor* desc, StateValueList* nested,
    Translation* translation, InstructionOperandIterator* iter) {
  // Note:
  // If translation is null, we just skip the relevant instruction operands.
  if (desc->IsNested()) {
    if (translation != nullptr) {
      translation->BeginCapturedObject(static_cast<int>(nested->size()));
    }
    for (auto field : *nested) {
      TranslateStateValueDescriptor(field.desc, field.nested, translation,
                                    iter);
    }
  } else if (desc->IsArgumentsElements()) {
    if (translation != nullptr) {
      translation->ArgumentsElements(desc->arguments_type());
    }
  } else if (desc->IsArgumentsLength()) {
    if (translation != nullptr) {
      translation->ArgumentsLength(desc->arguments_type());
    }
  } else if (desc->IsDuplicate()) {
    if (translation != nullptr) {
      translation->DuplicateObject(static_cast<int>(desc->id()));
    }
  } else if (desc->IsPlain()) {
    InstructionOperand* op = iter->Advance();
    if (translation != nullptr) {
      AddTranslationForOperand(translation, iter->instruction(), op,
                               desc->type());
    }
  } else {
    DCHECK(desc->IsOptimizedOut());
    if (translation != nullptr) {
      if (optimized_out_literal_id_ == -1) {
        optimized_out_literal_id_ = DefineDeoptimizationLiteral(
            DeoptimizationLiteral(isolate()->factory()->optimized_out()));
      }
      translation->StoreLiteral(optimized_out_literal_id_);
    }
  }
}


void CodeGenerator::TranslateFrameStateDescriptorOperands(
    FrameStateDescriptor* desc, InstructionOperandIterator* iter,
    OutputFrameStateCombine combine, Translation* translation) {
  size_t index = 0;
  StateValueList* values = desc->GetStateValueDescriptors();
  for (StateValueList::iterator it = values->begin(); it != values->end();
       ++it, ++index) {
    StateValueDescriptor* value_desc = (*it).desc;
    if (!combine.IsOutputIgnored()) {
      // The result of the call should be placed at position
      // [index_from_top] in the stack (overwriting whatever was
      // previously there).
      size_t index_from_top = desc->GetSize() - 1 - combine.GetOffsetToPokeAt();
      if (index >= index_from_top &&
          index < index_from_top + iter->instruction()->OutputCount()) {
        DCHECK_NOT_NULL(translation);
        AddTranslationForOperand(
            translation, iter->instruction(),
            iter->instruction()->OutputAt(index - index_from_top),
            MachineType::AnyTagged());
        // Skip the instruction operands.
        TranslateStateValueDescriptor(value_desc, (*it).nested, nullptr, iter);
        continue;
      }
    }
    TranslateStateValueDescriptor(value_desc, (*it).nested, translation, iter);
  }
  DCHECK_EQ(desc->GetSize(), index);
}


void CodeGenerator::BuildTranslationForFrameStateDescriptor(
    FrameStateDescriptor* descriptor, InstructionOperandIterator* iter,
    Translation* translation, OutputFrameStateCombine state_combine) {
  // Outer-most state must be added to translation first.
  if (descriptor->outer_state() != nullptr) {
    BuildTranslationForFrameStateDescriptor(descriptor->outer_state(), iter,
                                            translation,
                                            OutputFrameStateCombine::Ignore());
  }

  Handle<SharedFunctionInfo> shared_info;
  if (!descriptor->shared_info().ToHandle(&shared_info)) {
    if (!info()->has_shared_info()) {
      return;  // Stub with no SharedFunctionInfo.
    }
    shared_info = info()->shared_info();
  }
  int shared_info_id =
      DefineDeoptimizationLiteral(DeoptimizationLiteral(shared_info));

  switch (descriptor->type()) {
    case FrameStateType::kInterpretedFunction:
      translation->BeginInterpretedFrame(
          descriptor->bailout_id(), shared_info_id,
          static_cast<unsigned int>(descriptor->locals_count() + 1));
      break;
    case FrameStateType::kArgumentsAdaptor:
      translation->BeginArgumentsAdaptorFrame(
          shared_info_id,
          static_cast<unsigned int>(descriptor->parameters_count()));
      break;
    case FrameStateType::kConstructStub:
      DCHECK(descriptor->bailout_id().IsValidForConstructStub());
      translation->BeginConstructStubFrame(
          descriptor->bailout_id(), shared_info_id,
          static_cast<unsigned int>(descriptor->parameters_count() + 1));
      break;
    case FrameStateType::kBuiltinContinuation: {
      BailoutId bailout_id = descriptor->bailout_id();
      int parameter_count =
          static_cast<unsigned int>(descriptor->parameters_count());
      translation->BeginBuiltinContinuationFrame(bailout_id, shared_info_id,
                                                 parameter_count);
      break;
    }
    case FrameStateType::kJavaScriptBuiltinContinuation: {
      BailoutId bailout_id = descriptor->bailout_id();
      int parameter_count =
          static_cast<unsigned int>(descriptor->parameters_count());
      translation->BeginJavaScriptBuiltinContinuationFrame(
          bailout_id, shared_info_id, parameter_count);
      break;
    }
    case FrameStateType::kJavaScriptBuiltinContinuationWithCatch: {
      BailoutId bailout_id = descriptor->bailout_id();
      int parameter_count =
          static_cast<unsigned int>(descriptor->parameters_count());
      translation->BeginJavaScriptBuiltinContinuationWithCatchFrame(
          bailout_id, shared_info_id, parameter_count);
      break;
    }
  }

  TranslateFrameStateDescriptorOperands(descriptor, iter, state_combine,
                                        translation);
}


int CodeGenerator::BuildTranslation(Instruction* instr, int pc_offset,
                                    size_t frame_state_offset,
                                    OutputFrameStateCombine state_combine) {
  DeoptimizationEntry const& entry =
      GetDeoptimizationEntry(instr, frame_state_offset);
  FrameStateDescriptor* const descriptor = entry.descriptor();
  frame_state_offset++;

  int update_feedback_count = entry.feedback().IsValid() ? 1 : 0;
  Translation translation(&translations_,
                          static_cast<int>(descriptor->GetFrameCount()),
                          static_cast<int>(descriptor->GetJSFrameCount()),
                          update_feedback_count, zone());
  if (entry.feedback().IsValid()) {
    DeoptimizationLiteral literal =
        DeoptimizationLiteral(entry.feedback().vector());
    int literal_id = DefineDeoptimizationLiteral(literal);
    translation.AddUpdateFeedback(literal_id, entry.feedback().slot().ToInt());
  }
  InstructionOperandIterator iter(instr, frame_state_offset);
  BuildTranslationForFrameStateDescriptor(descriptor, &iter, &translation,
                                          state_combine);

  int deoptimization_id = static_cast<int>(deoptimization_states_.size());

  deoptimization_states_.push_back(new (zone()) DeoptimizationState(
      descriptor->bailout_id(), translation.index(), pc_offset, entry.kind(),
      entry.reason()));

  return deoptimization_id;
}

void CodeGenerator::AddTranslationForOperand(Translation* translation,
                                             Instruction* instr,
                                             InstructionOperand* op,
                                             MachineType type) {
  if (op->IsStackSlot()) {
    if (type.representation() == MachineRepresentation::kBit) {
      translation->StoreBoolStackSlot(LocationOperand::cast(op)->index());
    } else if (type == MachineType::Int8() || type == MachineType::Int16() ||
               type == MachineType::Int32()) {
      translation->StoreInt32StackSlot(LocationOperand::cast(op)->index());
    } else if (type == MachineType::Uint8() || type == MachineType::Uint16() ||
               type == MachineType::Uint32()) {
      translation->StoreUint32StackSlot(LocationOperand::cast(op)->index());
    } else if (type == MachineType::Int64()) {
      translation->StoreInt64StackSlot(LocationOperand::cast(op)->index());
    } else {
      CHECK_EQ(MachineRepresentation::kTagged, type.representation());
      translation->StoreStackSlot(LocationOperand::cast(op)->index());
    }
  } else if (op->IsFPStackSlot()) {
    if (type.representation() == MachineRepresentation::kFloat64) {
      translation->StoreDoubleStackSlot(LocationOperand::cast(op)->index());
    } else {
      CHECK_EQ(MachineRepresentation::kFloat32, type.representation());
      translation->StoreFloatStackSlot(LocationOperand::cast(op)->index());
    }
  } else if (op->IsRegister()) {
    InstructionOperandConverter converter(this, instr);
    if (type.representation() == MachineRepresentation::kBit) {
      translation->StoreBoolRegister(converter.ToRegister(op));
    } else if (type == MachineType::Int8() || type == MachineType::Int16() ||
               type == MachineType::Int32()) {
      translation->StoreInt32Register(converter.ToRegister(op));
    } else if (type == MachineType::Uint8() || type == MachineType::Uint16() ||
               type == MachineType::Uint32()) {
      translation->StoreUint32Register(converter.ToRegister(op));
    } else if (type == MachineType::Int64()) {
      translation->StoreInt64Register(converter.ToRegister(op));
    } else {
      CHECK_EQ(MachineRepresentation::kTagged, type.representation());
      translation->StoreRegister(converter.ToRegister(op));
    }
  } else if (op->IsFPRegister()) {
    InstructionOperandConverter converter(this, instr);
    if (type.representation() == MachineRepresentation::kFloat64) {
      translation->StoreDoubleRegister(converter.ToDoubleRegister(op));
    } else {
      CHECK_EQ(MachineRepresentation::kFloat32, type.representation());
      translation->StoreFloatRegister(converter.ToFloatRegister(op));
    }
  } else {
    CHECK(op->IsImmediate());
    InstructionOperandConverter converter(this, instr);
    Constant constant = converter.ToConstant(op);
    DeoptimizationLiteral literal;
    switch (constant.type()) {
      case Constant::kInt32:
        if (type.representation() == MachineRepresentation::kTagged) {
          // When pointers are 4 bytes, we can use int32 constants to represent
          // Smis.
          DCHECK_EQ(4, kPointerSize);
          Smi* smi = reinterpret_cast<Smi*>(constant.ToInt32());
          DCHECK(smi->IsSmi());
          literal = DeoptimizationLiteral(smi->value());
        } else if (type.representation() == MachineRepresentation::kBit) {
          if (constant.ToInt32() == 0) {
            literal =
                DeoptimizationLiteral(isolate()->factory()->false_value());
          } else {
            DCHECK_EQ(1, constant.ToInt32());
            literal = DeoptimizationLiteral(isolate()->factory()->true_value());
          }
        } else {
          DCHECK(type == MachineType::Int32() ||
                 type == MachineType::Uint32() ||
                 type.representation() == MachineRepresentation::kWord32 ||
                 type.representation() == MachineRepresentation::kNone);
          DCHECK(type.representation() != MachineRepresentation::kNone ||
                 constant.ToInt32() == FrameStateDescriptor::kImpossibleValue);
          if (type == MachineType::Uint32()) {
            literal = DeoptimizationLiteral(
                static_cast<uint32_t>(constant.ToInt32()));
          } else {
            literal = DeoptimizationLiteral(constant.ToInt32());
          }
        }
        break;
      case Constant::kInt64:
        DCHECK_EQ(8, kPointerSize);
        if (type.representation() == MachineRepresentation::kWord64) {
          literal =
              DeoptimizationLiteral(static_cast<double>(constant.ToInt64()));
        } else {
          // When pointers are 8 bytes, we can use int64 constants to represent
          // Smis.
          DCHECK_EQ(MachineRepresentation::kTagged, type.representation());
          Smi* smi = reinterpret_cast<Smi*>(constant.ToInt64());
          DCHECK(smi->IsSmi());
          literal = DeoptimizationLiteral(smi->value());
        }
        break;
      case Constant::kFloat32:
        DCHECK(type.representation() == MachineRepresentation::kFloat32 ||
               type.representation() == MachineRepresentation::kTagged);
        literal = DeoptimizationLiteral(constant.ToFloat32());
        break;
      case Constant::kFloat64:
        DCHECK(type.representation() == MachineRepresentation::kFloat64 ||
               type.representation() == MachineRepresentation::kTagged);
        literal = DeoptimizationLiteral(constant.ToFloat64().value());
        break;
      case Constant::kHeapObject:
        DCHECK_EQ(MachineRepresentation::kTagged, type.representation());
        literal = DeoptimizationLiteral(constant.ToHeapObject());
        break;
      case Constant::kDelayedStringConstant:
        DCHECK_EQ(MachineRepresentation::kTagged, type.representation());
        literal = DeoptimizationLiteral(constant.ToDelayedStringConstant());
        break;
      default:
        UNREACHABLE();
    }
    if (literal.object().equals(info()->closure())) {
      translation->StoreJSFrameFunction();
    } else {
      int literal_id = DefineDeoptimizationLiteral(literal);
      translation->StoreLiteral(literal_id);
    }
  }
}

void CodeGenerator::MarkLazyDeoptSite() {
  last_lazy_deopt_pc_ = tasm()->pc_offset();
}

DeoptimizationExit* CodeGenerator::AddDeoptimizationExit(
    Instruction* instr, size_t frame_state_offset) {
  int const deoptimization_id = BuildTranslation(
      instr, -1, frame_state_offset, OutputFrameStateCombine::Ignore());

  DeoptimizationExit* const exit = new (zone())
      DeoptimizationExit(deoptimization_id, current_source_position_);
  deoptimization_exits_.push_back(exit);
  return exit;
}

void CodeGenerator::InitializeSpeculationPoison() {
  if (poisoning_level_ == PoisoningMitigationLevel::kDontPoison) return;

  // Initialize {kSpeculationPoisonRegister} either by comparing the expected
  // with the actual call target, or by unconditionally using {-1} initially.
  // Masking register arguments with it only makes sense in the first case.
  if (info()->called_with_code_start_register()) {
    tasm()->RecordComment("-- Prologue: generate speculation poison --");
    GenerateSpeculationPoisonFromCodeStartRegister();
    if (info()->is_poisoning_register_arguments()) {
      AssembleRegisterArgumentPoisoning();
    }
  } else {
    ResetSpeculationPoison();
  }
}

void CodeGenerator::ResetSpeculationPoison() {
  if (poisoning_level_ != PoisoningMitigationLevel::kDontPoison) {
    tasm()->ResetSpeculationPoisonRegister();
  }
}

OutOfLineCode::OutOfLineCode(CodeGenerator* gen)
    : frame_(gen->frame()), tasm_(gen->tasm()), next_(gen->ools_) {
  gen->ools_ = this;
}

OutOfLineCode::~OutOfLineCode() = default;

Handle<Object> DeoptimizationLiteral::Reify(Isolate* isolate) const {
  switch (kind_) {
    case DeoptimizationLiteralKind::kObject: {
      return object_;
    }
    case DeoptimizationLiteralKind::kNumber: {
      return isolate->factory()->NewNumber(number_);
    }
    case DeoptimizationLiteralKind::kString: {
      return string_->AllocateStringConstant(isolate);
    }
  }
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
