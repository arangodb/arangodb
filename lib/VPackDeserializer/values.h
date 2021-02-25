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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef VELOCYPACK_DESERIALIZER_VALUES_H
#define VELOCYPACK_DESERIALIZER_VALUES_H
#include "gadgets.h"
#include "plan-executor.h"
#include "utilities.h"
#include "value-reader.h"
#include "vpack-types.h"

namespace arangodb {
namespace velocypack {
namespace deserializer::values {

namespace detail {
template <typename T>
struct value_type {
  using type = T;
};
}  // namespace detail

/*
 * Represents a numeric value of the given type.
 */
template <typename T, int V>
struct numeric_value : detail::value_type<T> {
  constexpr static auto value = T{V};
};

/*
 * Represents a string value.
 */
template <const char V[]>
struct string_value : detail::value_type<const char[]> {
  constexpr static auto value = V;
};
struct empty_string_value : detail::value_type<const char[]> {
  constexpr static auto value = "";
};

template <typename V>
struct is_string : std::false_type {};
template <const char V[]>
struct is_string<string_value<V>> : std::true_type {};
template <typename V>
constexpr bool is_string_v = is_string<V>::value;

template <typename T>
struct default_constructed_value {
  constexpr static T value = T{};
};

// TODO add proper static_asserts to ensure the correct behavior for values.
// This should include:
//  - having a `value` member.
//  - having a `type` member type.
//  - having a `to_string` implementation.

/*
 * value_slice_comparators have a static function `compare` that returns a
 * `bool`, if the value found is equal to the value specified.
 */
template <typename V, typename H = hints::hint_list_empty>
struct value_comparator {
  static_assert(utilities::always_false_v<V>, "value comparator not defined");
};

template <typename T, int v>
struct value_comparator<numeric_value<T, v>> {
  static bool compare(::arangodb::velocypack::deserializer::slice_type s) {
    if (s.isNumber<T>()) {
      return s.getNumber<T>() == numeric_value<T, v>::value;
    }

    return false;
  }
};

template <const char V[], typename H>
struct value_comparator<string_value<V>, H> {
  static bool compare(::arangodb::velocypack::deserializer::slice_type s) {
    if (hints::hint_is_string<H> || s.isString()) {
      return s.isEqualString(arangodb::velocypack::StringRef(V, strlen(V)));
    }

    return false;
  }
};

template <typename VC>
struct value_comparator_condition {
  static bool test(::arangodb::velocypack::deserializer::slice_type s) noexcept {
    return VC::compare(s);
  }
};

/*
 * Uses a value reader to deserialize the slice.
 */
template <typename T>
struct value_deserializer {
  using plan = value_deserializer<T>;
  using constructed_type = T;
  using factory = utilities::identity_factory<T>;
};

struct slice_deserializer {
  using plan = slice_deserializer;
  using constructed_type = slice_type;
  using factory = utilities::identity_factory<constructed_type>;
};

struct vpack_builder_deserializer {
  using plan = vpack_builder_deserializer;
  using constructed_type = builder_type;
  using factory = utilities::identity_factory<constructed_type>;
};

}  // namespace deserializer::values

template <typename T, int V>
std::string to_string(deserializer::values::numeric_value<T, V> const&) {
  return std::to_string(V);
}

template <const char V[]>
std::string to_string(deserializer::values::string_value<V> const&) {
  return V;
}

namespace deserializer::executor {
template <typename T, typename H>
struct deserialize_plan_executor<values::value_deserializer<T>, H> {
  using value_type = T;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s, typename H::state_type hints, C &&)
      -> result_type {
    return value_reader<T>::read(s).map(
        [](T t) { return std::make_tuple(std::move(t)); });
  }
};

template <typename H>
struct deserialize_plan_executor<values::slice_deserializer, H> {
  using value_type = slice_type ;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s, typename H::state_type, C &&)
  -> result_type {
    return result_type{s};
  }
};

template <typename H>
struct deserialize_plan_executor<values::vpack_builder_deserializer, H> {
  using value_type = builder_type;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s, typename H::state_type, C &&)
  -> result_type {
    builder_type builder;
    builder.add(s);
    return result_type{std::move(builder)};
  }
};

}  // namespace deserializer::executor
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_VALUES_H
