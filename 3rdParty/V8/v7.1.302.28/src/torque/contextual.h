// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_CONTEXTUAL_H_
#define V8_TORQUE_CONTEXTUAL_H_

#include <type_traits>

#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {
namespace torque {

// {ContextualVariable} provides a clean alternative to a global variable.
// The contextual variable is mutable, and supports managing the value of
// a variable in a well-nested fashion via the {Scope} class.
// {ContextualVariable} only stores a pointer to the current value, which
// is stored in a {Scope} object. The most recent value can be retrieved
// via Get(). Because only {Scope} has actual storage, there must be at
// least one active {Scope} (i.e. in a surrounding C++ scope), whenever Get()
// is called.
// Note that contextual variables must only be used from the same thread,
// i.e. {Scope} and Get() have to be in the same thread.
template <class Derived, class VarType>
class ContextualVariable {
 public:
  using VariableType = VarType;

  // A {Scope} contains a new object of type {VarType} and gives
  // ContextualVariable::Get() access to it. Upon destruction, the contextual
  // variable is restored to the state before the {Scope} was created. Scopes
  // have to follow a stack discipline:  A {Scope} has to be destructed before
  // any older scope is destructed.
  class Scope {
   public:
    template <class... Args>
    explicit Scope(Args&&... args)
        : current_(std::forward<Args>(args)...), previous_(Top()) {
      Top() = &current_;
    }
    ~Scope() {
      // Ensure stack discipline.
      DCHECK_EQ(&current_, Top());
      Top() = previous_;
    }

   private:
    VarType current_;
    VarType* previous_;

    static_assert(std::is_base_of<ContextualVariable, Derived>::value,
                  "Curiously Recurring Template Pattern");

    DISALLOW_NEW_AND_DELETE();
    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

  // Access the most recent active {Scope}. There has to be an active {Scope}
  // for this contextual variable.
  static VarType& Get() {
    DCHECK_NOT_NULL(Top());
    return *Top();
  }

 private:
  V8_EXPORT_PRIVATE static VarType*& Top();
};

// Usage: DECLARE_CONTEXTUAL_VARIABLE(VarName, VarType)
#define DECLARE_CONTEXTUAL_VARIABLE(VarName, ...) \
  struct VarName                                  \
      : v8::internal::torque::ContextualVariable<VarName, __VA_ARGS__> {};

#define DEFINE_CONTEXTUAL_VARIABLE(VarName)                   \
  template <>                                                 \
  V8_EXPORT_PRIVATE VarName::VariableType*&                   \
  ContextualVariable<VarName, VarName::VariableType>::Top() { \
    static thread_local VarName::VariableType* top = nullptr; \
    return top;                                               \
  }

// By inheriting from {ContextualClass} a class can become a contextual variable
// of itself, which is very similar to a singleton.
template <class T>
using ContextualClass = ContextualVariable<T, T>;

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_CONTEXTUAL_H_
