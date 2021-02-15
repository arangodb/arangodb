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
#ifndef VELOCYPACK_VALUE_READER_H
#define VELOCYPACK_VALUE_READER_H
#include "deserializer.h"
#include "gadgets.h"
#include "types.h"
#include "vpack-types.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

/*
 * value_reader is used to extract a value from a slice. It is specialised
 * for all types that can be read. It is expected to have a static `read` function
 * that receives a slice and returns a `result<double, deserialize_error>`.
 */
template <typename T, typename V = void>
struct value_reader {
  static_assert(utilities::always_false_v<T>,
                "no value reader for the given type available");
};

template <>
struct value_reader<std::string> {
  using value_type = std::string;
  using result_type = result<std::string, deserialize_error>;
  static result_type read(::arangodb::velocypack::deserializer::slice_type s) {
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
  static result_type read(::arangodb::velocypack::deserializer::slice_type s) {
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
  static result_type read(::arangodb::velocypack::deserializer::slice_type s) {
    if (s.isBool()) {
      return result_type{s.getBool()};
    }

    return result_type{deserialize_error{"value is not a bool"}};
  }
};

template <typename T>
struct value_reader<T, std::void_t<std::enable_if_t<std::is_arithmetic_v<T>>>> {
  using value_type = T;
  using result_type = result<T, deserialize_error>;
  static result_type read(::arangodb::velocypack::deserializer::slice_type s) {
    if (s.isNumber<T>()) {
      return result_type{s.getNumber<T>()};
    }
    return result_type{deserialize_error{"value is not a number that fits the required type"}};
  }
};

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_VALUE_READER_H
