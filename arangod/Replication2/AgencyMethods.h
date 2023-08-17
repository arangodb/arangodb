////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include <Agency/TransactionBuilder.h>
#include <Basics/ResultT.h>
#include <Cluster/ClusterTypes.h>
#include <Futures/Future.h>
#include <Replication2/ReplicatedLog/AgencyLogSpecification.h>
#include <optional>

namespace arangodb {
class Result;
}  // namespace arangodb
namespace arangodb::replication2 {
class LogId;
struct LogTerm;
}  // namespace arangodb::replication2
namespace arangodb::replication2::agency {
struct LogCurrentSupervision;
struct LogCurrentSupervisionElection;
struct LogPlanSpecification;
struct LogPlanTermSpecification;
}  // namespace arangodb::replication2::agency

struct TRI_vocbase_t;

namespace arangodb::replication2::agency::methods {

auto deleteReplicatedLogTrx(arangodb::agency::envelope envelope,
                            DatabaseID const& database, LogId id)
    -> arangodb::agency::envelope;
auto deleteReplicatedLog(DatabaseID const& database, LogId id)
    -> futures::Future<ResultT<uint64_t>>;

auto createReplicatedLogTrx(arangodb::agency::envelope envelope,
                            DatabaseID const& database, LogTarget const& spec)
    -> arangodb::agency::envelope;
auto createReplicatedLog(DatabaseID const& database, LogTarget const& spec)
    -> futures::Future<ResultT<uint64_t>>;

auto replaceReplicatedStateParticipant(
    std::string const& databaseName, LogId id,
    ParticipantId const& participantToRemove,
    ParticipantId const& participantToAdd,
    std::optional<ParticipantId> const& currentLeader)
    -> futures::Future<Result>;

auto replaceReplicatedSetLeader(std::string const& databaseName, LogId id,
                                std::optional<ParticipantId> const& leaderId)
    -> futures::Future<Result>;

}  // namespace arangodb::replication2::agency::methods
