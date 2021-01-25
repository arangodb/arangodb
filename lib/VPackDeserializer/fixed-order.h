////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#ifndef VELOCYPACK_FIXED_ORDER_H
#define VELOCYPACK_FIXED_ORDER_H
#include "deserialize-with.h"
#include "plan-executor.h"
#include "utilities.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

/*
 * Deserializes an array using a fixed order of deserializers. The result type
 * is a tuple containing the result types of deserializers in order they appear
 * in the template argument `Ds`.
 */
template <typename... Ds>
struct fixed_order_deserializer {
  using plan = fixed_order_deserializer<Ds...>;
  using constructed_type = std::tuple<typename Ds::constructed_type...>;
  using factory = utilities::identity_factory<constructed_type>;
};

template <typename... Ts>
struct tuple_factory {
  using constructed_type = std::tuple<Ts...>;

  template <typename... S>
  constructed_type operator()(S&&... s) {
    return std::make_tuple(std::forward<S>(s)...);
  }
};

template <typename... Ds>
struct tuple_deserializer {
  using constructed_type = std::tuple<typename Ds::constructed_type...>;
  using plan = fixed_order_deserializer<Ds...>;
  using factory = tuple_factory<typename Ds::constructed_type...>;
};

namespace executor {

template <typename... Ds>
struct plan_result_tuple<fixed_order_deserializer<Ds...>> {
  using type = typename fixed_order_deserializer<Ds...>::constructed_type;
};

namespace detail {

template <std::size_t I, typename T, typename E>
struct fixed_order_deserializer_executor_visitor {
  using value_type = std::optional<T>;

  value_type& value_store;
  E& error_store;
  explicit fixed_order_deserializer_executor_visitor(value_type& value_store, E& error_store)
      : value_store(value_store), error_store(error_store) {}

  template <typename U = T>
  bool operator()(U&& t) {
    value_store.emplace(std::forward<U>(t));
    return true;
  }

  bool operator()(E e) {
    using namespace std::string_literals;
    error_store = std::move(
        e.wrap("in fixed order array at position "s + std::to_string(I)).trace(I));
    return false;
  }
};

}  // namespace detail

template <typename... Ds, typename H>
struct deserialize_plan_executor<fixed_order_deserializer<Ds...>, H> {
  using value_type = typename fixed_order_deserializer<Ds...>::constructed_type;
  using tuple_type = value_type;  // std::tuple<value_type>;
  using result_type = result<tuple_type, deserialize_error>;

  constexpr static auto expected_array_length = sizeof...(Ds);

  template <typename ctx>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, ctx&& c) -> result_type {
    using namespace std::string_literals;

    if (!s.isArray()) {
      return result_type{deserialize_error{"expected array"}};
    }

    /*if (s.length() != expected_array_length) {
      return result_type{deserialize_error{
          "bad array length, found: "s + std::to_string(s.length()) +
          ", expected: " + std::to_string(expected_array_length)}};
    }*/
    return unpack_internal(s, std::index_sequence_for<Ds...>{}, std::forward<ctx>(c));
  }

 private:
  template <std::size_t... I, typename ctx>
  static auto unpack_internal(::arangodb::velocypack::deserializer::slice_type s,
                              std::index_sequence<I...>, ctx&& c) -> result_type {
    using namespace ::arangodb::velocypack::deserializer::detail;
    using namespace std::string_literals;

    gadgets::tuple_to_opts_t<value_type> values;
    deserialize_error error;

    ::arangodb::velocypack::deserializer::array_iterator iter(s);

    bool result = ([&error, &c](::arangodb::velocypack::deserializer::array_iterator const& iter,
                                auto& value) {
      if (error) {
        return false;
      }

      if (iter != iter.end()) {
        return deserialize<Ds, hints::hint_list_empty, ctx>(*iter, {},
                                                            std::forward<ctx>(c))
            .visit(detail::fixed_order_deserializer_executor_visitor<I, typename Ds::constructed_type, deserialize_error>(
                value, error));
      } else {
        error = deserialize_error{"bad array length, found: "s + std::to_string(I) +
                                  ", expected: " + std::to_string(expected_array_length)};
        return false;
      }
    }(iter++, std::get<I>(values)) &&
                   ...);

    if (iter != iter.end()) {
      result = false;
      error =
          deserialize_error{"bad array length, excess elements, expected: " +
                            std::to_string(expected_array_length)};
    }

    /*
        bool result =
            (deserialize_with<Ds>(*iter++).visit(
                 detail::fixed_order_deserializer_executor_visitor<I, typename
       Ds::constructed_type, deserialize_error>( std::get<I>(values), error)) &&
             ...);
    */
    if (result) {
      return result_type{gadgets::unpack_opt_tuple(std::move(values))};
    }

    return result_type{std::move(error)};
  }
};
}  // namespace executor
}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_FIXED_ORDER_H
