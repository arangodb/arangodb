////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#pragma once
#include <tuple>
#include "Replication2/ReplicatedLog/ILogInterfaces.h"

namespace arangodb {
template<typename T>
class ResultT;
namespace futures {
template<typename T>
class Future;
}

namespace replication2::replicated_log {
inline namespace comp {

struct IWaitQueueManager {
  using ResolveType =
      std::tuple<WaitForResult, std::unique_ptr<LogRangeIterator>>;

  virtual ~IWaitQueueManager() = default;
  virtual auto waitFor(LogIndex) noexcept -> ILogParticipant::WaitForFuture = 0;
  virtual auto waitForIterator(LogIndex) noexcept
      -> ILogParticipant::WaitForIteratorFuture = 0;
  virtual auto resolveIndex(LogIndex, futures::Try<ResolveType>)
      -> DeferredAction = 0;
  virtual auto resolveAll(futures::Try<ResolveType>) -> DeferredAction = 0;
};

}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
