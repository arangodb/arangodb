////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ReplaceIndex.h"

#include "Agency/AgentInterface.h"
#include "Agency/Node.h"
#include "Basics/StaticStrings.h"
#include "Basics/TimeString.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

namespace arangodb::consensus {

namespace {

// Marker on the shadow index entry in Plan; carries the old index id.
constexpr std::string_view kReplacesField = "replaces";

struct NoOp {
  void operator()(Builder&) const noexcept {}
};

// Submit a write transaction that updates `indexesKey` with
// `newIndexesArray`, bumps Plan/Version, and requires the original indexes
// array to be unchanged. Optional extra ops/preconds let the caller piggy-
// back additional mutations (e.g. moving the job ToDo→Pending) atop the
// same atomic txn.
template<class ExtraOps = NoOp, class ExtraPreconds = NoOp>
write_ret_t submitPlanIndexesUpdate(AgentInterface* agent,
                                    std::string const& indexesKey,
                                    Slice plannedOriginal,
                                    Slice newIndexesArray,
                                    ExtraOps extraOps = {},
                                    ExtraPreconds extraPreconds = {}) {
  Builder trx;
  {
    VPackArrayBuilder transactions(&trx);
    {
      VPackObjectBuilder ops(&trx);
      extraOps(trx);
      trx.add(VPackValue(indexesKey));
      trx.add(newIndexesArray);
      Job::addIncreasePlanVersion(trx);
    }
    {
      VPackObjectBuilder pre(&trx);
      Job::addPreconditionUnchanged(trx, indexesKey, plannedOriginal);
      extraPreconds(trx);
    }
  }
  return singleWriteTransaction(agent, trx, false);
}

enum class ShardOutcome { kPending, kDone, kFailed, kNotFound };

// Inspect Current's indexes slice for one shard and classify the new index
// entry's build state. "Unusable" counts as a successful build attempt:
// insufficient-training-data is no longer treated as a failure in the
// replace flow.
ShardOutcome classifyShard(Slice indexesSlice, std::string_view indexId) {
  if (!indexesSlice.isArray()) {
    return ShardOutcome::kNotFound;
  }
  for (auto indexEntry : VPackArrayIterator(indexesSlice)) {
    if (auto const indexEntryId = indexEntry.get(StaticStrings::IndexId);
        !indexEntryId.isString() || indexEntryId.stringView() != indexId) {
      continue;
    }
    auto err = indexEntry.get(StaticStrings::Error);
    if (err.isBool() && err.getBool()) {
      auto errNum = indexEntry.get(StaticStrings::ErrorNum);
      if (errNum.isNumber<int>() &&
          errNum.getNumber<int>() ==
              static_cast<int>(TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY)) {
        return ShardOutcome::kDone;
      }
      return ShardOutcome::kFailed;
    }
    auto state = indexEntry.get(StaticStrings::IndexTrainingState);
    if (!state.isString()) {
      return ShardOutcome::kDone;
    }
    auto s = state.stringView();
    if (s == "ready" || s == "unusable") {
      return ShardOutcome::kDone;
    }
    return ShardOutcome::kPending;
  }
  return ShardOutcome::kPending;
}

}  // namespace

ReplaceIndexPayload ReplaceIndexPayload::make(
    std::string database, std::string collection, std::string oldIndexId,
    std::string newIndexId, std::string jobId, std::string creator,
    Slice newDefinitionSlice) {
  ReplaceIndexPayload payload{
      .database = std::move(database),
      .collection = std::move(collection),
      .oldIndexId = std::move(oldIndexId),
      .newIndexId = std::move(newIndexId),
      .jobId = std::move(jobId),
      .creator = std::move(creator),
      .timeCreated = timepointToString(std::chrono::system_clock::now()),
      .newDefinition = {},
  };
  payload.newDefinition.add(newDefinitionSlice);
  return payload;
}

ReplaceIndex::ReplaceIndex(Node const& snapshot, AgentInterface* agent,
                           std::string const& jobId, std::string const& creator,
                           std::string const& database,
                           std::string const& collection,
                           std::string const& oldIndexId,
                           std::string const& newIndexId,
                           std::shared_ptr<Builder> newDefinition)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _oldIndexId(oldIndexId),
      _newIndexId(newIndexId),
      _newDefinition(std::move(newDefinition)) {}

ReplaceIndex::ReplaceIndex(Node const& snapshot, AgentInterface* agent,
                           JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  auto node = _snapshot.get(pos[status] + _jobId);
  if (node == nullptr) {
    auto const err =
        "ReplaceIndex job " + _jobId + " not found in agency snapshot";
    LOG_TOPIC("12fa1", ERR, Logger::SUPERVISION) << err;
    finish("", "", false, err);
    _status = FAILED;
    return;
  }

  ReplaceIndexPayload payload;
  auto jobBuilder = node->toBuilder();
  // The supervision framework annotates the job entry with extra bookkeeping
  // fields (e.g. `timeStarted` when moving ToDo→Pending), so we must allow
  // unknown fields here.
  auto res = velocypack::deserializeWithStatus(jobBuilder.slice(), payload,
                                               {.ignoreUnknownFields = true});
  if (!res.ok()) {
    auto const err = "Failed to load ReplaceIndex job " + _jobId +
                     " from agency snapshot: " + res.error() +
                     " (path: " + res.path() + ")";
    LOG_TOPIC("12fab", ERR, Logger::SUPERVISION) << err;
    finish("", "", false, err);
    _status = FAILED;
    return;
  }
  _database = std::move(payload.database);
  _collection = std::move(payload.collection);
  _oldIndexId = std::move(payload.oldIndexId);
  _newIndexId = std::move(payload.newIndexId);
  _creator = std::move(payload.creator);
  _timeCreated = std::move(payload.timeCreated);
  _newDefinition = std::make_shared<Builder>(std::move(payload.newDefinition));
}

ReplaceIndex::~ReplaceIndex() = default;

void ReplaceIndex::run(bool& aborts) { runHelper("", "", aborts); }

bool ReplaceIndex::create(std::shared_ptr<Builder> envelope) {
  LOG_TOPIC("12fa2", INFO, Logger::SUPERVISION)
      << "Todo: ReplaceIndex on " << _database << "/" << _collection
      << ", old=" << _oldIndexId << ", new=" << _newIndexId;

  if (_newDefinition == nullptr) {
    LOG_TOPIC("12fa3", ERR, Logger::SUPERVISION)
        << "ReplaceIndex " << _jobId
        << " missing new index definition; refusing to create.";
    return false;
  }

  bool const selfCreate = (envelope == nullptr);
  _jb = selfCreate ? std::make_shared<Builder>() : envelope;

  if (selfCreate) {
    _jb->openArray();
    _jb->openObject();
  }

  auto payload = ReplaceIndexPayload::make(_database, _collection, _oldIndexId,
                                           _newIndexId, _jobId, _creator,
                                           _newDefinition->slice());

  _jb->add(VPackValue(toDoPrefix + _jobId));
  velocypack::serialize(*_jb, payload);

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
  LOG_TOPIC("12fa4", INFO, Logger::SUPERVISION)
      << "Failed to insert ReplaceIndex job " << _jobId;
  return false;
}

bool ReplaceIndex::start(bool& /*aborts*/) {
  // Validate the collection still exists.
  std::string const collKey = planColPrefix + _database + "/" + _collection;
  if (!_snapshot.has(collKey)) {
    finish("", "", false,
           "collection has been dropped before ReplaceIndex started");
    return false;
  }

  // Load the current indexes array from Plan.
  std::string const indexesKey = collKey + "/indexes";
  auto plannedBuilder = _snapshot.hasAsBuilder(indexesKey);
  if (!plannedBuilder) {
    finish("", "", false,
           "indexes array missing from Plan; nothing to replace");
    return false;
  }
  Slice planned = plannedBuilder->slice();
  if (!planned.isArray()) {
    finish("", "", false, "Plan indexes is not an array");
    return false;
  }

  // Confirm the old index is still present and that no other ReplaceIndex
  // is in flight for it (no existing entry with replaces=_oldIndexId).
  bool oldFound = false;
  bool conflict = false;
  for (auto entry : VPackArrayIterator(planned)) {
    auto idSlice = entry.get(StaticStrings::IndexId);
    if (idSlice.isString() && idSlice.stringView() == _oldIndexId) {
      oldFound = true;
    }
    auto replSlice = entry.get(kReplacesField);
    if (replSlice.isString() && replSlice.stringView() == _oldIndexId) {
      conflict = true;
    }
  }
  if (!oldFound) {
    finish("", "", false, "old index no longer present in Plan");
    return false;
  }
  if (conflict) {
    finish("", "", false,
           "another ReplaceIndex is already in flight for this index");
    return false;
  }

  // Build the new indexes array: existing entries + the new shadow entry.
  Builder newIndexes;
  {
    VPackArrayBuilder arr(&newIndexes);
    for (auto entry : VPackArrayIterator(planned)) {
      newIndexes.add(entry);
    }
    // Append the new definition (the REST handler already merged + assigned
    // ids and stamped `replaces`).
    newIndexes.add(_newDefinition->slice());
  }

  // Build the ToDo job entry to copy into Pending.
  Builder todo;
  {
    VPackArrayBuilder guard(&todo);
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo) {
        LOG_TOPIC("12fa5", INFO, Logger::SUPERVISION)
            << "Failed to read ToDo entry for ReplaceIndex " << _jobId;
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        LOG_TOPIC("12fa6", WARN, Logger::SUPERVISION) << e.what();
        return false;
      }
    }
  }

  auto res = submitPlanIndexesUpdate(
      _agent, indexesKey, planned, newIndexes.slice(),
      [this, &todo](Builder& trx) {
        addPutJobIntoSomewhere(trx, "Pending", todo.slice()[0]);
        addRemoveJobFromSomewhere(trx, "ToDo", _jobId);
      },
      [this](Builder& trx) {
        addPreconditionCollectionStillThere(trx, _database, _collection);
      });
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    _status = PENDING;
    LOG_TOPIC("12fa7", INFO, Logger::SUPERVISION)
        << "ReplaceIndex " << _jobId << " moved ToDo→Pending: " << _database
        << "/" << _collection << ", old=" << _oldIndexId
        << ", new=" << _newIndexId;
    return true;
  }

  LOG_TOPIC("12fa8", INFO, Logger::SUPERVISION)
      << "ReplaceIndex " << _jobId << " start precondition failed";
  return false;
}

JOB_STATUS ReplaceIndex::status() {
  if (_status != PENDING) {
    return _status;
  }

  // Walk shards from Plan; check Current for each shard's view of the new
  // index entry.
  std::string const shardsKey =
      planColPrefix + _database + "/" + _collection + "/shards";
  auto shardsNode = _snapshot.get(shardsKey);
  if (shardsNode == nullptr) {
    abort("collection dropped while ReplaceIndex was pending");
    return _status;
  }
  auto shardChildren = shardsNode->hasAsChildren("");
  if (!shardChildren) {
    return PENDING;  // try again next round
  }

  bool anyPending = false;
  bool anyFailed = false;
  std::string failureReason;
  for (auto const& [shardId, _] : *shardChildren) {
    std::string curKey = curColPrefix + _database + "/" + _collection + "/" +
                         shardId + "/indexes";
    auto curNode = _snapshot.get(curKey);
    if (curNode == nullptr) {
      anyPending = true;
      continue;
    }
    auto curBuilder = curNode->toBuilder();
    auto outcome = classifyShard(curBuilder.slice(), _newIndexId);
    switch (outcome) {
      case ShardOutcome::kPending:
      case ShardOutcome::kNotFound:
        anyPending = true;
        break;
      case ShardOutcome::kFailed:
        anyFailed = true;
        if (failureReason.empty()) {
          failureReason = "shard " + std::string{shardId} +
                          " reported a transient build error";
        }
        break;
      case ShardOutcome::kDone:
        break;
    }
  }

  if (anyFailed) {
    abort(failureReason.empty()
              ? "transient shadow-build failure on at least one shard"
              : failureReason);
    return _status;
  }
  if (anyPending) {
    return PENDING;
  }

  // Commit: drop the old entry, strip the `replaces` marker from the new
  // one, move job Pending→Finished.
  std::string const indexesKey =
      planColPrefix + _database + "/" + _collection + "/indexes";
  auto plannedBuilder = _snapshot.hasAsBuilder(indexesKey);
  if (!plannedBuilder) {
    finish("", "", false, "Plan indexes vanished while committing replace");
    return _status;
  }
  Slice planned = plannedBuilder->slice();

  Builder newIndexes;
  {
    VPackArrayBuilder arr(&newIndexes);
    for (auto entry : VPackArrayIterator(planned)) {
      auto idSlice = entry.get(StaticStrings::IndexId);
      if (idSlice.isString() && idSlice.stringView() == _oldIndexId) {
        continue;
      }
      if (idSlice.isString() && idSlice.stringView() == _newIndexId) {
        VPackObjectBuilder ob(&newIndexes);
        for (auto pair : VPackObjectIterator(entry)) {
          if (pair.key.stringView() == kReplacesField) {
            continue;
          }
          newIndexes.add(pair.key.stringView(), pair.value);
        }
        continue;
      }
      newIndexes.add(entry);
    }
  }

  auto res =
      submitPlanIndexesUpdate(_agent, indexesKey, planned, newIndexes.slice());
  if (!res.accepted || res.indices.empty() || !res.indices[0]) {
    LOG_TOPIC("12fa9", DEBUG, Logger::SUPERVISION)
        << "ReplaceIndex " << _jobId
        << " commit precondition failed; will retry next round";
    return PENDING;
  }

  finish("", "", true, "");
  return _status;
}

Result ReplaceIndex::abort(std::string const& reason) {
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting ReplaceIndex job; already terminal");
  }

  if (_status == TODO) {
    finish("", "", false, "job aborted: " + reason);
    return Result{};
  }

  // PENDING: drop the shadow entry; the old entry stays and keeps serving.
  std::string const indexesKey =
      planColPrefix + _database + "/" + _collection + "/indexes";
  auto plannedBuilder = _snapshot.hasAsBuilder(indexesKey);
  if (!plannedBuilder) {
    finish("", "", false, "job aborted (Plan indexes missing): " + reason);
    return Result{};
  }
  Slice planned = plannedBuilder->slice();

  Builder newIndexes;
  {
    VPackArrayBuilder arr(&newIndexes);
    for (auto entry : VPackArrayIterator(planned)) {
      auto idSlice = entry.get(StaticStrings::IndexId);
      if (idSlice.isString() && idSlice.stringView() == _newIndexId) {
        continue;
      }
      newIndexes.add(entry);
    }
  }

  auto res =
      submitPlanIndexesUpdate(_agent, indexesKey, planned, newIndexes.slice());
  if (!res.accepted || res.indices.empty() || !res.indices[0]) {
    LOG_TOPIC("12faa", INFO, Logger::SUPERVISION)
        << "ReplaceIndex " << _jobId
        << " abort precondition failed; will retry next round";
    return Result(TRI_ERROR_HTTP_PRECONDITION_FAILED,
                  "abort precondition failed");
  }

  finish("", "", false, "job aborted: " + reason);
  return Result{};
}

}  // namespace arangodb::consensus
