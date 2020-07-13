#ifndef VELOCYPACK_VALUES_H
#define VELOCYPACK_VALUES_H
#include "plan-executor.h"
#include "utilities.h"
#include "gadgets.h"
#include "vpack-types.h"
#include "value-reader.h"

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
template <typename V>
struct value_comparator;

template <typename T, int v>
struct value_comparator<numeric_value<T, v>> {
  static bool compare(::deserializer::slice_type s) {
    if (s.isNumber<T>()) {
      return s.getNumber<T>() == numeric_value<T, v>::value;
    }

    return false;
  }
};

template <const char V[]>
struct value_comparator<string_value<V>> {
  static bool compare(::deserializer::slice_type s) {
    if (s.isString()) {
      return s.isEqualString(arangodb::velocypack::StringRef(V, strlen(V)));
    }

    return false;
  }
};

template<typename VC>
struct value_comparator_condition {
  static bool test(::deserializer::slice_type s) noexcept {
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

/*
 * Assertion helper
 */

template <typename V>
constexpr const bool has_value_comparator_v =
    ::deserializer::detail::gadgets::is_complete_type_v<value_comparator<V>>;

template <typename V>
struct ensure_value_comparator {
  static_assert(has_value_comparator_v<V>,
                "value reader is not specialized for this type. You will "
                "get an incomplete type error.");

  static_assert(std::is_invocable_r_v<bool, decltype(&value_comparator<V>::compare), ::deserializer::slice_type>,
                "a value_comparator<V> must have a static read method returning "
                "bool and receiving a slice");
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

  template<typename C>
  static auto unpack(::deserializer::slice_type s, typename H::state_type hints, C&&) -> result_type {
    ensure_value_reader<T>{};
    return value_reader<T>::read(s).map(
        [](T t) { return std::make_tuple(std::move(t)); });
  }
};
}  // namespace deserializer::executor

#endif  // VELOCYPACK_VALUES_H
