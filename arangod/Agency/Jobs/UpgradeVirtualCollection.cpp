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
/// @author Dan Larkin-York
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
                       std::string const& jobId, arangodb::consensus::Node const& snapshot,
                       std::unordered_map<std::string, std::string> const& childMap) {
  using arangodb::consensus::toDoPrefix;
  using namespace arangodb::velocypack;

  Builder old;
  auto [garbage, found] = snapshot.hasAsBuilder(toDoPrefix + jobId, old);
  if (!found) {
    // Just in case, this is never going to happen, since we will only
    // call the start() method if the job is already in ToDo.
    LOG_TOPIC("2482c", INFO, arangodb::Logger::SUPERVISION)
        << "Failed to get key " + toDoPrefix + jobId + " from agency snapshot";
    return false;
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

void prepareSetUpgradedPropertiesTransaction(arangodb::velocypack::Builder& trx,
                                             std::string const& database,
                                             std::string const& collection,
                                             std::string const& jobId) {
  using namespace arangodb::velocypack;

  std::string const collectionPath = "/Plan/Collections/" + database + "/" + collection;
  std::string const collectionSyncByRevision =
      collectionPath + "/" + arangodb::StaticStrings::SyncByRevision;
  std::string const collectionUsesRevisionsAsDocumentIds =
      collectionPath + "/" + arangodb::StaticStrings::UsesRevisionsAsDocumentIds;
  std::string const collectionUpgradeStatus =
      collectionPath + "/" + arangodb::maintenance::UPGRADE_STATUS;
  std::string const collectionLock = collectionPath + "/" + arangodb::maintenance::LOCK;
  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      // and add the upgrade flag
      trx.add(collectionSyncByRevision, Value(true));
      trx.add(collectionUsesRevisionsAsDocumentIds, Value(true));

      trx.add(Value(collectionUpgradeStatus));
      {
        ObjectBuilder status(&trx);
        trx.add("op", Value("delete"));
      }

      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add("op", Value("delete"));
      }

      // make sure we don't try to rewrite history
      arangodb::consensus::Job::addIncreasePlanVersion(trx);
    }
    {
      ObjectBuilder preconditions(&trx);

      // collection exists
      trx.add(Value(collectionPath));
      {
        ObjectBuilder collection(&trx);
        trx.add("oldEmpty", Value(false));
      }

      // and we have it write locked
      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add(arangodb::consensus::PREC_IS_WRITE_LOCKED, Value(jobId));
      }
    }
  }
}

std::vector<std::string> getChildren(arangodb::consensus::Node const& snapshot,
                                     std::string const& jobId) {
  using namespace arangodb::velocypack;

  std::vector<std::string> children;

  std::string const key = "/Target/Pending/" + jobId + "/children";
  auto [childrenSlice, found] = snapshot.hasAsArray(key);
  if (found && childrenSlice.isArray()) {
    for (Slice child : ArrayIterator(childrenSlice)) {
      if (child.isString()) {
        children.emplace_back(child.copyString());
      }
    }
  }

  return children;
}

arangodb::consensus::JOB_STATUS childStatus(arangodb::consensus::Node const& snapshot,
                                            std::string const& childId) {
  std::string const keyToDo = "/Target/ToDo/" + childId + "/jobId";
  auto [idToDo, foundToDo] = snapshot.hasAsString(keyToDo);
  if (foundToDo && idToDo == childId) {
    return arangodb::consensus::JOB_STATUS::TODO;
  }

  std::string const keyPending = "/Target/Pending/" + childId + "/jobId";
  auto [idPending, foundPending] = snapshot.hasAsString(keyPending);
  if (foundPending && idPending == childId) {
    return arangodb::consensus::JOB_STATUS::PENDING;
  }

  std::string const keyFinished = "/Target/Finished/" + childId + "/jobId";
  auto [idFinished, foundFinished] = snapshot.hasAsString(keyFinished);
  if (foundFinished && idFinished == childId) {
    return arangodb::consensus::JOB_STATUS::FINISHED;
  }

  std::string const keyFailed = "/Target/Failed/" + childId + "/jobId";
  auto [idFailed, foundFailed] = snapshot.hasAsString(keyFailed);
  if (foundFailed && idFailed == childId) {
    return arangodb::consensus::JOB_STATUS::FAILED;
  }

  return arangodb::consensus::JOB_STATUS::NOTFOUND;
}

void prepareReleaseTransaction(arangodb::velocypack::Builder& trx, std::string const& database,
                               std::string const& collection, std::string const& jobId) {
  using namespace arangodb::velocypack;

  std::string const collectionPath = "/Plan/Collections/" + database + "/" + collection;
  std::string const collectionUpgradeStatus =
      collectionPath + "/" + arangodb::maintenance::UPGRADE_STATUS;
  std::string const collectionLock = collectionPath + "/" + arangodb::maintenance::LOCK;
  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      // unlock collection
      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add("op", Value("delete"));
      }

      // remove the upgrade flag
      trx.add(Value(collectionUpgradeStatus));
      {
        ObjectBuilder status(&trx);
        trx.add("op", Value("delete"));
      }

      // make sure we don't try to rewrite history
      arangodb::consensus::Job::addIncreasePlanVersion(trx);
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
        trx.add(arangodb::consensus::PREC_IS_WRITE_LOCKED, Value(jobId));
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
    LOG_TOPIC("4668e", ERR, Logger::SUPERVISION) << err.str();
    finish("", "", false, err.str());
    _status = FAILED;
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

bool UpgradeVirtualCollection::create(std::shared_ptr<VPackBuilder> envelope) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "create not implemented for UpgradeVirtualCollection");
  return false;
}

void UpgradeVirtualCollection::run(bool& aborts) { runHelper("", "", aborts); }

bool UpgradeVirtualCollection::start(bool& aborts) {
  using namespace cluster::paths::aliases;

  std::unordered_map<std::string, std::string> childMap =
      ::prepareChildMap(_supervision, _snapshot, _database, _collection);

  velocypack::Builder pending;
  bool success = ::preparePendingJob(pending, _jobId, _snapshot, childMap);
  if (!success) {
    abort("could not retrieve job info");
    return false;
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

  TRI_IF_FAILURE("UpgradeVirtualCollection::StartJobTransaction") {
    abort(messageIfError);
    return false;
  }

  bool ok = writeTransaction(trx, messageIfError);
  if (ok) {
    _status = PENDING;
    LOG_TOPIC("45122", DEBUG, Logger::SUPERVISION)
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

  std::vector<std::string> children = ::getChildren(_snapshot, _jobId);
  if (children.empty()) {
    abort("could not retrieve child job IDs");
  }

  bool haveFailure = false;
  bool stillWaiting = false;
  for (std::string const& child : children) {
    JOB_STATUS status = ::childStatus(_snapshot, child);
    if (status == FAILED || status == NOTFOUND) {
      haveFailure = true;
    } else if (status == TODO || status == PENDING) {
      stillWaiting = true;
    }
    // okay, this one is finished
  }

  if (haveFailure) {
    abort("one or more child jobs failed");
  }

  if (!stillWaiting) {
    // children all done! write updated properties
    velocypack::Builder trx;
    ::prepareSetUpgradedPropertiesTransaction(trx, _database, _collection, _jobId);
    std::string messageIfError =
        "could not set upgraded properties on collection";
    bool ok = writeTransaction(trx, messageIfError);

    finish("", "", ok);
  }

  return _status;
}

arangodb::Result UpgradeVirtualCollection::abort(std::string const& reason) {
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(
        TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
        "Failed aborting UpgradeVirtualCollection job beyond pending stage");
  }

  if (_status == TODO) {
    finish("", "", false, "job aborted: " + reason);
    return Result{};
  }

  velocypack::Builder trx;
  ::prepareReleaseTransaction(trx, _database, _collection, _jobId);
  std::string const messageIfError =
      "could not clean up after failed virtual collection upgrade";
  [[maybe_unused]] bool ok = writeTransaction(trx, messageIfError);

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
    abort(errorMessage);
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
  job.add(maintenance::DATABASE, velocypack::Value(_database));
  job.add(maintenance::COLLECTION, velocypack::Value(collection));
  job.add("jobId", velocypack::Value(jobId));
  job.add("timeCreated",
          velocypack::Value(timepointToString(std::chrono::system_clock::now())));
  job.add(maintenance::TIMEOUT, velocypack::Value(_timeout));
  job.add(StaticStrings::IsSmartChild, velocypack::Value(true));
}

}  // namespace arangodb::consensus