#ifndef DESERIALIZE_VALIDATE_H
#define DESERIALIZE_VALIDATE_H
#include "deserialize-with.h"
#include "plan-executor.h"

namespace deserializer {

template <typename D>
struct forwarding_plan {
  using constructed_type = typename D::constructed_type;
};

namespace validator {

template <typename V, typename C>
V construct_validator(C&& c) {
  if constexpr (detail::gadgets::is_braces_constructible_v<V, C>) {
    return V{std::forward<C>(c)};
  } else {
    static_assert(
        std::is_default_constructible_v<V>,
        "validator is not default constructible - does it require a context?");
    return V();
  }
}

template <typename X, typename C = void>
struct context_type_or_unit {
  using type = unit_type;
};

template <typename X>
struct context_type_or_unit<X, std::void_t<typename X::context_type>> {
  using type = typename X::context_type;
};

template <typename X>
using context_type_or_unit_t = typename context_type_or_unit<X>::type;

template <typename F, typename V>
struct validating_factory {
  using constructed_type = typename F::constructed_type;
  using result_type = result<constructed_type, deserialize_error>;
  using context_type = context_type_or_unit<V>;

  context_type ctx;

  template <typename... S>
  auto operator()(S&&... s) -> result<constructed_type, deserialize_error> {
    F factory = construct_factory<F>(ctx);

    // first execute the factory
    result_type result = factory(std::forward<S>(s)...);

    if (result) {
      V validator = construct_validator<V>(ctx);
      std::optional<deserialize_error> validatorResult = validator(result.get());

      if (validatorResult.has_value()) {
        return std::move(validatorResult).value();
      }
    }

    return result;
  }
};

template <typename D, typename V>
struct validate {
  using plan = validate<D, V>;  // forwarding_plan<D>;
  using constructed_type = typename D::constructed_type;
  using factory = utilities::identity_factory<constructed_type>;  // validating_factory<typename D::factory, V>;
};

}  // namespace validator

template <typename D, typename V>
using validate = validator::validate<D, V>;

template <typename D, typename H>
struct executor::deserialize_plan_executor<forwarding_plan<D>, H> {
  template <typename ctx>
  static auto unpack(::deserializer::slice_type s, typename H::state_type hints, ctx&& c) {
    return ::deserializer::deserialize<D, H, ctx>(s, hints, std::forward<ctx>(c));
  }
};

template <typename D, typename V, typename H>
struct executor::deserialize_plan_executor<validate<D, V>, H> {
  using value_type = typename D::constructed_type;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename ctx>
  static auto unpack(::deserializer::slice_type s, typename H::state_type hints, ctx&& c)
      -> result_type {
    auto result = deserialize<D, H, ctx>(s, hints, std::forward<ctx>(c));

    if (result) {
      V validator = validator::construct_validator<V>(std::forward<ctx>(c));

      using validator_result_type = std::optional<deserialize_error>;

      static_assert(std::is_invocable_r_v<validator_result_type, V, typename decltype(result)::value_type>,
                    "validator either not invocable or has wrong return type");

      validator_result_type validator_result = validator(result.get());
      if (validator_result.has_value()) {
        return std::move(validator_result).value();
      }
    }

    return result;
  }
};

}  // namespace deserializer

#endif  // DESERIALIZE_VALIDATE_H
