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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ILogInterfaces.h"

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"

#include <Basics/StaticStrings.h>

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::ILogParticipant::getTerm() const noexcept
    -> std::optional<LogTerm> {
  return getQuickStatus().getCurrentTerm();
}

replicated_log::WaitForResult::WaitForResult(
    LogIndex index, std::shared_ptr<QuorumData const> quorum)
    : currentCommitIndex(index), quorum(std::move(quorum)) {}

void replicated_log::WaitForResult::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::CommitIndex, VPackValue(currentCommitIndex));
  builder.add(VPackValue("quorum"));
  quorum->toVelocyPack(builder);
}

replicated_log::WaitForResult::WaitForResult(velocypack::Slice s)
    : currentCommitIndex(s.get(StaticStrings::CommitIndex).extract<LogIndex>()),
      quorum(std::make_shared<QuorumData>(s.get("quorum"))) {}
