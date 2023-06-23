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

#include "AgencyMethods.h"

#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <velocypack/velocypack-common.h>

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/TransactionBuilder.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "Inspection/VPack.h"

namespace arangodb {
class Result;
}  // namespace arangodb

using namespace std::chrono_literals;

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;

namespace paths = arangodb::cluster::paths::aliases;

namespace {
auto sendAgencyWriteTransaction(VPackBufferUInt8 trx)
    -> futures::Future<ResultT<uint64_t>> {
  AsyncAgencyComm ac;
  return ac.sendWriteTransaction(120s, std::move(trx))
      .thenValue([](AsyncAgencyCommResult&& res) -> ResultT<uint64_t> {
        if (res.fail()) {
          return res.asResult();
        }

        // extract raft index
        auto slice = res.slice().get("results");
        TRI_ASSERT(slice.isArray());
        TRI_ASSERT(!slice.isEmptyArray());
        return slice.at(slice.length() - 1).getNumericValue<uint64_t>();
      });
}
}  // namespace

auto methods::deleteReplicatedLogTrx(arangodb::agency::envelope envelope,
                                     DatabaseID const& database, LogId id)
    -> arangodb::agency::envelope {
  auto planPath =
      paths::plan()->replicatedLogs()->database(database)->log(id)->str();
  auto targetPath =
      paths::target()->replicatedLogs()->database(database)->log(id)->str();
  auto currentPath =
      paths::current()->replicatedLogs()->database(database)->log(id)->str();

  return envelope.write()
      .remove(planPath)
      .inc(paths::plan()->version()->str())
      .remove(targetPath)
      .inc(paths::target()->version()->str())
      .remove(currentPath)
      .inc(paths::current()->version()->str())
      .end();
}

auto methods::deleteReplicatedLog(DatabaseID const& database, LogId id)
    -> futures::Future<ResultT<uint64_t>> {
  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    deleteReplicatedLogTrx(arangodb::agency::envelope::into_builder(builder),
                           database, id)
        .done();
  }

  return sendAgencyWriteTransaction(std::move(trx));
}

auto methods::createReplicatedLogTrx(arangodb::agency::envelope envelope,
                                     DatabaseID const& database,
                                     LogTarget const& spec)
    -> arangodb::agency::envelope {
  auto path = paths::target()
                  ->replicatedLogs()
                  ->database(database)
                  ->log(spec.id)
                  ->str();

  return envelope.write()
      .emplace_object(
          path,
          [&](VPackBuilder& builder) { velocypack::serialize(builder, spec); })
      .inc(paths::target()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
}

auto methods::createReplicatedLog(DatabaseID const& database,
                                  LogTarget const& spec)
    -> futures::Future<ResultT<uint64_t>> {
  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    createReplicatedLogTrx(arangodb::agency::envelope::into_builder(builder),
                           database, spec)
        .done();
  }
  return sendAgencyWriteTransaction(std::move(trx));
}

auto methods::replaceReplicatedStateParticipant(
    std::string const& databaseName, LogId id,
    ParticipantId const& participantToRemove,
    ParticipantId const& participantToAdd,
    std::optional<ParticipantId> const& currentLeader)
    -> futures::Future<Result> {
  auto path =
      paths::target()->replicatedLogs()->database(databaseName)->log(id);

  bool replaceLeader = currentLeader == participantToRemove;
  // note that replaceLeader => currentLeader.has_value()
  TRI_ASSERT(!replaceLeader || currentLeader.has_value());

  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope::into_builder(builder)
        .write()
        // remove the old participant
        .remove(*path->participants()->server(participantToRemove))
        // add the new participant
        .set(*path->participants()->server(participantToAdd),
             VPackSlice::emptyObjectSlice())
        // if the old participant was the leader, set the leader to the new one
        .cond(replaceLeader,
              [&](auto&& write) {
                return std::move(write).set(*path->leader(), participantToAdd);
              })
        .inc(*paths::target()->version())
        .precs()
        // assert that the old participant actually was a participant
        .isNotEmpty(*path->participants()->server(participantToRemove))
        // assert that the new participant didn't exist
        .isEmpty(*path->participants()->server(participantToAdd))
        // if the old participant was the leader, assert that it
        .cond(replaceLeader,
              [&](auto&& precs) {
                return std::move(precs).isEqual(*path->leader(),
                                                *currentLeader);
              })
        .cond(not currentLeader.has_value(),
              [&](auto&& precs) {
                return std::move(precs).isEmpty(*path->leader());
              })
        .end()
        .done();
  }

  return sendAgencyWriteTransaction(std::move(trx))
      .thenValue([](ResultT<std::uint64_t>&& resultT) {
        if (resultT.ok() && *resultT == 0) {
          return Result(
              TRI_ERROR_HTTP_PRECONDITION_FAILED,
              "Refused to replace participant. Either the to-be-replaced one "
              "is "
              "not part of the participants, or the new one already was.");
        }
        return resultT.result();
      });
}

auto methods::replaceReplicatedSetLeader(
    std::string const& databaseName, LogId id,
    std::optional<ParticipantId> const& leaderId) -> futures::Future<Result> {
  auto path =
      paths::target()->replicatedLogs()->database(databaseName)->log(id);

  VPackBufferUInt8 trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope::into_builder(builder)
        .write()
        .cond(leaderId.has_value(),
              [&](auto&& write) {
                return std::move(write).set(*path->leader(), *leaderId);
              })
        .cond(!leaderId.has_value(),
              [&](auto&& write) {
                return std::move(write).remove(*path->leader());
              })
        .inc(*paths::target()->version())
        .precs()
        .cond(leaderId.has_value(),
              [&](auto&& precs) {
                return std::move(precs).isNotEmpty(
                    *path->participants()->server(*leaderId));
              })
        .end()
        .done();
  }

  return sendAgencyWriteTransaction(std::move(trx))
      .thenValue([](ResultT<std::uint64_t>&& resultT) {
        if (resultT.ok() && *resultT == 0) {
          return Result(TRI_ERROR_HTTP_PRECONDITION_FAILED,
                        "Refused to set the new leader: It's not part of the "
                        "participants.");
        }
        return resultT.result();
      });
}
