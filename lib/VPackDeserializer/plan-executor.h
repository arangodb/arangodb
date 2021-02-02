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
#ifndef VELOCYPACK_PLAN_EXECUTOR_H
#define VELOCYPACK_PLAN_EXECUTOR_H

namespace arangodb {
namespace velocypack {
namespace deserializer::executor {

/*
 * deserialize_plan_executor is specialized for different plan types. It contains
 * the actual logic and has a static `unpack` method receiving the slice and hints.
 */
template <typename P, typename H>
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

}  // namespace deserializer::executor
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_PLAN_EXECUTOR_H
