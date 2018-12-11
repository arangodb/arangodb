////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Syncer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace {

/// @brief extract the collection id from VelocyPack
TRI_voc_cid_t getCid(arangodb::velocypack::Slice const& slice) {
  return arangodb::basics::VelocyPackHelper::extractIdValue(slice);
}

/// @brief extract the collection name from VelocyPack
std::string getCName(arangodb::velocypack::Slice const& slice) {
  return arangodb::basics::VelocyPackHelper::getStringValue(slice, "cname", "");
}

/// @brief extract the collection by either id or name, may return nullptr!
std::shared_ptr<arangodb::LogicalCollection> getCollectionByIdOrName(
    TRI_vocbase_t& vocbase, TRI_voc_cid_t cid, std::string const& name) {
  auto idCol = vocbase.lookupCollection(cid);
  std::shared_ptr<arangodb::LogicalCollection> nameCol;

  if (!name.empty()) {
    // try looking up the collection by name then
    nameCol = vocbase.lookupCollection(name);
  }

  if (idCol != nullptr && nameCol != nullptr) {
    if (idCol->id() == nameCol->id()) {
      // found collection by id and name, and both are identical!
      return idCol;
    }

    // found different collections by id and name
    TRI_ASSERT(!name.empty());

    if (name[0] == '_') {
      // system collection. always return collection by name when in doubt
      return nameCol;
    }

    // no system collection. still prefer local collection
    return nameCol;
  }

  if (nameCol != nullptr) {
    TRI_ASSERT(idCol == nullptr);
    return nameCol;
  }

  // may be nullptr
  return idCol;
}

/// @brief apply the data from a collection dump or the continuous log
arangodb::Result applyCollectionDumpMarkerInternal(
    arangodb::Syncer::SyncerState const& state,
    arangodb::transaction::Methods& trx, arangodb::LogicalCollection* coll,
    arangodb::TRI_replication_operation_e type, VPackSlice const& slice) {
  using arangodb::OperationOptions;
  using arangodb::OperationResult;
  using arangodb::Result;

  if (type ==
      arangodb::TRI_replication_operation_e::REPLICATION_MARKER_DOCUMENT) {
    // {"type":2300,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    if (!state.leaderId.empty()) {
      options.isSynchronousReplicationFrom = state.leaderId;
    }
    // we want the conflicting other key returned in case of unique constraint
    // violation
    options.indexOperationMode = arangodb::Index::OperationMode::internal;

    try {
      // try insert first
      OperationResult opRes = trx.insert(coll->name(), slice, options);

      if (opRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
        bool useReplace = true;

        // conflicting key is contained in opRes.errorMessage() now
        VPackSlice keySlice = slice.get(arangodb::StaticStrings::KeyString);

        if (keySlice.isString()) {
          // let's check if the key we have got is the same as the one
          // that we would like to insert
          if (keySlice.copyString() != opRes.errorMessage()) {
            // different key
            VPackBuilder tmp;
            tmp.add(VPackValue(opRes.errorMessage()));

            opRes = trx.remove(coll->name(), tmp.slice(), options);
            if (opRes.ok() || !opRes.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
              useReplace = false;
            }
          }
        }

        options.indexOperationMode = arangodb::Index::OperationMode::normal;

        if (useReplace) {
          // perform a replace
          opRes = trx.replace(coll->name(), slice, options);
        } else {
          // perform a re-insert
          opRes = trx.insert(coll->name(), slice, options);
        }
      }

      return Result(opRes.result);
    } catch (arangodb::basics::Exception const& ex) {
      return Result(ex.code(),
                    std::string("document insert/replace operation failed: ") +
                        ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("document insert/replace operation failed: ") +
                        ex.what());
    } catch (...) {
      return Result(
          TRI_ERROR_INTERNAL,
          std::string(
              "document insert/replace operation failed: unknown exception"));
    }
  } else if (type == arangodb::TRI_replication_operation_e::REPLICATION_MARKER_REMOVE) {
    // {"type":2302,"key":"592063"}

    try {
      OperationOptions options;
      options.silent = true;
      options.ignoreRevs = true;
      if (!state.leaderId.empty()) {
        options.isSynchronousReplicationFrom = state.leaderId;
      }
      OperationResult opRes = trx.remove(coll->name(), slice, options);

      if (opRes.ok() || opRes.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // ignore document not found errors
        return Result();
      }

      return Result(opRes.result);
    } catch (arangodb::basics::Exception const& ex) {
      return Result(
          ex.code(),
          std::string("document remove operation failed: ") + ex.what());
    } catch (std::exception const& ex) {
      return Result(
          TRI_ERROR_INTERNAL,
          std::string("document remove operation failed: ") + ex.what());
    } catch (...) {
      return Result(
          TRI_ERROR_INTERNAL,
          std::string("document remove operation failed: unknown exception"));
    }
  }

  return Result(TRI_ERROR_REPLICATION_UNEXPECTED_MARKER,
                std::string("unexpected marker type ") +
                    arangodb::basics::StringUtils::itoa(type));
}

}  // namespace

namespace arangodb {

Syncer::JobSynchronizer::JobSynchronizer(std::shared_ptr<Syncer const> const& syncer)
    : _syncer(syncer),
      _gotResponse(false),
      _jobsInFlight(0) {}


Syncer::JobSynchronizer::~JobSynchronizer() {
  // signal that we have got something
  try {
    gotResponse(Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED));
  } catch (...) {
    // must not throw from here
  }

  // wait until all posted jobs have been completed/canceled
  while (hasJobInFlight()) {
    std::this_thread::sleep_for(std::chrono::microseconds(20000));
    std::this_thread::yield();
  }
}

/// @brief will be called whenever a response for the job comes in
void Syncer::JobSynchronizer::gotResponse(std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response) noexcept {
  CONDITION_LOCKER(guard, _condition);
  _res.reset(); // no error!
  _response = std::move(response);
  _gotResponse = true;

  guard.signal();
}

/// @brief will be called whenever an error occurred
/// expects "res" to be an error!
void Syncer::JobSynchronizer::gotResponse(arangodb::Result&& res) noexcept {
  TRI_ASSERT(res.fail());

  CONDITION_LOCKER(guard, _condition);
  _res = std::move(res);
  _response.reset();
  _gotResponse = true;

  guard.signal();
}

/// @brief the calling Syncer will call and block inside this function until
/// there is a response or the syncer/server is shut down
Result Syncer::JobSynchronizer::waitForResponse(std::unique_ptr<arangodb::httpclient::SimpleHttpResult>& response) {
  while (true) {
    {
      CONDITION_LOCKER(guard, _condition);

      if (!_gotResponse) {
        guard.wait(1 * 1000 * 1000);
      }

      // check again, _gotResponse may have changed
      if (_gotResponse) {
        _gotResponse = false;
        response = std::move(_response);
        return _res;
      }
    }

    if (_syncer->isAborted()) {
      // clear result response
      response.reset();

      CONDITION_LOCKER(guard, _condition);
      _gotResponse = false;
      _response.reset();
      _res.reset();

      // will result in returning TRI_ERROR_REPLICATION_APPLIER_STOPPED
      break;
    }
  }

  return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
}

void Syncer::JobSynchronizer::request(std::function<void()> const& cb) {
  // by indicating that we have posted an async job, the caller
  // will block on exit until all posted jobs have finished
  if (!jobPosted()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  try {
    auto self = shared_from_this();
    SchedulerFeature::SCHEDULER->queue(RequestPriority::LOW, [this, self, cb]() {
      // whatever happens next, when we leave this here, we need to indicate
      // that there is no more posted job.
      // otherwise the calling thread may block forever waiting on the posted jobs
      // to finish
      auto guard = scopeGuard([this]() {
        jobDone();
      });

      cb();
      });
  } catch (...) {
    // will get here only if Scheduler::post threw
    jobDone();
  }
}

/// @brief notifies that a job was posted
/// returns false if job counter could not be increased (e.g. because
/// the syncer was stopped/aborted already)
bool Syncer::JobSynchronizer::jobPosted() {
  while (true) {
    CONDITION_LOCKER(guard, _condition);

    // _jobsInFlight should be 0 in almost all cases, however, there
    // is a small window in which the request has been processed already
    // (i.e. after waitForResponse() has returned and before jobDone()
    // has been called and has decreased _jobsInFlight). For this
    // particular case, we simply wait for _jobsInFlight to become 0 again
    if (_jobsInFlight == 0) {
      ++_jobsInFlight;
      return true;
    }

    if (_syncer->isAborted()) {
      // syncer already stopped... no need to carry on here
      return false;
    }
    guard.wait(10 * 1000);
  }
}

/// @brief notifies that a job was done
void Syncer::JobSynchronizer::jobDone() {
  CONDITION_LOCKER(guard, _condition);

  TRI_ASSERT(_jobsInFlight == 1);
  --_jobsInFlight;
  _condition.signal();
}

/// @brief checks if there are jobs in flight (can be 0 or 1 job only)
bool Syncer::JobSynchronizer::hasJobInFlight() const noexcept {
  CONDITION_LOCKER(guard, _condition);

  TRI_ASSERT(_jobsInFlight <= 1);
  return _jobsInFlight > 0;
}


Syncer::SyncerState::SyncerState(
    Syncer* syncer, ReplicationApplierConfiguration const& configuration)
    : applier{configuration},
      connection{syncer, configuration},
      master{configuration} {}

Syncer::Syncer(ReplicationApplierConfiguration const& configuration)
    : _state{this, configuration} {
  if (!ServerState::instance()->isSingleServer() && !ServerState::instance()->isDBServer()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "the replication functionality is supposed to be invoked only on a single server or DB server");
  }
  if (!_state.applier._database.empty()) {
    // use name from configuration
    _state.databaseName = _state.applier._database;
  }

  if (_state.applier._chunkSize == 0) {
    _state.applier._chunkSize = 2 * 1024 * 1024;  // default: 2 MB
  } else if (_state.applier._chunkSize < 16 * 1024) {
    _state.applier._chunkSize = 16 * 1024;
  }

  // get our own server-id
  _state.localServerId = ServerIdFeature::getId();
  _state.localServerIdString = basics::StringUtils::itoa(_state.localServerId);

  _state.master.endpoint = _state.applier._endpoint;
}

Syncer::~Syncer() {
  if (!_state.isChildSyncer) {
    _state.barrier.remove(_state.connection);
  }
}

/// @brief request location rewriter (injects database name)
std::string Syncer::rewriteLocation(void* data, std::string const& location) {
  Syncer* s = static_cast<Syncer*>(data);
  TRI_ASSERT(s != nullptr);
  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }
  TRI_ASSERT(!s->_state.databaseName.empty());
  if (location[0] == '/') {
    return "/_db/" + s->_state.databaseName + location;
  }
  return "/_db/" + s->_state.databaseName + "/" + location;
}

/// @brief steal the barrier id from the syncer
TRI_voc_tick_t Syncer::stealBarrier() {
  auto id = _state.barrier.id;
  _state.barrier.id = 0;
  _state.barrier.updateTime = 0;
  return id;
}

void Syncer::setAborted(bool value) { _state.connection.setAborted(value); }

bool Syncer::isAborted() const { return _state.connection.isAborted(); }

TRI_vocbase_t* Syncer::resolveVocbase(VPackSlice const& slice) {
  std::string name;
  if (slice.isObject()) {
    VPackSlice tmp;
    if ((tmp = slice.get("db")).isString()) {  // wal access protocol
      name = tmp.copyString();
    } else if ((tmp = slice.get("database")).isString()) {  // pre 3.3
      name = tmp.copyString();
    }
  } else if (slice.isString()) {
    name = slice.copyString();
  }

  if (name.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                                   "could not resolve vocbase id / name");
  }

  // will work with either names or id's
  auto const& it = _state.vocbases.find(name);

  if (it == _state.vocbases.end()) {
    // automatically checks for id in string
    TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->lookupDatabase(name);

    if (vocbase != nullptr) {
      _state.vocbases.emplace(std::piecewise_construct,
                              std::forward_as_tuple(name),
                              std::forward_as_tuple(*vocbase));
    } else {
      LOG_TOPIC(DEBUG, Logger::REPLICATION)
          << "could not find database '" << name << "'";
    }

    return vocbase;
  } else {
    return &(it->second.database());
  }
}

std::shared_ptr<LogicalCollection> Syncer::resolveCollection(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& slice) {
  // extract "cid"
  TRI_voc_cid_t cid = ::getCid(slice);

  if (!_state.master.simulate32Client() || cid == 0) {
    VPackSlice uuid;

    if ((uuid = slice.get("cuid")).isString()) {
      return vocbase.lookupCollectionByUuid(uuid.copyString());
    } else if ((uuid = slice.get("globallyUniqueId")).isString()) {
      return vocbase.lookupCollectionByUuid(uuid.copyString());
    }
  }

  if (cid == 0) {
    LOG_TOPIC(ERR, Logger::REPLICATION) << "Invalid replication response: Was unable to resolve"
    << " collection from marker: " << slice.toJson();
    return nullptr;
  }

  // extract optional "cname"
  std::string cname = ::getCName(slice);

  if (cname.empty()) {
    cname =
        arangodb::basics::VelocyPackHelper::getStringValue(slice, "name", "");
  }

  return ::getCollectionByIdOrName(vocbase, cid, cname);
}

Result Syncer::applyCollectionDumpMarker(transaction::Methods& trx,
                                         LogicalCollection* coll,
                                         TRI_replication_operation_e type,
                                         VPackSlice const& slice) {
  if (_state.applier._lockTimeoutRetries > 0) {
    decltype(_state.applier._lockTimeoutRetries) tries = 0;

    while (true) {
      Result res =
          ::applyCollectionDumpMarkerInternal(_state, trx, coll, type, slice);

      if (res.errorNumber() != TRI_ERROR_LOCK_TIMEOUT) {
        return res;
      }

      // lock timeout
      if (++tries > _state.applier._lockTimeoutRetries) {
        // timed out
        return res;
      }

      LOG_TOPIC(DEBUG, Logger::REPLICATION) << "got lock timeout while waiting for lock on collection '" << coll->name() << "', retrying...";
      std::this_thread::sleep_for(std::chrono::microseconds(50000));
      // retry
    }
  } else {
    return ::applyCollectionDumpMarkerInternal(_state, trx, coll, type, slice);
  }
}

/// @brief creates a collection, based on the VelocyPack provided
Result Syncer::createCollection(TRI_vocbase_t& vocbase,
                                arangodb::velocypack::Slice const& slice,
                                LogicalCollection** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection slice is no object");
  }

  std::string const name =
      basics::VelocyPackHelper::getStringValue(slice, "name", "");

  if (name.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "no name specified for collection");
  }

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(
      basics::VelocyPackHelper::getNumericValue<int>(slice, "type",
                                                     TRI_COL_TYPE_DOCUMENT));

  // resolve collection by uuid, name, cid (in that order of preference)
  auto col = resolveCollection(vocbase, slice);

  if (col != nullptr && col->type() == type &&
      (!_state.master.simulate32Client() || col->name() == name)) {
    // collection already exists. TODO: compare attributes
    return Result();
  }

  bool forceRemoveCid = false;
  if (col != nullptr && _state.master.simulate32Client()) {
    forceRemoveCid = true;
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "would have got a wrong collection!";
    // go on now and truncate or drop/re-create the collection
  }

  // conflicting collections need to be dropped from 3.3 onwards
  col = vocbase.lookupCollection(name);

  if (col != nullptr) {
    if (col->system()) {
      TRI_ASSERT(!_state.master.simulate32Client() ||
                 col->guid() == col->name());
      SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(vocbase),
        *col,
        AccessMode::Type::WRITE
      );
      trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
      trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
      Result res = trx.begin();

      if (!res.ok()) {
        return res;
      }

      OperationOptions opts;
      OperationResult opRes = trx.truncate(col->name(), opts);

      if (opRes.fail()) {
        return opRes.result;
      }

      return trx.finish(opRes.result);
    } else {
      vocbase.dropCollection(col->id(), false, -1.0);
    }
  }

  VPackSlice uuid = slice.get(StaticStrings::DataSourceGuid);
  // merge in "isSystem" attribute, doesn't matter if name does not start with
  // '_'
  VPackBuilder s;

  s.openObject();
  s.add("isSystem", VPackValue(true));

  if ((uuid.isString() && !_state.master.simulate32Client()) ||
      forceRemoveCid) {  // need to use cid for 3.2 master
    // if we received a globallyUniqueId from the remote, then we will always
    // use this id so we can discard the "cid" and "id" values for the
    // collection
    s.add(StaticStrings::DataSourceId, VPackSlice::nullSlice());
    s.add("cid", VPackSlice::nullSlice());
  }

  s.close();

  VPackBuilder merged =
      VPackCollection::merge(slice, s.slice(), /*mergeValues*/ true,
                             /*nullMeansRemove*/ true);

  // we need to remove every occurence of objectId as a key
  auto stripped = rocksutils::stripObjectIds(merged.slice());

  try {
    col = vocbase.createCollection(stripped.first);
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(!uuid.isString() || uuid.compareString(col->guid()) == 0);

  if (dst != nullptr) {
    *dst = col.get();
  }

  return Result();
}

/// @brief drops a collection, based on the VelocyPack provided
Result Syncer::dropCollection(VPackSlice const& slice, bool reportError) {
  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto* col = resolveCollection(*vocbase, slice).get();

  if (col == nullptr) {
    if (reportError) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    return Result();
  }

  return vocbase->dropCollection(col->id(), true, -1.0);
}

/// @brief creates an index, based on the VelocyPack provided
Result Syncer::createIndex(VPackSlice const& slice) {
  VPackSlice indexSlice = slice.get("index");
  if (!indexSlice.isObject()) {
    indexSlice = slice.get("data");
  }

  if (!indexSlice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "index slice is not an object");
  }

  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto col = resolveCollection(*vocbase, slice);
  if (col == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection for index");
  }

  VPackBuilder s;
  s.openObject();
  s.add("objectId", VPackSlice::nullSlice());
  s.close();
  VPackBuilder merged =
      VPackCollection::merge(indexSlice, s.slice(),
                             /*mergeValues*/ true, /*nullMeansRemove*/ true);
  
  try {
    auto physical = col->getPhysical();
    TRI_ASSERT(physical != nullptr);
    
    std::shared_ptr<arangodb::Index> idx;
    bool created = false;
    idx = physical->createIndex(merged.slice(), /*restore*/true, created);
    TRI_ASSERT(idx != nullptr);

  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(),
                  std::string("caught exception while creating index: ") + ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL,
                  std::string("caught exception while creating index: ") + ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL,
                  "caught unknown exception while creating index");
  }
  return Result();
}

Result Syncer::dropIndex(arangodb::velocypack::Slice const& slice) {
  auto cb = [&](VPackSlice const& slice) {
    std::string id;

    if (slice.hasKey("data")) {
      id =
          basics::VelocyPackHelper::getStringValue(slice.get("data"), "id", "");
    } else {
      id = basics::VelocyPackHelper::getStringValue(slice, "id", "");
    }

    if (id.empty()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "id not found in index drop slice");
    }

    TRI_idx_iid_t const iid = basics::StringUtils::uint64(id);
    TRI_vocbase_t* vocbase = resolveVocbase(slice);

    if (vocbase == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    auto col = resolveCollection(*vocbase, slice);

    if (!col) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    try {
      CollectionGuard guard(vocbase, col);
      bool result = guard.collection()->dropIndex(iid);

      if (!result) {
        return Result();  // TODO: why do we ignore failures here?
      }

      return Result();
    } catch (arangodb::basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL);
    }
  };

  Result r = cb(slice);

  if (r.fail() && (r.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) ||
                   r.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND))) {
    // if dropping an index for a non-existing database or collection fails,
    // this is not a real problem
    return Result();
  }

  return r;
}

/// @brief creates a view, based on the VelocyPack provided
Result Syncer::createView(TRI_vocbase_t& vocbase,
                          arangodb::velocypack::Slice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection slice is no object");
  }

  VPackSlice nameSlice = slice.get(StaticStrings::DataSourceName);

  if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "no name specified for view");
  }

  VPackSlice guidSlice = slice.get(StaticStrings::DataSourceGuid);

  if (!guidSlice.isString() || guidSlice.getStringLength() == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "no guid specified for view");
  }

  VPackSlice typeSlice = slice.get(StaticStrings::DataSourceType);

  if (!typeSlice.isString() || typeSlice.getStringLength() == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "no type specified for view");
  }

  auto view = vocbase.lookupView(guidSlice.copyString());

  if (view) { // identical view already exists
    VPackSlice nameSlice = slice.get(StaticStrings::DataSourceName);

    if (nameSlice.isString() && !nameSlice.isEqualString(view->name())) {
      auto res = view->rename(nameSlice.copyString());

      if (!res.ok()) {
        return res;
      }
    }

    return view->properties(slice, false); // always a full-update
  }

  // check for name conflicts
  view = vocbase.lookupView(nameSlice.copyString());
  if (view) { // resolve name conflict by deleting existing
    Result res = view->drop();
    if (res.fail()) {
      return res;
    }
  }

  VPackBuilder s;

  s.openObject();
  s.add("id", VPackSlice::nullSlice());
  s.close();

  VPackBuilder merged =
  VPackCollection::merge(slice, s.slice(), /*mergeValues*/ true,
                         /*nullMeansRemove*/ true);

  try {
    LogicalView::ptr view; // ignore result
    return LogicalView::create(view, vocbase, merged.slice());
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}

/// @brief drops a view, based on the VelocyPack provided
Result Syncer::dropView(arangodb::velocypack::Slice const& slice,
                        bool reportError) {
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  VPackSlice guidSlice = slice.get("globallyUniqueId");
  if (guidSlice.isNone()) {
    guidSlice = slice.get("cuid");
  }
  if (!guidSlice.isString() || guidSlice.getStringLength() == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "no guid specified for view");
  }

  try {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());
    auto view = vocbase->lookupView(guidSlice.copyString());
    if (view) { // prevent dropping of system views ?
      return view->drop();
    }
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  return Result();
}

void Syncer::reloadUsers() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  auth::UserManager* um = af->userManager();
  if (um != nullptr) {
    um->triggerLocalReload();
  }
}

}  // namespace arangodb
