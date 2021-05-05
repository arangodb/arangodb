////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "LogParticipantI.h"

#include <Basics/Exceptions.h>

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::LogUnconfiguredParticipant::getStatus() const -> LogStatus {
  return LogStatus{UnconfiguredStatus{}};
}

replicated_log::LogUnconfiguredParticipant::LogUnconfiguredParticipant(std::unique_ptr<LogCore> logCore)
    : _logCore(std::move(logCore)) {}

auto replicated_log::LogUnconfiguredParticipant::resign() && -> std::unique_ptr<LogCore> {
  return std::move(_logCore);
}

auto replicated_log::LogUnconfiguredParticipant::waitFor(LogIndex)
    -> replicated_log::LogParticipantI::WaitForFuture {
  // TODO how to resolve a future with an exception?
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto replicated_log::LogParticipantI::waitForIterator(LogIndex index)
    -> replicated_log::LogParticipantI::WaitForIteratorFuture {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
