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
  template<typename T>
  static constexpr auto containts = Restriction::template containts<T>;
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
        } else {
          return false;
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

using my_accum_mapping = accum_mapping<
    accum_value_pair<MinAccumulator, AccumulatorType::MIN, integral_restriction>,
    accum_value_pair<MaxAccumulator, AccumulatorType::MAX, integral_restriction>,
    accum_value_pair<SumAccumulator, AccumulatorType::SUM, integral_restriction>
>;

using my_type_mapping = value_type_mapping<
    value_type_pair<AccumulatorValueType::INTS, int>,
    value_type_pair<AccumulatorValueType::DOUBLES, double>
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
