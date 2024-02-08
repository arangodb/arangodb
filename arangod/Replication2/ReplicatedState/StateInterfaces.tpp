////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "StateInterfaces.h"

#include "Assertions/ProdAssert.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
IReplicatedLeaderState<S>::IReplicatedLeaderState(
    std::shared_ptr<Stream> stream)
    : _stream(std::move(stream)) {
  ADB_PROD_ASSERT(_stream != nullptr);
}

template<typename S>
IReplicatedFollowerState<S>::IReplicatedFollowerState(
    std::shared_ptr<IReplicatedFollowerState<S>::Stream> stream)
    : _stream(std::move(stream)) {
  ADB_PROD_ASSERT(_stream != nullptr);
}

}  // namespace arangodb::replication2::replicated_state