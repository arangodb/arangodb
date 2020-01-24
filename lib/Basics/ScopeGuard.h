////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SCOPE_GUARD_H
#define ARANGODB_BASICS_SCOPE_GUARD_H 1

#include <type_traits>
#include <utility>

#define SCOPE_GUARD_TOKEN_PASTE_WRAPPED(x, y) x##y
#define SCOPE_GUARD_TOKEN_PASTE(x, y) SCOPE_GUARD_TOKEN_PASTE_WRAPPED(x, y)

// helper macros for creating a capture-all ScopeGuard
#define TRI_DEFER_BLOCK_INTERNAL(func, objname) \
  auto objname = arangodb::scopeGuard([&] { func; });

#define TRI_DEFER_BLOCK(func) \
  TRI_DEFER_BLOCK_INTERNAL(func, SCOPE_GUARD_TOKEN_PASTE(autoScopeGuardObj, __LINE__))

// TRI_DEFER currently just maps to TRI_DEFER_BLOCK
// we will fix this later
#define TRI_DEFER(func) TRI_DEFER_BLOCK(func)

namespace arangodb {

template <class T>
class ScopeGuard {
 public:
  // prevent empty construction
  ScopeGuard() = delete;

  // prevent copying
  ScopeGuard(ScopeGuard const&) = delete;
  ScopeGuard& operator=(ScopeGuard const&) = delete;

  ScopeGuard(T&& func) noexcept : _func(std::forward<T>(func)), _active(true) {}

  ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
      : _func(std::move_if_noexcept(other._func)), _active(other._active) {
    other.cancel();
  }

  // the actual work is done in the ScopeGuard's destructor
  ~ScopeGuard() noexcept {
    if (active()) {
      try {
        // call the scope exit function
        _func();
      } catch (...) {
        // we must not throw in destructors
      }
    }
  }

  // make the guard not trigger the function at scope exit
  void cancel() noexcept { _active = false; }

  // make the guard fire now and deactivate
  void fire() noexcept {
    if (active()) {
      _active = false;

      try {
        // call the scope exit function
        _func();
      } catch (...) {
        // we must not throw in destructors
      }
    }
  }

  // whether or not the guard will trigger the function at scope exit
  bool active() const noexcept { return _active; }

 private:
  // the function to be executed at scope exit
  T _func;

  // whether or not the guard will trigger the function at scope exit
  bool _active;
};

template <class T>
ScopeGuard<T> scopeGuard(T&& f) {
  return ScopeGuard<T>(std::forward<T>(f));
}

}  // namespace arangodb

#endif
