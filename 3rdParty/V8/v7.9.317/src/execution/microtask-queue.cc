// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/microtask-queue.h"

#include <stddef.h>
#include <algorithm>

#include "src/api/api-inl.h"
#include "src/base/logging.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/visitors.h"
#include "src/roots/roots-inl.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

const size_t MicrotaskQueue::kRingBufferOffset =
    OFFSET_OF(MicrotaskQueue, ring_buffer_);
const size_t MicrotaskQueue::kCapacityOffset =
    OFFSET_OF(MicrotaskQueue, capacity_);
const size_t MicrotaskQueue::kSizeOffset = OFFSET_OF(MicrotaskQueue, size_);
const size_t MicrotaskQueue::kStartOffset = OFFSET_OF(MicrotaskQueue, start_);
const size_t MicrotaskQueue::kFinishedMicrotaskCountOffset =
    OFFSET_OF(MicrotaskQueue, finished_microtask_count_);

const intptr_t MicrotaskQueue::kMinimumCapacity = 8;

// static
void MicrotaskQueue::SetUpDefaultMicrotaskQueue(Isolate* isolate) {
  DCHECK_NULL(isolate->default_microtask_queue());

  MicrotaskQueue* microtask_queue = new MicrotaskQueue;
  microtask_queue->next_ = microtask_queue;
  microtask_queue->prev_ = microtask_queue;
  isolate->set_default_microtask_queue(microtask_queue);
}

// static
std::unique_ptr<MicrotaskQueue> MicrotaskQueue::New(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate->default_microtask_queue());

  std::unique_ptr<MicrotaskQueue> microtask_queue(new MicrotaskQueue);

  // Insert the new instance to the next of last MicrotaskQueue instance.
  MicrotaskQueue* last = isolate->default_microtask_queue()->prev_;
  microtask_queue->next_ = last->next_;
  microtask_queue->prev_ = last;
  last->next_->prev_ = microtask_queue.get();
  last->next_ = microtask_queue.get();

  return microtask_queue;
}

MicrotaskQueue::MicrotaskQueue() = default;

MicrotaskQueue::~MicrotaskQueue() {
  if (next_ != this) {
    DCHECK_NE(prev_, this);
    next_->prev_ = prev_;
    prev_->next_ = next_;
  }
  delete[] ring_buffer_;
}

// static
Address MicrotaskQueue::CallEnqueueMicrotask(Isolate* isolate,
                                             intptr_t microtask_queue_pointer,
                                             Address raw_microtask) {
  Microtask microtask = Microtask::cast(Object(raw_microtask));
  reinterpret_cast<MicrotaskQueue*>(microtask_queue_pointer)
      ->EnqueueMicrotask(microtask);
  return ReadOnlyRoots(isolate).undefined_value().ptr();
}

void MicrotaskQueue::EnqueueMicrotask(v8::Isolate* v8_isolate,
                                      v8::Local<Function> function) {
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  HandleScope scope(isolate);
  Handle<CallableTask> microtask = isolate->factory()->NewCallableTask(
      Utils::OpenHandle(*function), isolate->native_context());
  EnqueueMicrotask(*microtask);
}

void MicrotaskQueue::EnqueueMicrotask(v8::Isolate* v8_isolate,
                                      v8::MicrotaskCallback callback,
                                      void* data) {
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  HandleScope scope(isolate);
  Handle<CallbackTask> microtask = isolate->factory()->NewCallbackTask(
      isolate->factory()->NewForeign(reinterpret_cast<Address>(callback)),
      isolate->factory()->NewForeign(reinterpret_cast<Address>(data)));
  EnqueueMicrotask(*microtask);
}

void MicrotaskQueue::EnqueueMicrotask(Microtask microtask) {
  if (size_ == capacity_) {
    // Keep the capacity of |ring_buffer_| power of 2, so that the JIT
    // implementation can calculate the modulo easily.
    intptr_t new_capacity = std::max(kMinimumCapacity, capacity_ << 1);
    ResizeBuffer(new_capacity);
  }

  DCHECK_LT(size_, capacity_);
  ring_buffer_[(start_ + size_) % capacity_] = microtask.ptr();
  ++size_;
}

void MicrotaskQueue::PerformCheckpoint(v8::Isolate* v8_isolate) {
  if (!IsRunningMicrotasks() && !GetMicrotasksScopeDepth() &&
      !HasMicrotasksSuppressions()) {
    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    RunMicrotasks(isolate);
  }
}

namespace {

class SetIsRunningMicrotasks {
 public:
  explicit SetIsRunningMicrotasks(bool* flag) : flag_(flag) {
    DCHECK(!*flag_);
    *flag_ = true;
  }

  ~SetIsRunningMicrotasks() {
    DCHECK(*flag_);
    *flag_ = false;
  }

 private:
  bool* flag_;
};

}  // namespace

int MicrotaskQueue::RunMicrotasks(Isolate* isolate) {
  if (!size()) {
    OnCompleted(isolate);
    return 0;
  }

  intptr_t base_count = finished_microtask_count_;

  HandleScope handle_scope(isolate);
  MaybeHandle<Object> maybe_exception;

  MaybeHandle<Object> maybe_result;

  int processed_microtask_count;
  {
    SetIsRunningMicrotasks scope(&is_running_microtasks_);
    v8::Isolate::SuppressMicrotaskExecutionScope suppress(
        reinterpret_cast<v8::Isolate*>(isolate));
    HandleScopeImplementer::EnteredContextRewindScope rewind_scope(
        isolate->handle_scope_implementer());
    TRACE_EVENT_BEGIN0("v8.execute", "RunMicrotasks");
    {
      TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.RunMicrotasks");
      maybe_result = Execution::TryRunMicrotasks(isolate, this,
                                                 &maybe_exception);
      processed_microtask_count =
          static_cast<int>(finished_microtask_count_ - base_count);
    }
    TRACE_EVENT_END1("v8.execute", "RunMicrotasks", "microtask_count",
                     processed_microtask_count);
  }

  // If execution is terminating, clean up and propagate that to TryCatch scope.
  if (maybe_result.is_null() && maybe_exception.is_null()) {
    delete[] ring_buffer_;
    ring_buffer_ = nullptr;
    capacity_ = 0;
    size_ = 0;
    start_ = 0;
    isolate->SetTerminationOnExternalTryCatch();
    OnCompleted(isolate);
    return -1;
  }
  DCHECK_EQ(0, size());
  OnCompleted(isolate);

  return processed_microtask_count;
}

void MicrotaskQueue::IterateMicrotasks(RootVisitor* visitor) {
  if (size_) {
    // Iterate pending Microtasks as root objects to avoid the write barrier for
    // all single Microtask. If this hurts the GC performance, use a FixedArray.
    visitor->VisitRootPointers(
        Root::kStrongRoots, nullptr, FullObjectSlot(ring_buffer_ + start_),
        FullObjectSlot(ring_buffer_ + std::min(start_ + size_, capacity_)));
    visitor->VisitRootPointers(
        Root::kStrongRoots, nullptr, FullObjectSlot(ring_buffer_),
        FullObjectSlot(ring_buffer_ + std::max(start_ + size_ - capacity_,
                                               static_cast<intptr_t>(0))));
  }

  if (capacity_ <= kMinimumCapacity) {
    return;
  }

  intptr_t new_capacity = capacity_;
  while (new_capacity > 2 * size_) {
    new_capacity >>= 1;
  }
  new_capacity = std::max(new_capacity, kMinimumCapacity);
  if (new_capacity < capacity_) {
    ResizeBuffer(new_capacity);
  }
}

int MicrotaskQueue::GetMicrotasksScopeDepth() const {
  return microtasks_depth_;
}

void MicrotaskQueue::AddMicrotasksCompletedCallback(
    MicrotasksCompletedCallbackWithData callback, void* data) {
  CallbackWithData callback_with_data(callback, data);
  auto pos =
      std::find(microtasks_completed_callbacks_.begin(),
                microtasks_completed_callbacks_.end(), callback_with_data);
  if (pos != microtasks_completed_callbacks_.end()) return;
  microtasks_completed_callbacks_.push_back(callback_with_data);
}

void MicrotaskQueue::RemoveMicrotasksCompletedCallback(
    MicrotasksCompletedCallbackWithData callback, void* data) {
  CallbackWithData callback_with_data(callback, data);
  auto pos =
      std::find(microtasks_completed_callbacks_.begin(),
                microtasks_completed_callbacks_.end(), callback_with_data);
  if (pos == microtasks_completed_callbacks_.end()) return;
  microtasks_completed_callbacks_.erase(pos);
}

void MicrotaskQueue::FireMicrotasksCompletedCallback(Isolate* isolate) const {
  std::vector<CallbackWithData> callbacks(microtasks_completed_callbacks_);
  for (auto& callback : callbacks) {
    callback.first(reinterpret_cast<v8::Isolate*>(isolate), callback.second);
  }
}

Microtask MicrotaskQueue::get(intptr_t index) const {
  DCHECK_LT(index, size_);
  Object microtask(ring_buffer_[(index + start_) % capacity_]);
  return Microtask::cast(microtask);
}

void MicrotaskQueue::OnCompleted(Isolate* isolate) {
  FireMicrotasksCompletedCallback(isolate);
}

void MicrotaskQueue::ResizeBuffer(intptr_t new_capacity) {
  DCHECK_LE(size_, new_capacity);
  Address* new_ring_buffer = new Address[new_capacity];
  for (intptr_t i = 0; i < size_; ++i) {
    new_ring_buffer[i] = ring_buffer_[(start_ + i) % capacity_];
  }

  delete[] ring_buffer_;
  ring_buffer_ = new_ring_buffer;
  capacity_ = new_capacity;
  start_ = 0;
}

}  // namespace internal
}  // namespace v8
