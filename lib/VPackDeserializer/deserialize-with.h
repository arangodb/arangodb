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
#ifndef VELOCYPACK_DESERIALIZE_WITH_H
#define VELOCYPACK_DESERIALIZE_WITH_H

#include "errors.h"
#include "gadgets.h"
#include "hints.h"
#include "plan-executor.h"
#include "types.h"
#include "vpack-types.h"
namespace arangodb {
namespace velocypack {

namespace deserializer {

template <typename F, typename C = void>
class is_factory : public std::false_type {};

template <typename F>
class is_factory<F, std::void_t<typename F::constructed_type>> : public std::true_type {
};

template <typename F>
constexpr bool is_factory_v = is_factory<F>::value;

template <typename F, typename C = void>
class factory_has_context : public std::false_type {};

template <typename F>
class factory_has_context<F, std::void_t<typename F::context_type>>
    : public std::true_type {};

template <typename F>
constexpr bool factory_has_context_v = factory_has_context<F>::value;

template <typename F, typename C>
F construct_factory(C&& c) {
  if constexpr (detail::gadgets::is_braces_constructible_v<F, C>) {
    return F{std::forward<C>(c)};
  } else {
    static_assert(
        std::is_default_constructible_v<F>,
        "factory is not default constructible - does it require a context?");
    return F();
  }
}

template <typename plan, typename H, typename C, typename Cx = void>
struct is_executor_callable : public std::false_type {};

template <typename plan, typename H, typename C>
struct is_executor_callable<plan, H, C, std::void_t<decltype(&executor::deserialize_plan_executor<plan, H>::template unpack<C>)>>
    : public std::true_type {};

template <typename P, typename H, typename C>
constexpr bool is_executor_callable_v = is_executor_callable<P, H, C>::value;

template <typename T, typename F, typename PR>
constexpr bool is_factory_callable_v = detail::gadgets::is_applicable_r<T, F, PR>::value;

template <typename F>
struct from_factory {
  using plan = typename F::plan;
  using factory = F;
  using constructed_type = typename F::constructed_type;

  static_assert(is_factory_v<F>, "factory does not have required fields");
};

/*
 * This is the prototype of every deserializer. `constructed_type` represents
 * the type that is constructed during deserialization. `plan` describes how the
 * object is read and `factory` is a callable object that is used to create the
 * result object with the values read during unpack phase.
 */
template <typename P, typename F, typename R = typename F::constructed_type>
struct deserializer {
  using plan = P;
  using factory = F;
  using constructed_type = R;
};

template <typename T, typename C = void>
class is_deserializer : public std::false_type {};

template <typename T>
class is_deserializer<T, std::void_t<typename T::plan, typename T::factory, typename T::constructed_type>>
    : public std::true_type {};

template <typename D>
constexpr bool is_deserializer_v = is_deserializer<D>::value;

template <typename D, typename F, typename H = hints::hint_list_empty, typename C = unit_type>
auto deserialize_with(F& factory, ::arangodb::velocypack::deserializer::slice_type slice,
                      typename H::state_type hints = {}, C&& c = {}) {
  static_assert(is_deserializer_v<D>,
                "given deserializer is missing some fields");

  using plan = typename D::plan;
  using factory_type = typename D::factory;
  using constructed_type = typename D::constructed_type;
  using result_type = result<constructed_type, deserialize_error>;

  using plan_unpack_result = typename executor::plan_result_tuple<plan>::type;
  using plan_result_type = result<plan_unpack_result, deserialize_error>;

  static_assert(is_factory_callable_v<constructed_type, factory_type, plan_unpack_result> ||
                    is_factory_callable_v<result_type, factory_type, plan_unpack_result>,
                "factory is not callable with result of plan unpacking or does "
                "not return the correct type");

  static_assert(detail::gadgets::is_complete_type_v<executor::deserialize_plan_executor<plan, H>>,
                "plan type does not specialize deserialize_plan_executor. You "
                "will get an incomplete type error.");

  static_assert(is_executor_callable_v<plan, H, C>,
                "something wrong with your `unpack` on executor");

  static_assert(
      std::is_invocable_r_v<plan_result_type, decltype(&executor::deserialize_plan_executor<plan, H>::template unpack<C>),
                            ::arangodb::velocypack::deserializer::slice_type, typename H::state_type, C>,
      "executor::unpack does not have the correct signature");

  // Simply forward to the plan_executor.
  plan_result_type plan_result =
      executor::deserialize_plan_executor<plan, H>::unpack(slice, hints,
                                                           std::forward<C>(c));
  if (plan_result) {
    // if successfully deserialized, apply to the factory.
    // Notice that if the factory can either return constructed_type or constructed_type_result
    return result_type(std::apply(factory, std::move(plan_result).get()));
  }
  // otherwise forward the error
  return result_type(std::move(plan_result).error());
}

/*
 * Deserializes the given slice using the deserializer D.
 */
template <typename D, typename H = hints::hint_list_empty, typename C = unit_type>
auto deserialize(::arangodb::velocypack::deserializer::slice_type slice,
                 typename H::state_type hints = {}, C&& c = {}) {
  using factory_type = typename D::factory;
  factory_type factory = construct_factory<factory_type>(std::forward<C>(c));
  return deserialize_with<D, factory_type, H, C>(factory, slice, hints,
                                                 std::forward<C>(c));
}

template <typename D, typename C>
auto deserialize_with_context(::arangodb::velocypack::deserializer::slice_type slice, C&& c) {
  return deserialize<D, hints::hint_list_empty, C>(slice, {}, std::forward<C>(c));
}

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_DESERIALIZE_WITH_H
