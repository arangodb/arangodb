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
#ifndef VELOCYPACK_ENUM_DESERIALIZER_H
#define VELOCYPACK_ENUM_DESERIALIZER_H
namespace arangodb {
namespace velocypack {

namespace deserializer {
template <auto EnumValue, typename Value, bool hidden = false>
struct enum_member {
  using enum_type = decltype(EnumValue);
  using value_type = Value;
  static constexpr auto is_hidden = hidden;
};

template <typename Enum, typename... Pairs>
struct enum_deserializer {
  static_assert(utilities::always_false_v<Enum>,
                "Invalid enum_deserializer specification. Are all values "
                "members of the enum?");
};

template <typename Enum, typename... Values, Enum... EnumValues, bool... IsHidden>
struct enum_deserializer<Enum, enum_member<EnumValues, Values, IsHidden>...> {
  using plan = enum_deserializer<Enum, enum_member<EnumValues, Values, IsHidden>...>;
  using constructed_type = Enum;
  using factory = utilities::identity_factory<Enum>;
};

}  // namespace deserializer

namespace deserializer::executor {
template <typename Enum, typename... Values, Enum... EnumValues, bool... IsHidden, typename H>
struct deserialize_plan_executor<enum_deserializer<Enum, enum_member<EnumValues, Values, IsHidden>...>, H> {
  using value_type = Enum;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template<typename...>
  struct type_list{};
  template<typename, bool>
  struct value_hidden_pair {};

  template <typename V, bool hidden, bool... hiddens, typename... Vs>
  static std::string joinValues(type_list<value_hidden_pair<V, hidden>, value_hidden_pair<Vs, hiddens>...>) {
    static_assert(!hidden, "please make the first entry not hidden");
    return to_string(V{}) + ((hiddens ? std::string{} : (", " + to_string(Vs{}))) + ...);
  }

  static constexpr bool all_strings = (values::is_string_v<Values> && ...);

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s, typename H::state_type hints, C &&)
      -> result_type {
    using comparator_hints =
        std::conditional_t<all_strings, hints::hint_list<hints::is_string>, hints::hint_list_empty>;

    if (!all_strings || s.isString()) {
      Enum result;
      bool found = ([&] {
        if (values::value_comparator<Values, comparator_hints>::compare(s)) {
          result = EnumValues;
          return true;
        }
        return false;
      }() || ...);
      if (found) {
        return result_type{result};
      }
    }
    return result_type{
        deserialize_error{"Unrecognized enum value: " + s.toJson() +
                          ", possible values are: " + joinValues(type_list<value_hidden_pair<Values, IsHidden>...>{})}};
  }
};

}  // namespace deserializer::executor
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_ENUM_DESERIALIZER_H
