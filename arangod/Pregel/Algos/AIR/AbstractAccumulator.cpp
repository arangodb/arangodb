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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "AbstractAccumulator.h"
#include <utility>
#include "Accumulators.h"

namespace arangodb::pregel::algos::accumulators {

template <typename... Ts>
struct type_list;
struct type_any;
template <template <typename...> typename A, auto V, typename Restriction = type_any>
struct accum_value_pair;
template <typename...>
struct accum_mapping;
template <auto V, typename T>
struct value_type_pair {};
template <typename...>
struct value_type_mapping;



/*
 * INSERT NEW ACCUMULATOR HERE.
 */
using integral_restriction = type_list<int, double>;
using bool_restriction = type_list<bool>;
using no_restriction = type_any;
using slice_restriction = type_list<VPackSlice>;

using my_accum_mapping =
    accum_mapping<accum_value_pair<MinAccumulator, AccumulatorType::MIN, integral_restriction>,
                  accum_value_pair<MaxAccumulator, AccumulatorType::MAX, integral_restriction>,
                  accum_value_pair<SumAccumulator, AccumulatorType::SUM, integral_restriction>,
                  accum_value_pair<AndAccumulator, AccumulatorType::AND, bool_restriction>,
                  accum_value_pair<OrAccumulator, AccumulatorType::OR, bool_restriction>,
                  accum_value_pair<StoreAccumulator, AccumulatorType::STORE, no_restriction>,
                  accum_value_pair<ListAccumulator, AccumulatorType::LIST, no_restriction>,
                  accum_value_pair<CustomAccumulator, AccumulatorType::CUSTOM, slice_restriction>>;

using my_type_mapping =
    value_type_mapping<value_type_pair<AccumulatorValueType::INT, int>,
                       value_type_pair<AccumulatorValueType::BOOL, bool>,
                       value_type_pair<AccumulatorValueType::DOUBLE, double>,
                       value_type_pair<AccumulatorValueType::STRING, std::string>,
                       value_type_pair<AccumulatorValueType::ANY, VPackSlice>>;


/*
 * YOU DO NOT NEED TO UNDERSTAND THE CODE BELOW.
 */



template <typename... Ts>
struct type_list {
  template <typename T>
  static constexpr bool contains = (std::is_same_v<Ts, T> || ...);
};

struct type_any {
  template <typename>
  static constexpr bool contains = true;
};

template <template <typename...> typename A, auto V, typename Restriction>
struct accum_value_pair {
  template <typename... Ts>
  using instance = A<Ts...>;
  static constexpr auto value = V;
};

template <typename E, template <typename> typename... accums, E... values, typename... Rs>
struct accum_mapping<accum_value_pair<accums, values, Rs>...> {
  static constexpr auto size = sizeof...(values);

  template <typename T, typename B, typename... Ps>
  static auto make_unique(E type, Ps&&... ps) -> std::unique_ptr<B> {
    std::unique_ptr<B> result = nullptr;

    ([&] {
      if (type == values) {
        if constexpr (Rs::template contains<T>) {
          result = std::make_unique<accums<T>>(std::forward<Ps>(ps)...);
          return true;
        }
      }
      return false;
    }() ||
     ...);

    return result;
  }

  template <typename T>
  static bool is_valid(E type) {
    return ([&] {
      if (type == values) {
        if constexpr (Rs::template contains<T>) {
          return true;
        }
      }
      return false;
    }() || ...);
  }
};

template <typename E, typename Type, E Value, typename... Types, E... Values>
struct value_type_mapping<value_type_pair<Value, Type>, value_type_pair<Values, Types>...> {
  template <typename T>
  struct type_tag {
    static constexpr bool found = true;
    using type = T;
  };

  struct type_tag_not_found {
    static constexpr bool found = false;
  };

  /**
   * Will invoke `f` either with a type_tag<T> where is the type that translates
   * from E. Or type_tag_not_found, if it was not found.
   * @tparam F
   * @param value
   * @param f
   * @return
   */
  template <typename F>
  static auto invoke(E value, F&& f) {
    if (value == Value) {
      return std::forward<F>(f)(type_tag<Type>{});
    } else if constexpr (sizeof...(Values) > 0) {
      return value_type_mapping<value_type_pair<Values, Types>...>::invoke(
          value, std::forward<F>(f));
    } else {
      return std::forward<F>(f)(type_tag_not_found{});
    }
  }
};

/**
 * Instantiates the correct type of accumulator.
 * @param options
 * @param customDefinitions
 * @return unique pointer to AccumulatorBase
 */
std::unique_ptr<AccumulatorBase> instantiateAccumulator(AccumulatorOptions const& options,
                                                        CustomAccumulatorDefinitions const& customDefinitions) {
  /*
   * WARNING: if you want to add a new accumulator see definition of `my_accum_mapping`. Just put it in there
   * and everything will happen automagically.
   */
  auto ptr = my_type_mapping::invoke(options.valueType, [&](auto type_tag) -> std::unique_ptr<AccumulatorBase> {
    using used_type = decltype(type_tag);
    if constexpr (used_type::found) {
      return my_accum_mapping::make_unique<typename used_type::type, AccumulatorBase>(
          options.type, options, customDefinitions);
    }

    return nullptr;
  });

  return ptr;
}

/**
 * Returns true if the given combination of type and accumulator is valid.
 * @param options
 * @return
 */
bool isValidAccumulatorOptions(const AccumulatorOptions& options) {
  return my_type_mapping::invoke(options.valueType, [&](auto type_tag) -> bool {
    using used_type = decltype(type_tag);
    if constexpr (used_type::found) {
      return my_accum_mapping::is_valid<typename used_type::type>(options.type);
    }

    return false;
  });
}

}  // namespace arangodb::pregel::algos::accumulators
