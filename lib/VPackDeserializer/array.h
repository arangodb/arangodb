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
#ifndef VELOCYPACK_ARRAY_H
#define VELOCYPACK_ARRAY_H
#include "deserialize-with.h"
#include "errors.h"
#include "types.h"
#include "utilities.h"
#include "vpack-types.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

/*
 * Array deserializer is used to deserialize an array of variable length but with
 * homogeneous types. Each entry is deserialized with the given deserializer.
 */
template <typename D, template <typename> typename C>
using array_deserializer_container_type = C<typename D::constructed_type>;

template <typename D, template <typename> typename C,
          typename F = utilities::identity_factory<array_deserializer_container_type<D, C>>>
struct array_deserializer {
  using plan = array_deserializer<D, C, F>;
  using factory = F;
  using constructed_type = array_deserializer_container_type<D, C>;
};

namespace executor {

template<typename T>
struct inserter {

  template<typename V>
  void insert(V&& v) {
    t.emplace_back(std::forward<V>(v));
  }

  T receiver() { return std::move(t); }

  T t;
};

template<typename D>
struct inserter<std::unordered_set<D>> {
  using T = std::unordered_set<D>;

  template<typename V>
  void insert(V&& v) {
    t.emplace(std::forward<V>(v));
  }

  T receiver() { return std::move(t); }

  T t;
};

template <typename D, template <typename> typename C, typename F, typename H>
struct deserialize_plan_executor<array_deserializer<D, C, F>, H> {
  using proxy_type = typename array_deserializer<D, C, F>::constructed_type;
  using tuple_type = std::tuple<proxy_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename ctx>
  static auto unpack(slice_type slice, typename H::state_type, ctx&& c) -> result_type {
    if constexpr (!hints::hint_is_array<H>) {
      if (!slice.isArray()) {
        return result_type{deserialize_error{"array expected"}};
      }
    }

    using namespace std::string_literals;
    std::size_t index = 0;
    inserter<proxy_type> result;

    for (auto const& member : ::arangodb::velocypack::deserializer::array_iterator(slice)) {
      auto member_result =
          deserialize<D, hints::hint_list_empty, ctx>(member, {}, std::forward<ctx>(c));
      if (member_result) {
        result.insert(std::move(member_result).get());
      } else {
        return std::move(member_result)
            .error()
            .wrap("at array index "s + std::to_string(index))
            .trace(index);
      }
      index++;
    }

    return result_type{std::move(result.receiver())};
  }
};

}  // namespace executor
}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb

#endif  // VELOCYPACK_ARRAY_H
