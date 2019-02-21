//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_PARAMETER_HPP
#define XENIUM_PARAMETER_HPP

#include <type_traits>

namespace xenium { namespace parameter {

  struct nil;

  template <class T> struct is_set : std::true_type {};
  template <> struct is_set<nil> : std::false_type {};

  template <class... Args>
  struct pack;

  // type_param

  template<template <class> class Policy, class Pack, class Default = nil>
  struct type_param;

  template<template <class> class Policy, class Default, class Arg, class... Args>
  struct type_param<Policy, pack<Arg, Args...>, Default> {
    using type = typename type_param<Policy, pack<Args...>, Default>::type;
  };

  template<template <class> class Policy, class T, class Default, class... Args>
  struct type_param<Policy, pack<Policy<T>, Args...>, Default> {
    using type = T;
  };

  template<template <class> class Policy, class Default>
  struct type_param<Policy, pack<>, Default> {
    using type = Default;
  };

  template<template <class> class Policy, class Default, class... Args>
  using type_param_t = typename type_param<Policy, pack<Args...>, Default>::type;

  // value_param

  template<class ValueType, template <ValueType> class Policy, class Pack, ValueType Default>
  struct value_param;

  template<class ValueType, template <ValueType> class Policy, ValueType Default, class Arg, class... Args>
  struct value_param<ValueType, Policy, pack<Arg, Args...>, Default> {
    static constexpr ValueType value = value_param<ValueType, Policy, pack<Args...>, Default>::value;
  };

  template<class ValueType, template <ValueType> class Policy, ValueType Value, ValueType Default, class... Args>
  struct value_param<ValueType, Policy, pack<Policy<Value>, Args...>, Default> {
    static constexpr ValueType value = Value;
  };

  template<class ValueType, template <ValueType> class Policy, ValueType Default>
  struct value_param<ValueType, Policy, pack<>, Default> {
    static constexpr ValueType value = Default;
  };

  template<class ValueType, template <ValueType> class Policy, ValueType Default, class... Args>
  using value_param_t = value_param<ValueType, Policy, pack<Args...>, Default>;
}}

#endif
