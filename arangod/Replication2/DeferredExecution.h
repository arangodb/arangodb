////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <utility>
#include <cstddef>
#include <functional>

namespace arangodb {

/**
 * @brief Just to move callable stuff around without allocations and exceptions
 */
struct DeferredAction {
  static constexpr std::size_t alloc_size = 24;
  DeferredAction() = default;

  DeferredAction(DeferredAction&& other) noexcept {
    *this = std::move(other);
  }

  DeferredAction& operator=(DeferredAction&& other) noexcept {
    fire();

    // other will have hasAction == false and invoke_func == false
    std::swap(invoke_func, other.invoke_func);
    if (invoke_func != nullptr) {
      // this will run the move constructor and then destroy other.storage
      invoke_func(&other.storage, action::move_construct_into_and_destroy, &storage);
    }

    return *this;
  }

  DeferredAction(DeferredAction const&) = delete;
  DeferredAction& operator=(DeferredAction const&) = delete;

  template <typename F, typename Func = std::decay_t<F>,
            std::enable_if_t<std::is_nothrow_invocable_r_v<void, F>, int> = 0>
  explicit DeferredAction(F&& f) noexcept
      : invoke_func(call_action<F>) {
    static_assert(sizeof(F) <= alloc_size);
    static_assert(std::is_nothrow_move_constructible_v<Func>);
    new (&storage) Func(std::forward<F>(f));
  }

  ~DeferredAction() {
    fire();
  }

  explicit operator bool() const noexcept { return invoke_func != nullptr; }

  void fire() noexcept {
    if (invoke_func != nullptr) {
      invoke_func(&storage, action::invoke_and_destroy, nullptr);
      invoke_func = nullptr;
    }
  }

 private:

  enum class action {
    invoke_and_destroy,
    move_construct_into_and_destroy,
  };

  template <typename F, typename Func = std::decay_t<F>>
  static void call_action(void *storage, action what, void* ptr) noexcept {
    auto& func = *reinterpret_cast<Func*>(storage);
    switch (what) {
      case action::invoke_and_destroy:
        std::invoke(std::forward<F>(func));
        static_assert(std::is_nothrow_destructible_v<Func>);
        func.~Func();
        break;
      case action::move_construct_into_and_destroy:
        new (ptr) Func(std::move(func));
        func.~Func();
        break;
    }
  }

  std::aligned_storage_t<alloc_size, alignof(std::max_align_t)> storage{};
  void (*invoke_func)(void*, action, void*) noexcept = nullptr;
};

}  // namespace arangodb
