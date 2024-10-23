////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <concepts>
#include <coroutine>
#include <utility>

namespace arangodb {

template<typename T>
concept HasOperatorCoAwait = requires(T t) {
  operator co_await(std::forward<T>(t));
};
template<typename T>
concept HasMemberOperatorCoAwait = requires(T t) {
  std::forward<T>(t).operator co_await();
};

template<typename T>
auto get_awaitable_object(T&& t) -> T&& {
  return std::forward<T>(t);
}
template<HasOperatorCoAwait T>
auto get_awaitable_object(T&& t) {
  return operator co_await(std::forward<T>(t));
}
template<HasMemberOperatorCoAwait T>
auto get_awaitable_object(T&& t) {
  return std::forward<T>(t).operator co_await();
}

template<typename T>
concept CanSetPromiseWaiter = requires(T t, void* waiter) {
  t.set_promise_waiter(waiter);
};
template<typename T>
concept HasId = requires(T t) {
  { t.id() } -> std::convertible_to<void*>;
};

}  // namespace arangodb
