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
#ifndef VELOCYPACK_MAP_H
#define VELOCYPACK_MAP_H
#include "plan-executor.h"
#include "utilities.h"
#include "vpack-types.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

/*
 * Map deserializer deserializes an object by constructing a container that maps
 * keys (std::string_view) to constructed types from a second deserializer.
 */

// TODO introduce key_reader to support different types for keys (currently only std::string_view)

template <template <typename, typename> typename C, typename D, typename K>
using map_deserializer_constructed_type =
    C<typename K::value_type, typename D::constructed_type>;

using default_key_read = value_reader<std::string>;

/*
 * D - value deserializer
 * C - container type
 * K - key reader (default = value_reader<std::string>)
 * F - factory (default = identity_factor, i.e. returns container type)
 */
template <typename D, template <typename, typename> typename C, typename K = default_key_read,
          typename F = utilities::identity_factory<map_deserializer_constructed_type<C, D, K>>>
struct map_deserializer {
  using plan = map_deserializer<D, C, K, F>;
  using factory = F;
  using constructed_type = map_deserializer_constructed_type<C, D, K>;
};

namespace executor {

template <typename D, template <typename, typename> typename C, typename F, typename K, typename H>
struct deserialize_plan_executor<map_deserializer<D, C, K, F>, H> {
  using proxy_type = typename map_deserializer<D, C, K, F>::constructed_type;
  using tuple_type = std::tuple<proxy_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename ctx>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, ctx&& c) -> result_type {
    proxy_type result;
    using namespace std::string_literals;

    if constexpr (!hints::hint_is_object<H>) {
      if (!s.isObject()) {
        return result_type{deserialize_error{"expected object"}};
      }
    }

    for (auto const& member :
         ::arangodb::velocypack::deserializer::object_iterator(s, true)) {  // use sequential deserialization
      auto member_result =
          deserialize<D, H, ctx>(member.value, {}, std::forward<ctx>(c));
      if (!member_result) {
        return result_type{std::move(member_result)
                               .error()
                               .wrap("when handling member `"s + member.key.copyString() + "`")
                               .trace(member.key.copyString())};
      }

      auto key_result = K::read(member.key);
      if (!key_result) {
        return result_type{
            std::move(member_result).error().wrap("when reading key")};
      }

      result.insert(result.cend(), std::make_pair(std::move(key_result).get(),
                                                  std::move(member_result).get()));
    }

    return result_type{std::move(result)};
  }
};

}  // namespace executor

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_MAP_H
