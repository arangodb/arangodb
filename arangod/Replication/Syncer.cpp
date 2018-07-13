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
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
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
  }

  else if (type ==
           arangodb::TRI_replication_operation_e::REPLICATION_MARKER_REMOVE) {
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

Syncer::SyncerState::SyncerState(
    Syncer* syncer, ReplicationApplierConfiguration const& configuration)
    : applier{configuration},
      connection{syncer, configuration},
      master{configuration} {}

Syncer::Syncer(ReplicationApplierConfiguration const& configuration)
    : _state{this, configuration} {
  TRI_ASSERT(ServerState::instance()->isSingleServer() ||
             ServerState::instance()->isDBServer());
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
  try {
    sendRemoveBarrier();
  } catch (...) {
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

/// @brief send a "remove barrier" command
Result Syncer::sendRemoveBarrier() {
  if (_state.isChildSyncer || _state.barrier.id == 0) {
    return Result();
  }

  try {
    std::string const url = replutils::ReplicationUrl + "/barrier/" +
                            basics::StringUtils::itoa(_state.barrier.id);

    // send request
    std::unique_ptr<httpclient::SimpleHttpResult> response(
        _state.connection.client->retryRequest(rest::RequestType::DELETE_REQ,
                                               url, nullptr, 0));

    if (replutils::hasFailed(response.get())) {
      return replutils::buildHttpError(response.get(), url, _state.connection);
    }
    _state.barrier.id = 0;
    _state.barrier.updateTime = 0;
    return Result();
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
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
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << TRI_errno_string(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
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
  auto* col = resolveCollection(vocbase, slice).get();

  if (col != nullptr && col->type() == type &&
      (!_state.master.simulate32Client() || col->name() == name)) {
    // collection already exists. TODO: compare attributes
    return Result();
  }

  bool forceRemoveCid = false;
  if (col != nullptr && _state.master.simulate32Client()) {
    forceRemoveCid = true;
    LOG_TOPIC(TRACE, Logger::REPLICATION)
        << "would have got a wrong collection!";
    // go on now and truncate or drop/re-create the collection
  }

  // conflicting collections need to be dropped from 3.3 onwards
  col = vocbase.lookupCollection(name).get();

  if (col != nullptr) {
    if (col->system()) {
      TRI_ASSERT(!_state.master.simulate32Client() ||
                 col->guid() == col->name());
      SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(vocbase),
        col,
        AccessMode::Type::WRITE
      );
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

  VPackSlice uuid = slice.get("globallyUniqueId");
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
    s.add("id", VPackSlice::nullSlice());
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
    *dst = col;
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
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(*vocbase),
      col.get(),
      AccessMode::Type::WRITE
    );
    Result res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    auto physical = trx.documentCollection()->getPhysical();
    TRI_ASSERT(physical != nullptr);
    std::shared_ptr<arangodb::Index> idx;
    res = physical->restoreIndex(&trx, merged.slice(), idx);
    res = trx.finish(res);

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    return Result(
        ex.code(),
        std::string("caught exception while creating index: ") + ex.what());
  } catch (std::exception const& ex) {
    return Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while creating index: ") + ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL,
                  "caught unknown exception while creating index");
  }
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

    auto* col = resolveCollection(*vocbase, slice).get();

    if (col == nullptr) {
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

void Syncer::reloadUsers() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  auth::UserManager* um = af->userManager();
  if (um != nullptr) {
    um->outdate();
  }
}

}  // namespace arangodb
