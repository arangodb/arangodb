#pragma once

#include <coroutine>
#include <utility>

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
