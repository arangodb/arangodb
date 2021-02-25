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
//
// Created by lars on 30.12.19.
//

#ifndef VELOCYPACK_UNPACK_PROXY_H
#define VELOCYPACK_UNPACK_PROXY_H

namespace arangodb {
namespace velocypack {
namespace deserializer {
template <typename D, typename P = D>
struct unpack_proxy {
  using constructed_type = std::unique_ptr<P>;
  using plan = unpack_proxy<D, P>;
  using factory = utilities::make_unique_factory<P>;
};

namespace executor {

template <typename D, typename P>
struct plan_result_tuple<unpack_proxy<D, P>> {
  using type = std::tuple<P>;
};

template <typename D, typename P, typename H>
struct deserialize_plan_executor<unpack_proxy<D, P>, H> {
  using proxy_type = typename D::constructed_type;
  using tuple_type = std::tuple<proxy_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template <typename C>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type h, C&& ctx) -> result_type {
    return deserialize<D, H, C>(s, h, std::forward<C>(ctx)).map([](typename D::constructed_type&& v) {
      return std::make_tuple(std::move(v));
    });
  }
};
}  // namespace executor

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif
