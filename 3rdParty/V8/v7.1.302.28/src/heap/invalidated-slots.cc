// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/invalidated-slots.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

InvalidatedSlotsFilter::InvalidatedSlotsFilter(MemoryChunk* chunk) {
  // Adjust slots_in_free_space_are_valid_ if more spaces are added.
  DCHECK_IMPLIES(chunk->invalidated_slots() != nullptr,
                 chunk->InOldSpace() || chunk->InLargeObjectSpace());
  // The sweeper removes invalid slots and makes free space available for
  // allocation. Slots for new objects can be recorded in the free space.
  // Note that we cannot simply check for SweepingDone because pages in large
  // object space are not swept but have SweepingDone() == true.
  slots_in_free_space_are_valid_ = chunk->SweepingDone() && chunk->InOldSpace();

  InvalidatedSlots* invalidated_slots =
      chunk->invalidated_slots() ? chunk->invalidated_slots() : &empty_;
  iterator_ = invalidated_slots->begin();
  iterator_end_ = invalidated_slots->end();
  sentinel_ = chunk->area_end();
  if (iterator_ != iterator_end_) {
    invalidated_start_ = iterator_->first->address();
    invalidated_end_ = invalidated_start_ + iterator_->second;
  } else {
    invalidated_start_ = sentinel_;
    invalidated_end_ = sentinel_;
  }
  // These values will be lazily set when needed.
  invalidated_object_ = nullptr;
  invalidated_object_size_ = 0;
#ifdef DEBUG
  last_slot_ = chunk->area_start();
#endif
}

}  // namespace internal
}  // namespace v8
