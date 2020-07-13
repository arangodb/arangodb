#ifndef VELOCYPACK_PLAN_EXECUTOR_H
#define VELOCYPACK_PLAN_EXECUTOR_H

namespace deserializer::executor {

/*
 * deserialize_plan_executor is specialized for different plan types. It contains
 * the actual logic and has a static `unpack` method receiving the slice and hints.
 */
template<typename P, typename H>
struct deserialize_plan_executor;

/*
 * plan_result_tuple is used to resolve the type of the tuple that a plan produces
 * during execution. This tuple then std::apply'ed to the factory.
 * Some plan types specialize this type. Below you can see the generic implementation.
 */
template <typename P>
struct plan_result_tuple {
  using type = std::tuple<typename P::constructed_type>;
};

}

#endif  // VELOCYPACK_PLAN_EXECUTOR_H
