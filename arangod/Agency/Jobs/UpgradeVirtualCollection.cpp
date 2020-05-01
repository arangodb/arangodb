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

#include "Agency/Jobs/UpgradeVirtualCollection.h"

#include "Agency/Agent.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Basics/StaticStrings.h"
#include "Cluster/AgencyPaths.h"
#include "Cluster/Maintenance/MaintenanceStrings.h"
#include "VocBase/LogicalCollection.h"

namespace {
using UpgradeState = arangodb::LogicalCollection::UpgradeStatus::State;

std::unordered_map<std::string, std::string> prepareChildMap(
    arangodb::consensus::Supervision& supervision, arangodb::consensus::Node const& snapshot,
    std::string const& database, std::string const& collection) {
  using namespace arangodb::velocypack;

  std::unordered_map<std::string, std::string> map;
  std::string shadowPath =
      "/Plan/Collections/" + database + "/" + collection + "/shadowCollections";
  auto [children, found] = snapshot.hasAsArray(shadowPath);
  if (found && children.isArray()) {
    for (Slice child : ArrayIterator(children)) {
      if (child.isInteger()) {
        TRI_voc_cid_t cid = child.getNumber<TRI_voc_cid_t>();
        uint64_t jobId = supervision.nextJobId();
        map.emplace(std::to_string(cid), std::to_string(jobId));
      }
    }
  }
  return map;
}

bool preparePendingJob(arangodb::velocypack::Builder& job,
                       std::shared_ptr<arangodb::velocypack::Builder> const& jb,
                       std::string const& jobId, arangodb::consensus::Node const& snapshot,
                       std::unordered_map<std::string, std::string> const& childMap) {
  using arangodb::consensus::toDoPrefix;
  using namespace arangodb::velocypack;

  // When create() was done with the current snapshot, then the job object
  // will not be in the snapshot under ToDo, but in this case we find it
  // in jb:
  Builder old;
  if (jb == nullptr) {
    auto [garbage, found] = snapshot.hasAsBuilder(toDoPrefix + jobId, old);
    if (!found) {
      // Just in case, this is never going to happen, since we will only
      // call the start() method if the job is already in ToDo.
      LOG_TOPIC("2482b", INFO, arangodb::Logger::SUPERVISION)
          << "Failed to get key " + toDoPrefix + jobId + " from agency snapshot";
      return false;
    }
  } else {
    try {
      old.add(jb->slice()[0].get(toDoPrefix + jobId));
    } catch (std::exception const& e) {
      // Just in case, this is never going to happen, since when _jb is
      // set, then the current job is stored under ToDo.
      LOG_TOPIC("34af1", WARN, arangodb::Logger::SUPERVISION)
          << e.what() << ": " << __FILE__ << ":" << __LINE__;
      return false;
    }
  }

  {
    ObjectBuilder guard(&job);
    for (ObjectIteratorPair pair : ObjectIterator(old.slice())) {
      job.add(pair.key);
      job.add(pair.value);
    }
    {
      ArrayBuilder children(&job, "children");
      for (auto it : childMap) {
        job.add(Value(it.second));
      }
    }
  }

  return true;
}

void prepareStartTransaction(arangodb::velocypack::Builder& trx, std::string const& database,
                             std::string const& collection, std::string const& jobId,
                             arangodb::velocypack::Slice pendingJob,
                             std::vector<arangodb::velocypack::Builder> children) {
  using namespace arangodb::velocypack;

  std::string collectionPath = "/Plan/Collections/" + database + "/" + collection;
  std::string collectionLock = collectionPath + "/" + arangodb::maintenance::LOCK;
  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      // lock collection
      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add("op", Value(arangodb::consensus::OP_WRITE_LOCK));
        trx.add("by", Value(jobId));
      }

      // and add the upgrade flag
      trx.add(collectionPath + "/" + arangodb::maintenance::UPGRADE_STATUS,
              arangodb::LogicalCollection::UpgradeStatus::stateToValue(::UpgradeState::Prepare));

      // make sure we don't try to rewrite history
      arangodb::consensus::Job::addIncreasePlanVersion(trx);

      // then move job from todo to pending
      arangodb::consensus::Job::addPutJobIntoSomewhere(trx, "Pending", pendingJob);
      arangodb::consensus::Job::addRemoveJobFromSomewhere(trx, "ToDo", jobId);

      for (Builder& child : children) {
        arangodb::consensus::Job::addPutJobIntoSomewhere(trx, "ToDo", child.slice());
      }
    }
    {
      ObjectBuilder preconditions(&trx);

      // collection exists
      trx.add(Value(collectionPath));
      {
        ObjectBuilder collection(&trx);
        trx.add("oldEmpty", Value(false));
      }

      // and we can write lock it
      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add(arangodb::consensus::PREC_CAN_WRITE_LOCK, Value(true));
      }
    }
  }
}

void prepareErrorTransaction(arangodb::velocypack::Builder& trx, std::string const& jobId,
                             std::string const& prefix, std::string const& errorMessage,
                             arangodb::velocypack::Slice const& oldJob) {
  using namespace arangodb::velocypack;

  Builder job;
  {
    ObjectBuilder guard(&job);
    for (ObjectIteratorPair pair : ObjectIterator(oldJob)) {
      if (pair.key.isEqualString("error")) {
        job.add("error", Value(errorMessage));
      } else {
        job.add(pair.key);
        job.add(pair.value);
      }
    }
  }

  std::string key = prefix + jobId;
  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      // update job
      trx.add(Value(key));
      trx.add(job.slice());
    }
    {
      ObjectBuilder preconditions(&trx);

      // job exists
      trx.add(Value(key));
      {
        ObjectBuilder collection(&trx);
        trx.add("oldEmpty", Value(false));
      }
    }
  }
}
}  // namespace

namespace arangodb::consensus {

UpgradeVirtualCollection::UpgradeVirtualCollection(Supervision& supervision,
                                                   Node const& snapshot,
                                                   AgentInterface* agent, JOB_STATUS status,
                                                   std::string const& jobId)
    : Job(supervision, status, snapshot, agent, jobId), _timeout(600) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto [tmpDatabase, foundDatabase] = _snapshot.hasAsString(path + "database");
  auto [tmpCollection, foundCollection] =
      _snapshot.hasAsString(path + "collection");

  auto [tmpCreator, foundCreator] = _snapshot.hasAsString(path + "creator");
  auto [tmpCreated, foundCreated] = _snapshot.hasAsString(path + "timeCreated");

  auto [tmpError, errorFound] = _snapshot.hasAsString(path + "error");
  auto [tmpChildren, childrenFound] = _snapshot.hasAsArray(path + "children");
  auto [tmpTimeout, timeoutFound] = _snapshot.hasAsUInt(path + "timeout");

  if (foundDatabase && foundCollection && foundCreator && foundCreated) {
    _database = tmpDatabase;
    _collection = tmpCollection;
    _creator = tmpCreator;
    _created = stringToTimepoint(tmpCreated);
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency";
    LOG_TOPIC("4668d", ERR, Logger::SUPERVISION) << err.str();
    finish("", "", false, err.str());
    _status = FAILED;
  }

  if (errorFound && !tmpError.empty()) {
    _error = tmpError;
  }

  if (childrenFound && tmpChildren.isArray()) {
    for (velocypack::Slice child : velocypack::ArrayIterator(tmpChildren)) {
      if (child.isString()) {
        _children.emplace_back(child.copyString());
      }
    }
  }

  if (timeoutFound) {
    _timeout = tmpTimeout;
  }
}

UpgradeVirtualCollection::~UpgradeVirtualCollection() = default;

void UpgradeVirtualCollection::run(bool& aborts) { runHelper("", "", aborts); }

bool UpgradeVirtualCollection::create(std::shared_ptr<VPackBuilder> envelope) {
  using namespace std::chrono;
  LOG_TOPIC("b0b34", INFO, Logger::SUPERVISION)
      << "Create upgradeCollection for " + _collection;

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
    _jb->add("type", velocypack::Value(maintenance::UPGRADE_VIRTUAL_COLLECTION));
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

bool UpgradeVirtualCollection::start(bool& aborts) {
  using namespace cluster::paths::aliases;

  if (!_error.empty()) {
    // TODO trigger rollback/cleanup
  }

  std::unordered_map<std::string, std::string> childMap =
      ::prepareChildMap(_supervision, _snapshot, _database, _collection);

  velocypack::Builder pending;
  bool success = ::preparePendingJob(pending, _jb, _jobId, _snapshot, childMap);
  if (!success) {
    return success;
  }

  std::vector<velocypack::Builder> children;
  for (auto it : childMap) {
    auto& b = children.emplace_back();
    prepareChildJob(b, it.first, it.second);
  }
  velocypack::Builder trx;
  ::prepareStartTransaction(trx, _database, _collection, _jobId, pending.slice(), children);

  std::string messageIfError =
      "could not begin upgrade of collection '" + _collection + "'";
  bool ok = writeTransaction(trx, messageIfError);
  if (ok) {
    _status = PENDING;
    LOG_TOPIC("45121", DEBUG, Logger::SUPERVISION)
        << "Pending: Upgrade collection '" + _collection + "'";
    return true;
  }

  return false;
}

JOB_STATUS UpgradeVirtualCollection::status() {
  if (_status != PENDING) {
    // either not started yet, or already failed/finished
    return _status;
  }

  if (!_error.empty()) {
    // TODO generate new job to roll back/clean up
  }

  return _status;
}

arangodb::Result UpgradeVirtualCollection::abort(std::string const& reason) {
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting failedFollower job beyond pending stage");
  }

  if (_status == TODO) {
    finish("", "", false, "job aborted: " + reason);
    return Result{};
  }

  // TODO generate new job to roll back/clean up

  finish("", "", false, "job aborted: " + reason);
  return Result{};
}

std::string UpgradeVirtualCollection::jobPrefix() const {
  return _status == TODO ? toDoPrefix : pendingPrefix;
}

velocypack::Slice UpgradeVirtualCollection::job() const {
  if (_jb == nullptr) {
    return velocypack::Slice::noneSlice();
  } else {
    return _jb->slice()[0].get(jobPrefix() + _jobId);
  }
}

bool UpgradeVirtualCollection::writeTransaction(velocypack::Builder const& trx,
                                                std::string const& errorMessage) {
  write_ret_t res = singleWriteTransaction(_agent, trx, true);
  if (!res.accepted || res.indices.size() != 1 || !res.indices[0]) {
    bool registered = registerError(errorMessage);
    if (!registered) {
      abort(errorMessage);
    }
    return false;
  }
  return true;
}

bool UpgradeVirtualCollection::registerError(std::string const& errorMessage) {
  _error = errorMessage;
  velocypack::Builder trx;
  velocypack::Slice jobData = job();
  if (!jobData.isObject()) {
    return false;
  }
  ::prepareErrorTransaction(trx, _jobId, jobPrefix(), errorMessage, jobData);
  write_ret_t res = singleWriteTransaction(_agent, trx, true);
  if (!res.accepted || res.indices.size() != 1 || !res.indices[0]) {
    return false;
  }
  return true;
}

void UpgradeVirtualCollection::prepareChildJob(arangodb::velocypack::Builder& job,
                                               std::string const& collection,
                                               std::string const& jobId) {
  using namespace arangodb::velocypack;
  ObjectBuilder guard(&job);
  job.add("creator", velocypack::Value(_creator));
  job.add("type", velocypack::Value(maintenance::UPGRADE_COLLECTION));
  job.add("database", velocypack::Value(_database));
  job.add("collection", velocypack::Value(collection));
  job.add("jobId", velocypack::Value(jobId));
  job.add("timeCreated",
          velocypack::Value(timepointToString(std::chrono::system_clock::now())));
  job.add("timeout", velocypack::Value(_timeout));
  job.add("isSmartChild", velocypack::Value(true));
}

}  // namespace arangodb::consensus