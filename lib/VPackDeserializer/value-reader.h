////////////////////////////////////////////////////////////////////////////////
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
///
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
template <typename T>
struct value_reader {
  static_assert(utilities::always_false_v<T>,
                "no value reader for the given type available");
};

template <>
struct value_reader<double> {
  using value_type = double;
  using result_type = result<double, deserialize_error>;
  static result_type read(::arangodb::velocypack::deserializer::slice_type s) {
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
  static result_type read(::arangodb::velocypack::deserializer::slice_type s) {
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

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_VALUE_READER_H
