// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-idle-time-handler.h"

#include "src/flags/flags.h"
#include "src/heap/gc-tracer.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

const double GCIdleTimeHandler::kConservativeTimeRatio = 0.9;
const size_t GCIdleTimeHandler::kMaxFinalIncrementalMarkCompactTimeInMs = 1000;
const double GCIdleTimeHandler::kHighContextDisposalRate = 100;
const size_t GCIdleTimeHandler::kMinTimeForOverApproximatingWeakClosureInMs = 1;


void GCIdleTimeHeapState::Print() {
  PrintF("contexts_disposed=%d ", contexts_disposed);
  PrintF("contexts_disposal_rate=%f ", contexts_disposal_rate);
  PrintF("size_of_objects=%zu ", size_of_objects);
  PrintF("incremental_marking_stopped=%d ", incremental_marking_stopped);
}

size_t GCIdleTimeHandler::EstimateMarkingStepSize(
    double idle_time_in_ms, double marking_speed_in_bytes_per_ms) {
  DCHECK_LT(0, idle_time_in_ms);

  if (marking_speed_in_bytes_per_ms == 0) {
    marking_speed_in_bytes_per_ms = kInitialConservativeMarkingSpeed;
  }

  double marking_step_size = marking_speed_in_bytes_per_ms * idle_time_in_ms;
  if (marking_step_size >= kMaximumMarkingStepSize) {
    return kMaximumMarkingStepSize;
  }
  return static_cast<size_t>(marking_step_size * kConservativeTimeRatio);
}

double GCIdleTimeHandler::EstimateFinalIncrementalMarkCompactTime(
    size_t size_of_objects,
    double final_incremental_mark_compact_speed_in_bytes_per_ms) {
  if (final_incremental_mark_compact_speed_in_bytes_per_ms == 0) {
    final_incremental_mark_compact_speed_in_bytes_per_ms =
        kInitialConservativeFinalIncrementalMarkCompactSpeed;
  }
  double result =
      size_of_objects / final_incremental_mark_compact_speed_in_bytes_per_ms;
  return Min<double>(result, kMaxFinalIncrementalMarkCompactTimeInMs);
}

bool GCIdleTimeHandler::ShouldDoContextDisposalMarkCompact(
    int contexts_disposed, double contexts_disposal_rate,
    size_t size_of_objects) {
  return contexts_disposed > 0 && contexts_disposal_rate > 0 &&
         contexts_disposal_rate < kHighContextDisposalRate &&
         size_of_objects <= kMaxHeapSizeForContextDisposalMarkCompact;
}

bool GCIdleTimeHandler::ShouldDoFinalIncrementalMarkCompact(
    double idle_time_in_ms, size_t size_of_objects,
    double final_incremental_mark_compact_speed_in_bytes_per_ms) {
  return idle_time_in_ms >=
         EstimateFinalIncrementalMarkCompactTime(
             size_of_objects,
             final_incremental_mark_compact_speed_in_bytes_per_ms);
}

bool GCIdleTimeHandler::ShouldDoOverApproximateWeakClosure(
    double idle_time_in_ms) {
  // TODO(jochen): Estimate the time it will take to build the object groups.
  return idle_time_in_ms >= kMinTimeForOverApproximatingWeakClosureInMs;
}


// The following logic is implemented by the controller:
// (1) If we don't have any idle time, do nothing, unless a context was
// disposed, incremental marking is stopped, and the heap is small. Then do
// a full GC.
// (2) If the context disposal rate is high and we cannot perform a full GC,
// we do nothing until the context disposal rate becomes lower.
// (3) If the new space is almost full and we can afford a scavenge or if the
// next scavenge will very likely take long, then a scavenge is performed.
// (4) If sweeping is in progress and we received a large enough idle time
// request, we finalize sweeping here.
// (5) If incremental marking is in progress, we perform a marking step. Note,
// that this currently may trigger a full garbage collection.
GCIdleTimeAction GCIdleTimeHandler::Compute(double idle_time_in_ms,
                                            GCIdleTimeHeapState heap_state) {
  if (static_cast<int>(idle_time_in_ms) <= 0) {
    if (heap_state.incremental_marking_stopped) {
      if (ShouldDoContextDisposalMarkCompact(heap_state.contexts_disposed,
                                             heap_state.contexts_disposal_rate,
                                             heap_state.size_of_objects)) {
        return GCIdleTimeAction::kFullGC;
      }
    }
    return GCIdleTimeAction::kDone;
  }

  if (FLAG_incremental_marking && !heap_state.incremental_marking_stopped) {
    return GCIdleTimeAction::kIncrementalStep;
  }

  return GCIdleTimeAction::kDone;
}

bool GCIdleTimeHandler::Enabled() { return FLAG_incremental_marking; }

}  // namespace internal
}  // namespace v8
