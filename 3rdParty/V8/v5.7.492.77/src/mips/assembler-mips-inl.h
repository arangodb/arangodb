
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


#ifndef V8_MIPS_ASSEMBLER_MIPS_INL_H_
#define V8_MIPS_ASSEMBLER_MIPS_INL_H_

#include "src/mips/assembler-mips.h"

#include "src/assembler.h"
#include "src/debug/debug.h"


namespace v8 {
namespace internal {


bool CpuFeatures::SupportsCrankshaft() { return IsSupported(FPU); }

bool CpuFeatures::SupportsSimd128() { return false; }

// -----------------------------------------------------------------------------
// Operand and MemOperand.

Operand::Operand(int32_t immediate, RelocInfo::Mode rmode)  {
  rm_ = no_reg;
  imm32_ = immediate;
  rmode_ = rmode;
}


Operand::Operand(const ExternalReference& f)  {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(f.address());
  rmode_ = RelocInfo::EXTERNAL_REFERENCE;
}


Operand::Operand(Smi* value) {
  rm_ = no_reg;
  imm32_ =  reinterpret_cast<intptr_t>(value);
  rmode_ = RelocInfo::NONE32;
}


Operand::Operand(Register rm) {
  rm_ = rm;
}


bool Operand::is_reg() const {
  return rm_.is_valid();
}


// -----------------------------------------------------------------------------
// RelocInfo.

void RelocInfo::apply(intptr_t delta) {
  if (IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    byte* p = reinterpret_cast<byte*>(pc_);
    int count = Assembler::RelocateInternalReference(rmode_, p, delta);
    Assembler::FlushICache(isolate_, p, count * sizeof(uint32_t));
  }
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) ||
         rmode_ == EMBEDDED_OBJECT ||
         rmode_ == EXTERNAL_REFERENCE);
  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like LUI/ORI where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written. In this case the target_address_address function should
  // return the end of the instructions to be patched, allowing the
  // deserializer to deserialize the instructions as raw bytes and put them in
  // place, ready to be patched with the target. After jump optimization,
  // that is the address of the instruction that follows J/JAL/JR/JALR
  // instruction.
  return reinterpret_cast<Address>(
    pc_ + Assembler::kInstructionsFor32BitConstant * Assembler::kInstrSize);
}


Address RelocInfo::constant_pool_entry_address() {
  UNREACHABLE();
  return NULL;
}


int RelocInfo::target_address_size() {
  return Assembler::kSpecialTargetSize;
}


Address Assembler::target_address_from_return_address(Address pc) {
  return pc - kCallTargetAddressOffset;
}


void Assembler::set_target_internal_reference_encoded_at(Address pc,
                                                         Address target) {
  Instr instr1 = Assembler::instr_at(pc + 0 * Assembler::kInstrSize);
  Instr instr2 = Assembler::instr_at(pc + 1 * Assembler::kInstrSize);
  DCHECK(Assembler::IsLui(instr1));
  DCHECK(Assembler::IsOri(instr2) || Assembler::IsJicOrJialc(instr2));
  instr1 &= ~kImm16Mask;
  instr2 &= ~kImm16Mask;
  int32_t imm = reinterpret_cast<int32_t>(target);
  DCHECK((imm & 3) == 0);
  if (Assembler::IsJicOrJialc(instr2)) {
    // Encoded internal references are lui/jic load of 32-bit absolute address.
    uint32_t lui_offset_u, jic_offset_u;
    Assembler::UnpackTargetAddressUnsigned(imm, lui_offset_u, jic_offset_u);

    Assembler::instr_at_put(pc + 0 * Assembler::kInstrSize,
                            instr1 | lui_offset_u);
    Assembler::instr_at_put(pc + 1 * Assembler::kInstrSize,
                            instr2 | jic_offset_u);
  } else {
    // Encoded internal references are lui/ori load of 32-bit absolute address.
    Assembler::instr_at_put(pc + 0 * Assembler::kInstrSize,
                            instr1 | ((imm >> kLuiShift) & kImm16Mask));
    Assembler::instr_at_put(pc + 1 * Assembler::kInstrSize,
                            instr2 | (imm & kImm16Mask));
  }

  // Currently used only by deserializer, and all code will be flushed
  // after complete deserialization, no need to flush on each reference.
}


void Assembler::deserialization_set_target_internal_reference_at(
    Isolate* isolate, Address pc, Address target, RelocInfo::Mode mode) {
  if (mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    DCHECK(IsLui(instr_at(pc)));
    set_target_internal_reference_encoded_at(pc, target);
  } else {
    DCHECK(mode == RelocInfo::INTERNAL_REFERENCE);
    Memory::Address_at(pc) = target;
  }
}


Object* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_, host_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<Object>(reinterpret_cast<Object**>(
      Assembler::target_address_at(pc_, host_)));
}


void RelocInfo::set_target_object(Object* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(isolate_, pc_, host_,
                                   reinterpret_cast<Address>(target),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER &&
      host() != NULL &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target));
    host()->GetHeap()->RecordWriteIntoCode(host(), this, target);
  }
}


Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_internal_reference() {
  if (rmode_ == INTERNAL_REFERENCE) {
    return Memory::Address_at(pc_);
  } else {
    // Encoded internal references are lui/ori or lui/jic load of 32-bit
    // absolute address.
    DCHECK(rmode_ == INTERNAL_REFERENCE_ENCODED);
    Instr instr1 = Assembler::instr_at(pc_ + 0 * Assembler::kInstrSize);
    Instr instr2 = Assembler::instr_at(pc_ + 1 * Assembler::kInstrSize);
    DCHECK(Assembler::IsLui(instr1));
    DCHECK(Assembler::IsOri(instr2) || Assembler::IsJicOrJialc(instr2));
    if (Assembler::IsJicOrJialc(instr2)) {
      return reinterpret_cast<Address>(
          Assembler::CreateTargetAddress(instr1, instr2));
    }
    int32_t imm = (instr1 & static_cast<int32_t>(kImm16Mask)) << kLuiShift;
    imm |= (instr2 & static_cast<int32_t>(kImm16Mask));
    return reinterpret_cast<Address>(imm);
  }
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE || rmode_ == INTERNAL_REFERENCE_ENCODED);
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}


void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target)
    set_target_address(target, write_barrier_mode, icache_flush_mode);
}


Handle<Cell> RelocInfo::target_cell_handle() {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = Memory::Address_at(pc_);
  return Handle<Cell>(reinterpret_cast<Cell**>(address));
}


Cell* RelocInfo::target_cell() {
  DCHECK(rmode_ == RelocInfo::CELL);
  return Cell::FromValueAddress(Memory::Address_at(pc_));
}


void RelocInfo::set_target_cell(Cell* cell,
                                WriteBarrierMode write_barrier_mode,
                                ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = cell->address() + Cell::kValueOffset;
  Memory::Address_at(pc_) = address;
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  cell);
  }
}


static const int kNoCodeAgeSequenceLength = 7 * Assembler::kInstrSize;


Handle<Object> RelocInfo::code_age_stub_handle(Assembler* origin) {
  UNREACHABLE();  // This should never be reached on Arm.
  return Handle<Object>();
}


Code* RelocInfo::code_age_stub() {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  return Code::GetCodeFromTargetAddress(
      Assembler::target_address_at(pc_ + Assembler::kInstrSize, host_));
}


void RelocInfo::set_code_age_stub(Code* stub,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  Assembler::set_target_address_at(isolate_, pc_ + Assembler::kInstrSize, host_,
                                   stub->instruction_start());
}


Address RelocInfo::debug_call_address() {
  // The pc_ offset of 0 assumes patched debug break slot or return
  // sequence.
  DCHECK(IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence());
  return Assembler::target_address_at(pc_, host_);
}


void RelocInfo::set_debug_call_address(Address target) {
  DCHECK(IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence());
  // The pc_ offset of 0 assumes patched debug break slot or return
  // sequence.
  Assembler::set_target_address_at(isolate_, pc_, host_, target);
  if (host() != NULL) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


void RelocInfo::WipeOut() {
  DCHECK(IsEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  if (IsInternalReference(rmode_)) {
    Memory::Address_at(pc_) = NULL;
  } else if (IsInternalReferenceEncoded(rmode_)) {
    Assembler::set_target_internal_reference_encoded_at(pc_, nullptr);
  } else {
    Assembler::set_target_address_at(isolate_, pc_, host_, NULL);
  }
}

template <typename ObjectVisitor>
void RelocInfo::Visit(Isolate* isolate, ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(this);
  } else if (mode == RelocInfo::CELL) {
    visitor->VisitCell(this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    visitor->VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    visitor->VisitCodeAgeSequence(this);
  } else if (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence()) {
    visitor->VisitDebugTarget(this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(this);
  }
}


template<typename StaticVisitor>
void RelocInfo::Visit(Heap* heap) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    StaticVisitor::VisitEmbeddedPointer(heap, this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    StaticVisitor::VisitCodeTarget(heap, this);
  } else if (mode == RelocInfo::CELL) {
    StaticVisitor::VisitCell(heap, this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    StaticVisitor::VisitExternalReference(this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    StaticVisitor::VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    StaticVisitor::VisitCodeAgeSequence(heap, this);
  } else if (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence()) {
    StaticVisitor::VisitDebugTarget(heap, this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}


// -----------------------------------------------------------------------------
// Assembler.


void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}


void Assembler::CheckTrampolinePoolQuick(int extra_instructions) {
  if (pc_offset() >= next_buffer_check_ - extra_instructions * kInstrSize) {
    CheckTrampolinePool();
  }
}


void Assembler::CheckForEmitInForbiddenSlot() {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  if (IsPrevInstrCompactBranch()) {
    // Nop instruction to preceed a CTI in forbidden slot:
    Instr nop = SPECIAL | SLL;
    *reinterpret_cast<Instr*>(pc_) = nop;
    pc_ += kInstrSize;

    ClearCompactBranchState();
  }
}


void Assembler::EmitHelper(Instr x, CompactBranchType is_compact_branch) {
  if (IsPrevInstrCompactBranch()) {
    if (Instruction::IsForbiddenAfterBranchInstr(x)) {
      // Nop instruction to preceed a CTI in forbidden slot:
      Instr nop = SPECIAL | SLL;
      *reinterpret_cast<Instr*>(pc_) = nop;
      pc_ += kInstrSize;
    }
    ClearCompactBranchState();
  }
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
  if (is_compact_branch == CompactBranchType::COMPACT_BRANCH) {
    EmittedCompactBranchInstruction();
  }
  CheckTrampolinePoolQuick();
}

template <>
inline void Assembler::EmitHelper(uint8_t x);

template <typename T>
void Assembler::EmitHelper(T x) {
  *reinterpret_cast<T*>(pc_) = x;
  pc_ += sizeof(x);
  CheckTrampolinePoolQuick();
}

template <>
void Assembler::EmitHelper(uint8_t x) {
  *reinterpret_cast<uint8_t*>(pc_) = x;
  pc_ += sizeof(x);
  if (reinterpret_cast<intptr_t>(pc_) % kInstrSize == 0) {
    CheckTrampolinePoolQuick();
  }
}

void Assembler::emit(Instr x, CompactBranchType is_compact_branch) {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  EmitHelper(x, is_compact_branch);
}


}  // namespace internal
}  // namespace v8

#endif  // V8_MIPS_ASSEMBLER_MIPS_INL_H_
