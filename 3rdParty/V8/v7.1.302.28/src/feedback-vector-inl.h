// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FEEDBACK_VECTOR_INL_H_
#define V8_FEEDBACK_VECTOR_INL_H_

#include "src/feedback-vector.h"
#include "src/globals.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/shared-function-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

INT32_ACCESSORS(FeedbackMetadata, slot_count, kSlotCountOffset)

int32_t FeedbackMetadata::synchronized_slot_count() const {
  return base::Acquire_Load(reinterpret_cast<const base::Atomic32*>(
      FIELD_ADDR(this, kSlotCountOffset)));
}

// static
FeedbackMetadata* FeedbackMetadata::cast(Object* obj) {
  DCHECK(obj->IsFeedbackMetadata());
  return reinterpret_cast<FeedbackMetadata*>(obj);
}

int32_t FeedbackMetadata::get(int index) const {
  DCHECK(index >= 0 && index < length());
  int offset = kHeaderSize + index * kInt32Size;
  return READ_INT32_FIELD(this, offset);
}

void FeedbackMetadata::set(int index, int32_t value) {
  DCHECK(index >= 0 && index < length());
  int offset = kHeaderSize + index * kInt32Size;
  WRITE_INT32_FIELD(this, offset, value);
}

bool FeedbackMetadata::is_empty() const { return slot_count() == 0; }

int FeedbackMetadata::length() const {
  return FeedbackMetadata::length(slot_count());
}

// static
FeedbackVector* FeedbackVector::cast(Object* obj) {
  DCHECK(obj->IsFeedbackVector());
  return reinterpret_cast<FeedbackVector*>(obj);
}

int FeedbackMetadata::GetSlotSize(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kForIn:
    case FeedbackSlotKind::kInstanceOf:
    case FeedbackSlotKind::kCompareOp:
    case FeedbackSlotKind::kBinaryOp:
    case FeedbackSlotKind::kLiteral:
    case FeedbackSlotKind::kCreateClosure:
    case FeedbackSlotKind::kTypeProfile:
      return 1;

    case FeedbackSlotKind::kCall:
    case FeedbackSlotKind::kCloneObject:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadKeyed:
    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreInArrayLiteral:
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return 2;

    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
      break;
  }
  return 1;
}

ACCESSORS(FeedbackVector, shared_function_info, SharedFunctionInfo,
          kSharedFunctionInfoOffset)
WEAK_ACCESSORS(FeedbackVector, optimized_code_weak_or_smi, kOptimizedCodeOffset)
INT32_ACCESSORS(FeedbackVector, length, kLengthOffset)
INT32_ACCESSORS(FeedbackVector, invocation_count, kInvocationCountOffset)
INT32_ACCESSORS(FeedbackVector, profiler_ticks, kProfilerTicksOffset)
INT32_ACCESSORS(FeedbackVector, deopt_count, kDeoptCountOffset)

bool FeedbackVector::is_empty() const { return length() == 0; }

FeedbackMetadata* FeedbackVector::metadata() const {
  return shared_function_info()->feedback_metadata();
}

void FeedbackVector::clear_invocation_count() { set_invocation_count(0); }

void FeedbackVector::increment_deopt_count() {
  int count = deopt_count();
  if (count < std::numeric_limits<int32_t>::max()) {
    set_deopt_count(count + 1);
  }
}

Code* FeedbackVector::optimized_code() const {
  MaybeObject* slot = optimized_code_weak_or_smi();
  DCHECK(slot->IsSmi() || slot->IsWeakOrCleared());
  HeapObject* heap_object;
  return slot->GetHeapObject(&heap_object) ? Code::cast(heap_object) : nullptr;
}

OptimizationMarker FeedbackVector::optimization_marker() const {
  MaybeObject* slot = optimized_code_weak_or_smi();
  Smi* value;
  if (!slot->ToSmi(&value)) return OptimizationMarker::kNone;
  return static_cast<OptimizationMarker>(value->value());
}

bool FeedbackVector::has_optimized_code() const {
  return optimized_code() != nullptr;
}

bool FeedbackVector::has_optimization_marker() const {
  return optimization_marker() != OptimizationMarker::kLogFirstExecution &&
         optimization_marker() != OptimizationMarker::kNone;
}

// Conversion from an integer index to either a slot or an ic slot.
// static
FeedbackSlot FeedbackVector::ToSlot(int index) {
  DCHECK_GE(index, 0);
  return FeedbackSlot(index);
}

MaybeObject* FeedbackVector::Get(FeedbackSlot slot) const {
  return get(GetIndex(slot));
}

MaybeObject* FeedbackVector::get(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kFeedbackSlotsOffset + index * kPointerSize;
  return RELAXED_READ_WEAK_FIELD(this, offset);
}

void FeedbackVector::Set(FeedbackSlot slot, MaybeObject* value,
                         WriteBarrierMode mode) {
  set(GetIndex(slot), value, mode);
}

void FeedbackVector::set(int index, MaybeObject* value, WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kFeedbackSlotsOffset + index * kPointerSize;
  RELAXED_WRITE_FIELD(this, offset, value);
  CONDITIONAL_WEAK_WRITE_BARRIER(this, offset, value, mode);
}

void FeedbackVector::Set(FeedbackSlot slot, Object* value,
                         WriteBarrierMode mode) {
  set(GetIndex(slot), MaybeObject::FromObject(value), mode);
}

void FeedbackVector::set(int index, Object* value, WriteBarrierMode mode) {
  set(index, MaybeObject::FromObject(value), mode);
}

inline MaybeObject** FeedbackVector::slots_start() {
  return HeapObject::RawMaybeWeakField(this, kFeedbackSlotsOffset);
}

// Helper function to transform the feedback to BinaryOperationHint.
BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case BinaryOperationFeedback::kNone:
      return BinaryOperationHint::kNone;
    case BinaryOperationFeedback::kSignedSmall:
      return BinaryOperationHint::kSignedSmall;
    case BinaryOperationFeedback::kSignedSmallInputs:
      return BinaryOperationHint::kSignedSmallInputs;
    case BinaryOperationFeedback::kNumber:
      return BinaryOperationHint::kNumber;
    case BinaryOperationFeedback::kNumberOrOddball:
      return BinaryOperationHint::kNumberOrOddball;
    case BinaryOperationFeedback::kString:
      return BinaryOperationHint::kString;
    case BinaryOperationFeedback::kBigInt:
      return BinaryOperationHint::kBigInt;
    default:
      return BinaryOperationHint::kAny;
  }
  UNREACHABLE();
}

// Helper function to transform the feedback to CompareOperationHint.
CompareOperationHint CompareOperationHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case CompareOperationFeedback::kNone:
      return CompareOperationHint::kNone;
    case CompareOperationFeedback::kSignedSmall:
      return CompareOperationHint::kSignedSmall;
    case CompareOperationFeedback::kNumber:
      return CompareOperationHint::kNumber;
    case CompareOperationFeedback::kNumberOrOddball:
      return CompareOperationHint::kNumberOrOddball;
    case CompareOperationFeedback::kInternalizedString:
      return CompareOperationHint::kInternalizedString;
    case CompareOperationFeedback::kString:
      return CompareOperationHint::kString;
    case CompareOperationFeedback::kSymbol:
      return CompareOperationHint::kSymbol;
    case CompareOperationFeedback::kBigInt:
      return CompareOperationHint::kBigInt;
    case CompareOperationFeedback::kReceiver:
      return CompareOperationHint::kReceiver;
    default:
      return CompareOperationHint::kAny;
  }
  UNREACHABLE();
}

// Helper function to transform the feedback to ForInHint.
ForInHint ForInHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case ForInFeedback::kNone:
      return ForInHint::kNone;
    case ForInFeedback::kEnumCacheKeys:
      return ForInHint::kEnumCacheKeys;
    case ForInFeedback::kEnumCacheKeysAndIndices:
      return ForInHint::kEnumCacheKeysAndIndices;
    default:
      return ForInHint::kAny;
  }
  UNREACHABLE();
}

void FeedbackVector::ComputeCounts(int* with_type_info, int* generic,
                                   int* vector_ic_count) {
  MaybeObject* megamorphic_sentinel = MaybeObject::FromObject(
      *FeedbackVector::MegamorphicSentinel(GetIsolate()));
  int with = 0;
  int gen = 0;
  int total = 0;
  FeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackSlot slot = iter.Next();
    FeedbackSlotKind kind = iter.kind();

    MaybeObject* const obj = Get(slot);
    AssertNoLegacyTypes(obj);
    switch (kind) {
      case FeedbackSlotKind::kCall:
      case FeedbackSlotKind::kLoadProperty:
      case FeedbackSlotKind::kLoadGlobalInsideTypeof:
      case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
      case FeedbackSlotKind::kLoadKeyed:
      case FeedbackSlotKind::kStoreNamedSloppy:
      case FeedbackSlotKind::kStoreNamedStrict:
      case FeedbackSlotKind::kStoreOwnNamed:
      case FeedbackSlotKind::kStoreGlobalSloppy:
      case FeedbackSlotKind::kStoreGlobalStrict:
      case FeedbackSlotKind::kStoreKeyedSloppy:
      case FeedbackSlotKind::kStoreKeyedStrict:
      case FeedbackSlotKind::kStoreInArrayLiteral:
      case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      case FeedbackSlotKind::kTypeProfile: {
        HeapObject* heap_object;
        if (obj->IsWeakOrCleared() ||
            (obj->GetHeapObjectIfStrong(&heap_object) &&
             (heap_object->IsWeakFixedArray() || heap_object->IsString()))) {
          with++;
        } else if (obj == megamorphic_sentinel) {
          gen++;
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kBinaryOp: {
        int const feedback = Smi::ToInt(obj->cast<Smi>());
        BinaryOperationHint hint = BinaryOperationHintFromFeedback(feedback);
        if (hint == BinaryOperationHint::kAny) {
          gen++;
        }
        if (hint != BinaryOperationHint::kNone) {
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kCompareOp: {
        int const feedback = Smi::ToInt(obj->cast<Smi>());
        CompareOperationHint hint = CompareOperationHintFromFeedback(feedback);
        if (hint == CompareOperationHint::kAny) {
          gen++;
        }
        if (hint != CompareOperationHint::kNone) {
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kForIn: {
        int const feedback = Smi::ToInt(obj->cast<Smi>());
        ForInHint hint = ForInHintFromFeedback(feedback);
        if (hint == ForInHint::kAny) {
          gen++;
        }
        if (hint != ForInHint::kNone) {
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kInstanceOf: {
        if (obj->IsWeakOrCleared()) {
          with++;
        } else if (obj == megamorphic_sentinel) {
          gen++;
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kCreateClosure:
      case FeedbackSlotKind::kLiteral:
      case FeedbackSlotKind::kCloneObject:
        break;
      case FeedbackSlotKind::kInvalid:
      case FeedbackSlotKind::kKindsNumber:
        UNREACHABLE();
        break;
    }
  }

  *with_type_info = with;
  *generic = gen;
  *vector_ic_count = total;
}

Handle<Symbol> FeedbackVector::UninitializedSentinel(Isolate* isolate) {
  return isolate->factory()->uninitialized_symbol();
}

Handle<Symbol> FeedbackVector::GenericSentinel(Isolate* isolate) {
  return isolate->factory()->generic_symbol();
}

Handle<Symbol> FeedbackVector::MegamorphicSentinel(Isolate* isolate) {
  return isolate->factory()->megamorphic_symbol();
}

Handle<Symbol> FeedbackVector::PremonomorphicSentinel(Isolate* isolate) {
  return isolate->factory()->premonomorphic_symbol();
}

Symbol* FeedbackVector::RawUninitializedSentinel(Isolate* isolate) {
  return ReadOnlyRoots(isolate).uninitialized_symbol();
}

bool FeedbackMetadataIterator::HasNext() const {
  return next_slot_.ToInt() < metadata()->slot_count();
}

FeedbackSlot FeedbackMetadataIterator::Next() {
  DCHECK(HasNext());
  cur_slot_ = next_slot_;
  slot_kind_ = metadata()->GetKind(cur_slot_);
  next_slot_ = FeedbackSlot(next_slot_.ToInt() + entry_size());
  return cur_slot_;
}

int FeedbackMetadataIterator::entry_size() const {
  return FeedbackMetadata::GetSlotSize(kind());
}

MaybeObject* FeedbackNexus::GetFeedback() const {
  MaybeObject* feedback = vector()->Get(slot());
  FeedbackVector::AssertNoLegacyTypes(feedback);
  return feedback;
}

MaybeObject* FeedbackNexus::GetFeedbackExtra() const {
#ifdef DEBUG
  FeedbackSlotKind kind = vector()->GetKind(slot());
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
#endif
  int extra_index = vector()->GetIndex(slot()) + 1;
  return vector()->get(extra_index);
}

void FeedbackNexus::SetFeedback(Object* feedback, WriteBarrierMode mode) {
  SetFeedback(MaybeObject::FromObject(feedback));
}

void FeedbackNexus::SetFeedback(MaybeObject* feedback, WriteBarrierMode mode) {
  FeedbackVector::AssertNoLegacyTypes(feedback);
  vector()->Set(slot(), feedback, mode);
}

void FeedbackNexus::SetFeedbackExtra(Object* feedback_extra,
                                     WriteBarrierMode mode) {
#ifdef DEBUG
  FeedbackSlotKind kind = vector()->GetKind(slot());
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
  FeedbackVector::AssertNoLegacyTypes(MaybeObject::FromObject(feedback_extra));
#endif
  int index = vector()->GetIndex(slot()) + 1;
  vector()->set(index, MaybeObject::FromObject(feedback_extra), mode);
}

void FeedbackNexus::SetFeedbackExtra(MaybeObject* feedback_extra,
                                     WriteBarrierMode mode) {
#ifdef DEBUG
  FeedbackVector::AssertNoLegacyTypes(feedback_extra);
#endif
  int index = vector()->GetIndex(slot()) + 1;
  vector()->set(index, feedback_extra, mode);
}

Isolate* FeedbackNexus::GetIsolate() const { return vector()->GetIsolate(); }
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_FEEDBACK_VECTOR_INL_H_
