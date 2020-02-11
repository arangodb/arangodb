// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/serializer-allocator.h"

#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/snapshot/references.h"
#include "src/snapshot/serializer.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

SerializerAllocator::SerializerAllocator(Serializer* serializer)
    : serializer_(serializer) {
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) {
    pending_chunk_[i] = 0;
  }
}

void SerializerAllocator::UseCustomChunkSize(uint32_t chunk_size) {
  custom_chunk_size_ = chunk_size;
}

static uint32_t PageSizeOfSpace(SnapshotSpace space) {
  return static_cast<uint32_t>(
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
          static_cast<AllocationSpace>(space)));
}

uint32_t SerializerAllocator::TargetChunkSize(SnapshotSpace space) {
  if (custom_chunk_size_ == 0) return PageSizeOfSpace(space);
  DCHECK_LE(custom_chunk_size_, PageSizeOfSpace(space));
  return custom_chunk_size_;
}

SerializerReference SerializerAllocator::Allocate(SnapshotSpace space,
                                                  uint32_t size) {
  const int space_number = static_cast<int>(space);
  DCHECK(IsPreAllocatedSpace(space));
  DCHECK(size > 0 && size <= PageSizeOfSpace(space));

  // Maps are allocated through AllocateMap.
  DCHECK_NE(SnapshotSpace::kMap, space);

  uint32_t old_chunk_size = pending_chunk_[space_number];
  uint32_t new_chunk_size = old_chunk_size + size;
  // Start a new chunk if the new size exceeds the target chunk size.
  // We may exceed the target chunk size if the single object size does.
  if (new_chunk_size > TargetChunkSize(space) && old_chunk_size != 0) {
    serializer_->PutNextChunk(space);
    completed_chunks_[space_number].push_back(pending_chunk_[space_number]);
    pending_chunk_[space_number] = 0;
    new_chunk_size = size;
  }
  uint32_t offset = pending_chunk_[space_number];
  pending_chunk_[space_number] = new_chunk_size;
  return SerializerReference::BackReference(
      space, static_cast<uint32_t>(completed_chunks_[space_number].size()),
      offset);
}

SerializerReference SerializerAllocator::AllocateMap() {
  // Maps are allocated one-by-one when deserializing.
  return SerializerReference::MapReference(num_maps_++);
}

SerializerReference SerializerAllocator::AllocateLargeObject(uint32_t size) {
  // Large objects are allocated one-by-one when deserializing. We do not
  // have to keep track of multiple chunks.
  large_objects_total_size_ += size;
  return SerializerReference::LargeObjectReference(seen_large_objects_index_++);
}

SerializerReference SerializerAllocator::AllocateOffHeapBackingStore() {
  DCHECK_NE(0, seen_backing_stores_index_);
  return SerializerReference::OffHeapBackingStoreReference(
      seen_backing_stores_index_++);
}

#ifdef DEBUG
bool SerializerAllocator::BackReferenceIsAlreadyAllocated(
    SerializerReference reference) const {
  DCHECK(reference.is_back_reference());
  SnapshotSpace space = reference.space();
  if (space == SnapshotSpace::kLargeObject) {
    return reference.large_object_index() < seen_large_objects_index_;
  } else if (space == SnapshotSpace::kMap) {
    return reference.map_index() < num_maps_;
  } else if (space == SnapshotSpace::kReadOnlyHeap &&
             serializer_->isolate()->heap()->deserialization_complete()) {
    // If not deserializing the isolate itself, then we create BackReferences
    // for all read-only heap objects without ever allocating.
    return true;
  } else {
    const int space_number = static_cast<int>(space);
    size_t chunk_index = reference.chunk_index();
    if (chunk_index == completed_chunks_[space_number].size()) {
      return reference.chunk_offset() < pending_chunk_[space_number];
    } else {
      return chunk_index < completed_chunks_[space_number].size() &&
             reference.chunk_offset() <
                 completed_chunks_[space_number][chunk_index];
    }
  }
}
#endif

std::vector<SerializedData::Reservation>
SerializerAllocator::EncodeReservations() const {
  std::vector<SerializedData::Reservation> out;

  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) {
    for (size_t j = 0; j < completed_chunks_[i].size(); j++) {
      out.emplace_back(completed_chunks_[i][j]);
    }

    if (pending_chunk_[i] > 0 || completed_chunks_[i].size() == 0) {
      out.emplace_back(pending_chunk_[i]);
    }
    out.back().mark_as_last();
  }

  STATIC_ASSERT(SnapshotSpace::kMap ==
                SnapshotSpace::kNumberOfPreallocatedSpaces);
  out.emplace_back(num_maps_ * Map::kSize);
  out.back().mark_as_last();

  STATIC_ASSERT(static_cast<int>(SnapshotSpace::kLargeObject) ==
                static_cast<int>(SnapshotSpace::kNumberOfPreallocatedSpaces) +
                    1);
  out.emplace_back(large_objects_total_size_);
  out.back().mark_as_last();

  return out;
}

void SerializerAllocator::OutputStatistics() {
  DCHECK(FLAG_serialization_statistics);

  PrintF("  Spaces (bytes):\n");

  for (int space = 0; space < kNumberOfSpaces; space++) {
    PrintF("%16s", Heap::GetSpaceName(static_cast<AllocationSpace>(space)));
  }
  PrintF("\n");

  for (int space = 0; space < kNumberOfPreallocatedSpaces; space++) {
    size_t s = pending_chunk_[space];
    for (uint32_t chunk_size : completed_chunks_[space]) s += chunk_size;
    PrintF("%16zu", s);
  }

  STATIC_ASSERT(SnapshotSpace::kMap ==
                SnapshotSpace::kNumberOfPreallocatedSpaces);
  PrintF("%16d", num_maps_ * Map::kSize);

  STATIC_ASSERT(static_cast<int>(SnapshotSpace::kLargeObject) ==
                static_cast<int>(SnapshotSpace::kNumberOfPreallocatedSpaces) +
                    1);
  PrintF("%16d\n", large_objects_total_size_);
}

}  // namespace internal
}  // namespace v8
