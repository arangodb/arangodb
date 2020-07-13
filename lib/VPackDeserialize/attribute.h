#ifndef DESERIALIZER_ATTRIBUTE_H
#define DESERIALIZER_ATTRIBUTE_H
#include "deserialize-with.h"
#include "errors.h"
#include "hints.h"
#include "plan-executor.h"
#include "types.h"
#include "utilities.h"

namespace deserializer {

/*
 * Deserializes the value of the attribute `N` using the deserializer D.
 */
template <const char N[], typename D>
struct attribute_deserializer {
  constexpr static auto name = N;
  using plan = attribute_deserializer<N, D>;
  using constructed_type = typename D::constructed_type;
  using factory = utilities::identity_factory<constructed_type>;
};

template<const char N[], typename V>
struct attribute_value_condition {
  static bool test(::deserializer::slice_type s) noexcept {
    // TODO add hints for this
    if (s.isObject()) {
      return V::compare(s.get(N));
    }
    return false;
  }
};

namespace executor {

template <const char N[], typename D, typename H>
struct deserialize_plan_executor<attribute_deserializer<N, D>, H> {
  using value_type = typename attribute_deserializer<N, D>::constructed_type;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template<typename ctx>
  auto static unpack(slice_type const& s, typename H::state_type hints, ctx && c) -> result_type {
    // if there is no hint that s is actually an object, we have to check that
    if constexpr (!hints::hint_is_object<H>) {
      if (!s.isObject()) {
        return result_type{deserialize_error{"object expected"}};
      }
    }

    slice_type value_slice;
    if constexpr (hints::hint_has_key<N, H>) {
      value_slice = hints::hint_list_state<hints::has_field<N>, H>::get(hints);
    } else {
      value_slice = s.get(N);
    }

    using namespace std::string_literals;

    return deserialize<D, hints::hint_list_empty, ctx>(value_slice, {}, std::forward<ctx>(c))
        .map([](value_type&& v) { return std::make_tuple(v); })
        .wrap([](deserialize_error&& e) {
          return e.wrap("when reading attribute "s + N).trace(N);
        });
  }
};

}  // namespace executor
}  // namespace deserializer

#endif  // DESERIALIZER_ATTRIBUTE_H
