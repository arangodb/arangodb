// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EMBEDDER_TRACING_H_
#define V8_HEAP_EMBEDDER_TRACING_H_

#include "include/v8.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

class Heap;
class JSObject;

class V8_EXPORT_PRIVATE LocalEmbedderHeapTracer final {
 public:
  using WrapperInfo = std::pair<void*, void*>;
  using WrapperCache = std::vector<WrapperInfo>;

  class V8_EXPORT_PRIVATE ProcessingScope {
   public:
    explicit ProcessingScope(LocalEmbedderHeapTracer* tracer);
    ~ProcessingScope();

    void TracePossibleWrapper(JSObject js_object);

    void AddWrapperInfoForTesting(WrapperInfo info);

   private:
    static constexpr size_t kWrapperCacheSize = 1000;

    void FlushWrapperCacheIfFull();

    LocalEmbedderHeapTracer* const tracer_;
    WrapperCache wrapper_cache_;
  };

  explicit LocalEmbedderHeapTracer(Isolate* isolate) : isolate_(isolate) {}

  ~LocalEmbedderHeapTracer() {
    if (remote_tracer_) remote_tracer_->isolate_ = nullptr;
  }

  bool InUse() const { return remote_tracer_ != nullptr; }
  EmbedderHeapTracer* remote_tracer() const { return remote_tracer_; }

  void SetRemoteTracer(EmbedderHeapTracer* tracer);
  void TracePrologue(EmbedderHeapTracer::TraceFlags flags);
  void TraceEpilogue();
  void EnterFinalPause();
  bool Trace(double deadline);
  bool IsRemoteTracingDone();

  bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle) {
    return !InUse() || remote_tracer_->IsRootForNonTracingGC(handle);
  }

  bool IsRootForNonTracingGC(const v8::TracedReference<v8::Value>& handle) {
    return !InUse() || remote_tracer_->IsRootForNonTracingGC(handle);
  }

  void ResetHandleInNonTracingGC(const v8::TracedReference<v8::Value>& handle) {
    // Resetting is only called when IsRootForNonTracingGC returns false which
    // can only happen the EmbedderHeapTracer is set on API level.
    DCHECK(InUse());
    remote_tracer_->ResetHandleInNonTracingGC(handle);
  }

  void NotifyV8MarkingWorklistWasEmpty() {
    num_v8_marking_worklist_was_empty_++;
  }

  bool ShouldFinalizeIncrementalMarking() {
    static const size_t kMaxIncrementalFixpointRounds = 3;
    return !FLAG_incremental_marking_wrappers || !InUse() ||
           (IsRemoteTracingDone() && embedder_worklist_empty_) ||
           num_v8_marking_worklist_was_empty_ > kMaxIncrementalFixpointRounds;
  }

  void SetEmbedderStackStateForNextFinalization(
      EmbedderHeapTracer::EmbedderStackState stack_state);

  void SetEmbedderWorklistEmpty(bool is_empty) {
    embedder_worklist_empty_ = is_empty;
  }

  void IncreaseAllocatedSize(size_t bytes) {
    remote_stats_.used_size += bytes;
    remote_stats_.allocated_size += bytes;
    if (remote_stats_.allocated_size >
        remote_stats_.allocated_size_limit_for_check) {
      StartIncrementalMarkingIfNeeded();
      remote_stats_.allocated_size_limit_for_check =
          remote_stats_.allocated_size + kEmbedderAllocatedThreshold;
    }
  }

  void DecreaseAllocatedSize(size_t bytes) {
    DCHECK_GE(remote_stats_.used_size, bytes);
    remote_stats_.used_size -= bytes;
  }

  void StartIncrementalMarkingIfNeeded();

  size_t used_size() const { return remote_stats_.used_size; }
  size_t allocated_size() const { return remote_stats_.allocated_size; }

 private:
  static constexpr size_t kEmbedderAllocatedThreshold = 128 * KB;

  Isolate* const isolate_;
  EmbedderHeapTracer* remote_tracer_ = nullptr;

  size_t num_v8_marking_worklist_was_empty_ = 0;
  EmbedderHeapTracer::EmbedderStackState embedder_stack_state_ =
      EmbedderHeapTracer::kUnknown;
  // Indicates whether the embedder worklist was observed empty on the main
  // thread. This is opportunistic as concurrent marking tasks may hold local
  // segments of potential embedder fields to move to the main thread.
  bool embedder_worklist_empty_ = false;

  struct RemoteStatistics {
    // Used size of objects in bytes reported by the embedder. Updated via
    // TraceSummary at the end of tracing and incrementally when the GC is not
    // in progress.
    size_t used_size = 0;
    // Totally bytes allocated by the embedder. Monotonically
    // increasing value. Used to approximate allocation rate.
    size_t allocated_size = 0;
    // Limit for |allocated_size| in bytes to avoid checking for starting a GC
    // on each increment.
    size_t allocated_size_limit_for_check = 0;
  } remote_stats_;

  friend class EmbedderStackStateScope;
};

class V8_EXPORT_PRIVATE EmbedderStackStateScope final {
 public:
  EmbedderStackStateScope(LocalEmbedderHeapTracer* local_tracer,
                          EmbedderHeapTracer::EmbedderStackState stack_state)
      : local_tracer_(local_tracer),
        old_stack_state_(local_tracer_->embedder_stack_state_) {
    local_tracer_->embedder_stack_state_ = stack_state;
  }

  ~EmbedderStackStateScope() {
    local_tracer_->embedder_stack_state_ = old_stack_state_;
  }

 private:
  LocalEmbedderHeapTracer* const local_tracer_;
  const EmbedderHeapTracer::EmbedderStackState old_stack_state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_H_
