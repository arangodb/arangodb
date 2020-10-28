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
#ifndef VELOCYPACK_PARAMETER_LIST_H
#define VELOCYPACK_PARAMETER_LIST_H
#include "gadgets.h"
#include "values.h"
#include "vpack-types.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

/*
 * parameter_list is used to specifiy a list of object fields that are decoded
 * and used as parameters to the factory. Possible types in `T` are
 * - factory_simple_parameter
 * - factory_slice_parameter
 * - expected_value
 */
template <typename... T>
struct parameter_list {
  constexpr static auto length = sizeof...(T);
};

/*
 * factory_simple_parameter describes a simple type parameter. `N` is the field
 * name, `T` the type of the parameter. If `required` is false,
 * `default_v::value` is used, otherwise the deserialization fails.
 */
template <char const N[], typename T, bool required, typename default_v = values::default_constructed_value<T>>
struct factory_simple_parameter {
  using value_type = T;
  constexpr static bool is_required = required;
  constexpr static auto name = N;
  constexpr static auto default_value = default_v::value;
};

/*
 * Same as factory_simple_parameter, except for Slices. The default value is the null slice.
 */

template <char const N[], bool required>
struct factory_slice_parameter {
  using value_type = ::arangodb::velocypack::deserializer::slice_type;
  constexpr static bool is_required = required;
  constexpr static auto name = N;
  constexpr static auto default_value =
      ::arangodb::velocypack::deserializer::slice_type::nullSlice();
};

template <char const N[], typename T>
struct factory_optional_parameter {
  using value_type = std::optional<T>;
  constexpr static auto name = N;
  constexpr static auto default_value = value_type{};
};

template <const char N[], typename D, bool required>
struct factory_deserialized_parameter {
  using value_type = typename D::constructed_type;
  constexpr static auto name = N;
};

template <const char N[], typename D>
struct factory_deserialized_parameter<N, D, false> {
  using value_type = std::optional<typename D::constructed_type>;
  constexpr static auto name = N;
};

template <const char N[], typename D, typename default_v = values::default_constructed_value<typename D::constructed_type>>
struct factory_deserialized_default {
  using value_type = typename D::constructed_type;
  constexpr static auto default_value = default_v::value;
  constexpr static auto name = N;
};

/*
 * expected_value does not generate a additional parameter to the factory but instead
 * checks if the field with name `N` has the expected value `V`. If not, deserialization fails.
 */

template <const char N[], typename V>
struct expected_value {
  using value_type = void;
  using value = V;
  constexpr static bool is_required = true;
  constexpr static auto name = N;
};

namespace detail {}  // namespace detail

namespace executor {

template <typename... T>
struct plan_result_tuple<parameter_list<T...>> {
  using type =
      typename ::arangodb::velocypack::deserializer::detail::gadgets::tuple_no_void<typename T::value_type...>::type;
};

namespace detail {

template <typename T, typename H>
struct parameter_executor {
  static_assert(utilities::always_false_v<T>,
                "missing parameter executor for type. Is it a parameter type?");
};

template <char const N[], typename T, bool required, typename default_v, typename H>
struct parameter_executor<factory_simple_parameter<N, T, required, default_v>, H> {
  using value_type = T;
  using result_type = result<std::pair<T, bool>, deserialize_error>;
  constexpr static bool has_value = true;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C&&) {
    using namespace std::string_literals;

    auto value_slice = s.get(N);
    if (!value_slice.isNone()) {
      return value_reader<T>::read(value_slice)
          .map([](T&& t) { return std::make_pair(std::move(t), true); })
          .wrap([](deserialize_error&& e) {
            return std::move(e.wrap("when reading value of field "s + N).trace(N));
          });
    }

    if (required) {
      return result_type{deserialize_error{"field `"s + N + "` is required"}};
    } else {
      return result_type{std::make_pair(default_v::value, false)};
    }
  }
};

template <char const N[], typename T, typename H>
struct parameter_executor<factory_optional_parameter<N, T>, H> {
  using value_type = std::optional<T>;
  using result_type = result<std::pair<value_type, bool>, deserialize_error>;
  constexpr static bool has_value = true;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C &&) -> result_type {
    using namespace std::string_literals;

    auto value_slice = s.get(N);
    if (!value_slice.isNone()) {
      return value_reader<T>::read(value_slice)
          .map([](T&& t) { return std::make_pair(std::move(t), true); })
          .wrap([](deserialize_error&& e) {
            return std::move(e.wrap("when reading value of field "s + N).trace(N));
          });
    }

    return result_type{std::make_pair(value_type{}, false)};
  }
};

template <char const N[], typename D, bool required, typename H>
struct parameter_executor<factory_deserialized_parameter<N, D, required>, H> {
  using parameter_type = factory_deserialized_parameter<N, D, required>;
  using value_type = typename parameter_type::value_type;
  using result_type = result<std::pair<value_type, bool>, deserialize_error>;
  constexpr static bool has_value = true;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C&& c) -> result_type {
    using namespace std::string_literals;

    auto value_slice = s.get(N);
    if (!value_slice.isNone()) {
      return ::arangodb::velocypack::deserializer::deserialize<D, hints::hint_list_empty, C>(
                 value_slice, {}, std::forward<C>(c))
          .map([](typename D::constructed_type&& t) {
            return std::make_pair(std::move(t), true);
          })
          .wrap([](deserialize_error&& e) {
            return std::move(e.wrap("when reading value of field "s + N).trace(N));
          });
    }

    if constexpr (required) {
      return result_type{deserialize_error{"field `"s + N + "` is required"}};
    } else {
      return result_type{std::make_pair(value_type{}, false)};
    }
  }
};

template <char const N[], bool required, typename H>
struct parameter_executor<factory_slice_parameter<N, required>, H> {
  using parameter_type = factory_slice_parameter<N, required>;
  using value_type = typename parameter_type::value_type;
  using result_type = result<std::pair<value_type, bool>, deserialize_error>;
  constexpr static bool has_value = true;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C &&) -> result_type {
    auto value_slice = s.get(N);
    if (!value_slice.isNone()) {
      return result_type{std::make_pair(value_slice, true)};
    }

    using namespace std::string_literals;

    if (required) {
      return result_type{deserialize_error{"field `"s + N + "` is required"}};
    } else {
      return result_type{std::make_pair(parameter_type::default_value, false)};
    }
  }
};

template <char const N[], typename D, typename V, typename H>
struct parameter_executor<factory_deserialized_default<N, D, V>, H> {
  using parameter_type = factory_deserialized_default<N, D, V>;
  using value_type = typename parameter_type::value_type;
  using result_type = result<std::pair<value_type, bool>, deserialize_error>;
  constexpr static bool has_value = true;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C&& c) -> result_type {
    using namespace std::string_literals;

    auto value_slice = s.get(N);
    if (!value_slice.isNone()) {
      return ::arangodb::velocypack::deserializer::deserialize<D, hints::hint_list_empty, C>(
                 value_slice, {}, std::forward<C>(c))
          .map([](typename D::constructed_type&& t) {
            return std::make_pair(std::move(t), true);
          })
          .wrap([](deserialize_error&& e) {
            return std::move(e.wrap("when reading value of field "s + N).trace(N));
          });
    }

    return result_type{std::make_pair(parameter_type::default_value, false)};
  }
};

template <const char N[], typename V, typename H>
struct parameter_executor<expected_value<N, V>, H> {
  using parameter_type = expected_value<N, V>;
  using value_type = void;
  using result_type = result<std::pair<unit_type, bool>, deserialize_error>;
  constexpr static bool has_value = false;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C &&) -> result_type {
    if constexpr (!hints::hint_list_contains_v<hints::has_field_with_value<N, V>, H>) {
      auto value_slice = s.get(N);
      if (!values::value_comparator<V>::compare(value_slice)) {
        using namespace std::string_literals;

        return result_type{deserialize_error{
            "value at `"s + N + "` not as expected, found: `" +
            value_slice.toJson() + "`, expected: `" + to_string(V{}) + "`"}
                               .trace(N)};
      }
    }

    return result_type{std::make_pair(unit_type{}, true)};
  }
};

/*
 * I encodes the offset in the result tuple.
 * K counts the number of visited fields.
 */

template <int I, int K, typename...>
struct parameter_list_executor;

template <int I, int K, typename P, typename... Ps, typename H>
struct parameter_list_executor<I, K, parameter_list<P, Ps...>, H> {
  using unpack_result = result<unit_type, deserialize_error>;

  template <typename T, typename C>
  static auto unpack(T& t, ::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C&& ctx) -> unpack_result {
    static_assert(::arangodb::velocypack::deserializer::detail::gadgets::is_complete_type_v<detail::parameter_executor<P, H>>,
                  "parameter executor is not defined");
    // maybe one can do this with folding?
    using executor = detail::parameter_executor<P, H>;
    using namespace std::string_literals;

    // expect_value does not have a value
    if constexpr (executor::has_value) {
      auto result = executor::unpack(s, hints, std::forward<C>(ctx));
      if (result) {
        auto& [value, read_value] = result.get();
        std::get<I>(t) = value;
        if (read_value) {
          return parameter_list_executor<I + 1, K + 1, parameter_list<Ps...>, H>::unpack(
              t, s, hints, std::forward<C>(ctx));
        } else {
          return parameter_list_executor<I + 1, K, parameter_list<Ps...>, H>::unpack(
              t, s, hints, std::forward<C>(ctx));
        }
      }
      return unpack_result{std::move(result).error().wrap(
          "during read of "s + std::to_string(I) + "th parameters value")};
    } else {
      auto result = executor::unpack(s, hints, std::forward<C>(ctx));
      if (result) {
        return parameter_list_executor<I, K + 1, parameter_list<Ps...>, H>::unpack(
            t, s, hints, std::forward<C>(ctx));
      }
      return unpack_result{std::move(result).error()};
    }
  }
};

template <int I, int K, typename H>
struct parameter_list_executor<I, K, parameter_list<>, H> {
  using unpack_result = result<unit_type, deserialize_error>;

  template <typename T, typename C>
  static auto unpack(T& t, ::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C &&) -> unpack_result {
    if constexpr (!hints::hint_has_ignore_unknown<H>) {
      if (s.length() != K) {
        return unpack_result{deserialize_error{
            "superfluous field in object, found " + std::to_string(s.length()) +
            " fields, expected " + std::to_string(K) + " fields"}};
      }
    }
    return unpack_result{unit_type{}};
  }
};
}  // namespace detail

template <typename... Ps, typename H>
struct deserialize_plan_executor<parameter_list<Ps...>, H> {
  using tuple_type = typename plan_result_tuple<parameter_list<Ps...>>::type;
  using unpack_result = result<tuple_type, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, C&& ctx) -> unpack_result {
    using namespace ::arangodb::velocypack::deserializer::detail;

    if constexpr (!hints::hint_is_object<H>) {
      if (!s.isObject()) {
        return unpack_result{deserialize_error(std::string{"object expected"})};
      }
    }

    // store all read parameter in this tuple
    gadgets::tuple_to_opts_t<tuple_type> parameter;

    // forward to the parameter execution
    auto parameter_result =
        detail::parameter_list_executor<0, 0, parameter_list<Ps...>, H>::unpack(
            parameter, s, hints, std::forward<C>(ctx));
    if (parameter_result) {
      return unpack_result{gadgets::unpack_opt_tuple(std::move(parameter))};
    }

    return unpack_result{std::move(parameter_result).error()};
  }
};

}  // namespace executor
}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_PARAMETER_LIST_H
