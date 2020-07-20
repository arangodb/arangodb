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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
///
////////////////////////////////////////////////////////////////////////////////

#include <utility>
#include "AbstractAccumulator.h"
#include "Accumulators.h"

using namespace arangodb::pregel::algos;


template<typename... Ts>
struct type_list {
  template<typename T>
  static constexpr bool contains = (std::is_same_v<Ts, T> || ...);
};

struct type_any {
  template<typename>
  static constexpr bool contains = true;
};

template<template <typename...> typename A, auto V, typename Restriction = type_any>
struct accum_value_pair{
  template<typename... Ts>
  using instance = A<Ts...>;
  static constexpr auto value = V;
};

template<typename...>
struct accum_mapping;
template<typename E, template <typename> typename... accums, E... values, typename... Rs>
struct accum_mapping<accum_value_pair<accums, values, Rs>...> {
  static constexpr auto size = sizeof...(values);

  template<typename T, typename B, typename... Ps>
  static auto make_unique(E type, Ps&&... ps) -> std::unique_ptr<B> {
    std::unique_ptr<B> result = nullptr;

    ([&]{
      if(type == values) {
        if constexpr (Rs::template contains<T>) {
          result = std::make_unique<accums<T>>(std::forward<Ps>(ps)...);
          return true;
        }
      }
      return false;
    }() || ...);

    return result;
  }
};



template<auto V, typename T>
struct value_type_pair {};
template<typename...>
struct value_type_mapping;
template<typename E, typename Type, E Value, typename... Types, E... Values>
struct value_type_mapping<value_type_pair<Value, Type>, value_type_pair<Values, Types>...> {
  template<typename T>
  struct type_tag {
    static constexpr bool found = true;
    using type = T;
  };

  struct type_tag_not_found {
    static constexpr bool found = false;
  };

  template<typename F>
  static auto invoke(E value, F&& f) {
    if (value == Value) {
      return std::forward<F>(f)(type_tag<Type>{});
    } else if constexpr (sizeof...(Values) > 0) {
      return value_type_mapping<value_type_pair<Values, Types>...>
      ::invoke(value, std::forward<F>(f));
    } else {
      return std::forward<F>(f)(type_tag_not_found{});
    }
  }
};


using integral_restriction = type_list<int, double>;
using bool_restriction = type_list<bool>;
using no_restriction = type_any;

using my_accum_mapping = accum_mapping<
    accum_value_pair<MinAccumulator, AccumulatorType::MIN, integral_restriction>,
    accum_value_pair<MaxAccumulator, AccumulatorType::MAX, integral_restriction>,
    accum_value_pair<SumAccumulator, AccumulatorType::SUM, integral_restriction>,
    accum_value_pair<OrAccumulator, AccumulatorType::AND, bool_restriction>,
    accum_value_pair<AndAccumulator, AccumulatorType::OR, bool_restriction>,
    accum_value_pair<StoreAccumulator, AccumulatorType::STORE, no_restriction>
>;

using my_type_mapping = value_type_mapping<
    value_type_pair<AccumulatorValueType::INTS, int>,
    value_type_pair<AccumulatorValueType::BOOL, bool>,
    value_type_pair<AccumulatorValueType::DOUBLES, double>,
    value_type_pair<AccumulatorValueType::STRINGS, std::string>,
    value_type_pair<AccumulatorValueType::SLICE, VPackSlice>
>;


std::unique_ptr<AccumulatorBase> arangodb::pregel::algos::instanciateAccumulator(::AccumulatorOptions const& options) {
  auto ptr = my_type_mapping::invoke(options.valueType, [&](auto type_tag) -> std::unique_ptr<AccumulatorBase> {
    using used_type = decltype(type_tag);
    if constexpr (used_type::found) {
      return my_accum_mapping::make_unique<typename used_type::type, AccumulatorBase>(options.type, options);
    }

    return nullptr;
  });

  if (!ptr) {
    std::abort();
  }

  return ptr;
}
