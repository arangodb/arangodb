// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_INL_H_
#define V8_HEAP_SCAVENGER_INL_H_

#include "src/heap/scavenger.h"

#include "src/heap/incremental-marking-inl.h"
#include "src/heap/local-allocator-inl.h"
#include "src/objects/map.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots-inl.h"

namespace v8 {
namespace internal {

void Scavenger::PromotionList::View::PushRegularObject(HeapObject object,
                                                       int size) {
  promotion_list_->PushRegularObject(task_id_, object, size);
}

void Scavenger::PromotionList::View::PushLargeObject(HeapObject object, Map map,
                                                     int size) {
  promotion_list_->PushLargeObject(task_id_, object, map, size);
}

bool Scavenger::PromotionList::View::IsEmpty() {
  return promotion_list_->IsEmpty();
}

size_t Scavenger::PromotionList::View::LocalPushSegmentSize() {
  return promotion_list_->LocalPushSegmentSize(task_id_);
}

bool Scavenger::PromotionList::View::Pop(struct PromotionListEntry* entry) {
  return promotion_list_->Pop(task_id_, entry);
}

bool Scavenger::PromotionList::View::IsGlobalPoolEmpty() {
  return promotion_list_->IsGlobalPoolEmpty();
}

bool Scavenger::PromotionList::View::ShouldEagerlyProcessPromotionList() {
  return promotion_list_->ShouldEagerlyProcessPromotionList(task_id_);
}

void Scavenger::PromotionList::PushRegularObject(int task_id, HeapObject object,
                                                 int size) {
  regular_object_promotion_list_.Push(task_id, ObjectAndSize(object, size));
}

void Scavenger::PromotionList::PushLargeObject(int task_id, HeapObject object,
                                               Map map, int size) {
  large_object_promotion_list_.Push(task_id, {object, map, size});
}

bool Scavenger::PromotionList::IsEmpty() {
  return regular_object_promotion_list_.IsEmpty() &&
         large_object_promotion_list_.IsEmpty();
}

size_t Scavenger::PromotionList::LocalPushSegmentSize(int task_id) {
  return regular_object_promotion_list_.LocalPushSegmentSize(task_id) +
         large_object_promotion_list_.LocalPushSegmentSize(task_id);
}

bool Scavenger::PromotionList::Pop(int task_id,
                                   struct PromotionListEntry* entry) {
  ObjectAndSize regular_object;
  if (regular_object_promotion_list_.Pop(task_id, &regular_object)) {
    entry->heap_object = regular_object.first;
    entry->size = regular_object.second;
    entry->map = entry->heap_object.map();
    return true;
  }
  return large_object_promotion_list_.Pop(task_id, entry);
}

bool Scavenger::PromotionList::IsGlobalPoolEmpty() {
  return regular_object_promotion_list_.IsGlobalPoolEmpty() &&
         large_object_promotion_list_.IsGlobalPoolEmpty();
}

bool Scavenger::PromotionList::ShouldEagerlyProcessPromotionList(int task_id) {
  // Threshold when to prioritize processing of the promotion list. Right
  // now we only look into the regular object list.
  const int kProcessPromotionListThreshold =
      kRegularObjectPromotionListSegmentSize / 2;
  return LocalPushSegmentSize(task_id) < kProcessPromotionListThreshold;
}

void Scavenger::PageMemoryFence(MaybeObject object) {
#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race
  // with  page initialization.
  HeapObject heap_object;
  if (object->GetHeapObject(&heap_object)) {
    MemoryChunk::FromHeapObject(heap_object)->SynchronizedHeapLoad();
  }
#endif
}

bool Scavenger::MigrateObject(Map map, HeapObject source, HeapObject target,
                              int size) {
  // Copy the content of source to target.
  target.set_map_word(MapWord::FromMap(map));
  heap()->CopyBlock(target.address() + kTaggedSize,
                    source.address() + kTaggedSize, size - kTaggedSize);

  if (!source.synchronized_compare_and_swap_map_word(
          MapWord::FromMap(map), MapWord::FromForwardingAddress(target))) {
    // Other task migrated the object.
    return false;
  }

  if (V8_UNLIKELY(is_logging_)) {
    heap()->OnMoveEvent(target, source, size);
  }

  if (is_incremental_marking_) {
    heap()->incremental_marking()->TransferColor(source, target);
  }
  heap()->UpdateAllocationSite(map, source, &local_pretenuring_feedback_);
  return true;
}

template <typename THeapObjectSlot>
CopyAndForwardResult Scavenger::SemiSpaceCopyObject(
    Map map, THeapObjectSlot slot, HeapObject object, int object_size,
    ObjectFields object_fields) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK(heap()->AllowedToBeMigrated(map, object, NEW_SPACE));
  AllocationAlignment alignment = HeapObject::RequiredAlignment(map);
  AllocationResult allocation = allocator_.Allocate(
      NEW_SPACE, object_size, AllocationOrigin::kGC, alignment);

  HeapObject target;
  if (allocation.To(&target)) {
    DCHECK(heap()->incremental_marking()->non_atomic_marking_state()->IsWhite(
        target));
    const bool self_success = MigrateObject(map, object, target, object_size);
    if (!self_success) {
      allocator_.FreeLast(NEW_SPACE, target, object_size);
      MapWord map_word = object.synchronized_map_word();
      HeapObjectReference::Update(slot, map_word.ToForwardingAddress());
      DCHECK(!Heap::InFromPage(*slot));
      return Heap::InToPage(*slot)
                 ? CopyAndForwardResult::SUCCESS_YOUNG_GENERATION
                 : CopyAndForwardResult::SUCCESS_OLD_GENERATION;
    }
    HeapObjectReference::Update(slot, target);
    if (object_fields == ObjectFields::kMaybePointers) {
      copied_list_.Push(ObjectAndSize(target, object_size));
    }
    copied_size_ += object_size;
    return CopyAndForwardResult::SUCCESS_YOUNG_GENERATION;
  }
  return CopyAndForwardResult::FAILURE;
}

template <typename THeapObjectSlot>
CopyAndForwardResult Scavenger::PromoteObject(Map map, THeapObjectSlot slot,
                                              HeapObject object,
                                              int object_size,
                                              ObjectFields object_fields) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  AllocationAlignment alignment = HeapObject::RequiredAlignment(map);
  AllocationResult allocation = allocator_.Allocate(
      OLD_SPACE, object_size, AllocationOrigin::kGC, alignment);

  HeapObject target;
  if (allocation.To(&target)) {
    DCHECK(heap()->incremental_marking()->non_atomic_marking_state()->IsWhite(
        target));
    const bool self_success = MigrateObject(map, object, target, object_size);
    if (!self_success) {
      allocator_.FreeLast(OLD_SPACE, target, object_size);
      MapWord map_word = object.synchronized_map_word();
      HeapObjectReference::Update(slot, map_word.ToForwardingAddress());
      DCHECK(!Heap::InFromPage(*slot));
      return Heap::InToPage(*slot)
                 ? CopyAndForwardResult::SUCCESS_YOUNG_GENERATION
                 : CopyAndForwardResult::SUCCESS_OLD_GENERATION;
    }
    HeapObjectReference::Update(slot, target);
    if (object_fields == ObjectFields::kMaybePointers) {
      promotion_list_.PushRegularObject(target, object_size);
    }
    promoted_size_ += object_size;
    return CopyAndForwardResult::SUCCESS_OLD_GENERATION;
  }
  return CopyAndForwardResult::FAILURE;
}

SlotCallbackResult Scavenger::RememberedSetEntryNeeded(
    CopyAndForwardResult result) {
  DCHECK_NE(CopyAndForwardResult::FAILURE, result);
  return result == CopyAndForwardResult::SUCCESS_YOUNG_GENERATION ? KEEP_SLOT
                                                                  : REMOVE_SLOT;
}

bool Scavenger::HandleLargeObject(Map map, HeapObject object, int object_size,
                                  ObjectFields object_fields) {
  // TODO(hpayer): Make this check size based, i.e.
  // object_size > kMaxRegularHeapObjectSize
  if (V8_UNLIKELY(
          FLAG_young_generation_large_objects &&
          MemoryChunk::FromHeapObject(object)->InNewLargeObjectSpace())) {
    DCHECK_EQ(NEW_LO_SPACE,
              MemoryChunk::FromHeapObject(object)->owner_identity());
    if (object.synchronized_compare_and_swap_map_word(
            MapWord::FromMap(map), MapWord::FromForwardingAddress(object))) {
      surviving_new_large_objects_.insert({object, map});
      promoted_size_ += object_size;
      if (object_fields == ObjectFields::kMaybePointers) {
        promotion_list_.PushLargeObject(object, map, object_size);
      }
    }
    return true;
  }
  return false;
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateObjectDefault(
    Map map, THeapObjectSlot slot, HeapObject object, int object_size,
    ObjectFields object_fields) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  SLOW_DCHECK(object.SizeFromMap(map) == object_size);
  CopyAndForwardResult result;

  if (HandleLargeObject(map, object, object_size, object_fields)) {
    return KEEP_SLOT;
  }

  SLOW_DCHECK(static_cast<size_t>(object_size) <=
              MemoryChunkLayout::AllocatableMemoryInDataPage());

  if (!heap()->ShouldBePromoted(object.address())) {
    // A semi-space copy may fail due to fragmentation. In that case, we
    // try to promote the object.
    result = SemiSpaceCopyObject(map, slot, object, object_size, object_fields);
    if (result != CopyAndForwardResult::FAILURE) {
      return RememberedSetEntryNeeded(result);
    }
  }

  // We may want to promote this object if the object was already semi-space
  // copied in a previes young generation GC or if the semi-space copy above
  // failed.
  result = PromoteObject(map, slot, object, object_size, object_fields);
  if (result != CopyAndForwardResult::FAILURE) {
    return RememberedSetEntryNeeded(result);
  }

  // If promotion failed, we try to copy the object to the other semi-space.
  result = SemiSpaceCopyObject(map, slot, object, object_size, object_fields);
  if (result != CopyAndForwardResult::FAILURE) {
    return RememberedSetEntryNeeded(result);
  }

  heap()->FatalProcessOutOfMemory("Scavenger: semi-space copy");
  UNREACHABLE();
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateThinString(Map map, THeapObjectSlot slot,
                                                 ThinString object,
                                                 int object_size) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  if (!is_incremental_marking_) {
    // The ThinString should die after Scavenge, so avoid writing the proper
    // forwarding pointer and instead just signal the actual object as forwarded
    // reference.
    String actual = object.actual();
    // ThinStrings always refer to internalized strings, which are always in old
    // space.
    DCHECK(!Heap::InYoungGeneration(actual));
    HeapObjectReference::Update(slot, actual);
    return REMOVE_SLOT;
  }

  DCHECK_EQ(ObjectFields::kMaybePointers,
            Map::ObjectFieldsFrom(map.visitor_id()));
  return EvacuateObjectDefault(map, slot, object, object_size,
                               ObjectFields::kMaybePointers);
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateShortcutCandidate(Map map,
                                                        THeapObjectSlot slot,
                                                        ConsString object,
                                                        int object_size) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK(IsShortcutCandidate(map.instance_type()));
  if (!is_incremental_marking_ &&
      object.unchecked_second() == ReadOnlyRoots(heap()).empty_string()) {
    HeapObject first = HeapObject::cast(object.unchecked_first());

    HeapObjectReference::Update(slot, first);

    if (!Heap::InYoungGeneration(first)) {
      object.synchronized_set_map_word(MapWord::FromForwardingAddress(first));
      return REMOVE_SLOT;
    }

    MapWord first_word = first.synchronized_map_word();
    if (first_word.IsForwardingAddress()) {
      HeapObject target = first_word.ToForwardingAddress();

      HeapObjectReference::Update(slot, target);
      object.synchronized_set_map_word(MapWord::FromForwardingAddress(target));
      return Heap::InYoungGeneration(target) ? KEEP_SLOT : REMOVE_SLOT;
    }
    Map map = first_word.ToMap();
    SlotCallbackResult result =
        EvacuateObjectDefault(map, slot, first, first.SizeFromMap(map),
                              Map::ObjectFieldsFrom(map.visitor_id()));
    object.synchronized_set_map_word(
        MapWord::FromForwardingAddress(slot.ToHeapObject()));
    return result;
  }
  DCHECK_EQ(ObjectFields::kMaybePointers,
            Map::ObjectFieldsFrom(map.visitor_id()));
  return EvacuateObjectDefault(map, slot, object, object_size,
                               ObjectFields::kMaybePointers);
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateObject(THeapObjectSlot slot, Map map,
                                             HeapObject source) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  SLOW_DCHECK(Heap::InFromPage(source));
  SLOW_DCHECK(!MapWord::FromMap(map).IsForwardingAddress());
  int size = source.SizeFromMap(map);
  // Cannot use ::cast() below because that would add checks in debug mode
  // that require re-reading the map.
  VisitorId visitor_id = map.visitor_id();
  switch (visitor_id) {
    case kVisitThinString:
      // At the moment we don't allow weak pointers to thin strings.
      DCHECK(!(*slot)->IsWeak());
      return EvacuateThinString(map, slot, ThinString::unchecked_cast(source),
                                size);
    case kVisitShortcutCandidate:
      DCHECK(!(*slot)->IsWeak());
      // At the moment we don't allow weak pointers to cons strings.
      return EvacuateShortcutCandidate(
          map, slot, ConsString::unchecked_cast(source), size);
    default:
      return EvacuateObjectDefault(map, slot, source, size,
                                   Map::ObjectFieldsFrom(visitor_id));
  }
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::ScavengeObject(THeapObjectSlot p,
                                             HeapObject object) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK(Heap::InFromPage(object));

  // Synchronized load that consumes the publishing CAS of MigrateObject.
  MapWord first_word = object.synchronized_map_word();

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    HeapObject dest = first_word.ToForwardingAddress();
    HeapObjectReference::Update(p, dest);
    DCHECK_IMPLIES(Heap::InYoungGeneration(dest),
                   Heap::InToPage(dest) || Heap::IsLargeObject(dest));

    return Heap::InYoungGeneration(dest) ? KEEP_SLOT : REMOVE_SLOT;
  }

  Map map = first_word.ToMap();
  // AllocationMementos are unrooted and shouldn't survive a scavenge
  DCHECK_NE(ReadOnlyRoots(heap()).allocation_memento_map(), map);
  // Call the slow part of scavenge object.
  return EvacuateObject(p, map, object);
}

template <typename TSlot>
SlotCallbackResult Scavenger::CheckAndScavengeObject(Heap* heap, TSlot slot) {
  static_assert(
      std::is_same<TSlot, FullMaybeObjectSlot>::value ||
          std::is_same<TSlot, MaybeObjectSlot>::value,
      "Only FullMaybeObjectSlot and MaybeObjectSlot are expected here");
  using THeapObjectSlot = typename TSlot::THeapObjectSlot;
  MaybeObject object = *slot;
  if (Heap::InFromPage(object)) {
    HeapObject heap_object = object->GetHeapObject();

    SlotCallbackResult result =
        ScavengeObject(THeapObjectSlot(slot), heap_object);
    DCHECK_IMPLIES(result == REMOVE_SLOT,
                   !heap->InYoungGeneration((*slot)->GetHeapObject()));
    return result;
  } else if (Heap::InToPage(object)) {
    // Already updated slot. This can happen when processing of the work list
    // is interleaved with processing roots.
    return KEEP_SLOT;
  }
  // Slots can point to "to" space if the slot has been recorded multiple
  // times in the remembered set. We remove the redundant slot now.
  return REMOVE_SLOT;
}

void ScavengeVisitor::VisitPointers(HeapObject host, ObjectSlot start,
                                    ObjectSlot end) {
  return VisitPointersImpl(host, start, end);
}

void ScavengeVisitor::VisitPointers(HeapObject host, MaybeObjectSlot start,
                                    MaybeObjectSlot end) {
  return VisitPointersImpl(host, start, end);
}

void ScavengeVisitor::VisitCodeTarget(Code host, RelocInfo* rinfo) {
  Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
#ifdef DEBUG
  Code old_target = target;
#endif
  FullObjectSlot slot(&target);
  VisitHeapObjectImpl(slot, target);
  // Code objects are never in new-space, so the slot contents must not change.
  DCHECK_EQ(old_target, target);
}

void ScavengeVisitor::VisitEmbeddedPointer(Code host, RelocInfo* rinfo) {
  HeapObject heap_object = rinfo->target_object();
#ifdef DEBUG
  HeapObject old_heap_object = heap_object;
#endif
  FullObjectSlot slot(&heap_object);
  VisitHeapObjectImpl(slot, heap_object);
  // We don't embed new-space objects into code, so the slot contents must not
  // change.
  DCHECK_EQ(old_heap_object, heap_object);
}

template <typename TSlot>
void ScavengeVisitor::VisitHeapObjectImpl(TSlot slot, HeapObject heap_object) {
  if (Heap::InYoungGeneration(heap_object)) {
    using THeapObjectSlot = typename TSlot::THeapObjectSlot;
    scavenger_->ScavengeObject(THeapObjectSlot(slot), heap_object);
  }
}

template <typename TSlot>
void ScavengeVisitor::VisitPointersImpl(HeapObject host, TSlot start,
                                        TSlot end) {
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object = *slot;
    HeapObject heap_object;
    // Treat weak references as strong.
    if (object.GetHeapObject(&heap_object)) {
      VisitHeapObjectImpl(slot, heap_object);
    }
  }
}

int ScavengeVisitor::VisitEphemeronHashTable(Map map,
                                             EphemeronHashTable table) {
  // Register table with the scavenger, so it can take care of the weak keys
  // later. This allows to only iterate the tables' values, which are treated
  // as strong independetly of whether the key is live.
  scavenger_->AddEphemeronHashTable(table);
  for (int i = 0; i < table.Capacity(); i++) {
    ObjectSlot value_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));
    VisitPointer(table, value_slot);
  }

  return table.SizeFromMap(map);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_INL_H_
