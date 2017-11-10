// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_INL_H_
#define V8_HEAP_HEAP_INL_H_

#include <cmath>

#include "src/base/platform/platform.h"
#include "src/counters-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/object-stats.h"
#include "src/heap/remembered-set.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/store-buffer.h"
#include "src/isolate.h"
#include "src/list-inl.h"
#include "src/log.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/objects/scope-info.h"
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {

AllocationSpace AllocationResult::RetrySpace() {
  DCHECK(IsRetry());
  return static_cast<AllocationSpace>(Smi::cast(object_)->value());
}

HeapObject* AllocationResult::ToObjectChecked() {
  CHECK(!IsRetry());
  return HeapObject::cast(object_);
}

void PromotionQueue::insert(HeapObject* target, int32_t size,
                            bool was_marked_black) {
  if (emergency_stack_ != NULL) {
    emergency_stack_->Add(Entry(target, size, was_marked_black));
    return;
  }

  if ((rear_ - 1) < limit_) {
    RelocateQueueHead();
    emergency_stack_->Add(Entry(target, size, was_marked_black));
    return;
  }

  struct Entry* entry = reinterpret_cast<struct Entry*>(--rear_);
  entry->obj_ = target;
  entry->size_ = size;
  entry->was_marked_black_ = was_marked_black;

// Assert no overflow into live objects.
#ifdef DEBUG
  SemiSpace::AssertValidRange(target->GetIsolate()->heap()->new_space()->top(),
                              reinterpret_cast<Address>(rear_));
#endif
}

void PromotionQueue::remove(HeapObject** target, int32_t* size,
                            bool* was_marked_black) {
  DCHECK(!is_empty());
  if (front_ == rear_) {
    Entry e = emergency_stack_->RemoveLast();
    *target = e.obj_;
    *size = e.size_;
    *was_marked_black = e.was_marked_black_;
    return;
  }

  struct Entry* entry = reinterpret_cast<struct Entry*>(--front_);
  *target = entry->obj_;
  *size = entry->size_;
  *was_marked_black = entry->was_marked_black_;

  // Assert no underflow.
  SemiSpace::AssertValidRange(reinterpret_cast<Address>(rear_),
                              reinterpret_cast<Address>(front_));
}

Page* PromotionQueue::GetHeadPage() {
  return Page::FromAllocationAreaAddress(reinterpret_cast<Address>(rear_));
}

void PromotionQueue::SetNewLimit(Address limit) {
  // If we are already using an emergency stack, we can ignore it.
  if (emergency_stack_) return;

  // If the limit is not on the same page, we can ignore it.
  if (Page::FromAllocationAreaAddress(limit) != GetHeadPage()) return;

  limit_ = reinterpret_cast<struct Entry*>(limit);

  if (limit_ <= rear_) {
    return;
  }

  RelocateQueueHead();
}

bool PromotionQueue::IsBelowPromotionQueue(Address to_space_top) {
  // If an emergency stack is used, the to-space address cannot interfere
  // with the promotion queue.
  if (emergency_stack_) return true;

  // If the given to-space top pointer and the head of the promotion queue
  // are not on the same page, then the to-space objects are below the
  // promotion queue.
  if (GetHeadPage() != Page::FromAddress(to_space_top)) {
    return true;
  }
  // If the to space top pointer is smaller or equal than the promotion
  // queue head, then the to-space objects are below the promotion queue.
  return reinterpret_cast<struct Entry*>(to_space_top) <= rear_;
}

#define ROOT_ACCESSOR(type, name, camel_name) \
  type* Heap::name() { return type::cast(roots_[k##camel_name##RootIndex]); }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name) \
  Map* Heap::name##_map() { return Map::cast(roots_[k##Name##MapRootIndex]); }
STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) \
  String* Heap::name() { return String::cast(roots_[k##name##RootIndex]); }
INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) \
  Symbol* Heap::name() { return Symbol::cast(roots_[k##name##RootIndex]); }
PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description) \
  Symbol* Heap::name() { return Symbol::cast(roots_[k##name##RootIndex]); }
PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define ROOT_ACCESSOR(type, name, camel_name)                                 \
  void Heap::set_##name(type* value) {                                        \
    /* The deserializer makes use of the fact that these common roots are */  \
    /* never in new space and never on a page that is being compacted.    */  \
    DCHECK(!deserialization_complete() ||                                     \
           RootCanBeWrittenAfterInitialization(k##camel_name##RootIndex));    \
    DCHECK(k##camel_name##RootIndex >= kOldSpaceRoots || !InNewSpace(value)); \
    roots_[k##camel_name##RootIndex] = value;                                 \
  }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

PagedSpace* Heap::paged_space(int idx) {
  DCHECK_NE(idx, LO_SPACE);
  DCHECK_NE(idx, NEW_SPACE);
  return static_cast<PagedSpace*>(space_[idx]);
}

Space* Heap::space(int idx) { return space_[idx]; }

Address* Heap::NewSpaceAllocationTopAddress() {
  return new_space_->allocation_top_address();
}

Address* Heap::NewSpaceAllocationLimitAddress() {
  return new_space_->allocation_limit_address();
}

Address* Heap::OldSpaceAllocationTopAddress() {
  return old_space_->allocation_top_address();
}

Address* Heap::OldSpaceAllocationLimitAddress() {
  return old_space_->allocation_limit_address();
}

void Heap::UpdateNewSpaceAllocationCounter() {
  new_space_allocation_counter_ = NewSpaceAllocationCounter();
}

size_t Heap::NewSpaceAllocationCounter() {
  return new_space_allocation_counter_ + new_space()->AllocatedSinceLastGC();
}

template <>
bool inline Heap::IsOneByte(Vector<const char> str, int chars) {
  // TODO(dcarney): incorporate Latin-1 check when Latin-1 is supported?
  return chars == str.length();
}


template <>
bool inline Heap::IsOneByte(String* str, int chars) {
  return str->IsOneByteRepresentation();
}


AllocationResult Heap::AllocateInternalizedStringFromUtf8(
    Vector<const char> str, int chars, uint32_t hash_field) {
  if (IsOneByte(str, chars)) {
    return AllocateOneByteInternalizedString(Vector<const uint8_t>::cast(str),
                                             hash_field);
  }
  return AllocateInternalizedStringImpl<false>(str, chars, hash_field);
}


template <typename T>
AllocationResult Heap::AllocateInternalizedStringImpl(T t, int chars,
                                                      uint32_t hash_field) {
  if (IsOneByte(t, chars)) {
    return AllocateInternalizedStringImpl<true>(t, chars, hash_field);
  }
  return AllocateInternalizedStringImpl<false>(t, chars, hash_field);
}


AllocationResult Heap::AllocateOneByteInternalizedString(
    Vector<const uint8_t> str, uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, str.length());
  // Compute map and object size.
  Map* map = one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(str.length());

  // Allocate string.
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  // String maps are all immortal immovable objects.
  result->set_map_no_write_barrier(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  DCHECK_EQ(size, answer->Size());

  // Fill in the characters.
  MemCopy(answer->address() + SeqOneByteString::kHeaderSize, str.start(),
          str.length());

  return answer;
}


AllocationResult Heap::AllocateTwoByteInternalizedString(Vector<const uc16> str,
                                                         uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, str.length());
  // Compute map and object size.
  Map* map = internalized_string_map();
  int size = SeqTwoByteString::SizeFor(str.length());

  // Allocate string.
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  DCHECK_EQ(size, answer->Size());

  // Fill in the characters.
  MemCopy(answer->address() + SeqTwoByteString::kHeaderSize, str.start(),
          str.length() * kUC16Size);

  return answer;
}

AllocationResult Heap::CopyFixedArray(FixedArray* src) {
  if (src->length() == 0) return src;
  return CopyFixedArrayWithMap(src, src->map());
}


AllocationResult Heap::CopyFixedDoubleArray(FixedDoubleArray* src) {
  if (src->length() == 0) return src;
  return CopyFixedDoubleArrayWithMap(src, src->map());
}


AllocationResult Heap::AllocateRaw(int size_in_bytes, AllocationSpace space,
                                   AllocationAlignment alignment) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(gc_state_ == NOT_IN_GC);
#ifdef DEBUG
  if (FLAG_gc_interval >= 0 && !always_allocate() &&
      Heap::allocation_timeout_-- <= 0) {
    return AllocationResult::Retry(space);
  }
  isolate_->counters()->objs_since_last_full()->Increment();
  isolate_->counters()->objs_since_last_young()->Increment();
#endif

  bool large_object = size_in_bytes > kMaxRegularHeapObjectSize;
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (NEW_SPACE == space) {
    if (large_object) {
      space = LO_SPACE;
    } else {
      allocation = new_space_->AllocateRaw(size_in_bytes, alignment);
      if (allocation.To(&object)) {
        OnAllocationEvent(object, size_in_bytes);
      }
      return allocation;
    }
  }

  // Here we only allocate in the old generation.
  if (OLD_SPACE == space) {
    if (large_object) {
      allocation = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
    } else {
      allocation = old_space_->AllocateRaw(size_in_bytes, alignment);
    }
  } else if (CODE_SPACE == space) {
    if (size_in_bytes <= code_space()->AreaSize()) {
      allocation = code_space_->AllocateRawUnaligned(size_in_bytes);
    } else {
      allocation = lo_space_->AllocateRaw(size_in_bytes, EXECUTABLE);
    }
  } else if (LO_SPACE == space) {
    DCHECK(large_object);
    allocation = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
  } else if (MAP_SPACE == space) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
  } else {
    // NEW_SPACE is not allowed here.
    UNREACHABLE();
  }
  if (allocation.To(&object)) {
    OnAllocationEvent(object, size_in_bytes);
  }

  return allocation;
}


void Heap::OnAllocationEvent(HeapObject* object, int size_in_bytes) {
  HeapProfiler* profiler = isolate_->heap_profiler();
  if (profiler->is_tracking_allocations()) {
    profiler->AllocationEvent(object->address(), size_in_bytes);
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(object);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAlloctionsHash();
    }
  }

  if (FLAG_trace_allocation_stack_interval > 0) {
    if (!FLAG_verify_predictable) ++allocations_count_;
    if (allocations_count_ % FLAG_trace_allocation_stack_interval == 0) {
      isolate()->PrintStack(stdout, Isolate::kPrintStackConcise);
    }
  }
}


void Heap::OnMoveEvent(HeapObject* target, HeapObject* source,
                       int size_in_bytes) {
  HeapProfiler* heap_profiler = isolate_->heap_profiler();
  if (heap_profiler->is_tracking_object_moves()) {
    heap_profiler->ObjectMoveEvent(source->address(), target->address(),
                                   size_in_bytes);
  }
  if (target->IsSharedFunctionInfo()) {
    LOG_CODE_EVENT(isolate_, SharedFunctionInfoMoveEvent(source->address(),
                                                         target->address()));
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(source);
    UpdateAllocationsHash(target);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAlloctionsHash();
    }
  }
}


void Heap::UpdateAllocationsHash(HeapObject* object) {
  Address object_address = object->address();
  MemoryChunk* memory_chunk = MemoryChunk::FromAddress(object_address);
  AllocationSpace allocation_space = memory_chunk->owner()->identity();

  STATIC_ASSERT(kSpaceTagSize + kPageSizeBits <= 32);
  uint32_t value =
      static_cast<uint32_t>(object_address - memory_chunk->address()) |
      (static_cast<uint32_t>(allocation_space) << kPageSizeBits);

  UpdateAllocationsHash(value);
}


void Heap::UpdateAllocationsHash(uint32_t value) {
  uint16_t c1 = static_cast<uint16_t>(value);
  uint16_t c2 = static_cast<uint16_t>(value >> 16);
  raw_allocations_hash_ =
      StringHasher::AddCharacterCore(raw_allocations_hash_, c1);
  raw_allocations_hash_ =
      StringHasher::AddCharacterCore(raw_allocations_hash_, c2);
}


void Heap::RegisterExternalString(String* string) {
  external_string_table_.AddString(string);
}


void Heap::FinalizeExternalString(String* string) {
  DCHECK(string->IsExternalString());
  v8::String::ExternalStringResourceBase** resource_addr =
      reinterpret_cast<v8::String::ExternalStringResourceBase**>(
          reinterpret_cast<byte*>(string) + ExternalString::kResourceOffset -
          kHeapObjectTag);

  // Dispose of the C++ object if it has not already been disposed.
  if (*resource_addr != NULL) {
    (*resource_addr)->Dispose();
    *resource_addr = NULL;
  }
}

Address Heap::NewSpaceTop() { return new_space_->top(); }

bool Heap::DeoptMaybeTenuredAllocationSites() {
  return new_space_->IsAtMaximumCapacity() && maximum_size_scavenges_ == 0;
}

bool Heap::InNewSpace(Object* object) {
  // Inlined check from NewSpace::Contains.
  bool result =
      object->IsHeapObject() &&
      Page::FromAddress(HeapObject::cast(object)->address())->InNewSpace();
  DCHECK(!result ||                 // Either not in new space
         gc_state_ != NOT_IN_GC ||  // ... or in the middle of GC
         InToSpace(object));        // ... or in to-space (where we allocate).
  return result;
}

bool Heap::InFromSpace(Object* object) {
  return object->IsHeapObject() &&
         MemoryChunk::FromAddress(HeapObject::cast(object)->address())
             ->IsFlagSet(Page::IN_FROM_SPACE);
}


bool Heap::InToSpace(Object* object) {
  return object->IsHeapObject() &&
         MemoryChunk::FromAddress(HeapObject::cast(object)->address())
             ->IsFlagSet(Page::IN_TO_SPACE);
}

bool Heap::InOldSpace(Object* object) { return old_space_->Contains(object); }

bool Heap::InNewSpaceSlow(Address address) {
  return new_space_->ContainsSlow(address);
}

bool Heap::InOldSpaceSlow(Address address) {
  return old_space_->ContainsSlow(address);
}

bool Heap::ShouldBePromoted(Address old_address, int object_size) {
  Page* page = Page::FromAddress(old_address);
  Address age_mark = new_space_->age_mark();
  return page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK) &&
         (!page->ContainsLimit(age_mark) || old_address < age_mark);
}

void Heap::RecordWrite(Object* object, int offset, Object* o) {
  if (!InNewSpace(o) || !object->IsHeapObject() || InNewSpace(object)) {
    return;
  }
  store_buffer()->InsertEntry(HeapObject::cast(object)->address() + offset);
}

void Heap::RecordWriteIntoCode(Code* host, RelocInfo* rinfo, Object* value) {
  if (InNewSpace(value)) {
    RecordWriteIntoCodeSlow(host, rinfo, value);
  }
}

void Heap::RecordFixedArrayElements(FixedArray* array, int offset, int length) {
  if (InNewSpace(array)) return;
  for (int i = 0; i < length; i++) {
    if (!InNewSpace(array->get(offset + i))) continue;
    store_buffer()->InsertEntry(
        reinterpret_cast<Address>(array->RawFieldOfElementAt(offset + i)));
  }
}

Address* Heap::store_buffer_top_address() {
  return store_buffer()->top_address();
}

bool Heap::AllowedToBeMigrated(HeapObject* obj, AllocationSpace dst) {
  // Object migration is governed by the following rules:
  //
  // 1) Objects in new-space can be migrated to the old space
  //    that matches their target space or they stay in new-space.
  // 2) Objects in old-space stay in the same space when migrating.
  // 3) Fillers (two or more words) can migrate due to left-trimming of
  //    fixed arrays in new-space or old space.
  // 4) Fillers (one word) can never migrate, they are skipped by
  //    incremental marking explicitly to prevent invalid pattern.
  //
  // Since this function is used for debugging only, we do not place
  // asserts here, but check everything explicitly.
  if (obj->map() == one_pointer_filler_map()) return false;
  InstanceType type = obj->map()->instance_type();
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  AllocationSpace src = chunk->owner()->identity();
  switch (src) {
    case NEW_SPACE:
      return dst == src || dst == OLD_SPACE;
    case OLD_SPACE:
      return dst == src &&
             (dst == OLD_SPACE || obj->IsFiller() || obj->IsExternalString());
    case CODE_SPACE:
      return dst == src && type == CODE_TYPE;
    case MAP_SPACE:
    case LO_SPACE:
      return false;
  }
  UNREACHABLE();
  return false;
}

void Heap::CopyBlock(Address dst, Address src, int byte_size) {
  CopyWords(reinterpret_cast<Object**>(dst), reinterpret_cast<Object**>(src),
            static_cast<size_t>(byte_size / kPointerSize));
}

template <Heap::FindMementoMode mode>
AllocationMemento* Heap::FindAllocationMemento(HeapObject* object) {
  Address object_address = object->address();
  Address memento_address = object_address + object->Size();
  Address last_memento_word_address = memento_address + kPointerSize;
  // If the memento would be on another page, bail out immediately.
  if (!Page::OnSamePage(object_address, last_memento_word_address)) {
    return nullptr;
  }
  HeapObject* candidate = HeapObject::FromAddress(memento_address);
  Map* candidate_map = candidate->map();
  // This fast check may peek at an uninitialized word. However, the slow check
  // below (memento_address == top) ensures that this is safe. Mark the word as
  // initialized to silence MemorySanitizer warnings.
  MSAN_MEMORY_IS_INITIALIZED(&candidate_map, sizeof(candidate_map));
  if (candidate_map != allocation_memento_map()) {
    return nullptr;
  }

  // Bail out if the memento is below the age mark, which can happen when
  // mementos survived because a page got moved within new space.
  Page* object_page = Page::FromAddress(object_address);
  if (object_page->IsFlagSet(Page::NEW_SPACE_BELOW_AGE_MARK)) {
    Address age_mark =
        reinterpret_cast<SemiSpace*>(object_page->owner())->age_mark();
    if (!object_page->Contains(age_mark)) {
      return nullptr;
    }
    // Do an exact check in the case where the age mark is on the same page.
    if (object_address < age_mark) {
      return nullptr;
    }
  }

  AllocationMemento* memento_candidate = AllocationMemento::cast(candidate);

  // Depending on what the memento is used for, we might need to perform
  // additional checks.
  Address top;
  switch (mode) {
    case Heap::kForGC:
      return memento_candidate;
    case Heap::kForRuntime:
      if (memento_candidate == nullptr) return nullptr;
      // Either the object is the last object in the new space, or there is
      // another object of at least word size (the header map word) following
      // it, so suffices to compare ptr and top here.
      top = NewSpaceTop();
      DCHECK(memento_address == top ||
             memento_address + HeapObject::kHeaderSize <= top ||
             !Page::OnSamePage(memento_address, top - 1));
      if ((memento_address != top) && memento_candidate->IsValid()) {
        return memento_candidate;
      }
      return nullptr;
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return nullptr;
}

template <Heap::UpdateAllocationSiteMode mode>
void Heap::UpdateAllocationSite(HeapObject* object,
                                base::HashMap* pretenuring_feedback) {
  DCHECK(InFromSpace(object) ||
         (InToSpace(object) &&
          Page::FromAddress(object->address())
              ->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) ||
         (!InNewSpace(object) &&
          Page::FromAddress(object->address())
              ->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)));
  if (!FLAG_allocation_site_pretenuring ||
      !AllocationSite::CanTrack(object->map()->instance_type()))
    return;
  AllocationMemento* memento_candidate = FindAllocationMemento<kForGC>(object);
  if (memento_candidate == nullptr) return;

  if (mode == kGlobal) {
    DCHECK_EQ(pretenuring_feedback, global_pretenuring_feedback_);
    // Entering global pretenuring feedback is only used in the scavenger, where
    // we are allowed to actually touch the allocation site.
    if (!memento_candidate->IsValid()) return;
    AllocationSite* site = memento_candidate->GetAllocationSite();
    DCHECK(!site->IsZombie());
    // For inserting in the global pretenuring storage we need to first
    // increment the memento found count on the allocation site.
    if (site->IncrementMementoFoundCount()) {
      global_pretenuring_feedback_->LookupOrInsert(site,
                                                   ObjectHash(site->address()));
    }
  } else {
    DCHECK_EQ(mode, kCached);
    DCHECK_NE(pretenuring_feedback, global_pretenuring_feedback_);
    // Entering cached feedback is used in the parallel case. We are not allowed
    // to dereference the allocation site and rather have to postpone all checks
    // till actually merging the data.
    Address key = memento_candidate->GetAllocationSiteUnchecked();
    base::HashMap::Entry* e =
        pretenuring_feedback->LookupOrInsert(key, ObjectHash(key));
    DCHECK(e != nullptr);
    (*bit_cast<intptr_t*>(&e->value))++;
  }
}


void Heap::RemoveAllocationSitePretenuringFeedback(AllocationSite* site) {
  global_pretenuring_feedback_->Remove(
      site, static_cast<uint32_t>(bit_cast<uintptr_t>(site)));
}

bool Heap::CollectGarbage(AllocationSpace space,
                          GarbageCollectionReason gc_reason,
                          const v8::GCCallbackFlags callbackFlags) {
  const char* collector_reason = NULL;
  GarbageCollector collector = SelectGarbageCollector(space, &collector_reason);
  return CollectGarbage(collector, gc_reason, collector_reason, callbackFlags);
}


Isolate* Heap::isolate() {
  return reinterpret_cast<Isolate*>(
      reinterpret_cast<intptr_t>(this) -
      reinterpret_cast<size_t>(reinterpret_cast<Isolate*>(16)->heap()) + 16);
}


void Heap::ExternalStringTable::AddString(String* string) {
  DCHECK(string->IsExternalString());
  if (heap_->InNewSpace(string)) {
    new_space_strings_.Add(string);
  } else {
    old_space_strings_.Add(string);
  }
}

void Heap::ExternalStringTable::IterateNewSpaceStrings(ObjectVisitor* v) {
  if (!new_space_strings_.is_empty()) {
    Object** start = &new_space_strings_[0];
    v->VisitPointers(start, start + new_space_strings_.length());
  }
}

void Heap::ExternalStringTable::IterateAll(ObjectVisitor* v) {
  IterateNewSpaceStrings(v);
  if (!old_space_strings_.is_empty()) {
    Object** start = &old_space_strings_[0];
    v->VisitPointers(start, start + old_space_strings_.length());
  }
}


// Verify() is inline to avoid ifdef-s around its calls in release
// mode.
void Heap::ExternalStringTable::Verify() {
#ifdef DEBUG
  for (int i = 0; i < new_space_strings_.length(); ++i) {
    Object* obj = Object::cast(new_space_strings_[i]);
    DCHECK(heap_->InNewSpace(obj));
    DCHECK(!obj->IsTheHole(heap_->isolate()));
  }
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    Object* obj = Object::cast(old_space_strings_[i]);
    DCHECK(!heap_->InNewSpace(obj));
    DCHECK(!obj->IsTheHole(heap_->isolate()));
  }
#endif
}


void Heap::ExternalStringTable::AddOldString(String* string) {
  DCHECK(string->IsExternalString());
  DCHECK(!heap_->InNewSpace(string));
  old_space_strings_.Add(string);
}


void Heap::ExternalStringTable::ShrinkNewStrings(int position) {
  new_space_strings_.Rewind(position);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}

void Heap::ClearInstanceofCache() { set_instanceof_cache_function(Smi::kZero); }

Oddball* Heap::ToBoolean(bool condition) {
  return condition ? true_value() : false_value();
}


void Heap::CompletelyClearInstanceofCache() {
  set_instanceof_cache_map(Smi::kZero);
  set_instanceof_cache_function(Smi::kZero);
}


uint32_t Heap::HashSeed() {
  uint32_t seed = static_cast<uint32_t>(hash_seed()->value());
  DCHECK(FLAG_randomize_hashes || seed == 0);
  return seed;
}


int Heap::NextScriptId() {
  int last_id = last_script_id()->value();
  if (last_id == Smi::kMaxValue) {
    last_id = 1;
  } else {
    last_id++;
  }
  set_last_script_id(Smi::FromInt(last_id));
  return last_id;
}

void Heap::SetArgumentsAdaptorDeoptPCOffset(int pc_offset) {
  DCHECK(arguments_adaptor_deopt_pc_offset() == Smi::kZero);
  set_arguments_adaptor_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubDeoptPCOffset(int pc_offset) {
  DCHECK(construct_stub_deopt_pc_offset() == Smi::kZero);
  set_construct_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetGetterStubDeoptPCOffset(int pc_offset) {
  DCHECK(getter_stub_deopt_pc_offset() == Smi::kZero);
  set_getter_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetSetterStubDeoptPCOffset(int pc_offset) {
  DCHECK(setter_stub_deopt_pc_offset() == Smi::kZero);
  set_setter_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetInterpreterEntryReturnPCOffset(int pc_offset) {
  DCHECK(interpreter_entry_return_pc_offset() == Smi::kZero);
  set_interpreter_entry_return_pc_offset(Smi::FromInt(pc_offset));
}

int Heap::GetNextTemplateSerialNumber() {
  int next_serial_number = next_template_serial_number()->value() + 1;
  set_next_template_serial_number(Smi::FromInt(next_serial_number));
  return next_serial_number;
}

void Heap::SetSerializedTemplates(FixedArray* templates) {
  DCHECK_EQ(empty_fixed_array(), serialized_templates());
  DCHECK(isolate()->serializer_enabled());
  set_serialized_templates(templates);
}

void Heap::SetSerializedGlobalProxySizes(FixedArray* sizes) {
  DCHECK_EQ(empty_fixed_array(), serialized_global_proxy_sizes());
  DCHECK(isolate()->serializer_enabled());
  set_serialized_global_proxy_sizes(sizes);
}

void Heap::CreateObjectStats() {
  if (V8_LIKELY(FLAG_gc_stats == 0)) return;
  if (!live_object_stats_) {
    live_object_stats_ = new ObjectStats(this);
  }
  if (!dead_object_stats_) {
    dead_object_stats_ = new ObjectStats(this);
  }
}

AlwaysAllocateScope::AlwaysAllocateScope(Isolate* isolate)
    : heap_(isolate->heap()) {
  heap_->always_allocate_scope_count_.Increment(1);
}


AlwaysAllocateScope::~AlwaysAllocateScope() {
  heap_->always_allocate_scope_count_.Increment(-1);
}


void VerifyPointersVisitor::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    if ((*current)->IsHeapObject()) {
      HeapObject* object = HeapObject::cast(*current);
      CHECK(object->GetIsolate()->heap()->Contains(object));
      CHECK(object->map()->IsMap());
    }
  }
}


void VerifySmisVisitor::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    CHECK((*current)->IsSmi());
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_INL_H_
