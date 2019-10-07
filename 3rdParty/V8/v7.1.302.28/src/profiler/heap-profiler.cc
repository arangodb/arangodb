// Copyright 2009-2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/heap-profiler.h"

#include "src/api-inl.h"
#include "src/debug/debug.h"
#include "src/heap/heap-inl.h"
#include "src/profiler/allocation-tracker.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "src/profiler/sampling-heap-profiler.h"

namespace v8 {
namespace internal {

HeapProfiler::HeapProfiler(Heap* heap)
    : ids_(new HeapObjectsMap(heap)),
      names_(new StringsStorage()),
      is_tracking_object_moves_(false) {}

HeapProfiler::~HeapProfiler() = default;

void HeapProfiler::DeleteAllSnapshots() {
  snapshots_.clear();
  MaybeClearStringsStorage();
}

void HeapProfiler::MaybeClearStringsStorage() {
  if (snapshots_.empty() && !sampling_heap_profiler_ && !allocation_tracker_) {
    names_.reset(new StringsStorage());
  }
}

void HeapProfiler::RemoveSnapshot(HeapSnapshot* snapshot) {
  snapshots_.erase(
      std::find_if(snapshots_.begin(), snapshots_.end(),
                   [&](const std::unique_ptr<HeapSnapshot>& entry) {
                     return entry.get() == snapshot;
                   }));
}


void HeapProfiler::DefineWrapperClass(
    uint16_t class_id, v8::HeapProfiler::WrapperInfoCallback callback) {
  DCHECK_NE(class_id, v8::HeapProfiler::kPersistentHandleNoClassId);
  if (wrapper_callbacks_.size() <= class_id) {
    wrapper_callbacks_.insert(wrapper_callbacks_.end(),
                              class_id - wrapper_callbacks_.size() + 1,
                              nullptr);
  }
  wrapper_callbacks_[class_id] = callback;
}


v8::RetainedObjectInfo* HeapProfiler::ExecuteWrapperClassCallback(
    uint16_t class_id, Object** wrapper) {
  if (wrapper_callbacks_.size() <= class_id) return nullptr;
  return wrapper_callbacks_[class_id](
      class_id, Utils::ToLocal(Handle<Object>(wrapper)));
}

void HeapProfiler::SetGetRetainerInfosCallback(
    v8::HeapProfiler::GetRetainerInfosCallback callback) {
  get_retainer_infos_callback_ = callback;
}

v8::HeapProfiler::RetainerInfos HeapProfiler::GetRetainerInfos(
    Isolate* isolate) {
  v8::HeapProfiler::RetainerInfos infos;
  if (get_retainer_infos_callback_ != nullptr)
    infos =
        get_retainer_infos_callback_(reinterpret_cast<v8::Isolate*>(isolate));
  return infos;
}

void HeapProfiler::AddBuildEmbedderGraphCallback(
    v8::HeapProfiler::BuildEmbedderGraphCallback callback, void* data) {
  build_embedder_graph_callbacks_.push_back({callback, data});
}

void HeapProfiler::RemoveBuildEmbedderGraphCallback(
    v8::HeapProfiler::BuildEmbedderGraphCallback callback, void* data) {
  auto it = std::find(build_embedder_graph_callbacks_.begin(),
                      build_embedder_graph_callbacks_.end(),
                      std::make_pair(callback, data));
  if (it != build_embedder_graph_callbacks_.end())
    build_embedder_graph_callbacks_.erase(it);
}

void HeapProfiler::BuildEmbedderGraph(Isolate* isolate,
                                      v8::EmbedderGraph* graph) {
  for (const auto& cb : build_embedder_graph_callbacks_) {
    cb.first(reinterpret_cast<v8::Isolate*>(isolate), graph, cb.second);
  }
}

HeapSnapshot* HeapProfiler::TakeSnapshot(
    v8::ActivityControl* control,
    v8::HeapProfiler::ObjectNameResolver* resolver) {
  HeapSnapshot* result = new HeapSnapshot(this);
  {
    HeapSnapshotGenerator generator(result, control, resolver, heap());
    if (!generator.GenerateSnapshot()) {
      delete result;
      result = nullptr;
    } else {
      snapshots_.emplace_back(result);
    }
  }
  ids_->RemoveDeadEntries();
  is_tracking_object_moves_ = true;

  heap()->isolate()->debug()->feature_tracker()->Track(
      DebugFeatureTracker::kHeapSnapshot);

  return result;
}

bool HeapProfiler::StartSamplingHeapProfiler(
    uint64_t sample_interval, int stack_depth,
    v8::HeapProfiler::SamplingFlags flags) {
  if (sampling_heap_profiler_.get()) {
    return false;
  }
  sampling_heap_profiler_.reset(new SamplingHeapProfiler(
      heap(), names_.get(), sample_interval, stack_depth, flags));
  return true;
}


void HeapProfiler::StopSamplingHeapProfiler() {
  sampling_heap_profiler_.reset();
  MaybeClearStringsStorage();
}


v8::AllocationProfile* HeapProfiler::GetAllocationProfile() {
  if (sampling_heap_profiler_.get()) {
    return sampling_heap_profiler_->GetAllocationProfile();
  } else {
    return nullptr;
  }
}


void HeapProfiler::StartHeapObjectsTracking(bool track_allocations) {
  ids_->UpdateHeapObjectsMap();
  is_tracking_object_moves_ = true;
  DCHECK(!allocation_tracker_);
  if (track_allocations) {
    allocation_tracker_.reset(new AllocationTracker(ids_.get(), names_.get()));
    heap()->AddHeapObjectAllocationTracker(this);
    heap()->isolate()->debug()->feature_tracker()->Track(
        DebugFeatureTracker::kAllocationTracking);
  }
}

SnapshotObjectId HeapProfiler::PushHeapObjectsStats(OutputStream* stream,
                                                    int64_t* timestamp_us) {
  return ids_->PushHeapObjectsStats(stream, timestamp_us);
}

void HeapProfiler::StopHeapObjectsTracking() {
  ids_->StopHeapObjectsTracking();
  if (allocation_tracker_) {
    allocation_tracker_.reset();
    MaybeClearStringsStorage();
    heap()->RemoveHeapObjectAllocationTracker(this);
  }
}

int HeapProfiler::GetSnapshotsCount() {
  return static_cast<int>(snapshots_.size());
}

HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  return snapshots_.at(index).get();
}

SnapshotObjectId HeapProfiler::GetSnapshotObjectId(Handle<Object> obj) {
  if (!obj->IsHeapObject())
    return v8::HeapProfiler::kUnknownObjectId;
  return ids_->FindEntry(HeapObject::cast(*obj)->address());
}

void HeapProfiler::ObjectMoveEvent(Address from, Address to, int size) {
  base::LockGuard<base::Mutex> guard(&profiler_mutex_);
  bool known_object = ids_->MoveObject(from, to, size);
  if (!known_object && allocation_tracker_) {
    allocation_tracker_->address_to_trace()->MoveObject(from, to, size);
  }
}

void HeapProfiler::AllocationEvent(Address addr, int size) {
  DisallowHeapAllocation no_allocation;
  if (allocation_tracker_) {
    allocation_tracker_->AllocationEvent(addr, size);
  }
}


void HeapProfiler::UpdateObjectSizeEvent(Address addr, int size) {
  ids_->UpdateObjectSize(addr, size);
}

Handle<HeapObject> HeapProfiler::FindHeapObjectById(SnapshotObjectId id) {
  HeapObject* object = nullptr;
  HeapIterator iterator(heap(), HeapIterator::kFilterUnreachable);
  // Make sure that object with the given id is still reachable.
  for (HeapObject* obj = iterator.next(); obj != nullptr;
       obj = iterator.next()) {
    if (ids_->FindEntry(obj->address()) == id) {
      DCHECK_NULL(object);
      object = obj;
      // Can't break -- kFilterUnreachable requires full heap traversal.
    }
  }
  return object != nullptr ? Handle<HeapObject>(object, isolate())
                           : Handle<HeapObject>();
}


void HeapProfiler::ClearHeapObjectMap() {
  ids_.reset(new HeapObjectsMap(heap()));
  if (!allocation_tracker_) is_tracking_object_moves_ = false;
}


Heap* HeapProfiler::heap() const { return ids_->heap(); }

Isolate* HeapProfiler::isolate() const { return heap()->isolate(); }

void HeapProfiler::QueryObjects(Handle<Context> context,
                                debug::QueryObjectPredicate* predicate,
                                PersistentValueVector<v8::Object>* objects) {
  // We should return accurate information about live objects, so we need to
  // collect all garbage first.
  heap()->CollectAllAvailableGarbage(GarbageCollectionReason::kHeapProfiler);
  HeapIterator heap_iterator(heap());
  HeapObject* heap_obj;
  while ((heap_obj = heap_iterator.next()) != nullptr) {
    if (!heap_obj->IsJSObject() || heap_obj->IsExternal(isolate())) continue;
    v8::Local<v8::Object> v8_obj(
        Utils::ToLocal(handle(JSObject::cast(heap_obj), isolate())));
    if (!predicate->Filter(v8_obj)) continue;
    objects->Append(v8_obj);
  }
}

}  // namespace internal
}  // namespace v8
