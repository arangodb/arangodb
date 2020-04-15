////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Agency/Jobs/UpgradeCollection.h"

#include "Agency/Agent.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Cluster/AgencyPaths.h"
#include "Cluster/Maintenance/MaintenanceStrings.h"
#include "VocBase/LogicalCollection.h"

namespace {
using UpgradeState = arangodb::LogicalCollection::UpgradeStatus::State;
arangodb::velocypack::Value stateToValue(UpgradeState state){
  return arangodb::velocypack::Value(static_cast<std::underlying_type<UpgradeState>::type>(state));
}
}

namespace arangodb::consensus {

UpgradeCollection::UpgradeCollection(Node const& snapshot, AgentInterface* agent,
                               std::string const& jobId, std::string const& creator,
                               std::string const& database, std::string const& collection)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection) {
        LOG_DEVEL << "argument constructor";
      }

UpgradeCollection::UpgradeCollection(Node const& snapshot, AgentInterface* agent,
                               JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_collection = _snapshot.hasAsString(path + "collection");

  auto tmp_creator = _snapshot.hasAsString(path + "creator");
  auto tmp_created = _snapshot.hasAsString(path + "timeCreated");

  if (tmp_database.second && tmp_collection.second &&
      tmp_creator.second && tmp_created.second) {
    _database = tmp_database.first;
    _collection = tmp_collection.first;
    _creator = tmp_creator.first;
    _created = stringToTimepoint(tmp_created.first);
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency";
    LOG_TOPIC("4667d", ERR, Logger::SUPERVISION) << err.str();
    finish("", "", false, err.str());
    _status = FAILED;
  }
}

UpgradeCollection::~UpgradeCollection() = default;

void UpgradeCollection::run(bool& aborts) { runHelper("", "", aborts); }

bool UpgradeCollection::create(std::shared_ptr<VPackBuilder> envelope) {
  using namespace std::chrono;
  LOG_TOPIC("b0a34", INFO, Logger::SUPERVISION)
      << "Create upgradeCollection for " + _collection;
      LOG_DEVEL << "create";

  _created = system_clock::now();

  if (envelope == nullptr) {
    _jb = std::make_shared<Builder>();
    _jb->openArray();
    _jb->openObject();
  } else {
    _jb = envelope;
  }

  // Todo entry
  _jb->add(velocypack::Value(toDoPrefix + _jobId));
  {
    velocypack::ObjectBuilder todo(_jb.get());
    _jb->add("creator", velocypack::Value(_creator));
    _jb->add("type", velocypack::Value("upgradeCollection"));
    _jb->add("database", velocypack::Value(_database));
    _jb->add("collection", velocypack::Value(_collection));
    _jb->add("jobId", velocypack::Value(_jobId));
    _jb->add("timeCreated", velocypack::Value(timepointToString(_created)));
  }

  if (envelope == nullptr) {
    _jb->close();  // object
    _jb->close();  // array
    write_ret_t res = singleWriteTransaction(_agent, *_jb, false);
    return (res.accepted && res.indices.size() == 1 && res.indices[0]);
  }

  return true;
}

bool UpgradeCollection::start(bool& aborts) {
  using namespace cluster::paths::aliases;
  std::string collectionPath = "/Plan/Collections/" + _database + "/" + _collection;
  std::string collectionLock = collectionPath + "/Lock";
  velocypack::Builder trx;
  velocypack::Builder todo;

  {
    VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo.second) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC("2482b", INFO, Logger::SUPERVISION)
            << "Failed to get key " + toDoPrefix + _jobId +
                   " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC("34af1", WARN, Logger::SUPERVISION)
            << e.what() << ": " << __FILE__ << ":" << __LINE__;
        return false;
      }
    }
  }

  {
    velocypack::ArrayBuilder listofTransactions(&trx);
    {
      velocypack::ObjectBuilder mutations(&trx);

      // lock collection
      trx.add(velocypack::Value(collectionLock));
      {
        velocypack::ObjectBuilder lock(&trx);
        trx.add("op", velocypack::Value(OP_WRITE_LOCK));
        trx.add("by", velocypack::Value(_jobId));
      }

      // and add the upgrade flag
      trx.add(collectionPath + "/" + maintenance::UPGRADE_STATUS,
              stateToValue(::UpgradeState::Upgrade));

      // make sure we don't try to rewrite history
      addIncreasePlanVersion(trx);

      // then move job from todo to pending
      addPutJobIntoSomewhere(trx, "Pending", todo.slice()[0]);
      addRemoveJobFromSomewhere(trx, "ToDo", _jobId);
    }
    {
      velocypack::ObjectBuilder preconditions(&trx);

      // collection exists
      trx.add(velocypack::Value(collectionPath));
      {
        velocypack::ObjectBuilder collection(&trx);
        trx.add("oldEmpty", velocypack::Value(false));
      }

      // and we can write lock it
      trx.add(velocypack::Value(collectionLock));
      {
        velocypack::ObjectBuilder lock(&trx);
        trx.add(PREC_CAN_WRITE_LOCK, velocypack::Value(true));
      }
    }
  }

  write_ret_t res = singleWriteTransaction(_agent, trx, true);
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("45121", DEBUG, Logger::SUPERVISION)
        << "Pending: Upgrade collection '" + _collection + "'";
    return true;
  }

  return false;
}

JOB_STATUS UpgradeCollection::status() {
  // We can only be hanging around TODO. start === finished
  return TODO;
}

arangodb::Result UpgradeCollection::abort(std::string const& reason) {
  // We can assume that the job is in ToDo or not there:
  LOG_DEVEL << "abort";
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting failedFollower job beyond pending stage");
  }

  // Can now only be TODO
  if (_status == TODO) {
    finish("", "", false, "job aborted: " + reason);
    return Result{};
  }

  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return Result{};
}

}