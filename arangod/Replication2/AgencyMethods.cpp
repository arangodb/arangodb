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

#include "AgencyMethods.h"

#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <velocypack/velocypack-aliases.h>
#include <velocypack/velocypack-common.h>

#include "Agency/AsyncAgencyComm.h"
#include "Agency/TransactionBuilder.h"
#include "Agency/AgencyPaths.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb {
class Result;
}  // namespace arangodb

using namespace std::chrono_literals;

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;

namespace paths = arangodb::cluster::paths::aliases;

namespace {
auto sendAgencyWriteTransaction(VPackBufferUInt8 trx) {
  AsyncAgencyComm ac;
  return ac.sendWriteTransaction(120s, std::move(trx)).thenValue([](AsyncAgencyCommResult&& res) {
    return res.asResult();
  });
}
}  // namespace

auto methods::updateTermSpecificationTrx(arangodb::agency::envelope envelope,
                                         DatabaseID const& database, LogId id,
                                         LogPlanTermSpecification const& spec,
                                         std::optional<LogTerm> prevTerm)
    -> arangodb::agency::envelope {
  auto path = paths::plan()->replicatedLogs()->database(database)->log(to_string(id));
  auto logPath = path->str();
  auto termPath = path->currentTerm()->str();

  return envelope.write()
      .emplace_object(termPath,
                      [&](VPackBuilder& builder) { spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isNotEmpty(logPath)
      .cond(prevTerm.has_value(),
            [&](auto&& precs) {
              return std::move(precs).isEqual(path->currentTerm()->term()->str(),
                                              prevTerm->value);
            })
      .end();
}

auto methods::updateTermSpecification(DatabaseID const& database, LogId id,
                                      LogPlanTermSpecification const& spec,
                                      std::optional<LogTerm> prevTerm)
    -> futures::Future<Result> {
  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    updateTermSpecificationTrx(arangodb::agency::envelope::into_builder(builder),
                               database, id, spec, prevTerm)
        .done();
  }

  return sendAgencyWriteTransaction(std::move(trx));
}

auto methods::deleteReplicatedLogTrx(arangodb::agency::envelope envelope,
                                     DatabaseID const& database, LogId id)
    -> arangodb::agency::envelope {
  auto path = paths::plan()->replicatedLogs()->database(database)->log(id)->str();

  return envelope.write()
      .remove(path)
      .inc(paths::plan()->version()->str())
      .precs()
      .isNotEmpty(path)
      .end();
}

auto methods::deleteReplicatedLog(DatabaseID const& database, LogId id)
    -> futures::Future<Result> {
  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    deleteReplicatedLogTrx(arangodb::agency::envelope::into_builder(builder), database, id)
        .done();
  }

  return sendAgencyWriteTransaction(std::move(trx));
}

auto methods::createReplicatedLogTrx(arangodb::agency::envelope envelope,
                                     DatabaseID const& database,
                                     LogPlanSpecification const& spec)
    -> arangodb::agency::envelope {
  auto path = paths::plan()->replicatedLogs()->database(database)->log(spec.id)->str();

  return envelope.write()
      .emplace_object(path,
                      [&](VPackBuilder& builder) { spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
}

auto methods::createReplicatedLog(DatabaseID const& database, LogPlanSpecification const& spec)
    -> futures::Future<Result> {
  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    createReplicatedLogTrx(arangodb::agency::envelope::into_builder(builder), database, spec)
        .done();
  }

  return sendAgencyWriteTransaction(std::move(trx));
}

auto methods::updateElectionResult(arangodb::agency::envelope envelope,
                                   DatabaseID const& database, LogId id,
                                   LogCurrentSupervisionElection const& result)
    -> arangodb::agency::envelope {
  auto path = paths::current()->replicatedLogs()->database(database)->log(to_string(id))->str();

  return envelope.write()
      .emplace_object(path + "/supervision/election",
                      [&](VPackBuilder& builder) {
                        result.toVelocyPack(builder);
                      })
      .inc(paths::current()->version()->str())
      .end();
}
