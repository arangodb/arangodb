// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ALLOCATION_SITE_INL_H_
#define V8_OBJECTS_ALLOCATION_SITE_INL_H_

#include "src/objects/allocation-site.h"

#include "src/heap/heap-inl.h"
#include "src/objects/js-objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(AllocationMemento)
CAST_ACCESSOR(AllocationSite)

ACCESSORS(AllocationSite, transition_info_or_boilerplate, Object,
          kTransitionInfoOrBoilerplateOffset)
ACCESSORS(AllocationSite, nested_site, Object, kNestedSiteOffset)
INT32_ACCESSORS(AllocationSite, pretenure_data, kPretenureDataOffset)
INT32_ACCESSORS(AllocationSite, pretenure_create_count,
                kPretenureCreateCountOffset)
ACCESSORS(AllocationSite, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS_CHECKED(AllocationSite, weak_next, Object, kWeakNextOffset,
                  HasWeakNext())
ACCESSORS(AllocationMemento, allocation_site, Object, kAllocationSiteOffset)

JSObject* AllocationSite::boilerplate() const {
  DCHECK(PointsToLiteral());
  return JSObject::cast(transition_info_or_boilerplate());
}

void AllocationSite::set_boilerplate(JSObject* object, WriteBarrierMode mode) {
  set_transition_info_or_boilerplate(object, mode);
}

int AllocationSite::transition_info() const {
  DCHECK(!PointsToLiteral());
  return Smi::cast(transition_info_or_boilerplate())->value();
}

void AllocationSite::set_transition_info(int value) {
  DCHECK(!PointsToLiteral());
  set_transition_info_or_boilerplate(Smi::FromInt(value), SKIP_WRITE_BARRIER);
}

bool AllocationSite::HasWeakNext() const {
  return map() == GetReadOnlyRoots().allocation_site_map();
}

void AllocationSite::Initialize() {
  set_transition_info_or_boilerplate(Smi::kZero);
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::kZero);
  set_pretenure_data(0);
  set_pretenure_create_count(0);
  set_dependent_code(
      DependentCode::cast(GetReadOnlyRoots().empty_weak_fixed_array()),
      SKIP_WRITE_BARRIER);
}

bool AllocationSite::IsZombie() const {
  return pretenure_decision() == kZombie;
}

bool AllocationSite::IsMaybeTenure() const {
  return pretenure_decision() == kMaybeTenure;
}

bool AllocationSite::PretenuringDecisionMade() const {
  return pretenure_decision() != kUndecided;
}

void AllocationSite::MarkZombie() {
  DCHECK(!IsZombie());
  Initialize();
  set_pretenure_decision(kZombie);
}

ElementsKind AllocationSite::GetElementsKind() const {
  return ElementsKindBits::decode(transition_info());
}

void AllocationSite::SetElementsKind(ElementsKind kind) {
  set_transition_info(ElementsKindBits::update(transition_info(), kind));
}

bool AllocationSite::CanInlineCall() const {
  return DoNotInlineBit::decode(transition_info()) == 0;
}

void AllocationSite::SetDoNotInlineCall() {
  set_transition_info(DoNotInlineBit::update(transition_info(), true));
}

bool AllocationSite::PointsToLiteral() const {
  Object* raw_value = transition_info_or_boilerplate();
  DCHECK_EQ(!raw_value->IsSmi(),
            raw_value->IsJSArray() || raw_value->IsJSObject());
  return !raw_value->IsSmi();
}

// Heuristic: We only need to create allocation site info if the boilerplate
// elements kind is the initial elements kind.
bool AllocationSite::ShouldTrack(ElementsKind boilerplate_elements_kind) {
  return IsSmiElementsKind(boilerplate_elements_kind);
}

inline bool AllocationSite::CanTrack(InstanceType type) {
  if (FLAG_allocation_site_pretenuring) {
    // TurboFan doesn't care at all about String pretenuring feedback,
    // so don't bother even trying to track that.
    return type == JS_ARRAY_TYPE || type == JS_OBJECT_TYPE;
  }
  return type == JS_ARRAY_TYPE;
}

AllocationSite::PretenureDecision AllocationSite::pretenure_decision() const {
  return PretenureDecisionBits::decode(pretenure_data());
}

void AllocationSite::set_pretenure_decision(PretenureDecision decision) {
  int32_t value = pretenure_data();
  set_pretenure_data(PretenureDecisionBits::update(value, decision));
}

bool AllocationSite::deopt_dependent_code() const {
  return DeoptDependentCodeBit::decode(pretenure_data());
}

void AllocationSite::set_deopt_dependent_code(bool deopt) {
  int32_t value = pretenure_data();
  set_pretenure_data(DeoptDependentCodeBit::update(value, deopt));
}

int AllocationSite::memento_found_count() const {
  return MementoFoundCountBits::decode(pretenure_data());
}

inline void AllocationSite::set_memento_found_count(int count) {
  int32_t value = pretenure_data();
  // Verify that we can count more mementos than we can possibly find in one
  // new space collection.
  DCHECK((GetHeap()->MaxSemiSpaceSize() /
          (Heap::kMinObjectSizeInWords * kPointerSize +
           AllocationMemento::kSize)) < MementoFoundCountBits::kMax);
  DCHECK_LT(count, MementoFoundCountBits::kMax);
  set_pretenure_data(MementoFoundCountBits::update(value, count));
}

int AllocationSite::memento_create_count() const {
  return pretenure_create_count();
}

void AllocationSite::set_memento_create_count(int count) {
  set_pretenure_create_count(count);
}

bool AllocationSite::IncrementMementoFoundCount(int increment) {
  if (IsZombie()) return false;

  int value = memento_found_count();
  set_memento_found_count(value + increment);
  return memento_found_count() >= kPretenureMinimumCreated;
}

inline void AllocationSite::IncrementMementoCreateCount() {
  DCHECK(FLAG_allocation_site_pretenuring);
  int value = memento_create_count();
  set_memento_create_count(value + 1);
}

bool AllocationMemento::IsValid() const {
  return allocation_site()->IsAllocationSite() &&
         !AllocationSite::cast(allocation_site())->IsZombie();
}

AllocationSite* AllocationMemento::GetAllocationSite() const {
  DCHECK(IsValid());
  return AllocationSite::cast(allocation_site());
}

Address AllocationMemento::GetAllocationSiteUnchecked() const {
  return reinterpret_cast<Address>(allocation_site());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ALLOCATION_SITE_INL_H_
