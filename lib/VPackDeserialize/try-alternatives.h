#ifndef VELOCYPACK_TRY_ALTERNATIVES_H
#define VELOCYPACK_TRY_ALTERNATIVES_H
#include "utilities.h"

namespace deserializer {
namespace try_alternatives {

namespace detail {
template <std::size_t I, typename T, typename E>
struct try_alternatives_deserializer_executor_visitor {
  std::optional<T>& value_store;
  E& error_store;
  explicit try_alternatives_deserializer_executor_visitor(std::optional<T>& value_store,
                                                          E& error_store)
      : value_store(value_store), error_store(error_store) {}

  bool operator()(T t) {
    value_store = std::move(t);
    return true;
  }

  bool operator()(E e) {
    using namespace std::string_literals;
    error_store = std::move(e);
    return false;
  }
};
}  // namespace detail

template <typename... Ds>
struct try_alternatives_deserializer {
  using plan = try_alternatives_deserializer<Ds...>;
  using constructed_type = std::variant<typename Ds::constructed_type...>;
  using factory = utilities::identity_factory<constructed_type>;
};

}  // namespace try_alternatives

namespace executor {
template <typename... Ds, typename H>
struct deserialize_plan_executor<try_alternatives::try_alternatives_deserializer<Ds...>, H> {
  using value_type =
      typename try_alternatives::try_alternatives_deserializer<Ds...>::constructed_type;
  using tuple_type = std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;
  static auto unpack(::deserializer::slice_type s, typename H::state_type hints) -> result_type {
    /*
     * Try one alternative after the other. Take the first that does not fail. If all fail, fail.
     */
    return unpack_internal(s, std::index_sequence_for<Ds...>{});
  }

 private:
  constexpr auto static number_of_alternatives = sizeof...(Ds);

  template <std::size_t... Is>
  static auto unpack_internal(::deserializer::slice_type s, std::index_sequence<Is...>)
      -> result_type {
    std::optional<value_type> result_variant;
    std::array<deserialize_error, number_of_alternatives> errors;

    bool result =
        (deserialize_with<Ds>(s).visit(
             try_alternatives::detail::try_alternatives_deserializer_executor_visitor<Is, value_type, deserialize_error>{
                 result_variant, std::get<Is>(errors)}) ||
         ...);

    if (result) {
      return result_type{result_variant.value()};
    } else {
      /*
       * Join all the errors.
       */
      using namespace std::string_literals;

      return result_type{deserialize_error{
          "no matching alternative found, their failures in order are: ["s +
          join(errors) + ']'}};
    }
  }
  template <typename T>
  static std::string join(T const& t) {
    using namespace std::string_literals;
    std::string ss;
    bool first = true;
    for (auto const& k : t) {
      if (!first) {
        ss += ", ";
      }
      first = false;
      ss += k.what();
    }

    return ss;
  }
};
}  // namespace executor

}  // namespace deserializer

#endif  // VELOCYPACK_TRY_ALTERNATIVES_H
