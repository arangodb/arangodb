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

// A light-weight IA32 Assembler.

#ifndef V8_CODEGEN_IA32_ASSEMBLER_IA32_INL_H_
#define V8_CODEGEN_IA32_ASSEMBLER_IA32_INL_H_

#include "src/codegen/ia32/assembler-ia32.h"

#include "src/base/memory.h"
#include "src/codegen/assembler.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(SSE4_1); }

// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(intptr_t delta) {
  DCHECK_EQ(kApplyMask, (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
                         RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                         RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) |
                         RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY)));
  if (IsRuntimeEntry(rmode_) || IsCodeTarget(rmode_) ||
      IsOffHeapTarget(rmode_)) {
    base::WriteUnalignedValue(pc_,
                              base::ReadUnalignedValue<int32_t>(pc_) - delta);
  } else if (IsInternalReference(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    base::WriteUnalignedValue(pc_,
                              base::ReadUnalignedValue<int32_t>(pc_) + delta);
  }
}

Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(HasTargetAddressAddress());
  return pc_;
}

Address RelocInfo::constant_pool_entry_address() { UNREACHABLE(); }

int RelocInfo::target_address_size() { return Assembler::kSpecialTargetSize; }

HeapObject RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == FULL_EMBEDDED_OBJECT);
  return HeapObject::cast(Object(ReadUnalignedValue<Address>(pc_)));
}

HeapObject RelocInfo::target_object_no_host(Isolate* isolate) {
  return target_object();
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == FULL_EMBEDDED_OBJECT);
  return Handle<HeapObject>::cast(ReadUnalignedValue<Handle<Object>>(pc_));
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == FULL_EMBEDDED_OBJECT);
  WriteUnalignedValue(pc_, target.ptr());
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc_, sizeof(Address));
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && !host().is_null() &&
      !FLAG_disable_write_barriers) {
    WriteBarrierForCode(host(), this, target);
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  return ReadUnalignedValue<Address>(pc_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  WriteUnalignedValue(pc_, target);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc_, sizeof(Address));
  }
}

Address RelocInfo::target_internal_reference() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return ReadUnalignedValue<Address>(pc_);
}

Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return pc_;
}

Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return ReadUnalignedValue<Address>(pc_);
}

void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target) {
    set_target_address(target, write_barrier_mode, icache_flush_mode);
  }
}

Address RelocInfo::target_off_heap_target() {
  DCHECK(IsOffHeapTarget(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::WipeOut() {
  if (IsFullEmbeddedObject(rmode_) || IsExternalReference(rmode_) ||
      IsInternalReference(rmode_)) {
    WriteUnalignedValue(pc_, kNullAddress);
  } else if (IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) ||
             IsOffHeapTarget(rmode_)) {
    // Effectively write zero into the relocation.
    Assembler::set_target_address_at(pc_, constant_pool_,
                                     pc_ + sizeof(int32_t));
  } else {
    UNREACHABLE();
  }
}

void Assembler::emit(uint32_t x) {
  WriteUnalignedValue(reinterpret_cast<Address>(pc_), x);
  pc_ += sizeof(uint32_t);
}

void Assembler::emit_q(uint64_t x) {
  WriteUnalignedValue(reinterpret_cast<Address>(pc_), x);
  pc_ += sizeof(uint64_t);
}

void Assembler::emit(Handle<HeapObject> handle) {
  emit(handle.address(), RelocInfo::FULL_EMBEDDED_OBJECT);
}

void Assembler::emit(uint32_t x, RelocInfo::Mode rmode) {
  if (!RelocInfo::IsNone(rmode)) {
    RecordRelocInfo(rmode);
  }
  emit(x);
}

void Assembler::emit(Handle<Code> code, RelocInfo::Mode rmode) {
  emit(code.address(), rmode);
}

void Assembler::emit(const Immediate& x) {
  if (x.rmode_ == RelocInfo::INTERNAL_REFERENCE) {
    Label* label = reinterpret_cast<Label*>(x.immediate());
    emit_code_relative_offset(label);
    return;
  }
  if (!RelocInfo::IsNone(x.rmode_)) RecordRelocInfo(x.rmode_);
  if (x.is_heap_object_request()) {
    RequestHeapObject(x.heap_object_request());
    emit(0);
  } else {
    emit(x.immediate());
  }
}

void Assembler::emit_code_relative_offset(Label* label) {
  if (label->is_bound()) {
    int32_t pos;
    pos = label->pos() + Code::kHeaderSize - kHeapObjectTag;
    emit(pos);
  } else {
    emit_disp(label, Displacement::CODE_RELATIVE);
  }
}

void Assembler::emit_b(Immediate x) {
  DCHECK(x.is_int8() || x.is_uint8());
  uint8_t value = static_cast<uint8_t>(x.immediate());
  *pc_++ = value;
}

void Assembler::emit_w(const Immediate& x) {
  DCHECK(RelocInfo::IsNone(x.rmode_));
  uint16_t value = static_cast<uint16_t>(x.immediate());
  WriteUnalignedValue(reinterpret_cast<Address>(pc_), value);
  pc_ += sizeof(uint16_t);
}

Address Assembler::target_address_at(Address pc, Address constant_pool) {
  return pc + sizeof(int32_t) + ReadUnalignedValue<int32_t>(pc);
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  WriteUnalignedValue(pc, target - (pc + sizeof(int32_t)));
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc, sizeof(int32_t));
  }
}

void Assembler::deserialization_set_special_target_at(
    Address instruction_payload, Code code, Address target) {
  set_target_address_at(instruction_payload,
                        !code.is_null() ? code.constant_pool() : kNullAddress,
                        target);
}

int Assembler::deserialization_special_target_size(
    Address instruction_payload) {
  return kSpecialTargetSize;
}

Displacement Assembler::disp_at(Label* L) {
  return Displacement(long_at(L->pos()));
}

void Assembler::disp_at_put(Label* L, Displacement disp) {
  long_at_put(L->pos(), disp.data());
}

void Assembler::emit_disp(Label* L, Displacement::Type type) {
  Displacement disp(L, type);
  L->link_to(pc_offset());
  emit(static_cast<int>(disp.data()));
}

void Assembler::emit_near_disp(Label* L) {
  byte disp = 0x00;
  if (L->is_near_linked()) {
    int offset = L->near_link_pos() - pc_offset();
    DCHECK(is_int8(offset));
    disp = static_cast<byte>(offset & 0xFF);
  }
  L->link_to(pc_offset(), Label::kNear);
  *pc_++ = disp;
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  WriteUnalignedValue(pc, target);
}

void Operand::set_sib(ScaleFactor scale, Register index, Register base) {
  DCHECK_EQ(len_, 1);
  DCHECK_EQ(scale & -4, 0);
  // Use SIB with no index register only for base esp.
  DCHECK(index != esp || base == esp);
  buf_[1] = scale << 6 | index.code() << 3 | base.code();
  len_ = 2;
}

void Operand::set_disp8(int8_t disp) {
  DCHECK(len_ == 1 || len_ == 2);
  *reinterpret_cast<int8_t*>(&buf_[len_++]) = disp;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_IA32_ASSEMBLER_IA32_INL_H_
