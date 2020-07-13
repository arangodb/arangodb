#ifndef VELOCYPACK_VALUE_READER_H
#define VELOCYPACK_VALUE_READER_H
#include "deserializer.h"
#include "gadgets.h"
#include "types.h"
#include "vpack-types.h"

namespace deserializer {

/*
 * value_reader is used to extract a value from a slice. It is specialised
 * for all types that can be read. It is expected to have a static `read` function
 * that receives a slice and returns a `result<double, deserialize_error>`.
 */
template <typename T>
struct value_reader;

template <>
struct value_reader<double> {
  using value_type = double;
  using result_type = result<double, deserialize_error>;
  static result_type read(::deserializer::slice_type s) {
    if (s.isNumber<double>()) {
      return result_type{s.getNumber<double>()};
    }

    return result_type{deserialize_error{"value is not a double"}};
  }
};

template <>
struct value_reader<unsigned int> {
  using value_type = unsigned int;
  using result_type = result<unsigned int, deserialize_error>;
  static result_type read(::deserializer::slice_type s) {
    if (s.isNumber<unsigned int>()) {
      return result_type{s.getNumber<unsigned int>()};
    }

    return result_type{deserialize_error{"value is not a unsigned int"}};
  }
};

template <>
struct value_reader<std::string> {
  using value_type = std::string;
  using result_type = result<std::string, deserialize_error>;
  static result_type read(::deserializer::slice_type s) {
    if (s.isString()) {
      return result_type{s.copyString()};
    }

    return result_type{deserialize_error{"value is not a string"}};
  }
};


template <>
struct value_reader<std::string_view> {
  using value_type = std::string_view;
  using result_type = result<std::string_view, deserialize_error>;
  static result_type read(::deserializer::slice_type s) {
    if (s.isString()) {
      return result_type{s.stringView()};
    }

    return result_type{deserialize_error{"value is not a string"}};
  }
};

template <>
struct value_reader<bool> {
  using value_type = bool;
  using result_type = result<bool, deserialize_error>;
  static result_type read(::deserializer::slice_type s) {
    if (s.isBool()) {
      return result_type{s.getBool()};
    }

    return result_type{deserialize_error{"value is not a bool"}};
  }
};


/*
 * Helper for static assertion that value_reader is present.
 */
template <typename V>
constexpr const bool has_value_reader_v =
    ::deserializer::detail::gadgets::is_complete_type_v<value_reader<V>>;

template <typename V>
struct ensure_value_reader {
  static_assert(has_value_reader_v<V>,
                "value reader is not specialized for this type. You will "
                "get an incomplete type error.");

  using result_type = result<V, deserialize_error>;
  static_assert(std::is_invocable_r_v<result_type, decltype(&value_reader<V>::read), ::deserializer::slice_type>,
                "a value_reader<V> must have a static read method returning "
                "result<V, deserialize_error> and receiving a slice");
};



}  // namespace deserializer

#endif  // VELOCYPACK_VALUE_READER_H
