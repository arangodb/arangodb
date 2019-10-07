// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-compiler.h"

#include "src/assembler-inl.h"
#include "src/base/optional.h"
// TODO(clemensh): Remove dependences on compiler stuff.
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/macro-assembler-inl.h"
#include "src/tracing/trace-event.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

constexpr auto kRegister = LiftoffAssembler::VarState::kRegister;
constexpr auto KIntConst = LiftoffAssembler::VarState::KIntConst;
constexpr auto kStack = LiftoffAssembler::VarState::kStack;

namespace {

#define __ asm_.

#define TRACE(...)                                            \
  do {                                                        \
    if (FLAG_trace_liftoff) PrintF("[liftoff] " __VA_ARGS__); \
  } while (false)

#define WASM_INSTANCE_OBJECT_OFFSET(name) \
  ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset)

template <int expected_size, int actual_size>
struct assert_field_size {
  static_assert(expected_size == actual_size,
                "field in WasmInstance does not have the expected size");
  static constexpr int size = actual_size;
};

#define WASM_INSTANCE_OBJECT_SIZE(name)     \
  (WasmInstanceObject::k##name##OffsetEnd - \
   WasmInstanceObject::k##name##Offset + 1)  // NOLINT(whitespace/indent)

#define LOAD_INSTANCE_FIELD(dst, name, load_size)  \
  __ LoadFromInstance(                             \
      dst.gp(), WASM_INSTANCE_OBJECT_OFFSET(name), \
      assert_field_size<WASM_INSTANCE_OBJECT_SIZE(name), load_size>::size);

#ifdef DEBUG
#define DEBUG_CODE_COMMENT(str) \
  do {                          \
    __ RecordComment(str);      \
  } while (false)
#else
#define DEBUG_CODE_COMMENT(str) ((void)0)
#endif

constexpr LoadType::LoadTypeValue kPointerLoadType =
    kPointerSize == 8 ? LoadType::kI64Load : LoadType::kI32Load;

#if V8_TARGET_ARCH_ARM64
// On ARM64, the Assembler keeps track of pointers to Labels to resolve
// branches to distant targets. Moving labels would confuse the Assembler,
// thus store the label on the heap and keep a unique_ptr.
class MovableLabel {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(MovableLabel);
  MovableLabel() : label_(new Label()) {}

  Label* get() { return label_.get(); }

 private:
  std::unique_ptr<Label> label_;
};
#else
// On all other platforms, just store the Label directly.
class MovableLabel {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(MovableLabel);

  Label* get() { return &label_; }

 private:
  Label label_;
};
#endif

compiler::CallDescriptor* GetLoweredCallDescriptor(
    Zone* zone, compiler::CallDescriptor* call_desc) {
  return kPointerSize == 4 ? compiler::GetI32WasmCallDescriptor(zone, call_desc)
                           : call_desc;
}

constexpr ValueType kTypesArr_ilfd[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64};
constexpr Vector<const ValueType> kTypes_ilfd = ArrayVector(kTypesArr_ilfd);

class LiftoffCompiler {
 public:
  // TODO(clemensh): Make this a template parameter.
  static constexpr Decoder::ValidateFlag validate = Decoder::kValidate;

  using Value = ValueBase;

  struct ElseState {
    MovableLabel label;
    LiftoffAssembler::CacheState state;
  };

  struct Control : public ControlWithNamedConstructors<Control, Value> {
    MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(Control);

    std::unique_ptr<ElseState> else_state;
    LiftoffAssembler::CacheState label_state;
    MovableLabel label;
  };

  using FullDecoder = WasmFullDecoder<validate, LiftoffCompiler>;

  struct OutOfLineCode {
    MovableLabel label;
    MovableLabel continuation;
    WasmCode::RuntimeStubId stub;
    WasmCodePosition position;
    LiftoffRegList regs_to_save;
    uint32_t pc;  // for trap handler.

    // Named constructors:
    static OutOfLineCode Trap(WasmCode::RuntimeStubId s, WasmCodePosition pos,
                              uint32_t pc) {
      DCHECK_LT(0, pos);
      return {{}, {}, s, pos, {}, pc};
    }
    static OutOfLineCode StackCheck(WasmCodePosition pos, LiftoffRegList regs) {
      return {{}, {}, WasmCode::kWasmStackGuard, pos, regs, 0};
    }
  };

  LiftoffCompiler(compiler::CallDescriptor* call_descriptor, ModuleEnv* env,
                  Zone* compilation_zone)
      : descriptor_(
            GetLoweredCallDescriptor(compilation_zone, call_descriptor)),
        env_(env),
        compilation_zone_(compilation_zone),
        safepoint_table_builder_(compilation_zone_) {}

  ~LiftoffCompiler() { BindUnboundLabels(nullptr); }

  bool ok() const { return ok_; }

  void GetCode(CodeDesc* desc) { asm_.GetCode(nullptr, desc); }

  OwnedVector<uint8_t> GetSourcePositionTable() {
    return source_position_table_builder_.ToSourcePositionTableVector();
  }

  OwnedVector<trap_handler::ProtectedInstructionData> GetProtectedInstructions()
      const {
    return OwnedVector<trap_handler::ProtectedInstructionData>::Of(
        protected_instructions_);
  }

  uint32_t GetTotalFrameSlotCount() const {
    return __ GetTotalFrameSlotCount();
  }

  void unsupported(FullDecoder* decoder, const char* reason) {
    ok_ = false;
    TRACE("unsupported: %s\n", reason);
    decoder->errorf(decoder->pc(), "unsupported liftoff operation: %s", reason);
    BindUnboundLabels(decoder);
  }

  bool DidAssemblerBailout(FullDecoder* decoder) {
    if (decoder->failed() || !__ did_bailout()) return false;
    unsupported(decoder, __ bailout_reason());
    return true;
  }

  bool CheckSupportedType(FullDecoder* decoder,
                          Vector<const ValueType> supported_types,
                          ValueType type, const char* context) {
    char buffer[128];
    // Check supported types.
    for (ValueType supported : supported_types) {
      if (type == supported) return true;
    }
    SNPrintF(ArrayVector(buffer), "%s %s", ValueTypes::TypeName(type), context);
    unsupported(decoder, buffer);
    return false;
  }

  int GetSafepointTableOffset() const {
    return safepoint_table_builder_.GetCodeOffset();
  }

  void BindUnboundLabels(FullDecoder* decoder) {
#ifdef DEBUG
    // Bind all labels now, otherwise their destructor will fire a DCHECK error
    // if they where referenced before.
    uint32_t control_depth = decoder ? decoder->control_depth() : 0;
    for (uint32_t i = 0; i < control_depth; ++i) {
      Control* c = decoder->control_at(i);
      Label* label = c->label.get();
      if (!label->is_bound()) __ bind(label);
      if (c->else_state) {
        Label* else_label = c->else_state->label.get();
        if (!else_label->is_bound()) __ bind(else_label);
      }
    }
    for (auto& ool : out_of_line_code_) {
      if (!ool.label.get()->is_bound()) __ bind(ool.label.get());
    }
#endif
  }

  void StartFunction(FullDecoder* decoder) {
    int num_locals = decoder->NumLocals();
    __ set_num_locals(num_locals);
    for (int i = 0; i < num_locals; ++i) {
      __ set_local_type(i, decoder->GetLocalType(i));
    }
  }

  void CollectReservedRegsForParameters(uint32_t input_idx_start,
                                        uint32_t num_params,
                                        LiftoffRegList& param_regs) {
    uint32_t input_idx = input_idx_start;
    for (uint32_t param_idx = 0; param_idx < num_params; ++param_idx) {
      ValueType type = __ local_type(param_idx);
      const int num_lowered_params = 1 + needs_reg_pair(type);
      RegClass rc = num_lowered_params == 1 ? reg_class_for(type) : kGpReg;

      for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
        compiler::LinkageLocation param_loc =
            descriptor_->GetInputLocation(input_idx + pair_idx);
        if (param_loc.IsRegister()) {
          DCHECK(!param_loc.IsAnyRegister());
          int reg_code = param_loc.AsRegister();
          RegList cache_regs = rc == kGpReg ? kLiftoffAssemblerGpCacheRegs
                                            : kLiftoffAssemblerFpCacheRegs;
          if (cache_regs & (1 << reg_code)) {
            LiftoffRegister in_reg = LiftoffRegister::from_code(rc, reg_code);
            param_regs.set(in_reg);
          }
        }
      }
      input_idx += num_lowered_params;
    }
  }

  // Returns the number of inputs processed (1 or 2).
  uint32_t ProcessParameter(ValueType type, uint32_t input_idx) {
    const int num_lowered_params = 1 + needs_reg_pair(type);
    // Initialize to anything, will be set in the loop and used afterwards.
    LiftoffRegister reg = kGpCacheRegList.GetFirstRegSet();
    RegClass rc = num_lowered_params == 1 ? reg_class_for(type) : kGpReg;
    LiftoffRegList pinned;
    for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
      compiler::LinkageLocation param_loc =
          descriptor_->GetInputLocation(input_idx + pair_idx);
      // Initialize to anything, will be set in both arms of the if.
      LiftoffRegister in_reg = kGpCacheRegList.GetFirstRegSet();
      if (param_loc.IsRegister()) {
        DCHECK(!param_loc.IsAnyRegister());
        int reg_code = param_loc.AsRegister();
        RegList cache_regs = rc == kGpReg ? kLiftoffAssemblerGpCacheRegs
                                          : kLiftoffAssemblerFpCacheRegs;
        if (cache_regs & (1 << reg_code)) {
          // This is a cache register, just use it.
          in_reg = LiftoffRegister::from_code(rc, reg_code);
        } else {
          // Move to a cache register (spill one if necessary).
          // Note that we cannot create a {LiftoffRegister} for reg_code, since
          // {LiftoffRegister} can only store cache regs.
          in_reg = __ GetUnusedRegister(rc, pinned);
          if (rc == kGpReg) {
            __ Move(in_reg.gp(), Register::from_code(reg_code), type);
          } else {
            __ Move(in_reg.fp(), DoubleRegister::from_code(reg_code), type);
          }
        }
      } else if (param_loc.IsCallerFrameSlot()) {
        in_reg = __ GetUnusedRegister(rc, pinned);
        ValueType lowered_type = num_lowered_params == 1 ? type : kWasmI32;
        __ LoadCallerFrameSlot(in_reg, -param_loc.AsCallerFrameSlot(),
                               lowered_type);
      }
      reg = pair_idx == 0 ? in_reg
                          : LiftoffRegister::ForPair(reg.gp(), in_reg.gp());
      pinned.set(reg);
    }
    __ PushRegister(type, reg);
    return num_lowered_params;
  }

  void StackCheck(WasmCodePosition position) {
    if (FLAG_wasm_no_stack_checks || !env_->runtime_exception_support) return;
    out_of_line_code_.push_back(
        OutOfLineCode::StackCheck(position, __ cache_state()->used_registers));
    OutOfLineCode& ool = out_of_line_code_.back();
    LiftoffRegister limit_address = __ GetUnusedRegister(kGpReg);
    LOAD_INSTANCE_FIELD(limit_address, StackLimitAddress, kPointerSize);
    __ StackCheck(ool.label.get(), limit_address.gp());
    __ bind(ool.continuation.get());
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {
    for (uint32_t i = 0; i < __ num_locals(); ++i) {
      if (!CheckSupportedType(decoder, kTypes_ilfd, __ local_type(i), "param"))
        return;
    }

    // Input 0 is the call target, the instance is at 1.
    constexpr int kInstanceParameterIndex = 1;
    // Store the instance parameter to a special stack slot.
    compiler::LinkageLocation instance_loc =
        descriptor_->GetInputLocation(kInstanceParameterIndex);
    DCHECK(instance_loc.IsRegister());
    DCHECK(!instance_loc.IsAnyRegister());
    Register instance_reg = Register::from_code(instance_loc.AsRegister());
    DCHECK_EQ(kWasmInstanceRegister, instance_reg);

    // Parameter 0 is the instance parameter.
    uint32_t num_params =
        static_cast<uint32_t>(decoder->sig_->parameter_count());

    __ EnterFrame(StackFrame::WASM_COMPILED);
    __ set_has_frame(true);
    pc_offset_stack_frame_construction_ = __ PrepareStackFrame();
    // {PrepareStackFrame} is the first platform-specific assembler method.
    // If this failed, we can bail out immediately, avoiding runtime overhead
    // and potential failures because of other unimplemented methods.
    // A platform implementing {PrepareStackFrame} must ensure that we can
    // finish compilation without errors even if we hit unimplemented
    // LiftoffAssembler methods.
    if (DidAssemblerBailout(decoder)) return;

    __ SpillInstance(instance_reg);
    // Input 0 is the code target, 1 is the instance. First parameter at 2.
    uint32_t input_idx = kInstanceParameterIndex + 1;
    for (uint32_t param_idx = 0; param_idx < num_params; ++param_idx) {
      input_idx += ProcessParameter(__ local_type(param_idx), input_idx);
    }
    DCHECK_EQ(input_idx, descriptor_->InputCount());
    // Set to a gp register, to mark this uninitialized.
    LiftoffRegister zero_double_reg = kGpCacheRegList.GetFirstRegSet();
    DCHECK(zero_double_reg.is_gp());
    for (uint32_t param_idx = num_params; param_idx < __ num_locals();
         ++param_idx) {
      ValueType type = decoder->GetLocalType(param_idx);
      switch (type) {
        case kWasmI32:
          __ cache_state()->stack_state.emplace_back(kWasmI32, uint32_t{0});
          break;
        case kWasmI64:
          __ cache_state()->stack_state.emplace_back(kWasmI64, uint32_t{0});
          break;
        case kWasmF32:
        case kWasmF64:
          if (zero_double_reg.is_gp()) {
            // Note: This might spill one of the registers used to hold
            // parameters.
            zero_double_reg = __ GetUnusedRegister(kFpReg);
            // Zero is represented by the bit pattern 0 for both f32 and f64.
            __ LoadConstant(zero_double_reg, WasmValue(0.));
          }
          __ PushRegister(type, zero_double_reg);
          break;
        default:
          UNIMPLEMENTED();
      }
    }
    block->label_state.stack_base = __ num_locals();

    // The function-prologue stack check is associated with position 0, which
    // is never a position of any instruction in the function.
    StackCheck(0);

    DCHECK_EQ(__ num_locals(), __ cache_state()->stack_height());
  }

  void GenerateOutOfLineCode(OutOfLineCode& ool) {
    __ bind(ool.label.get());
    const bool is_stack_check = ool.stub == WasmCode::kWasmStackGuard;
    const bool is_mem_out_of_bounds =
        ool.stub == WasmCode::kThrowWasmTrapMemOutOfBounds;

    if (is_mem_out_of_bounds && env_->use_trap_handler) {
      uint32_t pc = static_cast<uint32_t>(__ pc_offset());
      DCHECK_EQ(pc, __ pc_offset());
      protected_instructions_.emplace_back(
          trap_handler::ProtectedInstructionData{ool.pc, pc});
    }

    if (!env_->runtime_exception_support) {
      // We cannot test calls to the runtime in cctest/test-run-wasm.
      // Therefore we emit a call to C here instead of a call to the runtime.
      // In this mode, we never generate stack checks.
      DCHECK(!is_stack_check);
      __ CallTrapCallbackForTesting();
      __ LeaveFrame(StackFrame::WASM_COMPILED);
      __ DropStackSlotsAndRet(
          static_cast<uint32_t>(descriptor_->StackParameterCount()));
      return;
    }

    if (!ool.regs_to_save.is_empty()) __ PushRegisters(ool.regs_to_save);

    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(ool.position), false);
    __ CallRuntimeStub(ool.stub);
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);
    DCHECK_EQ(ool.continuation.get()->is_bound(), is_stack_check);
    if (!ool.regs_to_save.is_empty()) __ PopRegisters(ool.regs_to_save);
    if (is_stack_check) {
      __ emit_jump(ool.continuation.get());
    } else {
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
    }
  }

  void FinishFunction(FullDecoder* decoder) {
    if (DidAssemblerBailout(decoder)) return;
    for (OutOfLineCode& ool : out_of_line_code_) {
      GenerateOutOfLineCode(ool);
    }
    __ PatchPrepareStackFrame(pc_offset_stack_frame_construction_,
                              __ GetTotalFrameSlotCount());
    __ FinishCode();
    safepoint_table_builder_.Emit(&asm_, __ GetTotalFrameSlotCount());
    // The previous calls may have also generated a bailout.
    DidAssemblerBailout(decoder);
  }

  void OnFirstError(FullDecoder* decoder) {
    ok_ = false;
    BindUnboundLabels(decoder);
    asm_.AbortCompilation();
  }

  void NextInstruction(FullDecoder* decoder, WasmOpcode opcode) {
    TraceCacheState(decoder);
    SLOW_DCHECK(__ ValidateCacheState());
    DEBUG_CODE_COMMENT(WasmOpcodes::OpcodeName(opcode));
  }

  void Block(FullDecoder* decoder, Control* block) {
    block->label_state.stack_base = __ cache_state()->stack_height();
  }

  void Loop(FullDecoder* decoder, Control* loop) {
    loop->label_state.stack_base = __ cache_state()->stack_height();

    // Before entering a loop, spill all locals to the stack, in order to free
    // the cache registers, and to avoid unnecessarily reloading stack values
    // into registers at branches.
    // TODO(clemensh): Come up with a better strategy here, involving
    // pre-analysis of the function.
    __ SpillLocals();

    // Loop labels bind at the beginning of the block.
    __ bind(loop->label.get());

    // Save the current cache state for the merge when jumping to this loop.
    loop->label_state.Split(*__ cache_state());

    // Execute a stack check in the loop header.
    StackCheck(decoder->position());
  }

  void Try(FullDecoder* decoder, Control* block) {
    unsupported(decoder, "try");
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    DCHECK_EQ(if_block, decoder->control_at(0));
    DCHECK(if_block->is_if());

    if (if_block->start_merge.arity > 0 || if_block->end_merge.arity > 1)
      return unsupported(decoder, "multi-value if");

    // Allocate the else state.
    if_block->else_state = base::make_unique<ElseState>();

    // Test the condition, jump to else if zero.
    Register value = __ PopToRegister().gp();
    __ emit_cond_jump(kEqual, if_block->else_state->label.get(), kWasmI32,
                      value);

    if_block->label_state.stack_base = __ cache_state()->stack_height();
    // Store the state (after popping the value) for executing the else branch.
    if_block->else_state->state.Split(*__ cache_state());
  }

  void FallThruTo(FullDecoder* decoder, Control* c) {
    if (c->end_merge.reached) {
      __ MergeFullStackWith(c->label_state);
    } else if (c->is_onearmed_if()) {
      c->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                               c->br_merge()->arity);
      __ MergeFullStackWith(c->label_state);
    } else {
      c->label_state.Split(*__ cache_state());
    }
    TraceCacheState(decoder);
  }

  void PopControl(FullDecoder* decoder, Control* c) {
    if (!c->is_loop() && c->end_merge.reached) {
      __ cache_state()->Steal(c->label_state);
    }
    if (!c->label.get()->is_bound()) {
      __ bind(c->label.get());
    }
  }

  void EndControl(FullDecoder* decoder, Control* c) {}

  enum CCallReturn : bool { kHasReturn = true, kNoReturn = false };

  void GenerateCCall(const LiftoffRegister* result_regs, FunctionSig* sig,
                     ValueType out_argument_type,
                     const LiftoffRegister* arg_regs,
                     ExternalReference ext_ref) {
    // Before making a call, spill all cache registers.
    __ SpillAllRegisters();

    // Store arguments on our stack, then align the stack for calling to C.
    int param_bytes = 0;
    for (ValueType param_type : sig->parameters()) {
      param_bytes += ValueTypes::MemSize(param_type);
    }
    int out_arg_bytes = out_argument_type == kWasmStmt
                            ? 0
                            : ValueTypes::MemSize(out_argument_type);
    int stack_bytes = std::max(param_bytes, out_arg_bytes);
    __ CallC(sig, arg_regs, result_regs, out_argument_type, stack_bytes,
             ext_ref);
  }

  template <ValueType src_type, ValueType result_type, class EmitFn>
  void EmitUnOp(EmitFn fn) {
    static RegClass src_rc = reg_class_for(src_type);
    static RegClass result_rc = reg_class_for(result_type);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {src})
                              : __ GetUnusedRegister(result_rc);
    fn(dst, src);
    __ PushRegister(result_type, dst);
  }

  void EmitI32UnOpWithCFallback(bool (LiftoffAssembler::*emit_fn)(Register,
                                                                  Register),
                                ExternalReference (*fallback_fn)()) {
    auto emit_with_c_fallback = [=](LiftoffRegister dst, LiftoffRegister src) {
      if (emit_fn && (asm_.*emit_fn)(dst.gp(), src.gp())) return;
      ExternalReference ext_ref = fallback_fn();
      ValueType sig_i_i_reps[] = {kWasmI32, kWasmI32};
      FunctionSig sig_i_i(1, 1, sig_i_i_reps);
      GenerateCCall(&dst, &sig_i_i, kWasmStmt, &src, ext_ref);
    };
    EmitUnOp<kWasmI32, kWasmI32>(emit_with_c_fallback);
  }

  template <ValueType type>
  void EmitFloatUnOpWithCFallback(
      bool (LiftoffAssembler::*emit_fn)(DoubleRegister, DoubleRegister),
      ExternalReference (*fallback_fn)()) {
    auto emit_with_c_fallback = [=](LiftoffRegister dst, LiftoffRegister src) {
      if ((asm_.*emit_fn)(dst.fp(), src.fp())) return;
      ExternalReference ext_ref = fallback_fn();
      ValueType sig_reps[] = {type};
      FunctionSig sig(0, 1, sig_reps);
      GenerateCCall(&dst, &sig, type, &src, ext_ref);
    };
    EmitUnOp<type, type>(emit_with_c_fallback);
  }

  enum TypeConversionTrapping : bool { kCanTrap = true, kNoTrap = false };
  template <ValueType dst_type, ValueType src_type,
            TypeConversionTrapping can_trap>
  void EmitTypeConversion(WasmOpcode opcode, ExternalReference (*fallback_fn)(),
                          WasmCodePosition trap_position) {
    static constexpr RegClass src_rc = reg_class_for(src_type);
    static constexpr RegClass dst_rc = reg_class_for(dst_type);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = src_rc == dst_rc ? __ GetUnusedRegister(dst_rc, {src})
                                           : __ GetUnusedRegister(dst_rc);
    DCHECK_EQ(!!can_trap, trap_position > 0);
    Label* trap = can_trap ? AddOutOfLineTrap(
                                 trap_position,
                                 WasmCode::kThrowWasmTrapFloatUnrepresentable)
                           : nullptr;
    if (!__ emit_type_conversion(opcode, dst, src, trap)) {
      DCHECK_NOT_NULL(fallback_fn);
      ExternalReference ext_ref = fallback_fn();
      if (can_trap) {
        // External references for potentially trapping conversions return int.
        ValueType sig_reps[] = {kWasmI32, src_type};
        FunctionSig sig(1, 1, sig_reps);
        LiftoffRegister ret_reg =
            __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst));
        LiftoffRegister dst_regs[] = {ret_reg, dst};
        GenerateCCall(dst_regs, &sig, dst_type, &src, ext_ref);
        __ emit_cond_jump(kEqual, trap, kWasmI32, ret_reg.gp());
      } else {
        ValueType sig_reps[] = {src_type};
        FunctionSig sig(0, 1, sig_reps);
        GenerateCCall(&dst, &sig, dst_type, &src, ext_ref);
      }
    }
    __ PushRegister(dst_type, dst);
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, FunctionSig*,
            const Value& value, Value* result) {
#define CASE_I32_UNOP(opcode, fn)                       \
  case WasmOpcode::kExpr##opcode:                       \
    EmitUnOp<kWasmI32, kWasmI32>(                       \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.gp(), src.gp());             \
        });                                             \
    break;
#define CASE_I32_SIGN_EXTENSION(opcode, fn)             \
  case WasmOpcode::kExpr##opcode:                       \
    EmitUnOp<kWasmI32, kWasmI32>(                       \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.gp(), src.gp());             \
        });                                             \
    break;
#define CASE_I64_SIGN_EXTENSION(opcode, fn)             \
  case WasmOpcode::kExpr##opcode:                       \
    EmitUnOp<kWasmI64, kWasmI64>(                       \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst, src);                       \
        });                                             \
    break;
#define CASE_FLOAT_UNOP(opcode, type, fn)               \
  case WasmOpcode::kExpr##opcode:                       \
    EmitUnOp<kWasm##type, kWasm##type>(                 \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.fp(), src.fp());             \
        });                                             \
    break;
#define CASE_FLOAT_UNOP_WITH_CFALLBACK(opcode, type, fn)                    \
  case WasmOpcode::kExpr##opcode:                                           \
    EmitFloatUnOpWithCFallback<kWasm##type>(&LiftoffAssembler::emit_##fn,   \
                                            &ExternalReference::wasm_##fn); \
    break;
#define CASE_TYPE_CONVERSION(opcode, dst_type, src_type, ext_ref, can_trap) \
  case WasmOpcode::kExpr##opcode:                                           \
    EmitTypeConversion<kWasm##dst_type, kWasm##src_type, can_trap>(         \
        kExpr##opcode, ext_ref, can_trap ? decoder->position() : 0);        \
    break;
    switch (opcode) {
      CASE_I32_UNOP(I32Eqz, i32_eqz)
      CASE_I32_UNOP(I32Clz, i32_clz)
      CASE_I32_UNOP(I32Ctz, i32_ctz)
      CASE_FLOAT_UNOP(F32Abs, F32, f32_abs)
      CASE_FLOAT_UNOP(F32Neg, F32, f32_neg)
      CASE_FLOAT_UNOP(F32Ceil, F32, f32_ceil)
      CASE_FLOAT_UNOP(F32Floor, F32, f32_floor)
      CASE_FLOAT_UNOP(F32Trunc, F32, f32_trunc)
      CASE_FLOAT_UNOP(F32NearestInt, F32, f32_nearest_int)
      CASE_FLOAT_UNOP(F32Sqrt, F32, f32_sqrt)
      CASE_FLOAT_UNOP(F64Abs, F64, f64_abs)
      CASE_FLOAT_UNOP(F64Neg, F64, f64_neg)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Ceil, F64, f64_ceil)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Floor, F64, f64_floor)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Trunc, F64, f64_trunc)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64NearestInt, F64, f64_nearest_int)
      CASE_FLOAT_UNOP(F64Sqrt, F64, f64_sqrt)
      CASE_TYPE_CONVERSION(I32ConvertI64, I32, I64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I32SConvertF32, I32, F32, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32UConvertF32, I32, F32, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32SConvertF64, I32, F64, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32UConvertF64, I32, F64, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32ReinterpretF32, I32, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertI32, I64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64UConvertI32, I64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertF32, I64, F32,
                           &ExternalReference::wasm_float32_to_int64, kCanTrap)
      CASE_TYPE_CONVERSION(I64UConvertF32, I64, F32,
                           &ExternalReference::wasm_float32_to_uint64, kCanTrap)
      CASE_TYPE_CONVERSION(I64SConvertF64, I64, F64,
                           &ExternalReference::wasm_float64_to_int64, kCanTrap)
      CASE_TYPE_CONVERSION(I64UConvertF64, I64, F64,
                           &ExternalReference::wasm_float64_to_uint64, kCanTrap)
      CASE_TYPE_CONVERSION(I64ReinterpretF64, I64, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32SConvertI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32UConvertI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32SConvertI64, F32, I64,
                           &ExternalReference::wasm_int64_to_float32, kNoTrap)
      CASE_TYPE_CONVERSION(F32UConvertI64, F32, I64,
                           &ExternalReference::wasm_uint64_to_float32, kNoTrap)
      CASE_TYPE_CONVERSION(F32ConvertF64, F32, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32ReinterpretI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64SConvertI32, F64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64UConvertI32, F64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64SConvertI64, F64, I64,
                           &ExternalReference::wasm_int64_to_float64, kNoTrap)
      CASE_TYPE_CONVERSION(F64UConvertI64, F64, I64,
                           &ExternalReference::wasm_uint64_to_float64, kNoTrap)
      CASE_TYPE_CONVERSION(F64ConvertF32, F64, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64ReinterpretI64, F64, I64, nullptr, kNoTrap)
      CASE_I32_SIGN_EXTENSION(I32SExtendI8, i32_signextend_i8)
      CASE_I32_SIGN_EXTENSION(I32SExtendI16, i32_signextend_i16)
      CASE_I64_SIGN_EXTENSION(I64SExtendI8, i64_signextend_i8)
      CASE_I64_SIGN_EXTENSION(I64SExtendI16, i64_signextend_i16)
      CASE_I64_SIGN_EXTENSION(I64SExtendI32, i64_signextend_i32)
      case kExprI32Popcnt:
        EmitI32UnOpWithCFallback(&LiftoffAssembler::emit_i32_popcnt,
                                 &ExternalReference::wasm_word32_popcnt);
        break;
      case WasmOpcode::kExprI64Eqz:
        EmitUnOp<kWasmI64, kWasmI32>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              __ emit_i64_eqz(dst.gp(), src);
            });
        break;
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_I32_UNOP
#undef CASE_I32_SIGN_EXTENSION
#undef CASE_I64_SIGN_EXTENSION
#undef CASE_FLOAT_UNOP
#undef CASE_FLOAT_UNOP_WITH_CFALLBACK
#undef CASE_TYPE_CONVERSION
  }

  template <ValueType src_type, ValueType result_type, typename EmitFn>
  void EmitBinOp(EmitFn fn) {
    static constexpr RegClass src_rc = reg_class_for(src_type);
    static constexpr RegClass result_rc = reg_class_for(result_type);
    LiftoffRegister rhs = __ PopToRegister();
    LiftoffRegister lhs = __ PopToRegister(LiftoffRegList::ForRegs(rhs));
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {lhs, rhs})
                              : __ GetUnusedRegister(result_rc);
    fn(dst, lhs, rhs);
    __ PushRegister(result_type, dst);
  }

  void EmitDivOrRem64CCall(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs, ExternalReference ext_ref,
                           Label* trap_by_zero,
                           Label* trap_unrepresentable = nullptr) {
    // Cannot emit native instructions, build C call.
    LiftoffRegister ret =
        __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst));
    LiftoffRegister tmp =
        __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst, ret));
    LiftoffRegister arg_regs[] = {lhs, rhs};
    LiftoffRegister result_regs[] = {ret, dst};
    ValueType sig_types[] = {kWasmI32, kWasmI64, kWasmI64};
    // <i64, i64> -> i32 (with i64 output argument)
    FunctionSig sig(1, 2, sig_types);
    GenerateCCall(result_regs, &sig, kWasmI64, arg_regs, ext_ref);
    __ LoadConstant(tmp, WasmValue(int32_t{0}));
    __ emit_cond_jump(kEqual, trap_by_zero, kWasmI32, ret.gp(), tmp.gp());
    if (trap_unrepresentable) {
      __ LoadConstant(tmp, WasmValue(int32_t{-1}));
      __ emit_cond_jump(kEqual, trap_unrepresentable, kWasmI32, ret.gp(),
                        tmp.gp());
    }
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, FunctionSig*,
             const Value& lhs, const Value& rhs, Value* result) {
#define CASE_I32_BINOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp());                        \
        });
#define CASE_I64_BINOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI64, kWasmI64>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst, lhs, rhs);                                       \
        });
#define CASE_FLOAT_BINOP(opcode, type, fn)                                   \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasm##type, kWasm##type>(                              \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.fp(), lhs.fp(), rhs.fp());                        \
        });
#define CASE_I32_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_i32_set_cond(cond, dst.gp(), lhs.gp(), rhs.gp());          \
        });
#define CASE_I64_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI64, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_i64_set_cond(cond, dst.gp(), lhs, rhs);                    \
        });
#define CASE_F32_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmF32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f32_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_F64_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmF64, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f64_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_I32_SHIFTOP(opcode, fn)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp(), {});                    \
        });
#define CASE_I64_SHIFTOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                              \
    return EmitBinOp<kWasmI64, kWasmI64>([=](LiftoffRegister dst,              \
                                             LiftoffRegister src,              \
                                             LiftoffRegister amount) {         \
      __ emit_##fn(dst, src, amount.is_pair() ? amount.low_gp() : amount.gp(), \
                   {});                                                        \
    });
#define CASE_CCALL_BINOP(opcode, type, ext_ref_fn)                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          LiftoffRegister args[] = {lhs, rhs};                               \
          auto ext_ref = ExternalReference::ext_ref_fn();                    \
          ValueType sig_i_ii_reps[] = {kWasmI32, kWasmI32, kWasmI32};        \
          FunctionSig sig_i_ii(1, 2, sig_i_ii_reps);                         \
          GenerateCCall(&dst, &sig_i_ii, kWasmStmt, args, ext_ref);          \
        });
    switch (opcode) {
      CASE_I32_BINOP(I32Add, i32_add)
      CASE_I32_BINOP(I32Sub, i32_sub)
      CASE_I32_BINOP(I32Mul, i32_mul)
      CASE_I32_BINOP(I32And, i32_and)
      CASE_I32_BINOP(I32Ior, i32_or)
      CASE_I32_BINOP(I32Xor, i32_xor)
      CASE_I64_BINOP(I64And, i64_and)
      CASE_I64_BINOP(I64Ior, i64_or)
      CASE_I64_BINOP(I64Xor, i64_xor)
      CASE_I32_CMPOP(I32Eq, kEqual)
      CASE_I32_CMPOP(I32Ne, kUnequal)
      CASE_I32_CMPOP(I32LtS, kSignedLessThan)
      CASE_I32_CMPOP(I32LtU, kUnsignedLessThan)
      CASE_I32_CMPOP(I32GtS, kSignedGreaterThan)
      CASE_I32_CMPOP(I32GtU, kUnsignedGreaterThan)
      CASE_I32_CMPOP(I32LeS, kSignedLessEqual)
      CASE_I32_CMPOP(I32LeU, kUnsignedLessEqual)
      CASE_I32_CMPOP(I32GeS, kSignedGreaterEqual)
      CASE_I32_CMPOP(I32GeU, kUnsignedGreaterEqual)
      CASE_I64_BINOP(I64Add, i64_add)
      CASE_I64_BINOP(I64Sub, i64_sub)
      CASE_I64_BINOP(I64Mul, i64_mul)
      CASE_I64_CMPOP(I64Eq, kEqual)
      CASE_I64_CMPOP(I64Ne, kUnequal)
      CASE_I64_CMPOP(I64LtS, kSignedLessThan)
      CASE_I64_CMPOP(I64LtU, kUnsignedLessThan)
      CASE_I64_CMPOP(I64GtS, kSignedGreaterThan)
      CASE_I64_CMPOP(I64GtU, kUnsignedGreaterThan)
      CASE_I64_CMPOP(I64LeS, kSignedLessEqual)
      CASE_I64_CMPOP(I64LeU, kUnsignedLessEqual)
      CASE_I64_CMPOP(I64GeS, kSignedGreaterEqual)
      CASE_I64_CMPOP(I64GeU, kUnsignedGreaterEqual)
      CASE_F32_CMPOP(F32Eq, kEqual)
      CASE_F32_CMPOP(F32Ne, kUnequal)
      CASE_F32_CMPOP(F32Lt, kUnsignedLessThan)
      CASE_F32_CMPOP(F32Gt, kUnsignedGreaterThan)
      CASE_F32_CMPOP(F32Le, kUnsignedLessEqual)
      CASE_F32_CMPOP(F32Ge, kUnsignedGreaterEqual)
      CASE_F64_CMPOP(F64Eq, kEqual)
      CASE_F64_CMPOP(F64Ne, kUnequal)
      CASE_F64_CMPOP(F64Lt, kUnsignedLessThan)
      CASE_F64_CMPOP(F64Gt, kUnsignedGreaterThan)
      CASE_F64_CMPOP(F64Le, kUnsignedLessEqual)
      CASE_F64_CMPOP(F64Ge, kUnsignedGreaterEqual)
      CASE_I32_SHIFTOP(I32Shl, i32_shl)
      CASE_I32_SHIFTOP(I32ShrS, i32_sar)
      CASE_I32_SHIFTOP(I32ShrU, i32_shr)
      CASE_I64_SHIFTOP(I64Shl, i64_shl)
      CASE_I64_SHIFTOP(I64ShrS, i64_sar)
      CASE_I64_SHIFTOP(I64ShrU, i64_shr)
      CASE_CCALL_BINOP(I32Rol, I32, wasm_word32_rol)
      CASE_CCALL_BINOP(I32Ror, I32, wasm_word32_ror)
      CASE_FLOAT_BINOP(F32Add, F32, f32_add)
      CASE_FLOAT_BINOP(F32Sub, F32, f32_sub)
      CASE_FLOAT_BINOP(F32Mul, F32, f32_mul)
      CASE_FLOAT_BINOP(F32Div, F32, f32_div)
      CASE_FLOAT_BINOP(F32Min, F32, f32_min)
      CASE_FLOAT_BINOP(F32Max, F32, f32_max)
      CASE_FLOAT_BINOP(F32CopySign, F32, f32_copysign)
      CASE_FLOAT_BINOP(F64Add, F64, f64_add)
      CASE_FLOAT_BINOP(F64Sub, F64, f64_sub)
      CASE_FLOAT_BINOP(F64Mul, F64, f64_mul)
      CASE_FLOAT_BINOP(F64Div, F64, f64_div)
      CASE_FLOAT_BINOP(F64Min, F64, f64_min)
      CASE_FLOAT_BINOP(F64Max, F64, f64_max)
      CASE_FLOAT_BINOP(F64CopySign, F64, f64_copysign)
      case WasmOpcode::kExprI32DivS:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          WasmCodePosition position = decoder->position();
          AddOutOfLineTrap(position, WasmCode::kThrowWasmTrapDivByZero);
          // Adding the second trap might invalidate the pointer returned for
          // the first one, thus get both pointers afterwards.
          AddOutOfLineTrap(position,
                           WasmCode::kThrowWasmTrapDivUnrepresentable);
          Label* div_by_zero = out_of_line_code_.end()[-2].label.get();
          Label* div_unrepresentable = out_of_line_code_.end()[-1].label.get();
          __ emit_i32_divs(dst.gp(), lhs.gp(), rhs.gp(), div_by_zero,
                           div_unrepresentable);
        });
        break;
      case WasmOpcode::kExprI32DivU:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* div_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapDivByZero);
          __ emit_i32_divu(dst.gp(), lhs.gp(), rhs.gp(), div_by_zero);
        });
        break;
      case WasmOpcode::kExprI32RemS:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          __ emit_i32_rems(dst.gp(), lhs.gp(), rhs.gp(), rem_by_zero);
        });
        break;
      case WasmOpcode::kExprI32RemU:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          __ emit_i32_remu(dst.gp(), lhs.gp(), rhs.gp(), rem_by_zero);
        });
        break;
      case WasmOpcode::kExprI64DivS:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          WasmCodePosition position = decoder->position();
          AddOutOfLineTrap(position, WasmCode::kThrowWasmTrapDivByZero);
          // Adding the second trap might invalidate the pointer returned for
          // the first one, thus get both pointers afterwards.
          AddOutOfLineTrap(position,
                           WasmCode::kThrowWasmTrapDivUnrepresentable);
          Label* div_by_zero = out_of_line_code_.end()[-2].label.get();
          Label* div_unrepresentable = out_of_line_code_.end()[-1].label.get();
          if (!__ emit_i64_divs(dst, lhs, rhs, div_by_zero,
                                div_unrepresentable)) {
            ExternalReference ext_ref = ExternalReference::wasm_int64_div();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, div_by_zero,
                                div_unrepresentable);
          }
        });
        break;
      case WasmOpcode::kExprI64DivU:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* div_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapDivByZero);
          if (!__ emit_i64_divu(dst, lhs, rhs, div_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_uint64_div();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, div_by_zero);
          }
        });
        break;
      case WasmOpcode::kExprI64RemS:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          if (!__ emit_i64_rems(dst, lhs, rhs, rem_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_int64_mod();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, rem_by_zero);
          }
        });
        break;
      case WasmOpcode::kExprI64RemU:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          if (!__ emit_i64_remu(dst, lhs, rhs, rem_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_uint64_mod();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, rem_by_zero);
          }
        });
        break;
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_I32_BINOP
#undef CASE_I64_BINOP
#undef CASE_FLOAT_BINOP
#undef CASE_I32_CMPOP
#undef CASE_I64_CMPOP
#undef CASE_F32_CMPOP
#undef CASE_F64_CMPOP
#undef CASE_I32_SHIFTOP
#undef CASE_I64_SHIFTOP
#undef CASE_CCALL_BINOP
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    __ cache_state()->stack_state.emplace_back(kWasmI32, value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    // The {VarState} stores constant values as int32_t, thus we only store
    // 64-bit constants in this field if it fits in an int32_t. Larger values
    // cannot be used as immediate value anyway, so we can also just put them in
    // a register immediately.
    int32_t value_i32 = static_cast<int32_t>(value);
    if (value_i32 == value) {
      __ cache_state()->stack_state.emplace_back(kWasmI64, value_i32);
    } else {
      LiftoffRegister reg = __ GetUnusedRegister(reg_class_for(kWasmI64));
      __ LoadConstant(reg, WasmValue(value));
      __ PushRegister(kWasmI64, reg);
    }
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF32, reg);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF64, reg);
  }

  void RefNull(FullDecoder* decoder, Value* result) {
    unsupported(decoder, "ref_null");
  }

  void Drop(FullDecoder* decoder, const Value& value) {
    auto& slot = __ cache_state()->stack_state.back();
    // If the dropped slot contains a register, decrement it's use count.
    if (slot.is_reg()) __ cache_state()->dec_used(slot.reg());
    __ cache_state()->stack_state.pop_back();
  }

  void DoReturn(FullDecoder* decoder, Vector<Value> values, bool implicit) {
    if (implicit) {
      DCHECK_EQ(1, decoder->control_depth());
      Control* func_block = decoder->control_at(0);
      __ bind(func_block->label.get());
      __ cache_state()->Steal(func_block->label_state);
    }
    if (!values.is_empty()) {
      if (values.size() > 1) return unsupported(decoder, "multi-return");
      LiftoffRegister reg = __ PopToRegister();
      LiftoffRegister return_reg =
          kNeedI64RegPair && values[0].type == kWasmI64
              ? LiftoffRegister::ForPair(kGpReturnRegisters[0],
                                         kGpReturnRegisters[1])
              : reg_class_for(values[0].type) == kGpReg
                    ? LiftoffRegister(kGpReturnRegisters[0])
                    : LiftoffRegister(kFpReturnRegisters[0]);
      if (reg != return_reg) __ Move(return_reg, reg, values[0].type);
    }
    __ LeaveFrame(StackFrame::WASM_COMPILED);
    __ DropStackSlotsAndRet(
        static_cast<uint32_t>(descriptor_->StackParameterCount()));
  }

  void GetLocal(FullDecoder* decoder, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    auto& slot = __ cache_state()->stack_state[imm.index];
    DCHECK_EQ(slot.type(), imm.type);
    switch (slot.loc()) {
      case kRegister:
        __ PushRegister(slot.type(), slot.reg());
        break;
      case KIntConst:
        __ cache_state()->stack_state.emplace_back(imm.type, slot.i32_const());
        break;
      case kStack: {
        auto rc = reg_class_for(imm.type);
        LiftoffRegister reg = __ GetUnusedRegister(rc);
        __ Fill(reg, imm.index, imm.type);
        __ PushRegister(slot.type(), reg);
        break;
      }
    }
  }

  void SetLocalFromStackSlot(LiftoffAssembler::VarState& dst_slot,
                             uint32_t local_index) {
    auto& state = *__ cache_state();
    ValueType type = dst_slot.type();
    if (dst_slot.is_reg()) {
      LiftoffRegister slot_reg = dst_slot.reg();
      if (state.get_use_count(slot_reg) == 1) {
        __ Fill(dst_slot.reg(), state.stack_height() - 1, type);
        return;
      }
      state.dec_used(slot_reg);
      dst_slot.MakeStack();
    }
    DCHECK_EQ(type, __ local_type(local_index));
    RegClass rc = reg_class_for(type);
    LiftoffRegister dst_reg = __ GetUnusedRegister(rc);
    __ Fill(dst_reg, __ cache_state()->stack_height() - 1, type);
    dst_slot = LiftoffAssembler::VarState(type, dst_reg);
    __ cache_state()->inc_used(dst_reg);
  }

  void SetLocal(uint32_t local_index, bool is_tee) {
    auto& state = *__ cache_state();
    auto& source_slot = state.stack_state.back();
    auto& target_slot = state.stack_state[local_index];
    switch (source_slot.loc()) {
      case kRegister:
        if (target_slot.is_reg()) state.dec_used(target_slot.reg());
        target_slot = source_slot;
        if (is_tee) state.inc_used(target_slot.reg());
        break;
      case KIntConst:
        if (target_slot.is_reg()) state.dec_used(target_slot.reg());
        target_slot = source_slot;
        break;
      case kStack:
        SetLocalFromStackSlot(target_slot, local_index);
        break;
    }
    if (!is_tee) __ cache_state()->stack_state.pop_back();
  }

  void SetLocal(FullDecoder* decoder, const Value& value,
                const LocalIndexImmediate<validate>& imm) {
    SetLocal(imm.index, false);
  }

  void TeeLocal(FullDecoder* decoder, const Value& value, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    SetLocal(imm.index, true);
  }

  LiftoffRegister GetGlobalBaseAndOffset(const WasmGlobal* global,
                                         LiftoffRegList& pinned,
                                         uint32_t* offset) {
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg));
    if (global->mutability && global->imported) {
      LOAD_INSTANCE_FIELD(addr, ImportedMutableGlobals, kPointerSize);
      __ Load(addr, addr.gp(), no_reg, global->index * sizeof(Address),
              kPointerLoadType, pinned);
      *offset = 0;
    } else {
      LOAD_INSTANCE_FIELD(addr, GlobalsStart, kPointerSize);
      *offset = global->offset;
    }
    return addr;
  }

  void GetGlobal(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    const auto* global = &env_->module->globals[imm.index];
    if (!CheckSupportedType(decoder, kTypes_ilfd, global->type, "global"))
      return;
    LiftoffRegList pinned;
    uint32_t offset = 0;
    LiftoffRegister addr = GetGlobalBaseAndOffset(global, pinned, &offset);
    LiftoffRegister value =
        pinned.set(__ GetUnusedRegister(reg_class_for(global->type), pinned));
    LoadType type = LoadType::ForValueType(global->type);
    __ Load(value, addr.gp(), no_reg, offset, type, pinned, nullptr, true);
    __ PushRegister(global->type, value);
  }

  void SetGlobal(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate<validate>& imm) {
    auto* global = &env_->module->globals[imm.index];
    if (!CheckSupportedType(decoder, kTypes_ilfd, global->type, "global"))
      return;
    LiftoffRegList pinned;
    uint32_t offset = 0;
    LiftoffRegister addr = GetGlobalBaseAndOffset(global, pinned, &offset);
    LiftoffRegister reg = pinned.set(__ PopToRegister(pinned));
    StoreType type = StoreType::ForValueType(global->type);
    __ Store(addr.gp(), no_reg, offset, reg, type, pinned, nullptr, true);
  }

  void Unreachable(FullDecoder* decoder) {
    Label* unreachable_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapUnreachable);
    __ emit_jump(unreachable_label);
    __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    LiftoffRegList pinned;
    Register condition = pinned.set(__ PopToRegister()).gp();
    ValueType type = __ cache_state()->stack_state.end()[-1].type();
    DCHECK_EQ(type, __ cache_state()->stack_state.end()[-2].type());
    LiftoffRegister false_value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister true_value = __ PopToRegister(pinned);
    LiftoffRegister dst =
        __ GetUnusedRegister(true_value.reg_class(), {true_value, false_value});
    __ PushRegister(type, dst);

    // Now emit the actual code to move either {true_value} or {false_value}
    // into {dst}.
    Label cont;
    Label case_false;
    __ emit_cond_jump(kEqual, &case_false, kWasmI32, condition);
    if (dst != true_value) __ Move(dst, true_value, type);
    __ emit_jump(&cont);

    __ bind(&case_false);
    if (dst != false_value) __ Move(dst, false_value, type);
    __ bind(&cont);
  }

  void Br(Control* target) {
    if (!target->br_merge()->reached) {
      target->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                                    target->br_merge()->arity);
    }
    __ MergeStackWith(target->label_state, target->br_merge()->arity);
    __ jmp(target->label.get());
  }

  void Br(FullDecoder* decoder, Control* target) { Br(target); }

  void BrIf(FullDecoder* decoder, const Value& cond, Control* target) {
    Label cont_false;
    Register value = __ PopToRegister().gp();
    __ emit_cond_jump(kEqual, &cont_false, kWasmI32, value);

    Br(target);
    __ bind(&cont_false);
  }

  // Generate a branch table case, potentially reusing previously generated
  // stack transfer code.
  void GenerateBrCase(FullDecoder* decoder, uint32_t br_depth,
                      std::map<uint32_t, MovableLabel>& br_targets) {
    MovableLabel& label = br_targets[br_depth];
    if (label.get()->is_bound()) {
      __ jmp(label.get());
    } else {
      __ bind(label.get());
      Br(decoder->control_at(br_depth));
    }
  }

  // Generate a branch table for input in [min, max).
  // TODO(wasm): Generate a real branch table (like TF TableSwitch).
  void GenerateBrTable(FullDecoder* decoder, LiftoffRegister tmp,
                       LiftoffRegister value, uint32_t min, uint32_t max,
                       BranchTableIterator<validate>& table_iterator,
                       std::map<uint32_t, MovableLabel>& br_targets) {
    DCHECK_LT(min, max);
    // Check base case.
    if (max == min + 1) {
      DCHECK_EQ(min, table_iterator.cur_index());
      GenerateBrCase(decoder, table_iterator.next(), br_targets);
      return;
    }

    uint32_t split = min + (max - min) / 2;
    Label upper_half;
    __ LoadConstant(tmp, WasmValue(split));
    __ emit_cond_jump(kUnsignedGreaterEqual, &upper_half, kWasmI32, value.gp(),
                      tmp.gp());
    // Emit br table for lower half:
    GenerateBrTable(decoder, tmp, value, min, split, table_iterator,
                    br_targets);
    __ bind(&upper_half);
    // Emit br table for upper half:
    GenerateBrTable(decoder, tmp, value, split, max, table_iterator,
                    br_targets);
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate<validate>& imm,
               const Value& key) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    BranchTableIterator<validate> table_iterator(decoder, imm);
    std::map<uint32_t, MovableLabel> br_targets;

    if (imm.table_count > 0) {
      LiftoffRegister tmp = __ GetUnusedRegister(kGpReg, pinned);
      __ LoadConstant(tmp, WasmValue(uint32_t{imm.table_count}));
      Label case_default;
      __ emit_cond_jump(kUnsignedGreaterEqual, &case_default, kWasmI32,
                        value.gp(), tmp.gp());

      GenerateBrTable(decoder, tmp, value, 0, imm.table_count, table_iterator,
                      br_targets);

      __ bind(&case_default);
    }

    // Generate the default case.
    GenerateBrCase(decoder, table_iterator.next(), br_targets);
    DCHECK(!table_iterator.has_next());
  }

  void Else(FullDecoder* decoder, Control* if_block) {
    if (if_block->reachable()) __ emit_jump(if_block->label.get());
    __ bind(if_block->else_state->label.get());
    __ cache_state()->Steal(if_block->else_state->state);
  }

  Label* AddOutOfLineTrap(WasmCodePosition position,
                          WasmCode::RuntimeStubId stub, uint32_t pc = 0) {
    DCHECK(!FLAG_wasm_no_bounds_checks);
    // The pc is needed for memory OOB trap with trap handler enabled. Other
    // callers should not even compute it.
    DCHECK_EQ(pc != 0, stub == WasmCode::kThrowWasmTrapMemOutOfBounds &&
                           env_->use_trap_handler);

    out_of_line_code_.push_back(OutOfLineCode::Trap(stub, position, pc));
    return out_of_line_code_.back().label.get();
  }

  // Returns true if the memory access is statically known to be out of bounds
  // (a jump to the trap was generated then); return false otherwise.
  bool BoundsCheckMem(FullDecoder* decoder, uint32_t access_size,
                      uint32_t offset, Register index, LiftoffRegList pinned) {
    const bool statically_oob = access_size > env_->max_memory_size ||
                                offset > env_->max_memory_size - access_size;

    if (!statically_oob &&
        (FLAG_wasm_no_bounds_checks || env_->use_trap_handler)) {
      return false;
    }

    // TODO(wasm): This adds protected instruction information for the jump
    // instruction we are about to generate. It would be better to just not add
    // protected instruction info when the pc is 0.
    Label* trap_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapMemOutOfBounds,
        env_->use_trap_handler ? __ pc_offset() : 0);

    if (statically_oob) {
      __ emit_jump(trap_label);
      Control* current_block = decoder->control_at(0);
      if (current_block->reachable()) {
        current_block->reachability = kSpecOnlyReachable;
      }
      return true;
    }

    DCHECK(!env_->use_trap_handler);
    DCHECK(!FLAG_wasm_no_bounds_checks);

    uint64_t end_offset = uint64_t{offset} + access_size - 1u;

    // If the end offset is larger than the smallest memory, dynamically check
    // the end offset against the actual memory size, which is not known at
    // compile time. Otherwise, only one check is required (see below).
    LiftoffRegister end_offset_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister mem_size = __ GetUnusedRegister(kGpReg, pinned);
    LOAD_INSTANCE_FIELD(mem_size, MemorySize, kPointerSize);

    if (kPointerSize == 8) {
      __ LoadConstant(end_offset_reg, WasmValue(end_offset));
    } else {
      __ LoadConstant(end_offset_reg,
                      WasmValue(static_cast<uint32_t>(end_offset)));
    }

    if (end_offset >= env_->min_memory_size) {
      __ emit_cond_jump(kUnsignedGreaterEqual, trap_label,
                        LiftoffAssembler::kWasmIntPtr, end_offset_reg.gp(),
                        mem_size.gp());
    }

    // Just reuse the end_offset register for computing the effective size.
    LiftoffRegister effective_size_reg = end_offset_reg;
    __ emit_ptrsize_sub(effective_size_reg.gp(), mem_size.gp(),
                        end_offset_reg.gp());

    __ emit_i32_to_intptr(index, index);

    __ emit_cond_jump(kUnsignedGreaterEqual, trap_label,
                      LiftoffAssembler::kWasmIntPtr, index,
                      effective_size_reg.gp());
    return false;
  }

  void TraceMemoryOperation(bool is_store, MachineRepresentation rep,
                            Register index, uint32_t offset,
                            WasmCodePosition position) {
    // Before making the runtime call, spill all cache registers.
    __ SpillAllRegisters();

    LiftoffRegList pinned = LiftoffRegList::ForRegs(index);
    // Get one register for computing the address (offset + index).
    LiftoffRegister address = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Compute offset+index in address.
    __ LoadConstant(address, WasmValue(offset));
    __ emit_i32_add(address.gp(), address.gp(), index);

    // Get a register to hold the stack slot for MemoryTracingInfo.
    LiftoffRegister info = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Allocate stack slot for MemoryTracingInfo.
    __ AllocateStackSlot(info.gp(), sizeof(MemoryTracingInfo));

    // Now store all information into the MemoryTracingInfo struct.
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, address), address,
             StoreType::kI32Store, pinned);
    __ LoadConstant(address, WasmValue(is_store ? 1 : 0));
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, is_store), address,
             StoreType::kI32Store8, pinned);
    __ LoadConstant(address, WasmValue(static_cast<int>(rep)));
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, mem_rep), address,
             StoreType::kI32Store8, pinned);

    source_position_table_builder_.AddPosition(__ pc_offset(),
                                               SourcePosition(position), false);

    Register args[] = {info.gp()};
    GenerateRuntimeCall(Runtime::kWasmTraceMemory, arraysize(args), args);
    __ DeallocateStackSlot(sizeof(MemoryTracingInfo));
  }

  void GenerateRuntimeCall(Runtime::FunctionId runtime_function, int num_args,
                           Register* args) {
    auto call_descriptor = compiler::Linkage::GetRuntimeCallDescriptor(
        compilation_zone_, runtime_function, num_args,
        compiler::Operator::kNoProperties, compiler::CallDescriptor::kNoFlags);
    // Currently, only one argument is supported. More arguments require some
    // caution for the parallel register moves (reuse StackTransferRecipe).
    DCHECK_EQ(1, num_args);
    constexpr size_t kInputShift = 1;  // Input 0 is the call target.
    compiler::LinkageLocation param_loc =
        call_descriptor->GetInputLocation(kInputShift);
    if (param_loc.IsRegister()) {
      Register reg = Register::from_code(param_loc.AsRegister());
      __ Move(LiftoffRegister(reg), LiftoffRegister(args[0]),
              LiftoffAssembler::kWasmIntPtr);
    } else {
      DCHECK(param_loc.IsCallerFrameSlot());
      LiftoffStackSlots stack_slots(&asm_);
      stack_slots.Add(LiftoffAssembler::VarState(LiftoffAssembler::kWasmIntPtr,
                                                 LiftoffRegister(args[0])));
      stack_slots.Construct();
    }

    // Set context to zero (Smi::kZero) for the runtime call.
    __ TurboAssembler::Move(kContextRegister, Smi::kZero);
    LiftoffRegister centry(kJavaScriptCallCodeStartRegister);
    LOAD_INSTANCE_FIELD(centry, CEntryStub, kPointerSize);
    __ CallRuntimeWithCEntry(runtime_function, centry.gp());
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);
  }

  LiftoffRegister AddMemoryMasking(LiftoffRegister index, uint32_t* offset,
                                   LiftoffRegList& pinned) {
    if (!FLAG_untrusted_code_mitigations || env_->use_trap_handler) {
      return index;
    }
    DEBUG_CODE_COMMENT("Mask memory index");
    // Make sure that we can overwrite {index}.
    if (__ cache_state()->is_used(index)) {
      LiftoffRegister old_index = index;
      pinned.clear(old_index);
      index = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      if (index != old_index) __ Move(index.gp(), old_index.gp(), kWasmI32);
    }
    LiftoffRegister tmp = __ GetUnusedRegister(kGpReg, pinned);
    __ LoadConstant(tmp, WasmValue(*offset));
    __ emit_ptrsize_add(index.gp(), index.gp(), tmp.gp());
    LOAD_INSTANCE_FIELD(tmp, MemoryMask, kPointerSize);
    __ emit_ptrsize_and(index.gp(), index.gp(), tmp.gp());
    *offset = 0;
    return index;
  }

  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate<validate>& imm,
               const Value& index_val, Value* result) {
    ValueType value_type = type.value_type();
    if (!CheckSupportedType(decoder, kTypes_ilfd, value_type, "load")) return;
    LiftoffRegList pinned;
    LiftoffRegister index = pinned.set(__ PopToRegister());
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index.gp(), pinned)) {
      return;
    }
    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, pinned);
    DEBUG_CODE_COMMENT("Load from memory");
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kPointerSize);
    RegClass rc = reg_class_for(value_type);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    uint32_t protected_load_pc = 0;
    __ Load(value, addr.gp(), index.gp(), offset, type, pinned,
            &protected_load_pc, true);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(),
                       WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_load_pc);
    }
    __ PushRegister(value_type, value);

    if (FLAG_trace_wasm_memory) {
      TraceMemoryOperation(false, type.mem_type().representation(), index.gp(),
                           offset, decoder->position());
    }
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate<validate>& imm,
                const Value& index_val, const Value& value_val) {
    ValueType value_type = type.value_type();
    if (!CheckSupportedType(decoder, kTypes_ilfd, value_type, "store")) return;
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    LiftoffRegister index = pinned.set(__ PopToRegister(pinned));
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index.gp(), pinned)) {
      return;
    }
    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, pinned);
    DEBUG_CODE_COMMENT("Store to memory");
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kPointerSize);
    uint32_t protected_store_pc = 0;
    __ Store(addr.gp(), index.gp(), offset, value, type, pinned,
             &protected_store_pc, true);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(),
                       WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_store_pc);
    }
    if (FLAG_trace_wasm_memory) {
      TraceMemoryOperation(true, type.mem_rep(), index.gp(), offset,
                           decoder->position());
    }
  }

  void CurrentMemoryPages(FullDecoder* decoder, Value* result) {
    LiftoffRegister mem_size = __ GetUnusedRegister(kGpReg);
    LOAD_INSTANCE_FIELD(mem_size, MemorySize, kPointerSize);
    __ emit_ptrsize_shr(mem_size.gp(), mem_size.gp(), kWasmPageSizeLog2);
    __ PushRegister(kWasmI32, mem_size);
  }

  void GrowMemory(FullDecoder* decoder, const Value& value, Value* result_val) {
    // Pop the input, then spill all cache registers to make the runtime call.
    LiftoffRegList pinned;
    LiftoffRegister input = pinned.set(__ PopToRegister());
    __ SpillAllRegisters();

    constexpr Register kGpReturnReg = kGpReturnRegisters[0];
    static_assert(kLiftoffAssemblerGpCacheRegs & Register::bit<kGpReturnReg>(),
                  "first return register is a cache register (needs more "
                  "complex code here otherwise)");
    LiftoffRegister result = pinned.set(LiftoffRegister(kGpReturnReg));

    WasmGrowMemoryDescriptor descriptor;
    DCHECK_EQ(0, descriptor.GetStackParameterCount());
    DCHECK_EQ(1, descriptor.GetRegisterParameterCount());
    DCHECK_EQ(ValueTypes::MachineTypeFor(kWasmI32),
              descriptor.GetParameterType(0));

    Register param_reg = descriptor.GetRegisterParameter(0);
    if (input.gp() != param_reg) __ Move(param_reg, input.gp(), kWasmI32);

    __ CallRuntimeStub(WasmCode::kWasmGrowMemory);
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);

    if (kReturnRegister0 != result.gp()) {
      __ Move(result.gp(), kReturnRegister0, kWasmI32);
    }

    __ PushRegister(kWasmI32, result);
  }

  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value returns[]) {
    if (imm.sig->return_count() > 1)
      return unsupported(decoder, "multi-return");
    if (imm.sig->return_count() == 1 &&
        !CheckSupportedType(decoder, kTypes_ilfd, imm.sig->GetReturn(0),
                            "return"))
      return;

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, imm.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    if (imm.index < env_->module->num_imported_functions) {
      // A direct call to an imported function.
      LiftoffRegList pinned;
      LiftoffRegister tmp = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      LiftoffRegister target = pinned.set(__ GetUnusedRegister(kGpReg, pinned));

      LiftoffRegister imported_targets = tmp;
      LOAD_INSTANCE_FIELD(imported_targets, ImportedFunctionTargets,
                          kPointerSize);
      __ Load(target, imported_targets.gp(), no_reg,
              imm.index * sizeof(Address), kPointerLoadType, pinned);

      LiftoffRegister imported_instances = tmp;
      LOAD_INSTANCE_FIELD(imported_instances, ImportedFunctionInstances,
                          kPointerSize);
      LiftoffRegister target_instance = tmp;
      __ Load(target_instance, imported_instances.gp(), no_reg,
              ObjectAccess::ElementOffsetInTaggedFixedArray(imm.index),
              kPointerLoadType, pinned);

      LiftoffRegister* explicit_instance = &target_instance;
      Register target_reg = target.gp();
      __ PrepareCall(imm.sig, call_descriptor, &target_reg, explicit_instance);
      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), false);

      __ CallIndirect(imm.sig, call_descriptor, target_reg);

      safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kSimple, 0,
                                               Safepoint::kNoLazyDeopt);

      __ FinishCall(imm.sig, call_descriptor);
    } else {
      // A direct call within this module just gets the current instance.
      __ PrepareCall(imm.sig, call_descriptor);

      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), false);

      // Just encode the function index. This will be patched at instantiation.
      Address addr = static_cast<Address>(imm.index);
      __ CallNativeWasmCode(addr);

      safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kSimple, 0,
                                               Safepoint::kNoLazyDeopt);

      __ FinishCall(imm.sig, call_descriptor);
    }
  }

  void CallIndirect(FullDecoder* decoder, const Value& index_val,
                    const CallIndirectImmediate<validate>& imm,
                    const Value args[], Value returns[]) {
    if (imm.sig->return_count() > 1) {
      return unsupported(decoder, "multi-return");
    }
    if (imm.sig->return_count() == 1 &&
        !CheckSupportedType(decoder, kTypes_ilfd, imm.sig->GetReturn(0),
                            "return")) {
      return;
    }

    // Pop the index.
    LiftoffRegister index = __ PopToRegister();
    // If that register is still being used after popping, we move it to another
    // register, because we want to modify that register.
    if (__ cache_state()->is_used(index)) {
      LiftoffRegister new_index =
          __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(index));
      __ Move(new_index, index, kWasmI32);
      index = new_index;
    }

    LiftoffRegList pinned = LiftoffRegList::ForRegs(index);
    // Get three temporary registers.
    LiftoffRegister table = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister tmp_const =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister scratch = pinned.set(__ GetUnusedRegister(kGpReg, pinned));

    // Bounds check against the table size.
    Label* invalid_func_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapFuncInvalid);

    uint32_t canonical_sig_num = env_->module->signature_ids[imm.sig_index];
    DCHECK_GE(canonical_sig_num, 0);
    DCHECK_GE(kMaxInt, canonical_sig_num);

    // Compare against table size stored in
    // {instance->indirect_function_table_size}.
    LOAD_INSTANCE_FIELD(tmp_const, IndirectFunctionTableSize, kUInt32Size);
    __ emit_cond_jump(kUnsignedGreaterEqual, invalid_func_label, kWasmI32,
                      index.gp(), tmp_const.gp());

    // Mask the index to prevent SSCA.
    if (FLAG_untrusted_code_mitigations) {
      DEBUG_CODE_COMMENT("Mask indirect call index");
      // mask = ((index - size) & ~index) >> 31
      // Reuse allocated registers; note: size is still stored in {tmp_const}.
      LiftoffRegister diff = table;
      LiftoffRegister neg_index = tmp_const;
      LiftoffRegister mask = scratch;
      // 1) diff = index - size
      __ emit_i32_sub(diff.gp(), index.gp(), tmp_const.gp());
      // 2) neg_index = ~index
      __ LoadConstant(neg_index, WasmValue(int32_t{-1}));
      __ emit_i32_xor(neg_index.gp(), neg_index.gp(), index.gp());
      // 3) mask = diff & neg_index
      __ emit_i32_and(mask.gp(), diff.gp(), neg_index.gp());
      // 4) mask = mask >> 31
      __ LoadConstant(tmp_const, WasmValue(int32_t{31}));
      __ emit_i32_sar(mask.gp(), mask.gp(), tmp_const.gp(), pinned);

      // Apply mask.
      __ emit_i32_and(index.gp(), index.gp(), mask.gp());
    }

    DEBUG_CODE_COMMENT("Check indirect call signature");
    // Load the signature from {instance->ift_sig_ids[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableSigIds, kPointerSize);
    __ LoadConstant(tmp_const,
                    WasmValue(static_cast<uint32_t>(sizeof(uint32_t))));
    // TODO(wasm): use a emit_i32_shli() instead of a multiply.
    // (currently cannot use shl on ia32/x64 because it clobbers %rcx).
    __ emit_i32_mul(index.gp(), index.gp(), tmp_const.gp());
    __ Load(scratch, table.gp(), index.gp(), 0, LoadType::kI32Load, pinned);

    // Compare against expected signature.
    __ LoadConstant(tmp_const, WasmValue(canonical_sig_num));

    Label* sig_mismatch_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapFuncSigMismatch);
    __ emit_cond_jump(kUnequal, sig_mismatch_label,
                      LiftoffAssembler::kWasmIntPtr, scratch.gp(),
                      tmp_const.gp());

    DEBUG_CODE_COMMENT("Execute indirect call");
    if (kPointerSize == 8) {
      // {index} has already been multiplied by 4. Multiply by another 2.
      __ LoadConstant(tmp_const, WasmValue(2));
      __ emit_i32_mul(index.gp(), index.gp(), tmp_const.gp());
    }

    // Load the target from {instance->ift_targets[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableTargets, kPointerSize);
    __ Load(scratch, table.gp(), index.gp(), 0, kPointerLoadType, pinned);

    // Load the instance from {instance->ift_instances[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableInstances, kPointerSize);
    __ Load(tmp_const, table.gp(), index.gp(),
            ObjectAccess::ElementOffsetInTaggedFixedArray(0), kPointerLoadType,
            pinned);
    LiftoffRegister* explicit_instance = &tmp_const;

    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, imm.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    Register target = scratch.gp();
    __ PrepareCall(imm.sig, call_descriptor, &target, explicit_instance);
    __ CallIndirect(imm.sig, call_descriptor, target);

    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);

    __ FinishCall(imm.sig, call_descriptor);
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
              Value* result) {
    unsupported(decoder, "simd");
  }
  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate<validate>& imm,
                  const Vector<Value> inputs, Value* result) {
    unsupported(decoder, "simd");
  }
  void SimdShiftOp(FullDecoder* decoder, WasmOpcode opcode,
                   const SimdShiftImmediate<validate>& imm, const Value& input,
                   Value* result) {
    unsupported(decoder, "simd");
  }
  void Simd8x16ShuffleOp(FullDecoder* decoder,
                         const Simd8x16ShuffleImmediate<validate>& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    unsupported(decoder, "simd");
  }
  void Throw(FullDecoder* decoder, const ExceptionIndexImmediate<validate>&,
             Control* block, const Vector<Value>& args) {
    unsupported(decoder, "throw");
  }
  void CatchException(FullDecoder* decoder,
                      const ExceptionIndexImmediate<validate>& imm,
                      Control* block, Vector<Value> caught_values) {
    unsupported(decoder, "catch");
  }
  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
                const MemoryAccessImmediate<validate>& imm, Value* result) {
    unsupported(decoder, "atomicop");
  }

 private:
  LiftoffAssembler asm_;
  compiler::CallDescriptor* const descriptor_;
  ModuleEnv* const env_;
  bool ok_ = true;
  std::vector<OutOfLineCode> out_of_line_code_;
  SourcePositionTableBuilder source_position_table_builder_;
  std::vector<trap_handler::ProtectedInstructionData> protected_instructions_;
  // Zone used to store information during compilation. The result will be
  // stored independently, such that this zone can die together with the
  // LiftoffCompiler after compilation.
  Zone* compilation_zone_;
  SafepointTableBuilder safepoint_table_builder_;
  // The pc offset of the instructions to reserve the stack frame. Needed to
  // patch the actually needed stack size in the end.
  uint32_t pc_offset_stack_frame_construction_ = 0;

  void TraceCacheState(FullDecoder* decoder) const {
#ifdef DEBUG
    if (!FLAG_trace_liftoff || !FLAG_trace_wasm_decoder) return;
    StdoutStream os;
    for (int control_depth = decoder->control_depth() - 1; control_depth >= -1;
         --control_depth) {
      auto* cache_state =
          control_depth == -1 ? __ cache_state()
                              : &decoder->control_at(control_depth)
                                     ->label_state;
      os << PrintCollection(cache_state->stack_state);
      if (control_depth != -1) PrintF("; ");
    }
    os << "\n";
#endif
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(LiftoffCompiler);
};

}  // namespace

bool LiftoffCompilationUnit::ExecuteCompilation(WasmFeatures* detected) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "ExecuteLiftoffCompilation");
  base::ElapsedTimer compile_timer;
  if (FLAG_trace_wasm_decode_time) {
    compile_timer.Start();
  }

  Zone zone(wasm_unit_->wasm_engine_->allocator(), "LiftoffCompilationZone");
  const WasmModule* module =
      wasm_unit_->env_ ? wasm_unit_->env_->module : nullptr;
  auto call_descriptor =
      compiler::GetWasmCallDescriptor(&zone, wasm_unit_->func_body_.sig);
  base::Optional<TimedHistogramScope> liftoff_compile_time_scope(
      base::in_place, wasm_unit_->counters_->liftoff_compile_time());
  WasmFullDecoder<Decoder::kValidate, LiftoffCompiler> decoder(
      &zone, module, wasm_unit_->native_module_->enabled_features(), detected,
      wasm_unit_->func_body_, call_descriptor, wasm_unit_->env_, &zone);
  decoder.Decode();
  liftoff_compile_time_scope.reset();
  LiftoffCompiler* compiler = &decoder.interface();
  if (decoder.failed()) return false;  // validation error
  if (!compiler->ok()) {
    // Liftoff compilation failed.
    wasm_unit_->counters_->liftoff_unsupported_functions()->Increment();
    return false;
  }

  wasm_unit_->counters_->liftoff_compiled_functions()->Increment();

  if (FLAG_trace_wasm_decode_time) {
    double compile_ms = compile_timer.Elapsed().InMillisecondsF();
    PrintF(
        "wasm-compilation liftoff phase 1 ok: %u bytes, %0.3f ms decode and "
        "compile\n",
        static_cast<unsigned>(wasm_unit_->func_body_.end -
                              wasm_unit_->func_body_.start),
        compile_ms);
  }

  CodeDesc desc;
  compiler->GetCode(&desc);
  OwnedVector<byte> source_positions = compiler->GetSourcePositionTable();
  OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions =
      compiler->GetProtectedInstructions();
  uint32_t frame_slot_count = compiler->GetTotalFrameSlotCount();
  int safepoint_table_offset = compiler->GetSafepointTableOffset();

  code_ = wasm_unit_->native_module_->AddCode(
      wasm_unit_->func_index_, desc, frame_slot_count, safepoint_table_offset,
      0, std::move(protected_instructions), std::move(source_positions),
      WasmCode::kLiftoff);
  wasm_unit_->native_module_->PublishCode(code_);

  return true;
}

WasmCode* LiftoffCompilationUnit::FinishCompilation(ErrorThrower*) {
  return code_;
}

#undef __
#undef TRACE
#undef WASM_INSTANCE_OBJECT_OFFSET
#undef WASM_INSTANCE_OBJECT_SIZE
#undef LOAD_INSTANCE_FIELD
#undef DEBUG_CODE_COMMENT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
