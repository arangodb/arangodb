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

#include "ReconfigureReplicatedLog.h"
#include "Logger/LogMacros.h"
#include "Agency/NodeDeserialization.h"
#include "Agency/AgencyPaths.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

namespace arangodb::consensus {
template<typename Inspector>
auto inspect(Inspector& f, ReconfigureOperation& x) {
  return f.variant(x.value)
      .embedded("operation")
      .alternatives(
          arangodb::inspection::type<ReconfigureOperation::SetLeader>(
              "set-leader"),
          arangodb::inspection::type<ReconfigureOperation::AddParticipant>(
              "add-participant"),
          arangodb::inspection::type<ReconfigureOperation::RemoveParticipant>(
              "remove-participant"));
}
template<typename Inspector>
auto inspect(Inspector& f, ReconfigureOperation::AddParticipant& x) {
  return f.object(x).fields(f.field("participant", x.participant));
}
template<typename Inspector>
auto inspect(Inspector& f, ReconfigureOperation::RemoveParticipant& x) {
  return f.object(x).fields(f.field("participant", x.participant));
}
template<typename Inspector>
auto inspect(Inspector& f, ReconfigureOperation::SetLeader& x) {
  return f.object(x).fields(f.field("participant", x.participant));
}
}  // namespace arangodb::consensus

ReconfigureReplicatedLog::ReconfigureReplicatedLog(Node const& snapshot,
                                                   AgentInterface* agent,
                                                   JOB_STATUS status,
                                                   std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_logId = _snapshot.hasAsUInt(path + "logId");
  auto tmp_operations = _snapshot.hasAsNode(path + "operations");
  auto tmp_expectedVersion = _snapshot.hasAsUInt(path + "expectedVersion");
  auto tmp_undoSetLeader = _snapshot.hasAsString(path + "undoSetLeader");

  if (tmp_database && tmp_logId && tmp_operations) {
    _database = tmp_database.value();
    _logId = replication2::LogId{tmp_logId.value()};
    _operations =
        deserialize<std::vector<ReconfigureOperation>>(tmp_operations);
    _expectedVersion = tmp_expectedVersion.value_or(0);
    _undoSetLeader = tmp_undoSetLeader;
  } else {
    std::string err = basics::StringUtils::concatT("Failed to find job ",
                                                   _jobId, " in agency.");
    LOG_TOPIC("af179", ERR, Logger::SUPERVISION) << err;
    finish("", "", false, err);
    _status = FAILED;
  }
}

JOB_STATUS ReconfigureReplicatedLog::status() {
  if (_status != PENDING) {
    return _status;
  }

  auto targetPath = cluster::paths::aliases::target()
                        ->replicatedLogs()
                        ->database(_database)
                        ->log(_logId);
  if (!_snapshot.hasAsNode(
          targetPath->str(cluster::paths::SkipComponents{1}))) {
    finish("", "", false, "replicated log already gone");
  }

  auto path = cluster::paths::aliases::current()
                  ->replicatedLogs()
                  ->database(_database)
                  ->log(_logId)
                  ->supervision()
                  ->targetVersion();
  auto currentSupervisionVersion =
      _snapshot.hasAsUInt(path->str(cluster::paths::SkipComponents{1}));
  if (currentSupervisionVersion < _expectedVersion) {
    return PENDING;  // not yet converged
  }

  Builder trx;
  {
    VPackArrayBuilder a(&trx);
    {
      VPackObjectBuilder b(&trx);
      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      Builder job;
      std::ignore = _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
      if (_undoSetLeader) {
        addUndoSetLeader(trx);
      }
    }
    { VPackObjectBuilder b(&trx); }
  }

  // Transact to agency:
  write_ret_t res = singleWriteTransaction(_agent, trx, false);
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return FINISHED;
  } else {
    LOG_TOPIC("368b2", WARN, Logger::SUPERVISION)
        << "failed to put job " << _jobId << " into finished";
    return PENDING;
  }
}

arangodb::Result ReconfigureReplicatedLog::abort(const std::string& reason) {
  finish("", "", false, "job aborted: " + reason);
  return {};
}

void ReconfigureReplicatedLog::run(bool& aborts) { runHelper("", "", aborts); }

bool ReconfigureReplicatedLog::create(std::shared_ptr<VPackBuilder> envelope) {
  bool selfCreate = (envelope == nullptr);  // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  std::string path = toDoPrefix + _jobId;

  if (selfCreate) {
    _jb->openArray();
    _jb->openObject();
  }

  _jb->add(VPackValue(path));
  {
    VPackObjectBuilder guard3(_jb.get());
    _jb->add("type", VPackValue("reconfigureReplicatedLog"));
    _jb->add("jobId", VPackValue(_jobId));
    _jb->add("database", VPackValue(_database));
    _jb->add("logId", VPackValue(_logId));
    _jb->add(VPackValue("operations"));
    velocypack::serialize(*_jb, _operations);
    _jb->add("creator", VPackValue(_creator));
    _jb->add("timeCreated",
             VPackValue(timepointToString(std::chrono::system_clock::now())));
    if (_undoSetLeader) {
      _jb->add("undoSetLeader", *_undoSetLeader);
    }
  }

  _status = TODO;

  if (!selfCreate) {
    return true;
  }

  _jb->close();  // transaction object
  _jb->close();  // close array

  write_ret_t res = singleWriteTransaction(_agent, *_jb, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  _status = NOTFOUND;

  LOG_TOPIC("52cfa", INFO, Logger::SUPERVISION)
      << "Failed to insert job " << _jobId;
  return false;
}

namespace {
struct OperationApplier {
  void operator()(ReconfigureOperation::SetLeader const& action) {
    target.leader = action.participant;
  }
  void operator()(ReconfigureOperation::RemoveParticipant const& action) {
    target.participants.erase(action.participant);
    if (target.leader == action.participant) {
      target.leader.reset();
    }
  }
  void operator()(ReconfigureOperation::AddParticipant const& action) {
    target.participants[action.participant];
  }

  arangodb::replication2::agency::LogTarget& target;
};
}  // namespace

bool ReconfigureReplicatedLog::start(bool&) {
  auto targetPath = cluster::paths::aliases::target()
                        ->replicatedLogs()
                        ->database(_database)
                        ->log(_logId);
  auto targetPathStr = targetPath->str(cluster::paths::SkipComponents{1});
  if (!_snapshot.hasAsNode(targetPathStr)) {
    finish("", "", false, "replicated log already gone");
  }

  // do the following checks:
  // - check if _to is not part of the replicated state
  // - check if _from is part of the replicated state
  // then perform the following:
  // - remove _from from target
  // - add _to to target
  // - update the targetVersion
  // - if leader move shard, set leader, otherwise if _from is leader, clear
  // Preconditions:
  //  - target version is as expected
  using namespace replication2;

  auto target = readStateTarget(_snapshot, _database, _logId).value();

  for (auto const& op : _operations) {
    std::visit(OperationApplier{target}, op.value);
  }

  auto oldTargetVersion = target.version;
  auto expected = target.version.value_or(1) + 1;
  target.version = expected;

  // write back to agency
  auto oldJob = _snapshot.hasAsBuilder(toDoPrefix + _jobId).value();
  Builder newJob;
  {
    VPackObjectBuilder ob(&newJob);
    newJob.add("expectedVersion", VPackValue(expected));
    newJob.add(VPackObjectIterator(oldJob.slice()));
  }

  Builder trx;
  {
    VPackArrayBuilder a(&trx);
    {
      VPackObjectBuilder b(&trx);
      addPutJobIntoSomewhere(trx, "Pending", newJob.slice());
      addRemoveJobFromSomewhere(trx, "ToDo", _jobId);

      trx.add(VPackValue(targetPathStr));
      velocypack::serialize(trx, target);
    }
    {
      VPackObjectBuilder b(&trx);

      {
        VPackObjectBuilder ob(&trx, targetPathStr + "/version");
        if (oldTargetVersion) {
          trx.add("old", VPackValue(*oldTargetVersion));
        } else {
          trx.add("oldEmpty", VPackValue(true));
        }
      }
    }
  }

  // Transact to agency:
  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  LOG_TOPIC("1a92d", DEBUG, Logger::SUPERVISION)
      << "Start precondition failed for MoveShard job " << _jobId;
  return false;
}

void ReconfigureReplicatedLog::addUndoSetLeader(Builder& trx) {
  std::string path = returnLeadershipPrefix + to_string(_logId);

  // Briefly check that the place in Target is still empty:
  if (_snapshot.has(path)) {
    LOG_TOPIC("3dc7c", WARN, Logger::SUPERVISION)
        << "failed to schedule undo \"SetLeader\" job for replicated log "
        << _logId << " since there was already one.";
    return;
  }

  std::string now(timepointToString(std::chrono::system_clock::now()));
  std::string deadline(timepointToString(std::chrono::system_clock::now() +
                                         std::chrono::minutes(20)));

  TRI_ASSERT(_undoSetLeader.has_value());
  auto server = _undoSetLeader.value();

  auto rebootId = _snapshot.hasAsUInt(basics::StringUtils::concatT(
      curServersKnown, server, "/", StaticStrings::RebootId));
  if (!rebootId) {
    // This should not happen, since we should have a rebootId for this.
    LOG_TOPIC("a337b", WARN, Logger::SUPERVISION)
        << "failed to schedule undo \"SetLeader\" job for replicated log "
        << _logId << " since there was no rebootId for server " << server;
    return;
  }
  trx.add(VPackValue(path));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("timeStamp", VPackValue(now));
    trx.add("removeIfNotStartedBy", VPackValue(deadline));
    trx.add("rebootId", VPackValue(rebootId.value()));
    trx.add(VPackValue("reconfigureReplicatedLog"));
    {
      VPackObjectBuilder description(&trx);
      trx.add("database", VPackValue(_database));
      trx.add("server", server);
    }
  }
}

ReconfigureReplicatedLog::ReconfigureReplicatedLog(
    Node const& snapshot, AgentInterface* agent, std::string const& jobId,
    std::string const& creator, std::string const& database,
    arangodb::replication2::LogId logId, std::vector<ReconfigureOperation> ops,
    std::optional<ServerID> undoSetLeader)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _logId(logId),
      _operations(std::move(ops)),
      _undoSetLeader(std::move(undoSetLeader)) {}
