// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_IC_H_
#define V8_IC_IC_H_

#include <vector>

#include "src/feedback-vector.h"
#include "src/heap/factory.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/messages.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"

namespace v8 {
namespace internal {

//
// IC is the base class for LoadIC, StoreIC, KeyedLoadIC, and KeyedStoreIC.
//
class IC {
 public:
  // Alias the inline cache state type to make the IC code more readable.
  typedef InlineCacheState State;

  static constexpr int kMaxKeyedPolymorphism = 4;

  // A polymorphic IC can handle at most 4 distinct maps before transitioning
  // to megamorphic state.
  static constexpr int kMaxPolymorphicMapCount = 4;

  // Construct the IC structure with the given number of extra
  // JavaScript frames on the stack.
  IC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot);
  virtual ~IC() = default;

  State state() const { return state_; }
  inline Address address() const;

  // Compute the current IC state based on the target stub, receiver and name.
  void UpdateState(Handle<Object> receiver, Handle<Object> name);

  bool RecomputeHandlerForName(Handle<Object> name);
  void MarkRecomputeHandler(Handle<Object> name) {
    DCHECK(RecomputeHandlerForName(name));
    old_state_ = state_;
    state_ = RECOMPUTE_HANDLER;
  }

  bool IsAnyLoad() const {
    return IsLoadIC() || IsLoadGlobalIC() || IsKeyedLoadIC();
  }
  bool IsAnyStore() const {
    return IsStoreIC() || IsStoreOwnIC() || IsStoreGlobalIC() ||
           IsKeyedStoreIC() || IsStoreInArrayLiteralICKind(kind());
  }

  static inline bool IsHandler(MaybeObject* object);

  // Nofity the IC system that a feedback has changed.
  static void OnFeedbackChanged(Isolate* isolate, FeedbackVector* vector,
                                FeedbackSlot slot, JSFunction* host_function,
                                const char* reason);

  static void OnFeedbackChanged(Isolate* isolate, FeedbackNexus* nexus,
                                JSFunction* host_function, const char* reason);

 protected:
  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }

  void set_slow_stub_reason(const char* reason) { slow_stub_reason_ = reason; }

  Isolate* isolate() const { return isolate_; }

  // Get the caller function object.
  JSFunction* GetHostFunction() const;

  inline bool AddressIsDeoptimizedCode() const;
  inline static bool AddressIsDeoptimizedCode(Isolate* isolate,
                                              Address address);

  bool is_vector_set() { return vector_set_; }
  bool vector_needs_update() {
    return (!vector_set_ &&
            (state() != MEGAMORPHIC ||
             Smi::ToInt(nexus()->GetFeedbackExtra()->cast<Smi>()) != ELEMENT));
  }

  // Configure for most states.
  bool ConfigureVectorState(IC::State new_state, Handle<Object> key);
  // Configure the vector for PREMONOMORPHIC.
  void ConfigureVectorState(Handle<Map> map);
  // Configure the vector for MONOMORPHIC.
  void ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                            Handle<Object> handler);
  void ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                            const MaybeObjectHandle& handler);
  // Configure the vector for POLYMORPHIC.
  void ConfigureVectorState(Handle<Name> name, MapHandles const& maps,
                            MaybeObjectHandles* handlers);

  char TransitionMarkFromState(IC::State state);
  void TraceIC(const char* type, Handle<Object> name);
  void TraceIC(const char* type, Handle<Object> name, State old_state,
               State new_state);

  MaybeHandle<Object> TypeError(MessageTemplate::Template,
                                Handle<Object> object, Handle<Object> key);
  MaybeHandle<Object> ReferenceError(Handle<Name> name);

  void TraceHandlerCacheHitStats(LookupIterator* lookup);

  void UpdateMonomorphicIC(const MaybeObjectHandle& handler, Handle<Name> name);
  bool UpdatePolymorphicIC(Handle<Name> name, const MaybeObjectHandle& handler);
  void UpdateMegamorphicCache(Handle<Map> map, Handle<Name> name,
                              const MaybeObjectHandle& handler);

  StubCache* stub_cache();

  void CopyICToMegamorphicCache(Handle<Name> name);
  bool IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map);
  void PatchCache(Handle<Name> name, Handle<Object> handler);
  void PatchCache(Handle<Name> name, const MaybeObjectHandle& handler);
  FeedbackSlotKind kind() const { return kind_; }
  bool IsGlobalIC() const { return IsLoadGlobalIC() || IsStoreGlobalIC(); }
  bool IsLoadIC() const { return IsLoadICKind(kind_); }
  bool IsLoadGlobalIC() const { return IsLoadGlobalICKind(kind_); }
  bool IsKeyedLoadIC() const { return IsKeyedLoadICKind(kind_); }
  bool IsStoreGlobalIC() const { return IsStoreGlobalICKind(kind_); }
  bool IsStoreIC() const { return IsStoreICKind(kind_); }
  bool IsStoreOwnIC() const { return IsStoreOwnICKind(kind_); }
  bool IsKeyedStoreIC() const { return IsKeyedStoreICKind(kind_); }
  bool is_keyed() const {
    return IsKeyedLoadIC() || IsKeyedStoreIC() ||
           IsStoreInArrayLiteralICKind(kind_);
  }
  bool ShouldRecomputeHandler(Handle<String> name);

  Handle<Map> receiver_map() { return receiver_map_; }
  inline void update_receiver_map(Handle<Object> receiver);

  void TargetMaps(MapHandles* list) {
    FindTargetMaps();
    for (Handle<Map> map : target_maps_) {
      list->push_back(map);
    }
  }

  Map* FirstTargetMap() {
    FindTargetMaps();
    return !target_maps_.empty() ? *target_maps_[0] : nullptr;
  }

  State saved_state() const {
    return state() == RECOMPUTE_HANDLER ? old_state_ : state();
  }

  const FeedbackNexus* nexus() const { return &nexus_; }
  FeedbackNexus* nexus() { return &nexus_; }

 private:
  inline Address constant_pool() const;
  inline Address raw_constant_pool() const;

  void FindTargetMaps() {
    if (target_maps_set_) return;
    target_maps_set_ = true;
    nexus()->ExtractMaps(&target_maps_);
  }

  // Frame pointer for the frame that uses (calls) the IC.
  Address fp_;

  // All access to the program counter and constant pool of an IC structure is
  // indirect to make the code GC safe. This feature is crucial since
  // GetProperty and SetProperty are called and they in turn might
  // invoke the garbage collector.
  Address* pc_address_;

  // The constant pool of the code which originally called the IC (which might
  // be for the breakpointed copy of the original code).
  Address* constant_pool_address_;

  Isolate* isolate_;

  bool vector_set_;
  State old_state_;  // For saving if we marked as prototype failure.
  State state_;
  FeedbackSlotKind kind_;
  Handle<Map> receiver_map_;
  MaybeObjectHandle maybe_handler_;

  MapHandles target_maps_;
  bool target_maps_set_;

  const char* slow_stub_reason_;

  FeedbackNexus nexus_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};


class LoadIC : public IC {
 public:
  LoadIC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot)
      : IC(isolate, vector, slot) {
    DCHECK(IsAnyLoad());
  }

  static bool ShouldThrowReferenceError(FeedbackSlotKind kind) {
    return kind == FeedbackSlotKind::kLoadGlobalNotInsideTypeof;
  }

  bool ShouldThrowReferenceError() const {
    return ShouldThrowReferenceError(kind());
  }

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                                 Handle<Name> name);

 protected:
  virtual Handle<Code> slow_stub() const {
    return BUILTIN_CODE(isolate(), LoadIC_Slow);
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup);

 private:
  Handle<Object> ComputeHandler(LookupIterator* lookup);

  friend class IC;
  friend class NamedLoadHandlerCompiler;
};

class LoadGlobalIC : public LoadIC {
 public:
  LoadGlobalIC(Isolate* isolate, Handle<FeedbackVector> vector,
               FeedbackSlot slot)
      : LoadIC(isolate, vector, slot) {}

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Load(Handle<Name> name);

 protected:
  Handle<Code> slow_stub() const override {
    return BUILTIN_CODE(isolate(), LoadGlobalIC_Slow);
  }
};

class KeyedLoadIC : public LoadIC {
 public:
  KeyedLoadIC(Isolate* isolate, Handle<FeedbackVector> vector,
              FeedbackSlot slot)
      : LoadIC(isolate, vector, slot) {}

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                                 Handle<Object> key);

 protected:
  // receiver is HeapObject because it could be a String or a JSObject
  void UpdateLoadElement(Handle<HeapObject> receiver,
                         KeyedAccessLoadMode load_mode);

 private:
  friend class IC;

  Handle<Object> LoadElementHandler(Handle<Map> receiver_map,
                                    KeyedAccessLoadMode load_mode);

  void LoadElementPolymorphicHandlers(MapHandles* receiver_maps,
                                      MaybeObjectHandles* handlers,
                                      KeyedAccessLoadMode load_mode);

  // Returns true if the receiver_map has a kElement or kIndexedString
  // handler in the nexus currently but didn't yet allow out of bounds
  // accesses.
  bool CanChangeToAllowOutOfBounds(Handle<Map> receiver_map);
};


class StoreIC : public IC {
 public:
  StoreIC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot)
      : IC(isolate, vector, slot) {
    DCHECK(IsAnyStore());
  }

  LanguageMode language_mode() const { return nexus()->GetLanguageMode(); }

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Store(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      StoreOrigin store_origin = StoreOrigin::kNamed);

  bool LookupForWrite(LookupIterator* it, Handle<Object> value,
                      StoreOrigin store_origin);

 protected:
  // Stub accessors.
  virtual Handle<Code> slow_stub() const {
    // All StoreICs share the same slow stub.
    return BUILTIN_CODE(isolate(), KeyedStoreIC_Slow);
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                    StoreOrigin store_origin);

 private:
  MaybeObjectHandle ComputeHandler(LookupIterator* lookup);

  friend class IC;
};

class StoreGlobalIC : public StoreIC {
 public:
  StoreGlobalIC(Isolate* isolate, Handle<FeedbackVector> vector,
                FeedbackSlot slot)
      : StoreIC(isolate, vector, slot) {}

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Store(Handle<Name> name,
                                                  Handle<Object> value);

 protected:
  Handle<Code> slow_stub() const override {
    return BUILTIN_CODE(isolate(), StoreGlobalIC_Slow);
  }
};

enum KeyedStoreCheckMap { kDontCheckMap, kCheckMap };


enum KeyedStoreIncrementLength { kDontIncrementLength, kIncrementLength };


class KeyedStoreIC : public StoreIC {
 public:
  KeyedAccessStoreMode GetKeyedAccessStoreMode() {
    return nexus()->GetKeyedAccessStoreMode();
  }

  KeyedStoreIC(Isolate* isolate, Handle<FeedbackVector> vector,
               FeedbackSlot slot)
      : StoreIC(isolate, vector, slot) {}

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Store(Handle<Object> object,
                                                  Handle<Object> name,
                                                  Handle<Object> value);

 protected:
  void UpdateStoreElement(Handle<Map> receiver_map,
                          KeyedAccessStoreMode store_mode,
                          bool receiver_was_cow);

  Handle<Code> slow_stub() const override {
    return BUILTIN_CODE(isolate(), KeyedStoreIC_Slow);
  }

 private:
  Handle<Map> ComputeTransitionedMap(Handle<Map> map,
                                     KeyedAccessStoreMode store_mode);

  Handle<Object> StoreElementHandler(Handle<Map> receiver_map,
                                     KeyedAccessStoreMode store_mode);

  void StoreElementPolymorphicHandlers(MapHandles* receiver_maps,
                                       MaybeObjectHandles* handlers,
                                       KeyedAccessStoreMode store_mode);

  friend class IC;
};

class StoreInArrayLiteralIC : public KeyedStoreIC {
 public:
  StoreInArrayLiteralIC(Isolate* isolate, Handle<FeedbackVector> vector,
                        FeedbackSlot slot)
      : KeyedStoreIC(isolate, vector, slot) {
    DCHECK(IsStoreInArrayLiteralICKind(kind()));
  }

  void Store(Handle<JSArray> array, Handle<Object> index, Handle<Object> value);

 private:
  Handle<Code> slow_stub() const override {
    return BUILTIN_CODE(isolate(), StoreInArrayLiteralIC_Slow);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_IC_H_
