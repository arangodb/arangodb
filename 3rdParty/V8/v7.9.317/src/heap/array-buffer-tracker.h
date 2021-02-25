// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ARRAY_BUFFER_TRACKER_H_
#define V8_HEAP_ARRAY_BUFFER_TRACKER_H_

#include <unordered_map>

#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/objects/backing-store.h"
#include "src/objects/js-array-buffer.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class MarkingState;
class Page;
class Space;

class ArrayBufferTracker : public AllStatic {
 public:
  enum ProcessingMode {
    kUpdateForwardedRemoveOthers,
    kUpdateForwardedKeepOthers,
  };

  // The following methods are used to track raw C++ pointers to externally
  // allocated memory used as backing store in live array buffers.

  // Register/unregister a new JSArrayBuffer |buffer| for tracking. Guards all
  // access to the tracker by taking the page lock for the corresponding page.
  inline static void RegisterNew(Heap* heap, JSArrayBuffer buffer,
                                 std::shared_ptr<BackingStore>);
  inline static std::shared_ptr<BackingStore> Unregister(Heap* heap,
                                                         JSArrayBuffer buffer);
  inline static std::shared_ptr<BackingStore> Lookup(Heap* heap,
                                                     JSArrayBuffer buffer);

  // Identifies all backing store pointers for dead JSArrayBuffers in new space.
  // Does not take any locks and can only be called during Scavenge.
  static void PrepareToFreeDeadInNewSpace(Heap* heap);

  // Frees all backing store pointers for dead JSArrayBuffer on a given page.
  // Requires marking information to be present. Requires the page lock to be
  // taken by the caller.
  template <typename MarkingState>
  static void FreeDead(Page* page, MarkingState* marking_state);

  // Frees all remaining, live or dead, array buffers on a page. Only useful
  // during tear down.
  static void FreeAll(Page* page);

  // Processes all array buffers on a given page. |mode| specifies the action
  // to perform on the buffers. Returns whether the tracker is empty or not.
  static bool ProcessBuffers(Page* page, ProcessingMode mode);

  // Returns whether a buffer is currently tracked.
  V8_EXPORT_PRIVATE static bool IsTracked(JSArrayBuffer buffer);

  // Tears down the tracker and frees up all registered array buffers.
  static void TearDown(Heap* heap);
};

// LocalArrayBufferTracker tracks internalized array buffers.
//
// Never use directly but instead always call through |ArrayBufferTracker|.
class LocalArrayBufferTracker {
 public:
  enum CallbackResult { kKeepEntry, kUpdateEntry, kRemoveEntry };
  enum FreeMode { kFreeDead, kFreeAll };

  explicit LocalArrayBufferTracker(Page* page) : page_(page) {}
  ~LocalArrayBufferTracker();

  inline void Add(JSArrayBuffer buffer,
                  std::shared_ptr<BackingStore> backing_store);
  inline std::shared_ptr<BackingStore> Remove(JSArrayBuffer buffer);
  inline std::shared_ptr<BackingStore> Lookup(JSArrayBuffer buffer);

  // Frees up array buffers.
  //
  // Sample usage:
  // Free([](HeapObject array_buffer) {
  //    if (should_free_internal(array_buffer)) return true;
  //    return false;
  // });
  template <typename Callback>
  void Free(Callback should_free);

  // Processes buffers one by one. The CallbackResult of the callback decides
  // what action to take on the buffer.
  //
  // Callback should be of type:
  //   CallbackResult fn(JSArrayBuffer buffer, JSArrayBuffer* new_buffer);
  template <typename Callback>
  void Process(Callback callback);

  bool IsEmpty() const { return array_buffers_.empty(); }

  bool IsTracked(JSArrayBuffer buffer) const {
    return array_buffers_.find(buffer) != array_buffers_.end();
  }

 private:
  class Hasher {
   public:
    size_t operator()(JSArrayBuffer buffer) const {
      return static_cast<size_t>(buffer.ptr() >> 3);
    }
  };

  using TrackingData =
      std::unordered_map<JSArrayBuffer, std::shared_ptr<BackingStore>, Hasher>;

  // Internal version of add that does not update counters. Requires separate
  // logic for updating external memory counters.
  inline void AddInternal(JSArrayBuffer buffer,
                          std::shared_ptr<BackingStore> backing_store);

  Page* page_;
  // The set contains raw heap pointers which are removed by the GC upon
  // processing the tracker through its owning page.
  TrackingData array_buffers_;
};

}  // namespace internal
}  // namespace v8
#endif  // V8_HEAP_ARRAY_BUFFER_TRACKER_H_
