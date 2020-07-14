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
#ifndef VELOCYPACK_ENUM_DESERIALIZER_H
#define VELOCYPACK_ENUM_DESERIALIZER_H

namespace deserializer {
template<auto EnumValue, typename Value>
struct enum_member {
  using enum_type = decltype(EnumValue);
  using value_type = Value;
};

template<typename Enum, typename... Pairs>
struct enum_deserializer{
  static_assert(utilities::always_false_v<Enum>, "Invalid enum_deserializer specification. Are all values members of the enum?");
};

template<typename Enum, typename... Values, Enum... EnumValues>
struct enum_deserializer<Enum, enum_member<EnumValues, Values>...> {
  using plan = enum_deserializer<Enum, enum_member<EnumValues, Values>...>;
  using constructed_type = Enum;
  using factory = utilities::identity_factory<Enum>;
};

}

namespace deserializer::executor {
template <typename Enum, typename... Values, Enum... EnumValues, typename H>
struct deserialize_plan_executor<enum_deserializer<Enum, enum_member<EnumValues, Values>...>, H> {
using value_type = Enum;
using tuple_type = std::tuple<value_type>;
using result_type = result<tuple_type, deserialize_error>;

template<typename V, typename... Vs>
static std::string joinValues() {
  return to_string(V{}) + ((", " + to_string(Vs{})) + ...);
}

template <typename C>
static auto unpack(::deserializer::slice_type s, typename H::state_type hints, C &&)
-> result_type {
  Enum result;
  bool found = ([&] {
    if (values::value_comparator<Values>::compare(s)) {
      result = EnumValues;
      return true;
    }
    return false;
  }() || ...);
  if (found) {
    return result_type{result};
  }
  return result_type{deserialize_error{"Unrecognized enum value: " + s.toJson() + ", possible values are: " + joinValues<Values...>()}};
}
};
}


#endif  // VELOCYPACK_ENUM_DESERIALIZER_H
