#ifndef VELOCYPACK_FIELD_NAME_DEPENDENT_H
#define VELOCYPACK_FIELD_NAME_DEPENDENT_H
#include "plan-executor.h"
#include "types.h"
#include "vpack-types.h"

namespace deserializer {

/*
 * field_name_dependent selects the deserializer by looking at the available
 * fields in the object. It takes the first deserializer that matches.
 */

template <const char N[], typename D>
struct field_name_deserializer_pair {
  constexpr static auto name = N;
  using deserializer = D;
};

// TODO add static_asserts like for field_value_depenent

template <typename... NDs>
struct field_name_dependent {
  using constructed_type = std::variant<typename NDs::deserializer::constructed_type...>;
};

namespace detail {
template <typename...>
struct field_name_dependent_executor;

template <typename R, const char N[], typename D, const char... Ns[], typename... Ds>
struct field_name_dependent_executor<R, field_name_deserializer_pair<N, D>, field_name_deserializer_pair<Ns, Ds>...> {
  using unpack_result = result<R, deserialize_error>;

  template<typename C>
  static auto unpack(::deserializer::slice_type s, C&& ctx) -> unpack_result {
    using namespace std::string_literals;

    auto keySlice = s.get(N);
    if (!keySlice.isNone()) {
      using hints = hints::hint_list<hints::has_field<N>>;
      return deserialize<D, hints, C>(s, std::make_tuple(keySlice), std::forward<C>(ctx))
          .map([](auto & v) { return R{std::move(v)}; })
          .wrap([](deserialize_error && e) {
            return std::move(e.wrap("during dependent parse (found field `"s + N + "`)").trace(N));
          });
    }

    return field_name_dependent_executor<R, field_name_deserializer_pair<Ns, Ds>...>::unpack(s, std::forward<C>(ctx));
  }
};

template <typename R>
struct field_name_dependent_executor<R> {
  using unpack_result = result<R, deserialize_error>;
  template<typename C>
  static auto unpack(::deserializer::slice_type s, C&&) -> unpack_result {
    using namespace std::string_literals;
    return unpack_result{deserialize_error{"format not recognized"}};
  }
};

}  // namespace detail

namespace executor {

template <typename... NDs>
struct plan_result_tuple<field_name_dependent<NDs...>> {
  using variant = typename field_name_dependent<NDs...>::constructed_type;
  using type = std::tuple<variant>;
};

template <typename... NDs, typename H>
struct deserialize_plan_executor<field_name_dependent<NDs...>, H> {
  using value_type = typename field_name_dependent<NDs...>::constructed_type;
  using variant_type = typename plan_result_tuple<field_name_dependent<NDs...>>::variant;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template<typename C>
  static auto unpack(::deserializer::slice_type s, typename H::state_type hints, C&& ctx) -> result_type {
    return ::deserializer::detail::field_name_dependent_executor<variant_type, NDs...>::unpack(s, std::forward<C>(ctx))
        .visit(::deserializer::detail::gadgets::visitor{
            [](variant_type const& v) { return result_type{std::make_tuple(v)}; },
            [](deserialize_error && e) { return result_type{std::move(e)}; }});
  }
};
}  // namespace executor
}  // namespace deserializer

#endif  // VELOCYPACK_FIELD_NAME_DEPENDENT_H
