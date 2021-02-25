////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef DESERIALIZER_TYPES_H
#define DESERIALIZER_TYPES_H
#include <variant>

namespace arangodb {
namespace velocypack {
namespace deserializer {

// struct unit_type {};
using unit_type = std::monostate;

struct error_tag_t {};
struct value_tag_t {};

inline constexpr auto error_tag = error_tag_t{};
inline constexpr auto value_tag = value_tag_t{};

template <typename T, typename E>
class result {
  static_assert(!std::is_same_v<std::decay_t<T>, std::decay_t<E>>);

 private:
  template <typename S, typename F>
  struct cast_visitor {
    auto operator()(S const& t) const {
      return std::variant<T, E>(std::in_place_index<0>, t);
    }
    auto operator()(F const& e) const {
      return std::variant<T, E>(std::in_place_index<1>, e);
    }
  };

 public:
  using variant_type = std::variant<T, E>;
  using value_type = T;
  using error_type = E;

  result(T t) : value(std::move(t)) {}
  result(E e) : value(std::move(e)) {}

  template <typename S, typename F,
            std::enable_if_t<std::is_convertible_v<S, T> && std::is_convertible_v<F, E>, int> = 0>
  result(result<S, F> r) : value(r.visit(cast_visitor<S, F>{})) {}

  result(value_tag_t, T t) : value(std::in_place_index<0>, std::move(t)) {}
  result(error_tag_t, E e) : value(std::in_place_index<1>, std::move(e)) {}

  result(result const&) = default;
  result(result&&) = default;
  result& operator=(result const&) = default;
  result& operator=(result&&) = default;

  operator bool() const noexcept { return ok(); }
  bool ok() const noexcept { return value.index() == 0; }

  T& get() & { return std::get<0>(value); }
  T const& get() const& { return std::get<0>(value); }
  T&& get() && { return std::get<0>(std::move(value)); }

  E& error() & { return std::get<1>(value); }
  E const& error() const& { return std::get<1>(value); }
  E&& error() && { return std::get<1>(std::move(value)); }

  variant_type& content() & { return value; }
  variant_type const& content() const& { return value; }
  variant_type&& content() && { return std::move(value); }

  template <typename F, typename R = std::invoke_result_t<F, T&>>
  result<R, E> map(F&& f) & noexcept(std::is_nothrow_invocable_v<F, T&>) {
    if (ok()) {
      return f(get());
    }
    return error();
  }

  template <typename F, typename R = std::invoke_result_t<F, T const&>>
  result<R, E> map(F&& f) const& noexcept(std::is_nothrow_invocable_v<F, T const&>) {
    if (ok()) {
      return f(get());
    }
    return error();
  }

  template <typename F, typename R = std::invoke_result_t<F, T&&>>
  result<R, E> map(F&& f) && noexcept(std::is_nothrow_invocable_v<F, T&&>) {
    if (ok()) {
      return f(std::move(*this).get());
    }
    return std::move(*this).error();
  }

  template <typename F, typename R = std::invoke_result_t<F, E&>>
  result<T, R> wrap(F&& f) & noexcept(std::is_nothrow_invocable_v<F, E&>) {
    if (!ok()) {
      return f(error());
    }
    return std::move(*this).get();
  }

  template <typename F, typename R = std::invoke_result_t<F, E const&>>
  result<T, R> wrap(F&& f) const& noexcept(std::is_nothrow_invocable_v<F, E const&>) {
    if (!ok()) {
      return f(error());
    }
    return get();
  }

  template <typename F, typename R = std::invoke_result_t<F, E&&>>
  result<T, R> wrap(F&& f) && noexcept(std::is_nothrow_invocable_v<F, E&&>) {
    if (!ok()) {
      return result<T, R>(error_tag, f(std::move(*this).error()));
    }
    return std::move(*this).get();
  }

  template <typename F>
  auto visit(F&& f) & {
    return std::visit(f, value);
  }

  template <typename F>
  auto visit(F&& f) const& {
    return std::visit(f, value);
  }

  template <typename F>
  auto visit(F&& f) && {
    return std::visit(f, std::move(value));
  }

 private:
  variant_type value;
};

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb

template <typename T>
using deserializer_result =
    arangodb::velocypack::deserializer::result<T, arangodb::velocypack::deserializer::error>;
#endif  // DESERIALIZER_TYPES_H
