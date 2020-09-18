////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef VELOCYPACK_CONDITIONAL_H
#define VELOCYPACK_CONDITIONAL_H
#include "plan-executor.h"
#include "values.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

/*
 * Selects a deserializer depending on the first condition that evaluates to true.
 */
template <typename... VSs>
struct conditional;

template <typename C, typename S>
struct condition_deserializer_pair {
  using condition = C;
  using deserializer = S;
};

template <typename D>
struct conditional_default {
  using deserializer = D;
};

template <typename F, typename C = void>
class is_condition_deserializer_pair : public std::true_type {};
template <typename F>
class is_condition_deserializer_pair<F, std::void_t<typename F::condition, typename F::deserializer>>
    : public std::true_type {
  static_assert(is_deserializer_v<typename F::deserializer>);
};

template <typename F, typename C = void>
class is_conditional_default : public std::true_type {};
template <typename F>
class is_conditional_default<F, std::void_t<typename F::deserializer>>
    : public std::true_type {
  static_assert(is_deserializer_v<typename F::deserializer>);
};

template <typename F>
constexpr bool is_condition_deserializer_pair_v =
    is_condition_deserializer_pair<F>::value;
template <typename F>
constexpr bool is_conditional_default_v = is_conditional_default<F>::value;

template <typename... CSs>
struct conditional {
  using constructed_type = std::variant<typename CSs::deserializer::constructed_type...>;

  static_assert(sizeof...(CSs) > 0, "need at lease one alternative");

  static_assert(((is_condition_deserializer_pair_v<CSs> || is_conditional_default_v<CSs>)&&...),
                "list shall only contain `condition_deserializer_pair`s");
};

template <typename... VSs>
struct conditional_deserializer {
  using plan = conditional<VSs...>;
  using constructed_type = typename plan::constructed_type;
  using factory = utilities::identity_factory<constructed_type>;
};

struct is_object_condition {
  using forward_hints = hints::hint_list<hints::is_object>;

  static bool test(::arangodb::velocypack::deserializer::slice_type s) noexcept {
    return s.isObject();
  }
};

template <typename T, typename C = void>
class condition_has_hints : public std::false_type {};
template <typename T>
class condition_has_hints<T, std::void_t<typename T::forward_hints>>
    : public std::true_type {};
template <typename D>
constexpr bool condition_has_hints_v = condition_has_hints<D>::value;

namespace detail {

template <std::size_t I, typename...>
struct conditional_executor;

template <std::size_t I, typename E, typename D, typename... CDs>
struct conditional_executor<I, E, conditional_default<D>, CDs...> {
  using R = typename E::variant_type;
  using unpack_result = result<R, deserialize_error>;

  template <typename ctx>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s, ctx&& c)
      -> unpack_result {
    static_assert(sizeof...(CDs) == 0, "conditional_default must be last");

    return deserialize<D, hints::hint_list_empty, ctx>(s, {}, std::forward<ctx>(c))
        .map([](auto const& v) { return R(std::in_place_index<I>, v); });
  }
};

template <std::size_t I, typename E, typename C, typename D, typename... CDs>
struct conditional_executor<I, E, condition_deserializer_pair<C, D>, CDs...> {
  using R = typename E::variant_type;
  using unpack_result = result<R, deserialize_error>;

  template <typename ctx>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s, ctx&& c)
      -> unpack_result {
    if (C::test(s)) {
      if constexpr (condition_has_hints_v<C>) {
        using hint = typename C::forward_hints;
        return deserialize<D, hint, ctx>(s, {}, std::forward<ctx>(c)).map([](typename D::constructed_type&& v) {
          return R(std::in_place_index<I>, std::move(v));
        });

      } else {
        return deserialize_with<D, hints::hint_list_empty, ctx>(s, {}, std::forward<ctx>(c))
            .map([](typename D::constructed_type&& v) {
              return R(std::in_place_index<I>, std::move(v));
            });
      }
    }

    return conditional_executor<I + 1, E, CDs...>::unpack(s, std::forward<ctx>(c));
  }
};

template <std::size_t I, typename E>
struct conditional_executor<I, E> {
  using R = typename E::variant_type;
  using unpack_result = result<R, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type v, C &&)
      -> unpack_result {
    using namespace std::string_literals;
    return unpack_result{deserialize_error{"unrecognized value `"s + v.toJson() + "`"}};
  }
};
}  // namespace detail

namespace executor {
template <typename... CSs>
struct plan_result_tuple<conditional<CSs...>> {
  using variant = typename conditional<CSs...>::constructed_type;
  using type = std::tuple<variant>;
};

template <typename... CSs, typename H>
struct deserialize_plan_executor<conditional<CSs...>, H> {
  using executor_type = deserialize_plan_executor<conditional<CSs...>, H>;
  using plan_result_tuple_type = plan_result_tuple<conditional<CSs...>>;
  using unpack_tuple_type = typename plan_result_tuple_type::type;
  using variant_type = typename plan_result_tuple_type::variant;
  using unpack_result = result<unpack_tuple_type, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C&& ctx) -> unpack_result {
    /*
     * Select the sub deserializer depending on the value.
     * Delegate to that deserializer.
     */
    using namespace std::string_literals;

    return ::arangodb::velocypack::deserializer::detail::conditional_executor<0, executor_type, CSs...>::unpack(
               s, std::forward<C>(ctx))
        .map([](variant_type&& v) { return std::make_tuple(std::move(v)); })
        .wrap([](deserialize_error&& e) {
          return std::move(e).wrap("when parsing conditionally");
        });
  }
};

}  // namespace executor
}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_CONDITIONAL_H
