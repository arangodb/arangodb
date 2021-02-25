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
#ifndef DESERIALIZER_CONTEXT_H
#define DESERIALIZER_CONTEXT_H
#include "plan-executor.h"
#include "utilities.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {

namespace context {

template <typename D, typename Q>
struct context_modify_plan {
  using constructed_type = typename D::constructed_type;
};

template <typename D, auto M>
struct from_member;

template <typename D, typename A, typename B, A B::*member>
struct from_member<D, member> {
  using plan = context_modify_plan<D, utilities::member_extractor<member>>;
  using factory = typename D::F;
  using constructed_type = typename D::R;
};

}  // namespace context

namespace executor {

template <typename D, typename Q, typename H>
struct deserialize_plan_executor<context::context_modify_plan<D, Q>, H> {
  template <typename ctx>
  static auto unpack(::arangodb::velocypack::deserializer::slice_type s,
                     typename H::state_type hints, ctx&& c) {
    return deserialize<D, H, Q::context_type>(s, hints, Q::exec(std::forward<ctx>(c)));
  }
};

}  // namespace executor

}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb

#endif  // DESERIALIZER_CONTEXT_H
