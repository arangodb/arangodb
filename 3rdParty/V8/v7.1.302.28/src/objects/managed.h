// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MANAGED_H_
#define V8_OBJECTS_MANAGED_H_

#include <memory>
#include "src/global-handles.h"
#include "src/handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

// Implements a doubly-linked lists of destructors for the isolate.
struct ManagedPtrDestructor {
  // Estimated size of external memory associated with the managed object.
  // This is used to adjust the garbage collector's heuristics upon
  // allocation and deallocation of a managed object.
  size_t estimated_size_ = 0;
  ManagedPtrDestructor* prev_ = nullptr;
  ManagedPtrDestructor* next_ = nullptr;
  void* shared_ptr_ptr_ = nullptr;
  void (*destructor_)(void* shared_ptr) = nullptr;
  Object** global_handle_location_ = nullptr;

  ManagedPtrDestructor(size_t estimated_size, void* shared_ptr_ptr,
                       void (*destructor)(void*))
      : estimated_size_(estimated_size),
        shared_ptr_ptr_(shared_ptr_ptr),
        destructor_(destructor) {}
};

// The GC finalizer of a managed object, which does not depend on
// the template parameter.
void ManagedObjectFinalizer(const v8::WeakCallbackInfo<void>& data);

// {Managed<T>} is essentially a {std::shared_ptr<T>} allocated on the heap
// that can be used to manage the lifetime of C++ objects that are shared
// across multiple isolates.
// When a {Managed<T>} object is garbage collected (or an isolate which
// contains {Managed<T>} is torn down), the {Managed<T>} deletes its underlying
// {std::shared_ptr<T>}, thereby decrementing its internal reference count,
// which will delete the C++ object when the reference count drops to 0.
template <class CppType>
class Managed : public Foreign {
 public:
  // Get a raw pointer to the C++ object.
  V8_INLINE CppType* raw() { return GetSharedPtrPtr()->get(); }

  // Get a copy of the shared pointer to the C++ object.
  V8_INLINE std::shared_ptr<CppType> get() { return *GetSharedPtrPtr(); }

  static Managed<CppType>* cast(Object* obj) {
    SLOW_DCHECK(obj->IsForeign());
    return reinterpret_cast<Managed<CppType>*>(obj);
  }

  // Allocate a new {CppType} and wrap it in a {Managed<CppType>}.
  template <typename... Args>
  static Handle<Managed<CppType>> Allocate(Isolate* isolate,
                                           size_t estimated_size,
                                           Args&&... args) {
    CppType* ptr = new CppType(std::forward<Args>(args)...);
    return FromSharedPtr(isolate, estimated_size,
                         std::shared_ptr<CppType>(ptr));
  }

  // Create a {Managed<CppType>} from an existing raw {CppType*}. The returned
  // object will now own the memory pointed to by {CppType}.
  static Handle<Managed<CppType>> FromRawPtr(Isolate* isolate,
                                             size_t estimated_size,
                                             CppType* ptr) {
    return FromSharedPtr(isolate, estimated_size,
                         std::shared_ptr<CppType>(ptr));
  }

  // Create a {Managed<CppType>} from an existing {std::unique_ptr<CppType>}.
  // The returned object will now own the memory pointed to by {CppType}, and
  // the unique pointer will be released.
  static Handle<Managed<CppType>> FromUniquePtr(
      Isolate* isolate, size_t estimated_size,
      std::unique_ptr<CppType> unique_ptr) {
    return FromSharedPtr(isolate, estimated_size, std::move(unique_ptr));
  }

  // Create a {Managed<CppType>} from an existing {std::shared_ptr<CppType>}.
  static Handle<Managed<CppType>> FromSharedPtr(
      Isolate* isolate, size_t estimated_size,
      std::shared_ptr<CppType> shared_ptr) {
    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(estimated_size);
    auto destructor = new ManagedPtrDestructor(
        estimated_size, new std::shared_ptr<CppType>(shared_ptr), Destructor);
    Handle<Managed<CppType>> handle = Handle<Managed<CppType>>::cast(
        isolate->factory()->NewForeign(reinterpret_cast<Address>(destructor)));
    Handle<Object> global_handle = isolate->global_handles()->Create(*handle);
    destructor->global_handle_location_ = global_handle.location();
    GlobalHandles::MakeWeak(destructor->global_handle_location_, destructor,
                            &ManagedObjectFinalizer,
                            v8::WeakCallbackType::kParameter);
    isolate->RegisterManagedPtrDestructor(destructor);
    return handle;
  }

 private:
  // Internally this {Foreign} object stores a pointer to a new
  // std::shared_ptr<CppType>.
  std::shared_ptr<CppType>* GetSharedPtrPtr() {
    auto destructor =
        reinterpret_cast<ManagedPtrDestructor*>(foreign_address());
    return reinterpret_cast<std::shared_ptr<CppType>*>(
        destructor->shared_ptr_ptr_);
  }

  // Called by either isolate shutdown or the {ManagedObjectFinalizer} in
  // order to actually delete the shared pointer (i.e. decrement its refcount).
  static void Destructor(void* ptr) {
    auto shared_ptr_ptr = reinterpret_cast<std::shared_ptr<CppType>*>(ptr);
    delete shared_ptr_ptr;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MANAGED_H_
