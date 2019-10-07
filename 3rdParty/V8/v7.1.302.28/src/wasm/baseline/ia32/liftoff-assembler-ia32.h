// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_
#define V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

#define REQUIRE_CPU_FEATURE(name, ...)   \
  if (!CpuFeatures::IsSupported(name)) { \
    bailout("no " #name);                \
    return __VA_ARGS__;                  \
  }                                      \
  CpuFeatureScope feature(this, name);

namespace liftoff {

// ebp-4 holds the stack marker, ebp-8 is the instance parameter, first stack
// slot is located at ebp-16.
constexpr int32_t kConstantStackSpace = 8;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline Operand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return Operand(ebp, -kFirstStackSlotOffset - offset);
}

inline Operand GetHalfStackSlot(uint32_t half_index) {
  int32_t offset = half_index * (LiftoffAssembler::kStackSlotSize / 2);
  return Operand(ebp, -kFirstStackSlotOffset - offset);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetInstanceOperand() { return Operand(ebp, -8); }

static constexpr LiftoffRegList kByteRegs =
    LiftoffRegList::FromBits<Register::ListOf<eax, ecx, edx, ebx>()>();
static_assert(kByteRegs.GetNumRegsSet() == 4, "should have four byte regs");
static_assert((kByteRegs & kGpCacheRegList) == kByteRegs,
              "kByteRegs only contains gp cache registers");

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Register base,
                 int32_t offset, ValueType type) {
  Operand src(base, offset);
  switch (type) {
    case kWasmI32:
      assm->mov(dst.gp(), src);
      break;
    case kWasmI64:
      assm->mov(dst.low_gp(), src);
      assm->mov(dst.high_gp(), Operand(base, offset + 4));
      break;
    case kWasmF32:
      assm->movss(dst.fp(), src);
      break;
    case kWasmF64:
      assm->movsd(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Register base, int32_t offset,
                  LiftoffRegister src, ValueType type) {
  Operand dst(base, offset);
  switch (type) {
    case kWasmI32:
      assm->mov(dst, src.gp());
      break;
    case kWasmI64:
      assm->mov(dst, src.low_gp());
      assm->mov(Operand(base, offset + 4), src.high_gp());
      break;
    case kWasmF32:
      assm->movss(dst, src.fp());
      break;
    case kWasmF64:
      assm->movsd(dst, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->push(reg.gp());
      break;
    case kWasmI64:
      assm->push(reg.high_gp());
      assm->push(reg.low_gp());
      break;
    case kWasmF32:
      assm->sub(esp, Immediate(sizeof(float)));
      assm->movss(Operand(esp, 0), reg.fp());
      break;
    case kWasmF64:
      assm->sub(esp, Immediate(sizeof(double)));
      assm->movsd(Operand(esp, 0), reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

template <typename... Regs>
inline void SpillRegisters(LiftoffAssembler* assm, Regs... regs) {
  for (LiftoffRegister r : {LiftoffRegister(regs)...}) {
    if (assm->cache_state()->is_used(r)) assm->SpillRegister(r);
  }
}

inline void SignExtendI32ToI64(Assembler* assm, LiftoffRegister reg) {
  assm->mov(reg.high_gp(), reg.low_gp());
  assm->sar(reg.high_gp(), 31);
}

constexpr DoubleRegister kScratchDoubleReg = xmm7;

constexpr int kSubSpSize = 6;  // 6 bytes for "sub esp, <imm32>"

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  sub_sp_32(0);
  DCHECK_EQ(liftoff::kSubSpSize, pc_offset() - offset);
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset,
                                              uint32_t stack_slots) {
  uint32_t bytes = liftoff::kConstantStackSpace + kStackSlotSize * stack_slots;
  DCHECK_LE(bytes, kMaxInt);
  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 64;
  Assembler patching_assembler(AssemblerOptions{}, buffer_ + offset,
                               kAvailableSpace);
#if V8_OS_WIN
  constexpr int kPageSize = 4 * 1024;
  if (bytes > kPageSize) {
    // Generate OOL code (at the end of the function, where the current
    // assembler is pointing) to do the explicit stack limit check (see
    // https://docs.microsoft.com/en-us/previous-versions/visualstudio/
    // visual-studio-6.0/aa227153(v=vs.60)).
    // At the function start, emit a jump to that OOL code (from {offset} to
    // {pc_offset()}).
    int ool_offset = pc_offset() - offset;
    patching_assembler.jmp_rel(ool_offset);
    DCHECK_GE(liftoff::kSubSpSize, patching_assembler.pc_offset());
    patching_assembler.Nop(liftoff::kSubSpSize -
                           patching_assembler.pc_offset());

    // Now generate the OOL code.
    // Use {edi} as scratch register; it is not being used as parameter
    // register (see wasm-linkage.h).
    mov(edi, bytes);
    AllocateStackFrame(edi);
    // Jump back to the start of the function (from {pc_offset()} to {offset +
    // kSubSpSize}).
    int func_start_offset = offset + liftoff::kSubSpSize - pc_offset();
    jmp_rel(func_start_offset);
    return;
  }
#endif
  patching_assembler.sub_sp_32(bytes);
  DCHECK_EQ(liftoff::kSubSpSize, patching_assembler.pc_offset());
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type()) {
    case kWasmI32:
      TurboAssembler::Move(reg.gp(), Immediate(value.to_i32(), rmode));
      break;
    case kWasmI64: {
      DCHECK(RelocInfo::IsNone(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::Move(reg.low_gp(), Immediate(low_word));
      TurboAssembler::Move(reg.high_gp(), Immediate(high_word));
      break;
    }
    case kWasmF32:
      TurboAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    case kWasmF64:
      TurboAssembler::Move(reg.fp(), value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  DCHECK_LE(offset, kMaxInt);
  mov(dst, liftoff::GetInstanceOperand());
  DCHECK_EQ(4, size);
  mov(dst, Operand(dst, offset));
}

void LiftoffAssembler::SpillInstance(Register instance) {
  mov(liftoff::GetInstanceOperand(), instance);
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  mov(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem) {
  DCHECK_EQ(type.value_type() == kWasmI64, dst.is_pair());
  // Wasm memory is limited to a size <2GB, so all offsets can be encoded as
  // immediate value (in 31 bits, interpreted as signed value).
  // If the offset is bigger, we always trap and this code is not reached.
  // Note: We shouldn't have memories larger than 2GiB on 32-bit, but if we
  // did, we encode {offset_im} as signed, and it will simply wrap around.
  Operand src_op = offset_reg == no_reg
                       ? Operand(src_addr, bit_cast<int32_t>(offset_imm))
                       : Operand(src_addr, offset_reg, times_1, offset_imm);
  if (protected_load_pc) *protected_load_pc = pc_offset();

  switch (type.value()) {
    case LoadType::kI32Load8U:
      movzx_b(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
      movsx_b(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8U:
      movzx_b(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load8S:
      movsx_b(dst.low_gp(), src_op);
      liftoff::SignExtendI32ToI64(this, dst);
      break;
    case LoadType::kI32Load16U:
      movzx_w(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
      movsx_w(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16U:
      movzx_w(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load16S:
      movsx_w(dst.low_gp(), src_op);
      liftoff::SignExtendI32ToI64(this, dst);
      break;
    case LoadType::kI32Load:
      mov(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32U:
      mov(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load32S:
      mov(dst.low_gp(), src_op);
      liftoff::SignExtendI32ToI64(this, dst);
      break;
    case LoadType::kI64Load: {
      // Compute the operand for the load of the upper half.
      Operand upper_src_op =
          offset_reg == no_reg
              ? Operand(src_addr, bit_cast<int32_t>(offset_imm + 4))
              : Operand(src_addr, offset_reg, times_1, offset_imm + 4);
      // The high word has to be mov'ed first, such that this is the protected
      // instruction. The mov of the low word cannot segfault.
      mov(dst.high_gp(), upper_src_op);
      mov(dst.low_gp(), src_op);
      break;
    }
    case LoadType::kF32Load:
      movss(dst.fp(), src_op);
      break;
    case LoadType::kF64Load:
      movsd(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  DCHECK_EQ(type.value_type() == kWasmI64, src.is_pair());
  // Wasm memory is limited to a size <2GB, so all offsets can be encoded as
  // immediate value (in 31 bits, interpreted as signed value).
  // If the offset is bigger, we always trap and this code is not reached.
  Operand dst_op = offset_reg == no_reg
                       ? Operand(dst_addr, bit_cast<int32_t>(offset_imm))
                       : Operand(dst_addr, offset_reg, times_1, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();

  switch (type.value()) {
    case StoreType::kI64Store8:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store8:
      // Only the lower 4 registers can be addressed as 8-bit registers.
      if (src.gp().is_byte_register()) {
        mov_b(dst_op, src.gp());
      } else {
        Register byte_src = GetUnusedRegister(liftoff::kByteRegs, pinned).gp();
        mov(byte_src, src.gp());
        mov_b(dst_op, byte_src);
      }
      break;
    case StoreType::kI64Store16:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store16:
      mov_w(dst_op, src.gp());
      break;
    case StoreType::kI64Store32:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store:
      mov(dst_op, src.gp());
      break;
    case StoreType::kI64Store: {
      // Compute the operand for the store of the upper half.
      Operand upper_dst_op =
          offset_reg == no_reg
              ? Operand(dst_addr, bit_cast<int32_t>(offset_imm + 4))
              : Operand(dst_addr, offset_reg, times_1, offset_imm + 4);
      // The high word has to be mov'ed first, such that this is the protected
      // instruction. The mov of the low word cannot segfault.
      mov(upper_dst_op, src.high_gp());
      mov(dst_op, src.low_gp());
      break;
    }
    case StoreType::kF32Store:
      movss(dst_op, src.fp());
      break;
    case StoreType::kF64Store:
      movsd(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  liftoff::Load(this, dst, ebp, kPointerSize * (caller_slot_idx + 1), type);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  if (cache_state_.has_unused_register(kGpReg)) {
    LiftoffRegister reg = GetUnusedRegister(kGpReg);
    Fill(reg, src_index, type);
    Spill(dst_index, reg, type);
  } else {
    push(liftoff::GetStackSlot(src_index));
    pop(liftoff::GetStackSlot(dst_index));
  }
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  DCHECK_EQ(kWasmI32, type);
  mov(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmF32) {
    movss(dst, src);
  } else {
    DCHECK_EQ(kWasmF64, type);
    movsd(dst, src);
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      mov(dst, reg.gp());
      break;
    case kWasmI64:
      mov(dst, reg.low_gp());
      mov(liftoff::GetHalfStackSlot(2 * index - 1), reg.high_gp());
      break;
    case kWasmF32:
      movss(dst, reg.fp());
      break;
    case kWasmF64:
      movsd(dst, reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32:
      mov(dst, Immediate(value.to_i32()));
      break;
    case kWasmI64: {
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      mov(dst, Immediate(low_word));
      mov(liftoff::GetHalfStackSlot(2 * index - 1), Immediate(high_word));
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  Operand src = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      mov(reg.gp(), src);
      break;
    case kWasmI64:
      mov(reg.low_gp(), src);
      mov(reg.high_gp(), liftoff::GetHalfStackSlot(2 * index - 1));
      break;
    case kWasmF32:
      movss(reg.fp(), src);
      break;
    case kWasmF64:
      movsd(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, uint32_t half_index) {
  mov(reg, liftoff::GetHalfStackSlot(half_index));
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, Register rhs) {
  if (lhs != dst) {
    lea(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    add(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_sub(Register dst, Register lhs, Register rhs) {
  if (dst == rhs) {
    neg(dst);
    add(dst, lhs);
  } else {
    if (dst != lhs) mov(dst, lhs);
    sub(dst, rhs);
  }
}

namespace liftoff {
template <void (Assembler::*op)(Register, Register)>
void EmitCommutativeBinOp(LiftoffAssembler* assm, Register dst, Register lhs,
                          Register rhs) {
  if (dst == rhs) {
    (assm->*op)(dst, lhs);
  } else {
    if (dst != lhs) assm->mov(dst, lhs);
    (assm->*op)(dst, rhs);
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imul>(this, dst, lhs, rhs);
}

namespace liftoff {
enum class DivOrRem : uint8_t { kDiv, kRem };
template <bool is_signed, DivOrRem div_or_rem>
void EmitInt32DivOrRem(LiftoffAssembler* assm, Register dst, Register lhs,
                       Register rhs, Label* trap_div_by_zero,
                       Label* trap_div_unrepresentable) {
  constexpr bool needs_unrepresentable_check =
      is_signed && div_or_rem == DivOrRem::kDiv;
  constexpr bool special_case_minus_1 =
      is_signed && div_or_rem == DivOrRem::kRem;
  DCHECK_EQ(needs_unrepresentable_check, trap_div_unrepresentable != nullptr);

  // For division, the lhs is always taken from {edx:eax}. Thus, make sure that
  // these registers are unused. If {rhs} is stored in one of them, move it to
  // another temporary register.
  // Do all this before any branch, such that the code is executed
  // unconditionally, as the cache state will also be modified unconditionally.
  liftoff::SpillRegisters(assm, eax, edx);
  if (rhs == eax || rhs == edx) {
    LiftoffRegList unavailable = LiftoffRegList::ForRegs(eax, edx, lhs);
    Register tmp = assm->GetUnusedRegister(kGpReg, unavailable).gp();
    assm->mov(tmp, rhs);
    rhs = tmp;
  }

  // Check for division by zero.
  assm->test(rhs, rhs);
  assm->j(zero, trap_div_by_zero);

  Label done;
  if (needs_unrepresentable_check) {
    // Check for {kMinInt / -1}. This is unrepresentable.
    Label do_div;
    assm->cmp(rhs, -1);
    assm->j(not_equal, &do_div);
    assm->cmp(lhs, kMinInt);
    assm->j(equal, trap_div_unrepresentable);
    assm->bind(&do_div);
  } else if (special_case_minus_1) {
    // {lhs % -1} is always 0 (needs to be special cased because {kMinInt / -1}
    // cannot be computed).
    Label do_rem;
    assm->cmp(rhs, -1);
    assm->j(not_equal, &do_rem);
    assm->xor_(dst, dst);
    assm->jmp(&done);
    assm->bind(&do_rem);
  }

  // Now move {lhs} into {eax}, then zero-extend or sign-extend into {edx}, then
  // do the division.
  if (lhs != eax) assm->mov(eax, lhs);
  if (is_signed) {
    assm->cdq();
    assm->idiv(rhs);
  } else {
    assm->xor_(edx, edx);
    assm->div(rhs);
  }

  // Move back the result (in {eax} or {edx}) into the {dst} register.
  constexpr Register kResultReg = div_or_rem == DivOrRem::kDiv ? eax : edx;
  if (dst != kResultReg) assm->mov(dst, kResultReg);
  if (special_case_minus_1) assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  liftoff::EmitInt32DivOrRem<true, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, trap_div_unrepresentable);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitInt32DivOrRem<false, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitInt32DivOrRem<true, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitInt32DivOrRem<false, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::and_>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::or_>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xor_>(this, dst, lhs, rhs);
}

namespace liftoff {
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register src, Register amount,
                               void (Assembler::*emit_shift)(Register),
                               LiftoffRegList pinned) {
  pinned.set(dst);
  pinned.set(src);
  pinned.set(amount);
  // If dst is ecx, compute into a tmp register first, then move to ecx.
  if (dst == ecx) {
    Register tmp = assm->GetUnusedRegister(kGpReg, pinned).gp();
    assm->mov(tmp, src);
    if (amount != ecx) assm->mov(ecx, amount);
    (assm->*emit_shift)(tmp);
    assm->mov(ecx, tmp);
    return;
  }

  // Move amount into ecx. If ecx is in use, move its content to a tmp register
  // first. If src is ecx, src is now the tmp register.
  Register tmp_reg = no_reg;
  if (amount != ecx) {
    if (assm->cache_state()->is_used(LiftoffRegister(ecx)) ||
        pinned.has(LiftoffRegister(ecx))) {
      tmp_reg = assm->GetUnusedRegister(kGpReg, pinned).gp();
      assm->mov(tmp_reg, ecx);
      if (src == ecx) src = tmp_reg;
    }
    assm->mov(ecx, amount);
  }

  // Do the actual shift.
  if (dst != src) assm->mov(dst, src);
  (assm->*emit_shift)(dst);

  // Restore ecx if needed.
  if (tmp_reg.is_valid()) assm->mov(ecx, tmp_reg);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::shl_cl,
                              pinned);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::sar_cl,
                              pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::shr_cl,
                              pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, int amount) {
  if (dst != src) mov(dst, src);
  DCHECK(is_uint5(amount));
  shr(dst, amount);
}

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  test(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  mov(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get most significant bit set (MSBS).
  bsr(dst, src);
  // CLZ = 31 - MSBS = MSBS ^ 31.
  xor_(dst, 31);

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  test(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  mov(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get least significant bit set, which equals number of trailing zeros.
  bsf(dst, src);

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  if (!CpuFeatures::IsSupported(POPCNT)) return false;
  CpuFeatureScope scope(this, POPCNT);
  popcnt(dst, src);
  return true;
}

namespace liftoff {
template <void (Assembler::*op)(Register, Register),
          void (Assembler::*op_with_carry)(Register, Register)>
inline void OpWithCarry(LiftoffAssembler* assm, LiftoffRegister dst,
                        LiftoffRegister lhs, LiftoffRegister rhs) {
  // First, compute the low half of the result, potentially into a temporary dst
  // register if {dst.low_gp()} equals {rhs.low_gp()} or any register we need to
  // keep alive for computing the upper half.
  LiftoffRegList keep_alive = LiftoffRegList::ForRegs(lhs.high_gp(), rhs);
  Register dst_low = keep_alive.has(dst.low_gp())
                         ? assm->GetUnusedRegister(kGpReg, keep_alive).gp()
                         : dst.low_gp();

  if (dst_low != lhs.low_gp()) assm->mov(dst_low, lhs.low_gp());
  (assm->*op)(dst_low, rhs.low_gp());

  // Now compute the upper half, while keeping alive the previous result.
  keep_alive = LiftoffRegList::ForRegs(dst_low, rhs.high_gp());
  Register dst_high = keep_alive.has(dst.high_gp())
                          ? assm->GetUnusedRegister(kGpReg, keep_alive).gp()
                          : dst.high_gp();

  if (dst_high != lhs.high_gp()) assm->mov(dst_high, lhs.high_gp());
  (assm->*op_with_carry)(dst_high, rhs.high_gp());

  // If necessary, move result into the right registers.
  LiftoffRegister tmp_result = LiftoffRegister::ForPair(dst_low, dst_high);
  if (tmp_result != dst) assm->Move(dst, tmp_result, kWasmI64);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::OpWithCarry<&Assembler::add, &Assembler::adc>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::OpWithCarry<&Assembler::sub, &Assembler::sbb>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  // Idea:
  //        [           lhs_hi  |           lhs_lo  ] * [  rhs_hi  |  rhs_lo  ]
  //    =   [  lhs_hi * rhs_lo  |                   ]  (32 bit mul, shift 32)
  //      + [  lhs_lo * rhs_hi  |                   ]  (32 bit mul, shift 32)
  //      + [             lhs_lo * rhs_lo           ]  (32x32->64 mul, shift 0)

  // For simplicity, we move lhs and rhs into fixed registers.
  Register dst_hi = edx;
  Register dst_lo = eax;
  Register lhs_hi = ecx;
  Register lhs_lo = dst_lo;
  Register rhs_hi = dst_hi;
  Register rhs_lo = ebx;

  // Spill all these registers if they are still holding other values.
  liftoff::SpillRegisters(this, dst_hi, dst_lo, lhs_hi, rhs_lo);

  // Move lhs and rhs into the respective registers.
  ParallelRegisterMove(
      {{LiftoffRegister::ForPair(lhs_lo, lhs_hi), lhs, kWasmI64},
       {LiftoffRegister::ForPair(rhs_lo, rhs_hi), rhs, kWasmI64}});

  // First mul: lhs_hi' = lhs_hi * rhs_lo.
  imul(lhs_hi, rhs_lo);
  // Second mul: rhi_hi' = rhs_hi * lhs_lo.
  imul(rhs_hi, lhs_lo);
  // Add them: lhs_hi'' = lhs_hi' + rhs_hi' = lhs_hi * rhs_lo + rhs_hi * lhs_lo.
  add(lhs_hi, rhs_hi);
  // Third mul: edx:eax (dst_hi:dst_lo) = eax * ebx (lhs_lo * rhs_lo).
  mul(rhs_lo);
  // Add lhs_hi'' to dst_hi.
  add(dst_hi, lhs_hi);

  // Finally, move back the temporary result to the actual dst register pair.
  LiftoffRegister dst_tmp = LiftoffRegister::ForPair(dst_lo, dst_hi);
  if (dst != dst_tmp) Move(dst, dst_tmp, kWasmI64);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  return false;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

namespace liftoff {
inline bool PairContains(LiftoffRegister pair, Register reg) {
  return pair.low_gp() == reg || pair.high_gp() == reg;
}

inline LiftoffRegister ReplaceInPair(LiftoffRegister pair, Register old_reg,
                                     Register new_reg) {
  if (pair.low_gp() == old_reg) {
    return LiftoffRegister::ForPair(new_reg, pair.high_gp());
  }
  if (pair.high_gp() == old_reg) {
    return LiftoffRegister::ForPair(pair.low_gp(), new_reg);
  }
  return pair;
}

inline void Emit64BitShiftOperation(
    LiftoffAssembler* assm, LiftoffRegister dst, LiftoffRegister src,
    Register amount, void (TurboAssembler::*emit_shift)(Register, Register),
    LiftoffRegList pinned) {
  pinned.set(dst);
  pinned.set(src);
  pinned.set(amount);
  // If {dst} contains {ecx}, replace it by an unused register, which is then
  // moved to {ecx} in the end.
  Register ecx_replace = no_reg;
  if (PairContains(dst, ecx)) {
    ecx_replace = pinned.set(assm->GetUnusedRegister(kGpReg, pinned)).gp();
    dst = ReplaceInPair(dst, ecx, ecx_replace);
    // If {amount} needs to be moved to {ecx}, but {ecx} is in use (and not part
    // of {dst}, hence overwritten anyway), move {ecx} to a tmp register and
    // restore it at the end.
  } else if (amount != ecx &&
             assm->cache_state()->is_used(LiftoffRegister(ecx))) {
    ecx_replace = assm->GetUnusedRegister(kGpReg, pinned).gp();
    assm->mov(ecx_replace, ecx);
  }

  assm->ParallelRegisterMove(
      {{dst, src, kWasmI64},
       {LiftoffRegister{ecx}, LiftoffRegister{amount}, kWasmI32}});

  // Do the actual shift.
  (assm->*emit_shift)(dst.high_gp(), dst.low_gp());

  // Restore {ecx} if needed.
  if (ecx_replace != no_reg) assm->mov(ecx, ecx_replace);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShlPair_cl, pinned);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::SarPair_cl, pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShrPair_cl, pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    int amount) {
  if (dst != src) Move(dst, src, kWasmI64);
  DCHECK(is_uint6(amount));
  ShrPair(dst.high_gp(), dst.low_gp(), amount);
}

void LiftoffAssembler::emit_i32_to_intptr(Register dst, Register src) {
  // This is a nop on ia32.
}

void LiftoffAssembler::emit_f32_add(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vaddss(dst, lhs, rhs);
  } else if (dst == rhs) {
    addss(dst, lhs);
  } else {
    if (dst != lhs) movss(dst, lhs);
    addss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_sub(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsubss(dst, lhs, rhs);
  } else if (dst == rhs) {
    movss(liftoff::kScratchDoubleReg, rhs);
    movss(dst, lhs);
    subss(dst, liftoff::kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    subss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_mul(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmulss(dst, lhs, rhs);
  } else if (dst == rhs) {
    mulss(dst, lhs);
  } else {
    if (dst != lhs) movss(dst, lhs);
    mulss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_div(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vdivss(dst, lhs, rhs);
  } else if (dst == rhs) {
    movss(liftoff::kScratchDoubleReg, rhs);
    movss(dst, lhs);
    divss(dst, liftoff::kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    divss(dst, rhs);
  }
}

namespace liftoff {
enum class MinOrMax : uint8_t { kMin, kMax };
template <typename type>
inline void EmitFloatMinOrMax(LiftoffAssembler* assm, DoubleRegister dst,
                              DoubleRegister lhs, DoubleRegister rhs,
                              MinOrMax min_or_max) {
  Label is_nan;
  Label lhs_below_rhs;
  Label lhs_above_rhs;
  Label done;

  // We need one tmp register to extract the sign bit. Get it right at the
  // beginning, such that the spilling code is not accidentially jumped over.
  Register tmp = assm->GetUnusedRegister(kGpReg).gp();

#define dop(name, ...)            \
  do {                            \
    if (sizeof(type) == 4) {      \
      assm->name##s(__VA_ARGS__); \
    } else {                      \
      assm->name##d(__VA_ARGS__); \
    }                             \
  } while (false)

  // Check the easy cases first: nan (e.g. unordered), smaller and greater.
  // NaN has to be checked first, because PF=1 implies CF=1.
  dop(ucomis, lhs, rhs);
  assm->j(parity_even, &is_nan, Label::kNear);   // PF=1
  assm->j(below, &lhs_below_rhs, Label::kNear);  // CF=1
  assm->j(above, &lhs_above_rhs, Label::kNear);  // CF=0 && ZF=0

  // If we get here, then either
  // a) {lhs == rhs},
  // b) {lhs == -0.0} and {rhs == 0.0}, or
  // c) {lhs == 0.0} and {rhs == -0.0}.
  // For a), it does not matter whether we return {lhs} or {rhs}. Check the sign
  // bit of {rhs} to differentiate b) and c).
  dop(movmskp, tmp, rhs);
  assm->test(tmp, Immediate(1));
  assm->j(zero, &lhs_below_rhs, Label::kNear);
  assm->jmp(&lhs_above_rhs, Label::kNear);

  assm->bind(&is_nan);
  // Create a NaN output.
  dop(xorp, dst, dst);
  dop(divs, dst, dst);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_below_rhs);
  DoubleRegister lhs_below_rhs_src = min_or_max == MinOrMax::kMin ? lhs : rhs;
  if (dst != lhs_below_rhs_src) dop(movs, dst, lhs_below_rhs_src);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_above_rhs);
  DoubleRegister lhs_above_rhs_src = min_or_max == MinOrMax::kMin ? rhs : lhs;
  if (dst != lhs_above_rhs_src) dop(movs, dst, lhs_above_rhs_src);

  assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<float>(this, dst, lhs, rhs,
                                    liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<float>(this, dst, lhs, rhs,
                                    liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  static constexpr int kF32SignBit = 1 << 31;
  Register scratch = GetUnusedRegister(kGpReg).gp();
  Register scratch2 =
      GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(scratch)).gp();
  Movd(scratch, lhs);                      // move {lhs} into {scratch}.
  and_(scratch, Immediate(~kF32SignBit));  // clear sign bit in {scratch}.
  Movd(scratch2, rhs);                     // move {rhs} into {scratch2}.
  and_(scratch2, Immediate(kF32SignBit));  // isolate sign bit in {scratch2}.
  or_(scratch, scratch2);                  // combine {scratch2} into {scratch}.
  Movd(dst, scratch);                      // move result into {dst}.
}

void LiftoffAssembler::emit_f32_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit - 1);
    Andps(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andps(dst, src);
  }
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit);
    Xorps(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorps(dst, src);
  }
}

void LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundUp);
}

void LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundDown);
}

void LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundToZero);
}

void LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundToNearest);
}

void LiftoffAssembler::emit_f32_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtss(dst, src);
}

void LiftoffAssembler::emit_f64_add(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vaddsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    addsd(dst, lhs);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    addsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_sub(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsubsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    movsd(liftoff::kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    subsd(dst, liftoff::kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    subsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_mul(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmulsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    mulsd(dst, lhs);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    mulsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_div(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vdivsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    movsd(liftoff::kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    divsd(dst, liftoff::kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    divsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  static constexpr int kF32SignBit = 1 << 31;
  // On ia32, we cannot hold the whole f64 value in a gp register, so we just
  // operate on the upper half (UH).
  Register scratch = GetUnusedRegister(kGpReg).gp();
  Register scratch2 =
      GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(scratch)).gp();

  Pextrd(scratch, lhs, 1);                 // move UH of {lhs} into {scratch}.
  and_(scratch, Immediate(~kF32SignBit));  // clear sign bit in {scratch}.
  Pextrd(scratch2, rhs, 1);                // move UH of {rhs} into {scratch2}.
  and_(scratch2, Immediate(kF32SignBit));  // isolate sign bit in {scratch2}.
  or_(scratch, scratch2);                  // combine {scratch2} into {scratch}.
  movsd(dst, lhs);                         // move {lhs} into {dst}.
  Pinsrd(dst, scratch, 1);                 // insert {scratch} into UH of {dst}.
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_f64_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit - 1);
    Andpd(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andpd(dst, src);
  }
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit);
    Xorpd(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorpd(dst, src);
  }
}

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  roundsd(dst, src, kRoundUp);
  return true;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  roundsd(dst, src, kRoundDown);
  return true;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  roundsd(dst, src, kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  roundsd(dst, src, kRoundToNearest);
  return true;
}

void LiftoffAssembler::emit_f64_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtsd(dst, src);
}

namespace liftoff {
// Used for float to int conversions. If the value in {converted_back} equals
// {src} afterwards, the conversion succeeded.
template <typename dst_type, typename src_type>
inline void ConvertFloatToIntAndBack(LiftoffAssembler* assm, Register dst,
                                     DoubleRegister src,
                                     DoubleRegister converted_back,
                                     LiftoffRegList pinned) {
  if (std::is_same<double, src_type>::value) {  // f64
    if (std::is_signed<dst_type>::value) {      // f64 -> i32
      assm->cvttsd2si(dst, src);
      assm->Cvtsi2sd(converted_back, dst);
    } else {  // f64 -> u32
      assm->Cvttsd2ui(dst, src, liftoff::kScratchDoubleReg);
      assm->Cvtui2sd(converted_back, dst);
    }
  } else {                                  // f32
    if (std::is_signed<dst_type>::value) {  // f32 -> i32
      assm->cvttss2si(dst, src);
      assm->Cvtsi2ss(converted_back, dst);
    } else {  // f32 -> u32
      assm->Cvttss2ui(dst, src, liftoff::kScratchDoubleReg);
      assm->Cvtui2ss(converted_back, dst,
                     assm->GetUnusedRegister(kGpReg, pinned).gp());
    }
  }
}

template <typename dst_type, typename src_type>
inline bool EmitTruncateFloatToInt(LiftoffAssembler* assm, Register dst,
                                   DoubleRegister src, Label* trap) {
  if (!CpuFeatures::IsSupported(SSE4_1)) {
    assm->bailout("no SSE4.1");
    return true;
  }
  CpuFeatureScope feature(assm, SSE4_1);

  LiftoffRegList pinned = LiftoffRegList::ForRegs(src, dst);
  DoubleRegister rounded =
      pinned.set(assm->GetUnusedRegister(kFpReg, pinned)).fp();
  DoubleRegister converted_back =
      pinned.set(assm->GetUnusedRegister(kFpReg, pinned)).fp();

  if (std::is_same<double, src_type>::value) {  // f64
    assm->roundsd(rounded, src, kRoundToZero);
  } else {  // f32
    assm->roundss(rounded, src, kRoundToZero);
  }
  ConvertFloatToIntAndBack<dst_type, src_type>(assm, dst, rounded,
                                               converted_back, pinned);
  if (std::is_same<double, src_type>::value) {  // f64
    assm->ucomisd(converted_back, rounded);
  } else {  // f32
    assm->ucomiss(converted_back, rounded);
  }

  // Jump to trap if PF is 0 (one of the operands was NaN) or they are not
  // equal.
  assm->j(parity_even, trap);
  assm->j(not_equal, trap);
  return true;
}
}  // namespace liftoff

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      if (dst.gp() != src.low_gp()) mov(dst.gp(), src.low_gp());
      return true;
    case kExprI32SConvertF32:
      return liftoff::EmitTruncateFloatToInt<int32_t, float>(this, dst.gp(),
                                                             src.fp(), trap);
    case kExprI32UConvertF32:
      return liftoff::EmitTruncateFloatToInt<uint32_t, float>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI32SConvertF64:
      return liftoff::EmitTruncateFloatToInt<int32_t, double>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI32UConvertF64:
      return liftoff::EmitTruncateFloatToInt<uint32_t, double>(this, dst.gp(),
                                                               src.fp(), trap);
    case kExprI32ReinterpretF32:
      Movd(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      if (dst.high_gp() != src.gp()) mov(dst.high_gp(), src.gp());
      sar(dst.high_gp(), 31);
      return true;
    case kExprI64UConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      xor_(dst.high_gp(), dst.high_gp());
      return true;
    case kExprI64ReinterpretF64:
      // Push src to the stack.
      sub(esp, Immediate(8));
      movsd(Operand(esp, 0), src.fp());
      // Pop to dst.
      pop(dst.low_gp());
      pop(dst.high_gp());
      return true;
    case kExprF32SConvertI32:
      cvtsi2ss(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI32: {
      LiftoffRegList pinned = LiftoffRegList::ForRegs(dst, src);
      Register scratch = GetUnusedRegister(kGpReg, pinned).gp();
      Cvtui2ss(dst.fp(), src.gp(), scratch);
      return true;
    }
    case kExprF32ConvertF64:
      cvtsd2ss(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      Movd(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32:
      Cvtsi2sd(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI32:
      Cvtui2sd(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      cvtss2sd(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      // Push src to the stack.
      push(src.high_gp());
      push(src.low_gp());
      // Pop to dst.
      movsd(dst.fp(), Operand(esp, 0));
      add(esp, Immediate(8));
      return true;
    default:
      return false;
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  movsx_b(dst, src);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  movsx_w(dst, src);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  movsx_b(dst.low_gp(), src.low_gp());
  liftoff::SignExtendI32ToI64(this, dst);
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  movsx_w(dst.low_gp(), src.low_gp());
  liftoff::SignExtendI32ToI64(this, dst);
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  if (dst.low_gp() != src.low_gp()) mov(dst.low_gp(), src.low_gp());
  liftoff::SignExtendI32ToI64(this, dst);
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_jump(Register target) { jmp(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    switch (type) {
      case kWasmI32:
        cmp(lhs, rhs);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, kWasmI32);
    test(lhs, lhs);
  }

  j(cond, label);
}

namespace liftoff {

// Get a temporary byte register, using {candidate} if possible.
// Might spill, but always keeps status flags intact.
inline Register GetTmpByteRegister(LiftoffAssembler* assm, Register candidate) {
  if (candidate.is_byte_register()) return candidate;
  LiftoffRegList pinned = LiftoffRegList::ForRegs(candidate);
  // {GetUnusedRegister()} may insert move instructions to spill registers to
  // the stack. This is OK because {mov} does not change the status flags.
  return assm->GetUnusedRegister(liftoff::kByteRegs, pinned).gp();
}

// Setcc into dst register, given a scratch byte register (might be the same as
// dst). Never spills.
inline void setcc_32_no_spill(LiftoffAssembler* assm, Condition cond,
                              Register dst, Register tmp_byte_reg) {
  assm->setcc(cond, tmp_byte_reg);
  assm->movzx_b(dst, tmp_byte_reg);
}

// Setcc into dst register (no contraints). Might spill.
inline void setcc_32(LiftoffAssembler* assm, Condition cond, Register dst) {
  Register tmp_byte_reg = GetTmpByteRegister(assm, dst);
  setcc_32_no_spill(assm, cond, dst, tmp_byte_reg);
}

}  // namespace liftoff

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  test(src, src);
  liftoff::setcc_32(this, equal, dst);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  cmp(lhs, rhs);
  liftoff::setcc_32(this, cond, dst);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  // Compute the OR of both registers in the src pair, using dst as scratch
  // register. Then check whether the result is equal to zero.
  if (src.low_gp() == dst) {
    or_(dst, src.high_gp());
  } else {
    if (src.high_gp() != dst) mov(dst, src.high_gp());
    or_(dst, src.low_gp());
  }
  liftoff::setcc_32(this, equal, dst);
}

namespace liftoff {
inline Condition cond_make_unsigned(Condition cond) {
  switch (cond) {
    case kSignedLessThan:
      return kUnsignedLessThan;
    case kSignedLessEqual:
      return kUnsignedLessEqual;
    case kSignedGreaterThan:
      return kUnsignedGreaterThan;
    case kSignedGreaterEqual:
      return kUnsignedGreaterEqual;
    default:
      return cond;
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  // Get the tmp byte register out here, such that we don't conditionally spill
  // (this cannot be reflected in the cache state).
  Register tmp_byte_reg = liftoff::GetTmpByteRegister(this, dst);

  // For signed i64 comparisons, we still need to use unsigned comparison for
  // the low word (the only bit carrying signedness information is the MSB in
  // the high word).
  Condition unsigned_cond = liftoff::cond_make_unsigned(cond);
  Label setcc;
  Label cont;
  // Compare high word first. If it differs, use if for the setcc. If it's
  // equal, compare the low word and use that for setcc.
  cmp(lhs.high_gp(), rhs.high_gp());
  j(not_equal, &setcc, Label::kNear);
  cmp(lhs.low_gp(), rhs.low_gp());
  if (unsigned_cond != cond) {
    // If the condition predicate for the low differs from that for the high
    // word, emit a separete setcc sequence for the low word.
    liftoff::setcc_32_no_spill(this, unsigned_cond, dst, tmp_byte_reg);
    jmp(&cont);
  }
  bind(&setcc);
  liftoff::setcc_32_no_spill(this, cond, dst, tmp_byte_reg);
  bind(&cont);
}

namespace liftoff {
template <void (Assembler::*cmp_op)(DoubleRegister, DoubleRegister)>
void EmitFloatSetCond(LiftoffAssembler* assm, Condition cond, Register dst,
                      DoubleRegister lhs, DoubleRegister rhs) {
  Label cont;
  Label not_nan;

  // Get the tmp byte register out here, such that we don't conditionally spill
  // (this cannot be reflected in the cache state).
  Register tmp_byte_reg = GetTmpByteRegister(assm, dst);

  (assm->*cmp_op)(lhs, rhs);
  // If PF is one, one of the operands was Nan. This needs special handling.
  assm->j(parity_odd, &not_nan, Label::kNear);
  // Return 1 for f32.ne, 0 for all other cases.
  if (cond == not_equal) {
    assm->mov(dst, Immediate(1));
  } else {
    assm->xor_(dst, dst);
  }
  assm->jmp(&cont, Label::kNear);
  assm->bind(&not_nan);

  setcc_32_no_spill(assm, cond, dst, tmp_byte_reg);
  assm->bind(&cont);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&Assembler::ucomiss>(this, cond, dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&Assembler::ucomisd>(this, cond, dst, lhs, rhs);
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  cmp(esp, Operand(limit_address, 0));
  j(below_equal, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg).gp());
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  TurboAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetFirstRegSet();
    push(reg.gp());
    gp_regs.clear(reg);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    sub(esp, Immediate(num_fp_regs * kStackSlotSize));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      movsd(Operand(esp, offset), reg.fp());
      fp_regs.clear(reg);
      offset += sizeof(double);
    }
    DCHECK_EQ(offset, num_fp_regs * sizeof(double));
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned fp_offset = 0;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    movsd(reg.fp(), Operand(esp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) add(esp, Immediate(fp_offset));
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    pop(reg.gp());
    gp_regs.clear(reg);
  }
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots, (1 << 16) / kPointerSize);  // 16 bit immediate
  ret(static_cast<int>(num_stack_slots * kPointerSize));
}

void LiftoffAssembler::CallC(wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  sub(esp, Immediate(stack_bytes));

  int arg_bytes = 0;
  for (ValueType param_type : sig->parameters()) {
    liftoff::Store(this, esp, arg_bytes, *args++, param_type);
    arg_bytes += ValueTypes::MemSize(param_type);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  constexpr Register kScratch = eax;
  constexpr Register kArgumentBuffer = ecx;
  constexpr int kNumCCallArgs = 1;
  mov(kArgumentBuffer, esp);
  PrepareCallCFunction(kNumCCallArgs, kScratch);

  // Pass a pointer to the buffer with the arguments to the C function. ia32
  // does not use registers here, so push to the stack.
  mov(Operand(esp, 0), kArgumentBuffer);

  // Now call the C function.
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = eax;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_type != kWasmStmt) {
    liftoff::Load(this, *next_result_reg, esp, 0, out_argument_type);
  }

  add(esp, Immediate(stack_bytes));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  wasm_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  // Since we have more cache registers than parameter registers, the
  // {LiftoffCompiler} should always be able to place {target} in a register.
  DCHECK(target.is_valid());
  if (FLAG_untrusted_code_mitigations) {
    RetpolineCall(target);
  } else {
    call(target);
  }
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  wasm_call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  sub(esp, Immediate(size));
  mov(addr, esp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  add(esp, Immediate(size));
}

void LiftoffStackSlots::Construct() {
  for (auto& slot : slots_) {
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack:
        if (src.type() == kWasmF64) {
          DCHECK_EQ(kLowWord, slot.half_);
          asm_->push(liftoff::GetHalfStackSlot(2 * slot.src_index_ - 1));
        }
        asm_->push(liftoff::GetHalfStackSlot(2 * slot.src_index_ -
                                             (slot.half_ == kLowWord ? 0 : 1)));
        break;
      case LiftoffAssembler::VarState::kRegister:
        if (src.type() == kWasmI64) {
          liftoff::push(
              asm_, slot.half_ == kLowWord ? src.reg().low() : src.reg().high(),
              kWasmI32);
        } else {
          liftoff::push(asm_, src.reg(), src.type());
        }
        break;
      case LiftoffAssembler::VarState::KIntConst:
        // The high word is the sign extension of the low word.
        asm_->push(Immediate(slot.half_ == kLowWord ? src.i32_const()
                                                    : src.i32_const() >> 31));
        break;
    }
  }
}

#undef REQUIRE_CPU_FEATURE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_
