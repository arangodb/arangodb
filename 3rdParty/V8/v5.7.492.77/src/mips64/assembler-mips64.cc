// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

#include "src/mips64/assembler-mips64.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/base/cpu.h"
#include "src/mips64/assembler-mips64-inl.h"

namespace v8 {
namespace internal {


// Get the CPU features enabled by the build. For cross compilation the
// preprocessor symbols CAN_USE_FPU_INSTRUCTIONS
// can be defined to enable FPU instructions when building the
// snapshot.
static unsigned CpuFeaturesImpliedByCompiler() {
  unsigned answer = 0;
#ifdef CAN_USE_FPU_INSTRUCTIONS
  answer |= 1u << FPU;
#endif  // def CAN_USE_FPU_INSTRUCTIONS

  // If the compiler is allowed to use FPU then we can use FPU too in our code
  // generation even when generating snapshots.  This won't work for cross
  // compilation.
#if defined(__mips__) && defined(__mips_hard_float) && __mips_hard_float != 0
  answer |= 1u << FPU;
#endif

  return answer;
}


void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

  // If the compiler is allowed to use fpu then we can use fpu too in our
  // code generation.
#ifndef __mips__
  // For the simulator build, use FPU.
  supported_ |= 1u << FPU;
#else
  // Probe for additional features at runtime.
  base::CPU cpu;
  if (cpu.has_fpu()) supported_ |= 1u << FPU;
#endif
}


void CpuFeatures::PrintTarget() { }
void CpuFeatures::PrintFeatures() { }


int ToNumber(Register reg) {
  DCHECK(reg.is_valid());
  const int kNumbers[] = {
    0,    // zero_reg
    1,    // at
    2,    // v0
    3,    // v1
    4,    // a0
    5,    // a1
    6,    // a2
    7,    // a3
    8,    // a4
    9,    // a5
    10,   // a6
    11,   // a7
    12,   // t0
    13,   // t1
    14,   // t2
    15,   // t3
    16,   // s0
    17,   // s1
    18,   // s2
    19,   // s3
    20,   // s4
    21,   // s5
    22,   // s6
    23,   // s7
    24,   // t8
    25,   // t9
    26,   // k0
    27,   // k1
    28,   // gp
    29,   // sp
    30,   // fp
    31,   // ra
  };
  return kNumbers[reg.code()];
}


Register ToRegister(int num) {
  DCHECK(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] = {
    zero_reg,
    at,
    v0, v1,
    a0, a1, a2, a3, a4, a5, a6, a7,
    t0, t1, t2, t3,
    s0, s1, s2, s3, s4, s5, s6, s7,
    t8, t9,
    k0, k1,
    gp,
    sp,
    fp,
    ra
  };
  return kRegisters[num];
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo.

const int RelocInfo::kApplyMask = RelocInfo::kCodeTargetMask |
                                  1 << RelocInfo::INTERNAL_REFERENCE |
                                  1 << RelocInfo::INTERNAL_REFERENCE_ENCODED;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on MIPS means that it is a lui/ori instruction, and that is
  // always the case inside code objects.
  return true;
}


bool RelocInfo::IsInConstantPool() {
  return false;
}

Address RelocInfo::wasm_memory_reference() {
  DCHECK(IsWasmMemoryReference(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

Address RelocInfo::wasm_global_reference() {
  DCHECK(IsWasmGlobalReference(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

uint32_t RelocInfo::wasm_memory_size_reference() {
  DCHECK(IsWasmMemorySizeReference(rmode_));
  return static_cast<uint32_t>(
      reinterpret_cast<intptr_t>((Assembler::target_address_at(pc_, host_))));
}

uint32_t RelocInfo::wasm_function_table_size_reference() {
  DCHECK(IsWasmFunctionTableSizeReference(rmode_));
  return static_cast<uint32_t>(
      reinterpret_cast<intptr_t>((Assembler::target_address_at(pc_, host_))));
}

void RelocInfo::unchecked_update_wasm_memory_reference(
    Address address, ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate_, pc_, host_, address, flush_mode);
}

void RelocInfo::unchecked_update_wasm_size(uint32_t size,
                                           ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate_, pc_, host_,
                                   reinterpret_cast<Address>(size), flush_mode);
}

// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand.
// See assembler-mips-inl.h for inlined constructors.

Operand::Operand(Handle<Object> handle) {
  AllowDeferredHandleDereference using_raw_address;
  rm_ = no_reg;
  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  if (obj->IsHeapObject()) {
    imm64_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // No relocation needed.
    imm64_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE64;
  }
}


MemOperand::MemOperand(Register rm, int32_t offset) : Operand(rm) {
  offset_ = offset;
}


MemOperand::MemOperand(Register rm, int32_t unit, int32_t multiplier,
                       OffsetAddend offset_addend)
    : Operand(rm) {
  offset_ = unit * multiplier + offset_addend;
}


// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

static const int kNegOffset = 0x00008000;
// daddiu(sp, sp, 8) aka Pop() operation or part of Pop(r)
// operations as post-increment of sp.
const Instr kPopInstruction = DADDIU | (Register::kCode_sp << kRsShift) |
                              (Register::kCode_sp << kRtShift) |
                              (kPointerSize & kImm16Mask);  // NOLINT
// daddiu(sp, sp, -8) part of Push(r) operation as pre-decrement of sp.
const Instr kPushInstruction = DADDIU | (Register::kCode_sp << kRsShift) |
                               (Register::kCode_sp << kRtShift) |
                               (-kPointerSize & kImm16Mask);  // NOLINT
// sd(r, MemOperand(sp, 0))
const Instr kPushRegPattern =
    SD | (Register::kCode_sp << kRsShift) | (0 & kImm16Mask);  // NOLINT
//  ld(r, MemOperand(sp, 0))
const Instr kPopRegPattern =
    LD | (Register::kCode_sp << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpOffsetPattern =
    LW | (Register::kCode_fp << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kSwRegFpOffsetPattern =
    SW | (Register::kCode_fp << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpNegOffsetPattern = LW | (Register::kCode_fp << kRsShift) |
                                       (kNegOffset & kImm16Mask);  // NOLINT

const Instr kSwRegFpNegOffsetPattern = SW | (Register::kCode_fp << kRsShift) |
                                       (kNegOffset & kImm16Mask);  // NOLINT
// A mask for the Rt register for push, pop, lw, sw instructions.
const Instr kRtMask = kRtFieldMask;
const Instr kLwSwInstrTypeMask = 0xffe00000;
const Instr kLwSwInstrArgumentMask  = ~kLwSwInstrTypeMask;
const Instr kLwSwOffsetMask = kImm16Mask;

Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size),
      recorded_ast_id_(TypeFeedbackId::None()) {
  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);

  last_trampoline_pool_end_ = 0;
  no_trampoline_pool_before_ = 0;
  trampoline_pool_blocked_nesting_ = 0;
  // We leave space (16 * kTrampolineSlotsSize)
  // for BlockTrampolinePoolScope buffer.
  next_buffer_check_ = FLAG_force_long_branches
      ? kMaxInt : kMaxBranchOffset - kTrampolineSlotsSize * 16;
  internal_trampoline_exception_ = false;
  last_bound_pos_ = 0;

  trampoline_emitted_ = FLAG_force_long_branches;
  unbound_labels_count_ = 0;
  block_buffer_growth_ = false;

  ClearRecordedAstId();
}


void Assembler::GetCode(CodeDesc* desc) {
  EmitForbiddenSlotInstruction();
  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.
  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size =
      static_cast<int>((buffer_ + buffer_size_) - reloc_info_writer.pos());
  desc->origin = this;
  desc->constant_pool_size = 0;
  desc->unwinding_info_size = 0;
  desc->unwinding_info = nullptr;
}


void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo32(m));
  EmitForbiddenSlotInstruction();
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::CodeTargetAlign() {
  // No advantage to aligning branch/call targets to more than
  // single instruction, that I am aware of.
  Align(4);
}


Register Assembler::GetRtReg(Instr instr) {
  Register rt;
  rt.reg_code = (instr & kRtFieldMask) >> kRtShift;
  return rt;
}


Register Assembler::GetRsReg(Instr instr) {
  Register rs;
  rs.reg_code = (instr & kRsFieldMask) >> kRsShift;
  return rs;
}


Register Assembler::GetRdReg(Instr instr) {
  Register rd;
  rd.reg_code = (instr & kRdFieldMask) >> kRdShift;
  return rd;
}


uint32_t Assembler::GetRt(Instr instr) {
  return (instr & kRtFieldMask) >> kRtShift;
}


uint32_t Assembler::GetRtField(Instr instr) {
  return instr & kRtFieldMask;
}


uint32_t Assembler::GetRs(Instr instr) {
  return (instr & kRsFieldMask) >> kRsShift;
}


uint32_t Assembler::GetRsField(Instr instr) {
  return instr & kRsFieldMask;
}


uint32_t Assembler::GetRd(Instr instr) {
  return  (instr & kRdFieldMask) >> kRdShift;
}


uint32_t Assembler::GetRdField(Instr instr) {
  return  instr & kRdFieldMask;
}


uint32_t Assembler::GetSa(Instr instr) {
  return (instr & kSaFieldMask) >> kSaShift;
}


uint32_t Assembler::GetSaField(Instr instr) {
  return instr & kSaFieldMask;
}


uint32_t Assembler::GetOpcodeField(Instr instr) {
  return instr & kOpcodeMask;
}


uint32_t Assembler::GetFunction(Instr instr) {
  return (instr & kFunctionFieldMask) >> kFunctionShift;
}


uint32_t Assembler::GetFunctionField(Instr instr) {
  return instr & kFunctionFieldMask;
}


uint32_t Assembler::GetImmediate16(Instr instr) {
  return instr & kImm16Mask;
}


uint32_t Assembler::GetLabelConst(Instr instr) {
  return instr & ~kImm16Mask;
}


bool Assembler::IsPop(Instr instr) {
  return (instr & ~kRtMask) == kPopRegPattern;
}


bool Assembler::IsPush(Instr instr) {
  return (instr & ~kRtMask) == kPushRegPattern;
}


bool Assembler::IsSwRegFpOffset(Instr instr) {
  return ((instr & kLwSwInstrTypeMask) == kSwRegFpOffsetPattern);
}


bool Assembler::IsLwRegFpOffset(Instr instr) {
  return ((instr & kLwSwInstrTypeMask) == kLwRegFpOffsetPattern);
}


bool Assembler::IsSwRegFpNegOffset(Instr instr) {
  return ((instr & (kLwSwInstrTypeMask | kNegOffset)) ==
          kSwRegFpNegOffsetPattern);
}


bool Assembler::IsLwRegFpNegOffset(Instr instr) {
  return ((instr & (kLwSwInstrTypeMask | kNegOffset)) ==
          kLwRegFpNegOffsetPattern);
}


// Labels refer to positions in the (to be) generated code.
// There are bound, linked, and unused labels.
//
// Bound labels refer to known positions in the already
// generated code. pos() is the position the label refers to.
//
// Linked labels refer to unknown positions in the code
// to be generated; pos() is the position of the last
// instruction using the label.

// The link chain is terminated by a value in the instruction of -1,
// which is an otherwise illegal value (branch -1 is inf loop).
// The instruction 16-bit offset field addresses 32-bit words, but in
// code is conv to an 18-bit value addressing bytes, hence the -4 value.

const int kEndOfChain = -4;
// Determines the end of the Jump chain (a subset of the label link chain).
const int kEndOfJumpChain = 0;


bool Assembler::IsBranch(Instr instr) {
  uint32_t opcode   = GetOpcodeField(instr);
  uint32_t rt_field = GetRtField(instr);
  uint32_t rs_field = GetRsField(instr);
  // Checks if the instruction is a branch.
  bool isBranch =
      opcode == BEQ || opcode == BNE || opcode == BLEZ || opcode == BGTZ ||
      opcode == BEQL || opcode == BNEL || opcode == BLEZL || opcode == BGTZL ||
      (opcode == REGIMM && (rt_field == BLTZ || rt_field == BGEZ ||
                            rt_field == BLTZAL || rt_field == BGEZAL)) ||
      (opcode == COP1 && rs_field == BC1) ||  // Coprocessor branch.
      (opcode == COP1 && rs_field == BC1EQZ) ||
      (opcode == COP1 && rs_field == BC1NEZ);
  if (!isBranch && kArchVariant == kMips64r6) {
    // All the 3 variants of POP10 (BOVC, BEQC, BEQZALC) and
    // POP30 (BNVC, BNEC, BNEZALC) are branch ops.
    isBranch |= opcode == POP10 || opcode == POP30 || opcode == BC ||
                opcode == BALC ||
                (opcode == POP66 && rs_field != 0) ||  // BEQZC
                (opcode == POP76 && rs_field != 0);    // BNEZC
  }
  return isBranch;
}


bool Assembler::IsBc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a BC or BALC.
  return opcode == BC || opcode == BALC;
}


bool Assembler::IsBzc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is BEQZC or BNEZC.
  return (opcode == POP66 && GetRsField(instr) != 0) ||
         (opcode == POP76 && GetRsField(instr) != 0);
}


bool Assembler::IsEmittedConstant(Instr instr) {
  uint32_t label_constant = GetLabelConst(instr);
  return label_constant == 0;  // Emitted label const in reg-exp engine.
}


bool Assembler::IsBeq(Instr instr) {
  return GetOpcodeField(instr) == BEQ;
}


bool Assembler::IsBne(Instr instr) {
  return GetOpcodeField(instr) == BNE;
}


bool Assembler::IsBeqzc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  return opcode == POP66 && GetRsField(instr) != 0;
}


bool Assembler::IsBnezc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  return opcode == POP76 && GetRsField(instr) != 0;
}


bool Assembler::IsBeqc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs = GetRsField(instr);
  uint32_t rt = GetRtField(instr);
  return opcode == POP10 && rs != 0 && rs < rt;  // && rt != 0
}


bool Assembler::IsBnec(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs = GetRsField(instr);
  uint32_t rt = GetRtField(instr);
  return opcode == POP30 && rs != 0 && rs < rt;  // && rt != 0
}


bool Assembler::IsJump(Instr instr) {
  uint32_t opcode   = GetOpcodeField(instr);
  uint32_t rt_field = GetRtField(instr);
  uint32_t rd_field = GetRdField(instr);
  uint32_t function_field = GetFunctionField(instr);
  // Checks if the instruction is a jump.
  return opcode == J || opcode == JAL ||
      (opcode == SPECIAL && rt_field == 0 &&
      ((function_field == JALR) || (rd_field == 0 && (function_field == JR))));
}


bool Assembler::IsJ(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a jump.
  return opcode == J;
}


bool Assembler::IsJal(Instr instr) {
  return GetOpcodeField(instr) == JAL;
}


bool Assembler::IsJr(Instr instr) {
  return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JR;
}


bool Assembler::IsJalr(Instr instr) {
  return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JALR;
}


bool Assembler::IsLui(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a load upper immediate.
  return opcode == LUI;
}


bool Assembler::IsOri(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a load upper immediate.
  return opcode == ORI;
}


bool Assembler::IsNop(Instr instr, unsigned int type) {
  // See Assembler::nop(type).
  DCHECK(type < 32);
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t function = GetFunctionField(instr);
  uint32_t rt = GetRt(instr);
  uint32_t rd = GetRd(instr);
  uint32_t sa = GetSa(instr);

  // Traditional mips nop == sll(zero_reg, zero_reg, 0)
  // When marking non-zero type, use sll(zero_reg, at, type)
  // to avoid use of mips ssnop and ehb special encodings
  // of the sll instruction.

  Register nop_rt_reg = (type == 0) ? zero_reg : at;
  bool ret = (opcode == SPECIAL && function == SLL &&
              rd == static_cast<uint32_t>(ToNumber(zero_reg)) &&
              rt == static_cast<uint32_t>(ToNumber(nop_rt_reg)) &&
              sa == type);

  return ret;
}


int32_t Assembler::GetBranchOffset(Instr instr) {
  DCHECK(IsBranch(instr));
  return (static_cast<int16_t>(instr & kImm16Mask)) << 2;
}


bool Assembler::IsLw(Instr instr) {
  return (static_cast<uint32_t>(instr & kOpcodeMask) == LW);
}


int16_t Assembler::GetLwOffset(Instr instr) {
  DCHECK(IsLw(instr));
  return ((instr & kImm16Mask));
}


Instr Assembler::SetLwOffset(Instr instr, int16_t offset) {
  DCHECK(IsLw(instr));

  // We actually create a new lw instruction based on the original one.
  Instr temp_instr = LW | (instr & kRsFieldMask) | (instr & kRtFieldMask)
      | (offset & kImm16Mask);

  return temp_instr;
}


bool Assembler::IsSw(Instr instr) {
  return (static_cast<uint32_t>(instr & kOpcodeMask) == SW);
}


Instr Assembler::SetSwOffset(Instr instr, int16_t offset) {
  DCHECK(IsSw(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAddImmediate(Instr instr) {
  return ((instr & kOpcodeMask) == ADDIU || (instr & kOpcodeMask) == DADDIU);
}


Instr Assembler::SetAddImmediateOffset(Instr instr, int16_t offset) {
  DCHECK(IsAddImmediate(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAndImmediate(Instr instr) {
  return GetOpcodeField(instr) == ANDI;
}


static Assembler::OffsetSize OffsetSizeInBits(Instr instr) {
  if (kArchVariant == kMips64r6) {
    if (Assembler::IsBc(instr)) {
      return Assembler::OffsetSize::kOffset26;
    } else if (Assembler::IsBzc(instr)) {
      return Assembler::OffsetSize::kOffset21;
    }
  }
  return Assembler::OffsetSize::kOffset16;
}


static inline int32_t AddBranchOffset(int pos, Instr instr) {
  int bits = OffsetSizeInBits(instr);
  const int32_t mask = (1 << bits) - 1;
  bits = 32 - bits;

  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmetic shifts for signed integers.
  int32_t imm = ((instr & mask) << bits) >> (bits - 2);

  if (imm == kEndOfChain) {
    // EndOfChain sentinel is returned directly, not relative to pc or pos.
    return kEndOfChain;
  } else {
    return pos + Assembler::kBranchPCOffset + imm;
  }
}


int Assembler::target_at(int pos, bool is_internal) {
  if (is_internal) {
    int64_t* p = reinterpret_cast<int64_t*>(buffer_ + pos);
    int64_t address = *p;
    if (address == kEndOfJumpChain) {
      return kEndOfChain;
    } else {
      int64_t instr_address = reinterpret_cast<int64_t>(p);
      DCHECK(instr_address - address < INT_MAX);
      int delta = static_cast<int>(instr_address - address);
      DCHECK(pos > delta);
      return pos - delta;
    }
  }
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    // Emitted label constant, not part of a branch.
    if (instr == 0) {
       return kEndOfChain;
     } else {
       int32_t imm18 =((instr & static_cast<int32_t>(kImm16Mask)) << 16) >> 14;
       return (imm18 + pos);
     }
  }
  // Check we have a branch or jump instruction.
  DCHECK(IsBranch(instr) || IsJ(instr) || IsJal(instr) || IsLui(instr));
  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmetic shifts for signed integers.
  if (IsBranch(instr)) {
    return AddBranchOffset(pos, instr);
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pos + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));

    // TODO(plind) create named constants for shift values.
    int64_t imm = static_cast<int64_t>(instr_lui & kImm16Mask) << 48;
    imm |= static_cast<int64_t>(instr_ori & kImm16Mask) << 32;
    imm |= static_cast<int64_t>(instr_ori2 & kImm16Mask) << 16;
    // Sign extend address;
    imm >>= 16;

    if (imm == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint64_t instr_address = reinterpret_cast<int64_t>(buffer_ + pos);
      DCHECK(instr_address - imm < INT_MAX);
      int delta = static_cast<int>(instr_address - imm);
      DCHECK(pos > delta);
      return pos - delta;
    }
  } else {
    DCHECK(IsJ(instr) || IsJal(instr));
    int32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    if (imm28 == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      // Sign extend 28-bit offset.
      int32_t delta = static_cast<int32_t>((imm28 << 4) >> 4);
      return pos + delta;
    }
  }
}


static inline Instr SetBranchOffset(int32_t pos, int32_t target_pos,
                                    Instr instr) {
  int32_t bits = OffsetSizeInBits(instr);
  int32_t imm = target_pos - (pos + Assembler::kBranchPCOffset);
  DCHECK((imm & 3) == 0);
  imm >>= 2;

  const int32_t mask = (1 << bits) - 1;
  instr &= ~mask;
  DCHECK(is_intn(imm, bits));

  return instr | (imm & mask);
}


void Assembler::target_at_put(int pos, int target_pos, bool is_internal) {
  if (is_internal) {
    uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
    *reinterpret_cast<uint64_t*>(buffer_ + pos) = imm;
    return;
  }
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    DCHECK(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    return;
  }

  if (IsBranch(instr)) {
    instr = SetBranchOffset(pos, target_pos, instr);
    instr_at_put(pos, instr);
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pos + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));

    uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
    DCHECK((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;
    instr_ori2 &= ~kImm16Mask;

    instr_at_put(pos + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> 32) & kImm16Mask));
    instr_at_put(pos + 1 * Assembler::kInstrSize,
                 instr_ori | ((imm >> 16) & kImm16Mask));
    instr_at_put(pos + 3 * Assembler::kInstrSize,
                 instr_ori2 | (imm & kImm16Mask));
  } else if (IsJ(instr) || IsJal(instr)) {
    int32_t imm28 = target_pos - pos;
    DCHECK((imm28 & 3) == 0);

    uint32_t imm26 = static_cast<uint32_t>(imm28 >> 2);
    DCHECK(is_uint26(imm26));
    // Place 26-bit signed offset with markings.
    // When code is committed it will be resolved to j/jal.
    int32_t mark = IsJ(instr) ? kJRawMark : kJalRawMark;
    instr_at_put(pos, mark | (imm26 & kImm26Mask));
  } else {
    int32_t imm28 = target_pos - pos;
    DCHECK((imm28 & 3) == 0);

    uint32_t imm26 = static_cast<uint32_t>(imm28 >> 2);
    DCHECK(is_uint26(imm26));
    // Place raw 26-bit signed offset.
    // When code is committed it will be resolved to j/jal.
    instr &= ~kImm26Mask;
    instr_at_put(pos, instr | (imm26 & kImm26Mask));
  }
}


void Assembler::print(Label* L) {
  if (L->is_unused()) {
    PrintF("unused label\n");
  } else if (L->is_bound()) {
    PrintF("bound label to %d\n", L->pos());
  } else if (L->is_linked()) {
    Label l = *L;
    PrintF("unbound label");
    while (l.is_linked()) {
      PrintF("@ %d ", l.pos());
      Instr instr = instr_at(l.pos());
      if ((instr & ~kImm16Mask) == 0) {
        PrintF("value\n");
      } else {
        PrintF("%d\n", instr);
      }
      next(&l, is_internal_reference(&l));
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}


void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // Must have valid binding position.
  int trampoline_pos = kInvalidSlotPos;
  bool is_internal = false;
  if (L->is_linked() && !trampoline_emitted_) {
    unbound_labels_count_--;
    if (!is_internal_reference(L)) {
      next_buffer_check_ += kTrampolineSlotsSize;
    }
  }

  while (L->is_linked()) {
    int fixup_pos = L->pos();
    int dist = pos - fixup_pos;
    is_internal = is_internal_reference(L);
    next(L, is_internal);  // Call next before overwriting link with target at
                           // fixup_pos.
    Instr instr = instr_at(fixup_pos);
    if (is_internal) {
      target_at_put(fixup_pos, pos, is_internal);
    } else {
      if (IsBranch(instr)) {
        int branch_offset = BranchOffset(instr);
        if (dist > branch_offset) {
          if (trampoline_pos == kInvalidSlotPos) {
            trampoline_pos = get_trampoline_entry(fixup_pos);
            CHECK(trampoline_pos != kInvalidSlotPos);
          }
          CHECK((trampoline_pos - fixup_pos) <= branch_offset);
          target_at_put(fixup_pos, trampoline_pos, false);
          fixup_pos = trampoline_pos;
        }
        target_at_put(fixup_pos, pos, false);
      } else {
        DCHECK(IsJ(instr) || IsJal(instr) || IsLui(instr) ||
               IsEmittedConstant(instr));
        target_at_put(fixup_pos, pos, false);
      }
    }
  }
  L->bind_to(pos);

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_)
    last_bound_pos_ = pos;
}


void Assembler::bind(Label* L) {
  DCHECK(!L->is_bound());  // Label can only be bound once.
  bind_to(L, pc_offset());
}


void Assembler::next(Label* L, bool is_internal) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos(), is_internal);
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    DCHECK(link >= 0);
    L->link_to(link);
  }
}


bool Assembler::is_near(Label* L) {
  DCHECK(L->is_bound());
  return pc_offset() - L->pos() < kMaxBranchOffset - 4 * kInstrSize;
}


bool Assembler::is_near(Label* L, OffsetSize bits) {
  if (L == nullptr || !L->is_bound()) return true;
  return ((pc_offset() - L->pos()) <
          (1 << (bits + 2 - 1)) - 1 - 5 * kInstrSize);
}


bool Assembler::is_near_branch(Label* L) {
  DCHECK(L->is_bound());
  return kArchVariant == kMips64r6 ? is_near_r6(L) : is_near_pre_r6(L);
}


int Assembler::BranchOffset(Instr instr) {
  // At pre-R6 and for other R6 branches the offset is 16 bits.
  int bits = OffsetSize::kOffset16;

  if (kArchVariant == kMips64r6) {
    uint32_t opcode = GetOpcodeField(instr);
    switch (opcode) {
      // Checks BC or BALC.
      case BC:
      case BALC:
        bits = OffsetSize::kOffset26;
        break;

      // Checks BEQZC or BNEZC.
      case POP66:
      case POP76:
        if (GetRsField(instr) != 0) bits = OffsetSize::kOffset21;
        break;
      default:
        break;
    }
  }

  return (1 << (bits + 2 - 1)) - 1;
}


// We have to use a temporary register for things that can be relocated even
// if they can be encoded in the MIPS's 16 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Assembler::MustUseReg(RelocInfo::Mode rmode) {
  return !RelocInfo::IsNone(rmode);
}

void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 Register rd,
                                 uint16_t sa,
                                 SecondaryField func) {
  DCHECK(rd.is_valid() && rs.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 uint16_t msb,
                                 uint16_t lsb,
                                 SecondaryField func) {
  DCHECK(rs.is_valid() && rt.is_valid() && is_uint5(msb) && is_uint5(lsb));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (msb << kRdShift) | (lsb << kSaShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | fmt | (ft.code() << kFtShift) | (fs.code() << kFsShift)
      | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 FPURegister fr,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fr.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | (fr.code() << kFrShift) | (ft.code() << kFtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fs.is_valid() && rt.is_valid());
  Instr instr = opcode | fmt | (rt.code() << kRtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPUControlRegister fs,
                                 SecondaryField func) {
  DCHECK(fs.is_valid() && rt.is_valid());
  Instr instr =
      opcode | fmt | (rt.code() << kRtShift) | (fs.code() << kFsShift) | func;
  emit(instr);
}


// Instructions with immediate value.
// Registers are in the order of the instruction encoding, from left to right.
void Assembler::GenInstrImmediate(Opcode opcode, Register rs, Register rt,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && rt.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, SecondaryField SF,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | SF | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, FPURegister ft,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && ft.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (ft.code() << kFtShift)
      | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, int32_t offset21,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && (is_int21(offset21)));
  Instr instr = opcode | (rs.code() << kRsShift) | (offset21 & kImm21Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs,
                                  uint32_t offset21) {
  DCHECK(rs.is_valid() && (is_uint21(offset21)));
  Instr instr = opcode | (rs.code() << kRsShift) | (offset21 & kImm21Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode, int32_t offset26,
                                  CompactBranchType is_compact_branch) {
  DCHECK(is_int26(offset26));
  Instr instr = opcode | (offset26 & kImm26Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrJump(Opcode opcode,
                             uint32_t address) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(is_uint26(address));
  Instr instr = opcode | address;
  emit(instr);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


// Returns the next free trampoline entry.
int32_t Assembler::get_trampoline_entry(int32_t pos) {
  int32_t trampoline_entry = kInvalidSlotPos;
  if (!internal_trampoline_exception_) {
    if (trampoline_.start() > pos) {
     trampoline_entry = trampoline_.take_slot();
    }

    if (kInvalidSlotPos == trampoline_entry) {
      internal_trampoline_exception_ = true;
    }
  }
  return trampoline_entry;
}


uint64_t Assembler::jump_address(Label* L) {
  int64_t target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      return kEndOfJumpChain;
    }
  }
  uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
  DCHECK((imm & 3) == 0);

  return imm;
}


uint64_t Assembler::jump_offset(Label* L) {
  int64_t target_pos;
  int32_t pad = IsPrevInstrCompactBranch() ? kInstrSize : 0;

  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset() + pad);
    } else {
      L->link_to(pc_offset() + pad);
      return kEndOfJumpChain;
    }
  }
  int64_t imm = target_pos - (pc_offset() + pad);
  DCHECK((imm & 3) == 0);

  return static_cast<uint64_t>(imm);
}


int32_t Assembler::branch_offset_helper(Label* L, OffsetSize bits) {
  int32_t target_pos;
  int32_t pad = IsPrevInstrCompactBranch() ? kInstrSize : 0;

  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset() + pad);
    } else {
      L->link_to(pc_offset() + pad);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - (pc_offset() + kBranchPCOffset + pad);
  DCHECK(is_intn(offset, bits + 2));
  DCHECK((offset & 3) == 0);

  return offset;
}


void Assembler::label_at_put(Label* L, int at_offset) {
  int target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
    instr_at_put(at_offset, target_pos + (Code::kHeaderSize - kHeapObjectTag));
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      int32_t imm18 = target_pos - at_offset;
      DCHECK((imm18 & 3) == 0);
      int32_t imm16 = imm18 >> 2;
      DCHECK(is_int16(imm16));
      instr_at_put(at_offset, (imm16 & kImm16Mask));
    } else {
      target_pos = kEndOfChain;
      instr_at_put(at_offset, 0);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
    }
    L->link_to(at_offset);
  }
}


//------- Branch and jump instructions --------

void Assembler::b(int16_t offset) {
  beq(zero_reg, zero_reg, offset);
}


void Assembler::bal(int16_t offset) {
  bgezal(zero_reg, offset);
}


void Assembler::bc(int32_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrImmediate(BC, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::balc(int32_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrImmediate(BALC, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beq(Register rs, Register rt, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BEQ, rs, rt, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgezc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZL, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgeuc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZ, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgec(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZL, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezal(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6 || rs.is(zero_reg));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZAL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgtz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BGTZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgtzc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZL, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::blez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BLEZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::blezc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZL, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltzc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!rt.is(zero_reg));
  GenInstrImmediate(BGTZL, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltuc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZ, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!rs.is(zero_reg));
  DCHECK(!rt.is(zero_reg));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZL, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzal(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6 || rs.is(zero_reg));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZAL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bne(Register rs, Register rt, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BNE, rs, rt, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bovc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  if (rs.code() >= rt.code()) {
    GenInstrImmediate(ADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(ADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::bnvc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  if (rs.code() >= rt.code()) {
    GenInstrImmediate(DADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(DADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::blezalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZ, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZ, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezall(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZALL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZ, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgtzalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZ, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beqzalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(ADDI, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bnezalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(DADDI, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beqc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.code() != rt.code() && rs.code() != 0 && rt.code() != 0);
  if (rs.code() < rt.code()) {
    GenInstrImmediate(ADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(ADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::beqzc(Register rs, int32_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  GenInstrImmediate(POP66, rs, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bnec(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.code() != rt.code() && rs.code() != 0 && rt.code() != 0);
  if (rs.code() < rt.code()) {
    GenInstrImmediate(DADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(DADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::bnezc(Register rs, int32_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  GenInstrImmediate(POP76, rs, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::j(int64_t target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrJump(J, static_cast<uint32_t>(target >> 2) & kImm26Mask);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::j(Label* target) {
  uint64_t imm = jump_offset(target);
  if (target->is_bound()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    GenInstrJump(static_cast<Opcode>(kJRawMark),
                 static_cast<uint32_t>(imm >> 2) & kImm26Mask);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    j(imm);
  }
}


void Assembler::jal(Label* target) {
  uint64_t imm = jump_offset(target);
  if (target->is_bound()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    GenInstrJump(static_cast<Opcode>(kJalRawMark),
                 static_cast<uint32_t>(imm >> 2) & kImm26Mask);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    jal(imm);
  }
}


void Assembler::jr(Register rs) {
  if (kArchVariant != kMips64r6) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    GenInstrRegister(SPECIAL, rs, zero_reg, zero_reg, 0, JR);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    jalr(rs, zero_reg);
  }
}


void Assembler::jal(int64_t target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrJump(JAL, static_cast<uint32_t>(target >> 2) & kImm26Mask);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jalr(Register rs, Register rd) {
  DCHECK(rs.code() != rd.code());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrRegister(SPECIAL, rs, zero_reg, rd, 0, JALR);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jic(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrImmediate(POP66, zero_reg, rt, offset);
}


void Assembler::jialc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrImmediate(POP76, zero_reg, rt, offset);
}


// -------Data-processing-instructions---------

// Arithmetic.

void Assembler::addu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, ADDU);
}


void Assembler::addiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(ADDIU, rs, rd, j);
}


void Assembler::subu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SUBU);
}


void Assembler::mul(Register rd, Register rs, Register rt) {
  if (kArchVariant == kMips64r6) {
      GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH);
  } else {
      GenInstrRegister(SPECIAL2, rs, rt, rd, 0, MUL);
  }
}


void Assembler::muh(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH);
}


void Assembler::mulu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH_U);
}


void Assembler::muhu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH_U);
}


void Assembler::dmul(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, D_MUL_MUH);
}


void Assembler::dmuh(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, D_MUL_MUH);
}


void Assembler::dmulu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, D_MUL_MUH_U);
}


void Assembler::dmuhu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, D_MUL_MUH_U);
}


void Assembler::mult(Register rs, Register rt) {
  DCHECK(kArchVariant != kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULT);
}


void Assembler::multu(Register rs, Register rt) {
  DCHECK(kArchVariant != kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULTU);
}


void Assembler::daddiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(DADDIU, rs, rd, j);
}


void Assembler::div(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIV);
}


void Assembler::div(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD);
}


void Assembler::mod(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD);
}


void Assembler::divu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIVU);
}


void Assembler::divu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD_U);
}


void Assembler::modu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD_U);
}


void Assembler::daddu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DADDU);
}


void Assembler::dsubu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSUBU);
}


void Assembler::dmult(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DMULT);
}


void Assembler::dmultu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DMULTU);
}


void Assembler::ddiv(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DDIV);
}


void Assembler::ddiv(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, D_DIV_MOD);
}


void Assembler::dmod(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, D_DIV_MOD);
}


void Assembler::ddivu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DDIVU);
}


void Assembler::ddivu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, D_DIV_MOD_U);
}


void Assembler::dmodu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, D_DIV_MOD_U);
}


// Logical.

void Assembler::and_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, AND);
}


void Assembler::andi(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(ANDI, rs, rt, j);
}


void Assembler::or_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, OR);
}


void Assembler::ori(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(ORI, rs, rt, j);
}


void Assembler::xor_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, XOR);
}


void Assembler::xori(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(XORI, rs, rt, j);
}


void Assembler::nor(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, NOR);
}


// Shifts.
void Assembler::sll(Register rd,
                    Register rt,
                    uint16_t sa,
                    bool coming_from_nop) {
  // Don't allow nop instructions in the form sll zero_reg, zero_reg to be
  // generated using the sll instruction. They must be generated using
  // nop(int/NopMarkerTypes) or MarkCode(int/NopMarkerTypes) pseudo
  // instructions.
  DCHECK(coming_from_nop || !(rd.is(zero_reg) && rt.is(zero_reg)));
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SLL);
}


void Assembler::sllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLLV);
}


void Assembler::srl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SRL);
}


void Assembler::srlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRLV);
}


void Assembler::sra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SRA);
}


void Assembler::srav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRAV);
}


void Assembler::rotr(Register rd, Register rt, uint16_t sa) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | SRL;
  emit(instr);
}


void Assembler::rotrv(Register rd, Register rt, Register rs) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
     | (rd.code() << kRdShift) | (1 << kSaShift) | SRLV;
  emit(instr);
}


void Assembler::dsll(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSLL);
}


void Assembler::dsllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSLLV);
}


void Assembler::dsrl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRL);
}


void Assembler::dsrlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSRLV);
}


void Assembler::drotr(Register rd, Register rt, uint16_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | DSRL;
  emit(instr);
}

void Assembler::drotr32(Register rd, Register rt, uint16_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift) |
                (rd.code() << kRdShift) | (sa << kSaShift) | DSRL32;
  emit(instr);
}

void Assembler::drotrv(Register rd, Register rt, Register rs) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid() );
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (1 << kSaShift) | DSRLV;
  emit(instr);
}


void Assembler::dsra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRA);
}


void Assembler::dsrav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSRAV);
}


void Assembler::dsll32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSLL32);
}


void Assembler::dsrl32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRL32);
}


void Assembler::dsra32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRA32);
}


void Assembler::lsa(Register rd, Register rt, Register rs, uint8_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK(sa <= 3);
  DCHECK(kArchVariant == kMips64r6);
  Instr instr = SPECIAL | rs.code() << kRsShift | rt.code() << kRtShift |
                rd.code() << kRdShift | sa << kSaShift | LSA;
  emit(instr);
}


void Assembler::dlsa(Register rd, Register rt, Register rs, uint8_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK(sa <= 3);
  DCHECK(kArchVariant == kMips64r6);
  Instr instr = SPECIAL | rs.code() << kRsShift | rt.code() << kRtShift |
                rd.code() << kRdShift | sa << kSaShift | DLSA;
  emit(instr);
}


// ------------Memory-instructions-------------

// Helper for base-reg + offset, when offset is larger than int16.
void Assembler::LoadRegPlusOffsetToAt(const MemOperand& src) {
  DCHECK(!src.rm().is(at));
  DCHECK(is_int32(src.offset_));

  if (kArchVariant == kMips64r6) {
    int32_t hi = (src.offset_ >> kLuiShift) & kImm16Mask;
    if (src.offset_ & kNegOffset) {
      if ((hi & kNegOffset) != ((hi + 1) & kNegOffset)) {
        lui(at, (src.offset_ >> kLuiShift) & kImm16Mask);
        ori(at, at, src.offset_ & kImm16Mask);  // Load 32-bit offset.
        daddu(at, at, src.rm());                // Add base register.
        return;
      }

      hi += 1;
    }

    daui(at, src.rm(), hi);
    daddiu(at, at, src.offset_ & kImm16Mask);
  } else {
    lui(at, (src.offset_ >> kLuiShift) & kImm16Mask);
    ori(at, at, src.offset_ & kImm16Mask);  // Load 32-bit offset.
    daddu(at, at, src.rm());                // Add base register.
  }
}

// Helper for base-reg + upper part of offset, when offset is larger than int16.
// Loads higher part of the offset to AT register.
// Returns lower part of the offset to be used as offset
// in Load/Store instructions
int32_t Assembler::LoadRegPlusUpperOffsetPartToAt(const MemOperand& src) {
  DCHECK(!src.rm().is(at));
  DCHECK(is_int32(src.offset_));
  int32_t hi = (src.offset_ >> kLuiShift) & kImm16Mask;
  // If the highest bit of the lower part of the offset is 1, this would make
  // the offset in the load/store instruction negative. We need to compensate
  // for this by adding 1 to the upper part of the offset.
  if (src.offset_ & kNegOffset) {
    if ((hi & kNegOffset) != ((hi + 1) & kNegOffset)) {
      LoadRegPlusOffsetToAt(src);
      return 0;
    }

    hi += 1;
  }

  if (kArchVariant == kMips64r6) {
    daui(at, src.rm(), hi);
  } else {
    lui(at, hi);
    daddu(at, at, src.rm());
  }
  return (src.offset_ & kImm16Mask);
}

void Assembler::lb(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LB, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LB, at, rd, off16);
  }
}


void Assembler::lbu(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LBU, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LBU, at, rd, off16);
  }
}


void Assembler::lh(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LH, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LH, at, rd, off16);
  }
}


void Assembler::lhu(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LHU, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LHU, at, rd, off16);
  }
}


void Assembler::lw(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LW, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LW, at, rd, off16);
  }
}


void Assembler::lwu(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LWU, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LWU, at, rd, off16);
  }
}


void Assembler::lwl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(LWL, rs.rm(), rd, rs.offset_);
}


void Assembler::lwr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(LWR, rs.rm(), rd, rs.offset_);
}


void Assembler::sb(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SB, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(SB, at, rd, off16);
  }
}


void Assembler::sh(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SH, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(SH, at, rd, off16);
  }
}


void Assembler::sw(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SW, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(SW, at, rd, off16);
  }
}


void Assembler::swl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(SWL, rs.rm(), rd, rs.offset_);
}


void Assembler::swr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(SWR, rs.rm(), rd, rs.offset_);
}


void Assembler::lui(Register rd, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, zero_reg, rd, j);
}


void Assembler::aui(Register rt, Register rs, int32_t j) {
  // This instruction uses same opcode as 'lui'. The difference in encoding is
  // 'lui' has zero reg. for rs field.
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, rs, rt, j);
}


void Assembler::daui(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  DCHECK(!rs.is(zero_reg));
  GenInstrImmediate(DAUI, rs, rt, j);
}


void Assembler::dahi(Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(REGIMM, rs, DAHI, j);
}


void Assembler::dati(Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(REGIMM, rs, DATI, j);
}


void Assembler::ldl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(LDL, rs.rm(), rd, rs.offset_);
}


void Assembler::ldr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(LDR, rs.rm(), rd, rs.offset_);
}


void Assembler::sdl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(SDL, rs.rm(), rd, rs.offset_);
}


void Assembler::sdr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK(kArchVariant == kMips64r2);
  GenInstrImmediate(SDR, rs.rm(), rd, rs.offset_);
}


void Assembler::ld(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LD, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(LD, at, rd, off16);
  }
}


void Assembler::sd(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SD, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(rs);
    GenInstrImmediate(SD, at, rd, off16);
  }
}


// ---------PC-Relative instructions-----------

void Assembler::addiupc(Register rs, int32_t imm19) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.is_valid() && is_int19(imm19));
  uint32_t imm21 = ADDIUPC << kImm19Bits | (imm19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::lwpc(Register rs, int32_t offset19) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.is_valid() && is_int19(offset19));
  uint32_t imm21 = LWPC << kImm19Bits | (offset19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::lwupc(Register rs, int32_t offset19) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.is_valid() && is_int19(offset19));
  uint32_t imm21 = LWUPC << kImm19Bits | (offset19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::ldpc(Register rs, int32_t offset18) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.is_valid() && is_int18(offset18));
  uint32_t imm21 = LDPC << kImm18Bits | (offset18 & kImm18Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::auipc(Register rs, int16_t imm16) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.is_valid());
  uint32_t imm21 = AUIPC << kImm16Bits | (imm16 & kImm16Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::aluipc(Register rs, int16_t imm16) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.is_valid());
  uint32_t imm21 = ALUIPC << kImm16Bits | (imm16 & kImm16Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


// -------------Misc-instructions--------------

// Break / Trap instructions.
void Assembler::break_(uint32_t code, bool break_as_stop) {
  DCHECK((code & ~0xfffff) == 0);
  // We need to invalidate breaks that could be stops as well because the
  // simulator expects a char pointer after the stop instruction.
  // See constants-mips.h for explanation.
  DCHECK((break_as_stop &&
          code <= kMaxStopCode &&
          code > kMaxWatchpointCode) ||
         (!break_as_stop &&
          (code > kMaxStopCode ||
           code <= kMaxWatchpointCode)));
  Instr break_instr = SPECIAL | BREAK | (code << 6);
  emit(break_instr);
}


void Assembler::stop(const char* msg, uint32_t code) {
  DCHECK(code > kMaxWatchpointCode);
  DCHECK(code <= kMaxStopCode);
#if defined(V8_HOST_ARCH_MIPS) || defined(V8_HOST_ARCH_MIPS64)
  break_(0x54321);
#else  // V8_HOST_ARCH_MIPS
  BlockTrampolinePoolFor(3);
  // The Simulator will handle the stop instruction and get the message address.
  // On MIPS stop() is just a special kind of break_().
  break_(code, true);
  // Do not embed the message string address! We used to do this, but that
  // made snapshots created from position-independent executable builds
  // non-deterministic.
  // TODO(yangguo): remove this field entirely.
  nop();
#endif
}


void Assembler::tge(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr = SPECIAL | TGE | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tgeu(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr = SPECIAL | TGEU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tlt(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TLT | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tltu(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TLTU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::teq(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TEQ | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tne(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TNE | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}

void Assembler::sync() {
  Instr sync_instr = SPECIAL | SYNC;
  emit(sync_instr);
}

// Move from HI/LO register.

void Assembler::mfhi(Register rd) {
  GenInstrRegister(SPECIAL, zero_reg, zero_reg, rd, 0, MFHI);
}


void Assembler::mflo(Register rd) {
  GenInstrRegister(SPECIAL, zero_reg, zero_reg, rd, 0, MFLO);
}


// Set on less than instructions.
void Assembler::slt(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLT);
}


void Assembler::sltu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLTU);
}


void Assembler::slti(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(SLTI, rs, rt, j);
}


void Assembler::sltiu(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(SLTIU, rs, rt, j);
}


// Conditional move.
void Assembler::movz(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVZ);
}


void Assembler::movn(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVN);
}


void Assembler::movt(Register rd, Register rs, uint16_t cc) {
  Register rt;
  rt.reg_code = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::movf(Register rd, Register rs, uint16_t cc) {
  Register rt;
  rt.reg_code = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::min_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  min(S, fd, fs, ft);
}


void Assembler::min_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  min(D, fd, fs, ft);
}


void Assembler::max_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  max(S, fd, fs, ft);
}


void Assembler::max_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  max(D, fd, fs, ft);
}


void Assembler::mina_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  mina(S, fd, fs, ft);
}


void Assembler::mina_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  mina(D, fd, fs, ft);
}


void Assembler::maxa_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  maxa(S, fd, fs, ft);
}


void Assembler::maxa_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  maxa(D, fd, fs, ft);
}


void Assembler::max(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAX);
}


void Assembler::min(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MIN);
}


// GPR.
void Assembler::seleqz(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELEQZ_S);
}


// GPR.
void Assembler::selnez(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELNEZ_S);
}


// Bit twiddling.
void Assembler::clz(Register rd, Register rs) {
  if (kArchVariant != kMips64r6) {
    // Clz instr requires same GPR number in 'rd' and 'rt' fields.
    GenInstrRegister(SPECIAL2, rs, rd, rd, 0, CLZ);
  } else {
    GenInstrRegister(SPECIAL, rs, zero_reg, rd, 1, CLZ_R6);
  }
}


void Assembler::dclz(Register rd, Register rs) {
  if (kArchVariant != kMips64r6) {
    // dclz instr requires same GPR number in 'rd' and 'rt' fields.
    GenInstrRegister(SPECIAL2, rs, rd, rd, 0, DCLZ);
  } else {
    GenInstrRegister(SPECIAL, rs, zero_reg, rd, 1, DCLZ_R6);
  }
}


void Assembler::ins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ins.
  // Ins instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK((kArchVariant == kMips64r2) || (kArchVariant == kMips64r6));
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, INS);
}


void Assembler::dins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dins.
  // Dext instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, DINS);
}


void Assembler::ext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ext.
  // Ext instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, EXT);
}


void Assembler::dext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dext.
  // Dext instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, DEXT);
}


void Assembler::dextm(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dextm.
  // Dextm instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1 - 32, pos, DEXTM);
}


void Assembler::dextu(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dextu.
  // Dext instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos - 32, DEXTU);
}


void Assembler::bitswap(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, 0, BSHFL);
}


void Assembler::dbitswap(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, 0, DBSHFL);
}


void Assembler::pref(int32_t hint, const MemOperand& rs) {
  DCHECK(is_uint5(hint) && is_uint16(rs.offset_));
  Instr instr = PREF | (rs.rm().code() << kRsShift) | (hint << kRtShift)
      | (rs.offset_);
  emit(instr);
}


void Assembler::align(Register rd, Register rs, Register rt, uint8_t bp) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(is_uint3(bp));
  uint16_t sa = (ALIGN << kBp2Bits) | bp;
  GenInstrRegister(SPECIAL3, rs, rt, rd, sa, BSHFL);
}


void Assembler::dalign(Register rd, Register rs, Register rt, uint8_t bp) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(is_uint3(bp));
  uint16_t sa = (DALIGN << kBp3Bits) | bp;
  GenInstrRegister(SPECIAL3, rs, rt, rd, sa, DBSHFL);
}

void Assembler::wsbh(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, WSBH, BSHFL);
}

void Assembler::dsbh(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, DSBH, DBSHFL);
}

void Assembler::dshd(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, DSHD, DBSHFL);
}

void Assembler::seh(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, SEH, BSHFL);
}

void Assembler::seb(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, SEB, BSHFL);
}

// --------Coprocessor-instructions----------------

// Load, store, move.
void Assembler::lwc1(FPURegister fd, const MemOperand& src) {
  if (is_int16(src.offset_)) {
    GenInstrImmediate(LWC1, src.rm(), fd, src.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(src);
    GenInstrImmediate(LWC1, at, fd, off16);
  }
}


void Assembler::ldc1(FPURegister fd, const MemOperand& src) {
  if (is_int16(src.offset_)) {
    GenInstrImmediate(LDC1, src.rm(), fd, src.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(src);
    GenInstrImmediate(LDC1, at, fd, off16);
  }
}


void Assembler::swc1(FPURegister fd, const MemOperand& src) {
  if (is_int16(src.offset_)) {
    GenInstrImmediate(SWC1, src.rm(), fd, src.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(src);
    GenInstrImmediate(SWC1, at, fd, off16);
  }
}


void Assembler::sdc1(FPURegister fd, const MemOperand& src) {
  DCHECK(!src.rm().is(at));
  if (is_int16(src.offset_)) {
    GenInstrImmediate(SDC1, src.rm(), fd, src.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    int32_t off16 = LoadRegPlusUpperOffsetPartToAt(src);
    GenInstrImmediate(SDC1, at, fd, off16);
  }
}


void Assembler::mtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTC1, rt, fs, f0);
}


void Assembler::mthc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTHC1, rt, fs, f0);
}


void Assembler::dmtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, DMTC1, rt, fs, f0);
}


void Assembler::mfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFC1, rt, fs, f0);
}


void Assembler::mfhc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFHC1, rt, fs, f0);
}


void Assembler::dmfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, DMFC1, rt, fs, f0);
}


void Assembler::ctc1(Register rt, FPUControlRegister fs) {
  GenInstrRegister(COP1, CTC1, rt, fs);
}


void Assembler::cfc1(Register rt, FPUControlRegister fs) {
  GenInstrRegister(COP1, CFC1, rt, fs);
}


void Assembler::DoubleAsTwoUInt32(double d, uint32_t* lo, uint32_t* hi) {
  uint64_t i;
  memcpy(&i, &d, 8);

  *lo = i & 0xffffffff;
  *hi = i >> 32;
}


void Assembler::sel(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));

  GenInstrRegister(COP1, fmt, ft, fs, fd, SEL);
}


void Assembler::sel_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  sel(S, fd, fs, ft);
}


void Assembler::sel_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  sel(D, fd, fs, ft);
}


// FPR.
void Assembler::seleqz(SecondaryField fmt, FPURegister fd, FPURegister fs,
                       FPURegister ft) {
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, SELEQZ_C);
}


void Assembler::seleqz_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  seleqz(D, fd, fs, ft);
}


void Assembler::seleqz_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  seleqz(S, fd, fs, ft);
}


void Assembler::selnez_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  selnez(D, fd, fs, ft);
}


void Assembler::selnez_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  selnez(S, fd, fs, ft);
}


void Assembler::movz_s(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, S, rt, fs, fd, MOVZ_C);
}


void Assembler::movz_d(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, D, rt, fs, fd, MOVZ_C);
}


void Assembler::movt_s(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(kArchVariant == kMips64r2);
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(COP1, S, ft, fs, fd, MOVF);
}


void Assembler::movt_d(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(kArchVariant == kMips64r2);
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(COP1, D, ft, fs, fd, MOVF);
}


void Assembler::movf_s(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(kArchVariant == kMips64r2);
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(COP1, S, ft, fs, fd, MOVF);
}


void Assembler::movf_d(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(kArchVariant == kMips64r2);
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(COP1, D, ft, fs, fd, MOVF);
}


void Assembler::movn_s(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, S, rt, fs, fd, MOVN_C);
}


void Assembler::movn_d(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, D, rt, fs, fd, MOVN_C);
}


// FPR.
void Assembler::selnez(SecondaryField fmt, FPURegister fd, FPURegister fs,
                       FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, SELNEZ_C);
}


// Arithmetic.

void Assembler::add_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, ADD_D);
}


void Assembler::add_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, ADD_D);
}


void Assembler::sub_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, SUB_D);
}


void Assembler::sub_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, SUB_D);
}


void Assembler::mul_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, MUL_D);
}


void Assembler::mul_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, MUL_D);
}

void Assembler::madd_s(FPURegister fd, FPURegister fr, FPURegister fs,
                       FPURegister ft) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1X, fr, ft, fs, fd, MADD_S);
}

void Assembler::madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
    FPURegister ft) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1X, fr, ft, fs, fd, MADD_D);
}

void Assembler::msub_s(FPURegister fd, FPURegister fr, FPURegister fs,
                       FPURegister ft) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1X, fr, ft, fs, fd, MSUB_S);
}

void Assembler::msub_d(FPURegister fd, FPURegister fr, FPURegister fs,
                       FPURegister ft) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1X, fr, ft, fs, fd, MSUB_D);
}

void Assembler::maddf_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, ft, fs, fd, MADDF_S);
}

void Assembler::maddf_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, ft, fs, fd, MADDF_D);
}

void Assembler::msubf_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, ft, fs, fd, MSUBF_S);
}

void Assembler::msubf_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, ft, fs, fd, MSUBF_D);
}

void Assembler::div_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, DIV_D);
}


void Assembler::div_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, DIV_D);
}


void Assembler::abs_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ABS_D);
}


void Assembler::abs_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ABS_D);
}


void Assembler::mov_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, MOV_D);
}


void Assembler::mov_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, MOV_S);
}


void Assembler::neg_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, NEG_D);
}


void Assembler::neg_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, NEG_D);
}


void Assembler::sqrt_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, SQRT_D);
}


void Assembler::sqrt_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, SQRT_D);
}


void Assembler::rsqrt_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, RSQRT_S);
}


void Assembler::rsqrt_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, RSQRT_D);
}


void Assembler::recip_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, RECIP_D);
}


void Assembler::recip_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, RECIP_S);
}


// Conversions.
void Assembler::cvt_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_W_S);
}


void Assembler::cvt_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_W_D);
}


void Assembler::trunc_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_W_S);
}


void Assembler::trunc_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_W_D);
}


void Assembler::round_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_W_S);
}


void Assembler::round_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_W_D);
}


void Assembler::floor_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_W_S);
}


void Assembler::floor_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_W_D);
}


void Assembler::ceil_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_W_S);
}


void Assembler::ceil_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_W_D);
}


void Assembler::rint_s(FPURegister fd, FPURegister fs) { rint(S, fd, fs); }


void Assembler::rint_d(FPURegister fd, FPURegister fs) { rint(D, fd, fs); }


void Assembler::rint(SecondaryField fmt, FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, fmt, f0, fs, fd, RINT);
}


void Assembler::cvt_l_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_L_S);
}


void Assembler::cvt_l_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_L_D);
}


void Assembler::trunc_l_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_L_S);
}


void Assembler::trunc_l_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_L_D);
}


void Assembler::round_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_L_S);
}


void Assembler::round_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_L_D);
}


void Assembler::floor_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_L_S);
}


void Assembler::floor_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_L_D);
}


void Assembler::ceil_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_L_S);
}


void Assembler::ceil_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_L_D);
}


void Assembler::class_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, f0, fs, fd, CLASS_S);
}


void Assembler::class_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, f0, fs, fd, CLASS_D);
}


void Assembler::mina(SecondaryField fmt, FPURegister fd, FPURegister fs,
                     FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MINA);
}


void Assembler::maxa(SecondaryField fmt, FPURegister fd, FPURegister fs,
                     FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAXA);
}


void Assembler::cvt_s_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_S_W);
}


void Assembler::cvt_s_l(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_S_L);
}


void Assembler::cvt_s_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_S_D);
}


void Assembler::cvt_d_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_D_W);
}


void Assembler::cvt_d_l(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_D_L);
}


void Assembler::cvt_d_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_D_S);
}


// Conditions for >= MIPSr6.
void Assembler::cmp(FPUCondition cond, SecondaryField fmt,
    FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | (0 << 5) | cond;
  emit(instr);
}


void Assembler::cmp_s(FPUCondition cond, FPURegister fd, FPURegister fs,
                      FPURegister ft) {
  cmp(cond, W, fd, fs, ft);
}

void Assembler::cmp_d(FPUCondition cond, FPURegister fd, FPURegister fs,
                      FPURegister ft) {
  cmp(cond, L, fd, fs, ft);
}


void Assembler::bc1eqz(int16_t offset, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  Instr instr = COP1 | BC1EQZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1nez(int16_t offset, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  Instr instr = COP1 | BC1NEZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


// Conditions for < MIPSr6.
void Assembler::c(FPUCondition cond, SecondaryField fmt,
    FPURegister fs, FPURegister ft, uint16_t cc) {
  DCHECK(kArchVariant != kMips64r6);
  DCHECK(is_uint3(cc));
  DCHECK(fmt == S || fmt == D);
  DCHECK((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift | fs.code() << kFsShift
      | cc << 8 | 3 << 4 | cond;
  emit(instr);
}


void Assembler::c_s(FPUCondition cond, FPURegister fs, FPURegister ft,
                    uint16_t cc) {
  c(cond, S, fs, ft, cc);
}


void Assembler::c_d(FPUCondition cond, FPURegister fs, FPURegister ft,
                    uint16_t cc) {
  c(cond, D, fs, ft, cc);
}


void Assembler::fcmp(FPURegister src1, const double src2,
      FPUCondition cond) {
  DCHECK(src2 == 0.0);
  mtc1(zero_reg, f14);
  cvt_d_w(f14, f14);
  c(cond, D, src1, f14, 0);
}


void Assembler::bc1f(int16_t offset, uint16_t cc) {
  DCHECK(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 0 << 16 | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1t(int16_t offset, uint16_t cc) {
  DCHECK(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 1 << 16 | (offset & kImm16Mask);
  emit(instr);
}


int Assembler::RelocateInternalReference(RelocInfo::Mode rmode, byte* pc,
                                         intptr_t pc_delta) {
  if (RelocInfo::IsInternalReference(rmode)) {
    int64_t* p = reinterpret_cast<int64_t*>(pc);
    if (*p == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    *p += pc_delta;
    return 2;  // Number of instructions patched.
  }
  Instr instr = instr_at(pc);
  DCHECK(RelocInfo::IsInternalReferenceEncoded(rmode));
  if (IsLui(instr)) {
    Instr instr_lui = instr_at(pc + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pc + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pc + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));
    // TODO(plind): symbolic names for the shifts.
    int64_t imm = (instr_lui & static_cast<int64_t>(kImm16Mask)) << 48;
    imm |= (instr_ori & static_cast<int64_t>(kImm16Mask)) << 32;
    imm |= (instr_ori2 & static_cast<int64_t>(kImm16Mask)) << 16;
    // Sign extend address.
    imm >>= 16;

    if (imm == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    imm += pc_delta;
    DCHECK((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;
    instr_ori2 &= ~kImm16Mask;

    instr_at_put(pc + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> 32) & kImm16Mask));
    instr_at_put(pc + 1 * Assembler::kInstrSize,
                 instr_ori | (imm >> 16 & kImm16Mask));
    instr_at_put(pc + 3 * Assembler::kInstrSize,
                 instr_ori2 | (imm & kImm16Mask));
    return 4;  // Number of instructions patched.
  } else if (IsJ(instr) || IsJal(instr)) {
    // Regular j/jal relocation.
    uint32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    imm28 += pc_delta;
    imm28 &= kImm28Mask;
    instr &= ~kImm26Mask;
    DCHECK((imm28 & 3) == 0);
    uint32_t imm26 = static_cast<uint32_t>(imm28 >> 2);
    instr_at_put(pc, instr | (imm26 & kImm26Mask));
    return 1;  // Number of instructions patched.
  } else {
    DCHECK(((instr & kJumpRawMask) == kJRawMark) ||
           ((instr & kJumpRawMask) == kJalRawMark));
    // Unbox raw offset and emit j/jal.
    int32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    // Sign extend 28-bit offset to 32-bit.
    imm28 = (imm28 << 4) >> 4;
    uint64_t target =
        static_cast<int64_t>(imm28) + reinterpret_cast<uint64_t>(pc);
    target &= kImm28Mask;
    DCHECK((imm28 & 3) == 0);
    uint32_t imm26 = static_cast<uint32_t>(target >> 2);
    // Check markings whether to emit j or jal.
    uint32_t unbox = (instr & kJRawMark) ? J : JAL;
    instr_at_put(pc, unbox | (imm26 & kImm26Mask));
    return 1;  // Number of instructions patched.
  }
}


void Assembler::GrowBuffer() {
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // The new buffer.
  if (buffer_size_ < 1 * MB) {
    desc.buffer_size = 2*buffer_size_;
  } else {
    desc.buffer_size = buffer_size_ + 1*MB;
  }
  CHECK_GT(desc.buffer_size, 0);  // No overflow.

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;

  desc.instr_size = pc_offset();
  desc.reloc_size =
      static_cast<int>((buffer_ + buffer_size_) - reloc_info_writer.pos());

  // Copy the data.
  intptr_t pc_delta = desc.buffer - buffer_;
  intptr_t rc_delta = (desc.buffer + desc.buffer_size) -
      (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(reloc_info_writer.pos() + rc_delta,
              reloc_info_writer.pos(), desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate runtime entries.
  for (RelocIterator it(desc); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::INTERNAL_REFERENCE) {
      byte* p = reinterpret_cast<byte*>(it.rinfo()->pc());
      RelocateInternalReference(rmode, p, pc_delta);
    }
  }
  DCHECK(!overflow());
}


void Assembler::db(uint8_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dd(uint32_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dq(uint64_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dd(Label* label) {
  uint64_t data;
  CheckForEmitInForbiddenSlot();
  if (label->is_bound()) {
    data = reinterpret_cast<uint64_t>(buffer_ + label->pos());
  } else {
    data = jump_address(label);
    unbound_labels_count_++;
    internal_reference_positions_.insert(label->pos());
  }
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  EmitHelper(data);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(isolate(), pc_, rmode, data, NULL);
  if (rmode >= RelocInfo::COMMENT &&
      rmode <= RelocInfo::DEBUG_BREAK_SLOT_AT_TAIL_CALL) {
    // Adjust code for new modes.
    DCHECK(RelocInfo::IsDebugBreakSlot(rmode) || RelocInfo::IsComment(rmode));
    // These modes do not need an entry in the constant pool.
  }
  if (!RelocInfo::IsNone(rinfo.rmode())) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
        !serializer_enabled() && !emit_debug_code()) {
      return;
    }
    DCHECK(buffer_space() >= kMaxRelocSize);  // Too late to grow buffer here.
    if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
      RelocInfo reloc_info_with_ast_id(isolate(), pc_, rmode,
                                       RecordedAstId().ToInt(), NULL);
      ClearRecordedAstId();
      reloc_info_writer.Write(&reloc_info_with_ast_id);
    } else {
      reloc_info_writer.Write(&rinfo);
    }
  }
}


void Assembler::BlockTrampolinePoolFor(int instructions) {
  CheckTrampolinePoolQuick(instructions);
  BlockTrampolinePoolBefore(pc_offset() + instructions * kInstrSize);
}


void Assembler::CheckTrampolinePool() {
  // Some small sequences of instructions must not be broken up by the
  // insertion of a trampoline pool; such sequences are protected by setting
  // either trampoline_pool_blocked_nesting_ or no_trampoline_pool_before_,
  // which are both checked here. Also, recursive calls to CheckTrampolinePool
  // are blocked by trampoline_pool_blocked_nesting_.
  if ((trampoline_pool_blocked_nesting_ > 0) ||
      (pc_offset() < no_trampoline_pool_before_)) {
    // Emission is currently blocked; make sure we try again as soon as
    // possible.
    if (trampoline_pool_blocked_nesting_ > 0) {
      next_buffer_check_ = pc_offset() + kInstrSize;
    } else {
      next_buffer_check_ = no_trampoline_pool_before_;
    }
    return;
  }

  DCHECK(!trampoline_emitted_);
  DCHECK(unbound_labels_count_ >= 0);
  if (unbound_labels_count_ > 0) {
    // First we emit jump (2 instructions), then we emit trampoline pool.
    { BlockTrampolinePoolScope block_trampoline_pool(this);
      Label after_pool;
      if (kArchVariant == kMips64r6) {
        bc(&after_pool);
      } else {
        b(&after_pool);
      }
      nop();

      int pool_start = pc_offset();
      for (int i = 0; i < unbound_labels_count_; i++) {
        { BlockGrowBufferScope block_buf_growth(this);
          // Buffer growth (and relocation) must be blocked for internal
          // references until associated instructions are emitted and available
          // to be patched.
          RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE_ENCODED);
          j(&after_pool);
        }
        nop();
      }
      bind(&after_pool);
      trampoline_ = Trampoline(pool_start, unbound_labels_count_);

      trampoline_emitted_ = true;
      // As we are only going to emit trampoline once, we need to prevent any
      // further emission.
      next_buffer_check_ = kMaxInt;
    }
  } else {
    // Number of branches to unbound label at this point is zero, so we can
    // move next buffer check to maximum.
    next_buffer_check_ = pc_offset() +
        kMaxBranchOffset - kTrampolineSlotsSize * 16;
  }
  return;
}


Address Assembler::target_address_at(Address pc) {
  Instr instr0 = instr_at(pc);
  Instr instr1 = instr_at(pc + 1 * kInstrSize);
  Instr instr3 = instr_at(pc + 3 * kInstrSize);

  // Interpret 4 instructions for address generated by li: See listing in
  // Assembler::set_target_address_at() just below.
  if ((GetOpcodeField(instr0) == LUI) && (GetOpcodeField(instr1) == ORI) &&
      (GetOpcodeField(instr3) == ORI)) {
    // Assemble the 48 bit value.
     int64_t addr  = static_cast<int64_t>(
          ((uint64_t)(GetImmediate16(instr0)) << 32) |
          ((uint64_t)(GetImmediate16(instr1)) << 16) |
          ((uint64_t)(GetImmediate16(instr3))));

    // Sign extend to get canonical address.
    addr = (addr << 16) >> 16;
    return reinterpret_cast<Address>(addr);
  }
  // We should never get here, force a bad address if we do.
  UNREACHABLE();
  return (Address)0x0;
}


// MIPS and ia32 use opposite encoding for qNaN and sNaN, such that ia32
// qNaN is a MIPS sNaN, and ia32 sNaN is MIPS qNaN. If running from a heap
// snapshot generated on ia32, the resulting MIPS sNaN must be quieted.
// OS::nan_value() returns a qNaN.
void Assembler::QuietNaN(HeapObject* object) {
  HeapNumber::cast(object)->set_value(std::numeric_limits<double>::quiet_NaN());
}


// On Mips64, a target address is stored in a 4-instruction sequence:
//    0: lui(rd, (j.imm64_ >> 32) & kImm16Mask);
//    1: ori(rd, rd, (j.imm64_ >> 16) & kImm16Mask);
//    2: dsll(rd, rd, 16);
//    3: ori(rd, rd, j.imm32_ & kImm16Mask);
//
// Patching the address must replace all the lui & ori instructions,
// and flush the i-cache.
//
// There is an optimization below, which emits a nop when the address
// fits in just 16 bits. This is unlikely to help, and should be benchmarked,
// and possibly removed.
void Assembler::set_target_address_at(Isolate* isolate, Address pc,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
// There is an optimization where only 4 instructions are used to load address
// in code on MIP64 because only 48-bits of address is effectively used.
// It relies on fact the upper [63:48] bits are not used for virtual address
// translation and they have to be set according to value of bit 47 in order
// get canonical address.
  Instr instr1 = instr_at(pc + kInstrSize);
  uint32_t rt_code = GetRt(instr1);
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
  uint64_t itarget = reinterpret_cast<uint64_t>(target);

#ifdef DEBUG
  // Check we have the result from a li macro-instruction.
  Instr instr0 = instr_at(pc);
  Instr instr3 = instr_at(pc + kInstrSize * 3);
  CHECK((GetOpcodeField(instr0) == LUI && GetOpcodeField(instr1) == ORI &&
         GetOpcodeField(instr3) == ORI));
#endif

  // Must use 4 instructions to insure patchable code.
  // lui rt, upper-16.
  // ori rt, rt, lower-16.
  // dsll rt, rt, 16.
  // ori rt rt, lower-16.
  *p = LUI | (rt_code << kRtShift) | ((itarget >> 32) & kImm16Mask);
  *(p + 1) = ORI | (rt_code << kRtShift) | (rt_code << kRsShift)
      | ((itarget >> 16) & kImm16Mask);
  *(p + 3) = ORI | (rt_code << kRsShift) | (rt_code << kRtShift)
      | (itarget & kImm16Mask);

  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(isolate, pc, 4 * Assembler::kInstrSize);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
