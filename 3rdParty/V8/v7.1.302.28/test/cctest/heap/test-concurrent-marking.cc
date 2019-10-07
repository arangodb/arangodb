// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/heap/concurrent-marking.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"
#include "src/heap/worklist.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

void PublishSegment(ConcurrentMarking::MarkingWorklist* worklist,
                    HeapObject* object) {
  for (size_t i = 0; i <= ConcurrentMarking::MarkingWorklist::kSegmentCapacity;
       i++) {
    worklist->Push(0, object);
  }
  CHECK(worklist->Pop(0, &object));
}

TEST(ConcurrentMarking) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }

  ConcurrentMarking::MarkingWorklist shared, bailout, on_hold;
  ConcurrentMarking::EmbedderTracingWorklist embedder_objects;
  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking = new ConcurrentMarking(
      heap, &shared, &bailout, &on_hold, &weak_objects, &embedder_objects);
  PublishSegment(&shared, ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->ScheduleTasks();
  concurrent_marking->Stop(
      ConcurrentMarking::StopRequest::COMPLETE_TASKS_FOR_TESTING);
  delete concurrent_marking;
}

TEST(ConcurrentMarkingReschedule) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }

  ConcurrentMarking::MarkingWorklist shared, bailout, on_hold;
  ConcurrentMarking::EmbedderTracingWorklist embedder_objects;
  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking = new ConcurrentMarking(
      heap, &shared, &bailout, &on_hold, &weak_objects, &embedder_objects);
  PublishSegment(&shared, ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->ScheduleTasks();
  concurrent_marking->Stop(
      ConcurrentMarking::StopRequest::COMPLETE_ONGOING_TASKS);
  PublishSegment(&shared, ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->RescheduleTasksIfNeeded();
  concurrent_marking->Stop(
      ConcurrentMarking::StopRequest::COMPLETE_TASKS_FOR_TESTING);
  delete concurrent_marking;
}

TEST(ConcurrentMarkingPreemptAndReschedule) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }

  ConcurrentMarking::MarkingWorklist shared, bailout, on_hold;
  ConcurrentMarking::EmbedderTracingWorklist embedder_objects;
  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking = new ConcurrentMarking(
      heap, &shared, &bailout, &on_hold, &weak_objects, &embedder_objects);
  for (int i = 0; i < 5000; i++)
    PublishSegment(&shared, ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->ScheduleTasks();
  concurrent_marking->Stop(ConcurrentMarking::StopRequest::PREEMPT_TASKS);
  for (int i = 0; i < 5000; i++)
    PublishSegment(&shared, ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->RescheduleTasksIfNeeded();
  concurrent_marking->Stop(
      ConcurrentMarking::StopRequest::COMPLETE_TASKS_FOR_TESTING);
  delete concurrent_marking;
}

TEST(ConcurrentMarkingMarkedBytes) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  HandleScope sc(isolate);
  Handle<FixedArray> root = isolate->factory()->NewFixedArray(1000000);
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  heap::SimulateIncrementalMarking(heap, false);
  heap->concurrent_marking()->Stop(
      ConcurrentMarking::StopRequest::COMPLETE_TASKS_FOR_TESTING);
  CHECK_GE(heap->concurrent_marking()->TotalMarkedBytes(), root->Size());
}

UNINITIALIZED_TEST(ConcurrentMarkingStoppedOnTeardown) {
  if (!i::FLAG_concurrent_marking) return;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
    Factory* factory = i_isolate->factory();

    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    for (int i = 0; i < 10000; i++) {
      factory->NewJSWeakMap();
    }

    Heap* heap = i_isolate->heap();
    heap::SimulateIncrementalMarking(heap, false);
  }

  isolate->Dispose();
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
