// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking-job.h"

#include "src/base/platform/time.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/isolate.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

void IncrementalMarkingJob::Start(Heap* heap) {
  DCHECK(!heap->incremental_marking()->IsStopped());
  ScheduleTask(heap);
}

void IncrementalMarkingJob::ScheduleTask(Heap* heap) {
  if (!task_pending_ && !heap->IsTearingDown()) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    task_pending_ = true;
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
    taskrunner->PostTask(base::make_unique<Task>(heap->isolate(), this));
  }
}

void IncrementalMarkingJob::Task::Step(Heap* heap) {
  const int kIncrementalMarkingDelayMs = 1;
  double deadline =
      heap->MonotonicallyIncreasingTimeInMs() + kIncrementalMarkingDelayMs;
  heap->incremental_marking()->AdvanceIncrementalMarking(
      deadline, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
      i::StepOrigin::kTask);
  heap->FinalizeIncrementalMarkingIfComplete(
      GarbageCollectionReason::kFinalizeMarkingViaTask);
}

void IncrementalMarkingJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");

  Heap* heap = isolate()->heap();
  IncrementalMarking* incremental_marking = heap->incremental_marking();
  if (incremental_marking->IsStopped()) {
    if (heap->IncrementalMarkingLimitReached() !=
        Heap::IncrementalMarkingLimit::kNoLimit) {
      heap->StartIncrementalMarking(heap->GCFlagsForIncrementalMarking(),
                                    GarbageCollectionReason::kIdleTask,
                                    kGCCallbackScheduleIdleGarbageCollection);
    }
  }

  // Clear this flag after StartIncrementalMarking call to avoid
  // scheduling a new task when startining incremental marking.
  job_->task_pending_ = false;

  if (!incremental_marking->IsStopped()) {
    Step(heap);
    if (!incremental_marking->IsStopped()) {
      job_->ScheduleTask(heap);
    }
  }
}

}  // namespace internal
}  // namespace v8
