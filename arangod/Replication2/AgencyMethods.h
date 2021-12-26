////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
#include <Replication2/ReplicatedLog/AgencyLogSpecification.h>
#include <optional>

#include "Futures/Future.h"

namespace arangodb {
class Result;
}  // namespace arangodb
namespace arangodb::replication2 {
class LogId;
struct LogTerm;
}  // namespace arangodb::replication2
namespace arangodb::replication2::agency {
struct LogCurrentSupervisionElection;
struct LogPlanSpecification;
struct LogPlanTermSpecification;
}  // namespace arangodb::replication2::agency

namespace arangodb::replication2::agency::methods {

auto updateTermSpecificationTrx(arangodb::agency::envelope envelope,
                                DatabaseID const& database, LogId id,
                                LogPlanTermSpecification const& spec,
                                std::optional<LogTerm> prevTerm = {})
    -> arangodb::agency::envelope;

auto updateTermSpecification(DatabaseID const& database, LogId id,
                             LogPlanTermSpecification const& spec,
                             std::optional<LogTerm> prevTerm = {})
    -> futures::Future<Result>;

auto updateElectionResult(arangodb::agency::envelope envelope, DatabaseID const& database,
                          LogId id, LogCurrentSupervisionElection const& result)
    -> arangodb::agency::envelope;

auto deleteReplicatedLogTrx(arangodb::agency::envelope envelope, DatabaseID const& database,
                            LogId id) -> arangodb::agency::envelope;
auto deleteReplicatedLog(DatabaseID const& database, LogId id) -> futures::Future<Result>;

auto createReplicatedLogTrx(arangodb::agency::envelope envelope, DatabaseID const& database,
                            LogPlanSpecification const& spec) -> arangodb::agency::envelope;
auto createReplicatedLog(DatabaseID const& database, LogPlanSpecification const& spec)
    -> futures::Future<Result>;

}  // namespace arangodb::replication2::agency::methods
