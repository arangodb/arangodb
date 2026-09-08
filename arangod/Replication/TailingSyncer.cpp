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
///
////////////////////////////////////////////////////////////////////////////////

#include "TailingSyncer.h"
#include "Metrics/MetricsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Replication/InitialSyncer.h"
#include "Replication/ReplicationTransaction.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Utils/CollectionGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

namespace {

constexpr std::string_view cnameRef("cname");
constexpr std::string_view dataRef("data");
constexpr std::string_view tickRef("tick");
constexpr std::string_view dbRef("db");

}  // namespace

/// @brief base url of the replication API
std::string const TailingSyncer::WalAccessUrl = "/_api/wal";

TailingSyncer::TailingSyncer(ReplicationSyncConfiguration const& configuration)
    : Syncer(configuration),
      _initialTick(0),
      _usersModified(false),
      _ignoreRenameCreateDrop(false),
      _ignoreDatabaseMarkers(true),
      _stats(_state.config._server
                 .getFeature<arangodb::ReplicationMetricsFeature>(),
             true) {}

TailingSyncer::~TailingSyncer() { abortOngoingTransactions(); }

/// @brief decide based on _state.leader which api to use
std::string TailingSyncer::tailingBaseUrl(std::string const& cc) {
  return absl::StrCat(TailingSyncer::WalAccessUrl, "/", cc, "?");
}

/// @brief set the sync progress
void TailingSyncer::setProgress(std::string const& msg) {
  if (_state.config._verbose) {
    LOG_TOPIC("cb1ba", INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC("452fc", DEBUG, Logger::REPLICATION) << msg;
  }
}

/// @brief abort all ongoing transactions
void TailingSyncer::abortOngoingTransactions() noexcept {
  try {
    // abort all running transactions
    _ongoingTransactions.clear();
  } catch (...) {
    // ignore errors here
  }
}

/// @brief abort all ongoing transactions for a specific database
void TailingSyncer::abortOngoingTransactions(std::string const& dbName) {
  for (auto it = _ongoingTransactions.begin(); it != _ongoingTransactions.end();
       /* no hoisting */) {
    auto& trx = (*it).second;
    if (trx != nullptr && trx->vocbase().name() == dbName) {
      LOG_TOPIC("c1ec8", TRACE, Logger::REPLICATION)
          << "aborting open transaction for db " << dbName;
      it = _ongoingTransactions.erase(it);
    } else {
      ++it;
    }
  }
}

/// @brief count all ongoing transactions for a specific database
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
size_t TailingSyncer::countOngoingTransactions(VPackSlice slice) const {
  size_t result = 0;

  TRI_ASSERT(slice.isObject());
  VPackSlice nameSlice = slice.get(::dbRef);

  if (nameSlice.isString()) {
    for (auto const& it : _ongoingTransactions) {
      auto const& trx = it.second;
      if (trx != nullptr && nameSlice.stringView() == trx->vocbase().name()) {
        ++result;
      }
    }
  }

  return result;
}
#endif

/// @brief whether or not the are multiple ongoing transactions for one
/// database
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool TailingSyncer::hasMultipleOngoingTransactions() const {
  std::unordered_set<TRI_voc_tick_t> found;
  for (auto const& it : _ongoingTransactions) {
    auto const& trx = it.second;
    if (trx != nullptr && !found.emplace(trx->vocbase().id()).second) {
      // found a duplicate
      return true;
    }
  }
  return false;
}
#endif

/// @brief whether or not a marker should be skipped
bool TailingSyncer::skipMarker(TRI_voc_tick_t firstRegularTick,
                               VPackSlice slice,
                               TRI_voc_tick_t actualMarkerTick,
                               TRI_replication_operation_e type) {
  TRI_ASSERT(slice.isObject());

  bool tooOld = (actualMarkerTick < firstRegularTick);

  if (tooOld) {
    // handle marker type

    if (type == REPLICATION_MARKER_DOCUMENT ||
        type == REPLICATION_MARKER_REMOVE ||
        type == REPLICATION_TRANSACTION_START ||
        type == REPLICATION_TRANSACTION_ABORT ||
        type == REPLICATION_TRANSACTION_COMMIT) {
      // read "tid" entry from marker
      VPackSlice tidSlice = slice.get("tid");
      if (tidSlice.isString() && tidSlice.getStringLength() > 0) {
        VPackValueLength len;
        char const* str = tidSlice.getStringUnchecked(len);
        TransactionId tid{
            NumberUtils::atoi_zero<TransactionId::BaseType>(str, str + len)};

        if (tid.isSet() &&
            _ongoingTransactions.find(tid) != _ongoingTransactions.end()) {
          // must still use this marker as it belongs to a transaction we need
          // to finish
          tooOld = false;
        }
      }
    }

    if (tooOld) {
      return true;
    }
  }

  // the transient sync config is just used for one shard / collection
  if (_state.config._restrictCollections.empty()) {
    return false;
  }

  if (_state.config._restrictType ==
          ReplicationSyncConfiguration::RestrictType::None &&
      _state.config._includeSystem) {
    return false;
  }

  VPackSlice name = slice.get(::cnameRef);
  if (name.isString()) {
    return isExcludedCollection(name.copyString());
  }

  // call virtual method
  return skipMarker(slice);
}

/// @brief whether or not a collection should be excluded
bool TailingSyncer::isExcludedCollection(
    std::string const& collectionName) const {
  if (collectionName.starts_with('_') && !_state.config._includeSystem) {
    // system collection
    return true;
  }

  auto const it = _state.config._restrictCollections.find(collectionName);

  bool found = (it != _state.config._restrictCollections.end());

  if (_state.config._restrictType ==
          ReplicationSyncConfiguration::RestrictType::Include &&
      !found) {
    // collection should not be included
    return true;
  } else if (_state.config._restrictType ==
                 ReplicationSyncConfiguration::RestrictType::Exclude &&
             found) {
    // collection should be excluded
    return true;
  }

  if (TRI_ExcludeCollectionReplication(collectionName, /*includeSystem*/ true,
                                       _state.config._includeFoxxQueues)) {
    return true;
  }

  return false;
}

/// @brief process db create or drop markers
Result TailingSyncer::processDBMarker(TRI_replication_operation_e type,
                                      velocypack::Slice const& slice) {
  TRI_ASSERT(!_ignoreDatabaseMarkers);

  // the new wal access protocol contains database names
  VPackSlice const nameSlice = slice.get(::dbRef);
  if (!nameSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        "create/drop database marker did not contain name");
  }
  std::string name = nameSlice.copyString();
  if (name.empty() || (name[0] >= '0' && name[0] <= '9')) {
    LOG_TOPIC("e9bdc", ERR, Logger::REPLICATION)
        << "invalid database name in log";
    return {TRI_ERROR_ARANGO_ILLEGAL_NAME,
            "illegal name: database named invalid"};
  }

  if (!_state.config._server.hasFeature<arangodb::SystemDatabaseFeature>()) {
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  auto& sysDbFeature =
      _state.config._server.getFeature<arangodb::SystemDatabaseFeature>();

  if (type == REPLICATION_DATABASE_CREATE) {
    VPackSlice const data = slice.get("data");

    if (!data.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_REPLICATION_INVALID_RESPONSE,
          "create database marker did not contain data");
    }
    TRI_ASSERT(
        basics::VelocyPackHelper::equal(data.get("name"), nameSlice, false));

    if (name != StaticStrings::SystemDatabase &&
        sysDbFeature.server().getFeature<DatabaseFeature>().existsDatabase(
            name)) {
      LOG_TOPIC("0a3a4", WARN, Logger::REPLICATION)
          << "seeing database creation marker "
          << "for an already existing db. Dropping db...";

      auto system = sysDbFeature.use();
      TRI_ASSERT(system.get());
      auto res =
          methods::Databases::drop(ExecContext::current(), system.get(), name);

      if (res.fail()) {
        LOG_TOPIC("e8595", ERR, Logger::REPLICATION) << res.errorMessage();
        return res;
      }
    }

    auto& server = _state.config._server;
    auto& engine = server.getFeature<DatabaseFeature>().engine();
    VPackSlice users = VPackSlice::emptyArraySlice();
    Result res =
        methods::Databases::create(server, engine, ExecContext::current(), name,
                                   users, VPackSlice::emptyObjectSlice());

    return res;
  } else if (type == REPLICATION_DATABASE_DROP) {
    if (name != StaticStrings::SystemDatabase &&
        sysDbFeature.server().getFeature<DatabaseFeature>().existsDatabase(
            name)) {
      // abort all ongoing transactions for the database to be dropped
      abortOngoingTransactions(name);

      auto system = sysDbFeature.use();
      TRI_ASSERT(system.get());
      // delete from cache by name
      _state.vocbases.erase(name);

      auto res =
          methods::Databases::drop(ExecContext::current(), system.get(), name);

      if (res.fail()) {
        LOG_TOPIC("21b6a", ERR, Logger::REPLICATION) << res.errorMessage();
      }

      return res;
    }

    return TRI_ERROR_NO_ERROR;  // ignoring because it's idempotent
  }

  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL);  // unreachable
}

/// @brief process a document operation, based on the VelocyPack provided
Result TailingSyncer::processDocument(TRI_replication_operation_e type,
                                      VPackSlice const& slice) {
  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto coll = resolveCollection(*vocbase, slice);

  if (coll == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  bool const isSystem = coll->system();
  bool const isUsers = coll->name() == StaticStrings::UsersCollection;
  bool const isAnalyzers = coll->name() == StaticStrings::AnalyzersCollection;

  // extract "data"
  VPackSlice const data = slice.get(::dataRef);

  if (!data.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "invalid document format");
  }

  // extract "key"
  VPackSlice const key = data.get(StaticStrings::KeyString);

  if (!key.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "invalid document key format");
  }

  // extract "rev"
  VPackSlice const rev = data.get(StaticStrings::RevString);

  if (!rev.isNone() && !rev.isString()) {
    // _rev is an optional attribute
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "invalid document revision format");
  }

  // extract "tid"
  std::string_view const transactionId =
      VelocyPackHelper::getStringView(slice, "tid", std::string_view());
  TransactionId tid = TransactionId::none();
  if (!transactionId.empty()) {
    // operation is part of a transaction
    tid = TransactionId{NumberUtils::atoi_zero<TransactionId::BaseType>(
        transactionId.data(), transactionId.data() + transactionId.size())};
  }

  // in case this is a removal we need to build our marker
  VPackSlice applySlice = data;
  if (type == REPLICATION_MARKER_REMOVE) {
    _documentBuilder.clear();
    _documentBuilder.openObject(true);
    _documentBuilder.add(StaticStrings::KeyString, key);
    if (rev.isString()) {
      // _rev is an optional attribute
      _documentBuilder.add(StaticStrings::RevString, rev);
    }
    _documentBuilder.close();
    applySlice = _documentBuilder.slice();
  }

  if (tid.isSet()) {  // part of a transaction
    auto it = _ongoingTransactions.find(tid);

    if (it == _ongoingTransactions.end()) {
      return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION,
                    absl::StrCat("unexpected transaction ", tid.id()));
    }

    std::unique_ptr<ReplicationTransaction>& trx = (*it).second;

    if (trx == nullptr) {
      return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION,
                    absl::StrCat("unexpected transaction ", tid.id()));
    }

    trx->addCollectionAtRuntime(coll->id(), coll->name(),
                                AccessMode::Type::EXCLUSIVE)
        .waitAndGet();
    std::string conflictingDocumentKey;
    Result r = applyCollectionDumpMarker(*trx, coll.get(), type, applySlice,
                                         conflictingDocumentKey);
    TRI_ASSERT(!r.is(TRI_ERROR_ARANGO_TRY_AGAIN));

    if (r.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) && isSystem) {
      // ignore unique constraint violations for system collections
      r.reset();
    }
    if (r.ok()) {
      if (isUsers) {
        _usersModified = true;
      } else if (isAnalyzers) {
        _analyzersModified.insert(vocbase);
      }
    }

    return r;  // done
  }

  // standalone operation

  // this variable will store the key of a conflicting document we will have to
  // remove first it is initially empty, and may be populated by a failing
  // operation
  std::string conflictDocumentKey;

  // normally we will go into this while loop just once. only in the very
  // exceptional case that there is a unique constraint violation in one of the
  // secondary indexes we will get into the while loop a second time
  int tries = 0;
  while (tries++ < 10) {
    if (!conflictDocumentKey.empty()) {
      // a rather exceptional case in which we have to remove a conflicting
      // document, which is conflicting with the to-be-inserted document in one
      // the unique secondary indexes

      // intentionally ignore the return code here, as the operation will be
      // followed by yet another insert/replace
      removeSingleDocument(coll.get(), conflictDocumentKey);
      conflictDocumentKey.clear();
    }

    auto operationOrigin =
        transaction::OperationOriginInternal{"applying replication change"};

    // update the apply tick for all standalone operations
    SingleCollectionTransaction trx(
        transaction::StandaloneContext::create(*vocbase, operationOrigin),
        *coll, AccessMode::Type::EXCLUSIVE);

    // we will always check if the target document already exists and then
    // either carry out an insert or a replace. so we will be carrying out
    // either a read-then-insert or a read-then-replace operation, which is a
    // single write operation.
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    Result res = trx.begin();

    // fix error handling here when function returns result
    if (!res.ok()) {
      return Result(res.errorNumber(),
                    absl::StrCat("unable to create replication transaction: ",
                                 res.errorMessage()));
    }

    res = applyCollectionDumpMarker(trx, coll.get(), type, applySlice,
                                    conflictDocumentKey);

    TRI_ASSERT(res.is(TRI_ERROR_ARANGO_TRY_AGAIN) ==
               !conflictDocumentKey.empty());

    if (res.is(TRI_ERROR_ARANGO_TRY_AGAIN)) {
      // TRY_AGAIN we will only be getting when there is a conflicting document.
      // the key of the conflicting document can be found in the errorMessage
      // of the result :-|
      TRI_ASSERT(type != REPLICATION_MARKER_REMOVE);
      // restart the while loop above
      continue;
    }

    if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) && isSystem) {
      // ignore unique constraint violations for system collections
      res.reset();
    }

    // fix error handling here when function returns result
    if (res.ok()) {
      res = trx.commit();
      if (res.ok()) {
        if (isUsers) {
          _usersModified = true;
        } else if (isAnalyzers) {
          _analyzersModified.insert(vocbase);
        }
      }
    }

    return res;
  }

  return Result(TRI_ERROR_INTERNAL, "invalid state reached in processDocument");
}

Result TailingSyncer::removeSingleDocument(LogicalCollection* coll,
                                           std::string const& key) {
  auto operationOrigin =
      transaction::OperationOriginInternal{"applying replication change"};

  SingleCollectionTransaction trx(
      transaction::StandaloneContext::create(coll->vocbase(), operationOrigin),
      *coll, AccessMode::Type::EXCLUSIVE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();
  if (res.fail()) {
    return res;
  }

  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.waitForSync = false;

  VPackBuilder tmp;
  tmp.add(VPackValue(key));

  OperationResult opRes = trx.remove(coll->name(), tmp.slice(), options);
  if (opRes.fail()) {
    return opRes.result;
  }

  return trx.commit();
}

/// @brief starts a transaction, based on the VelocyPack provided
Result TailingSyncer::startTransaction(VPackSlice const& slice) {
  // {"type":2200,"tid":"230920705812199", "database": "123",
  // "collections":[{"cid":"230920700700391","operations":10}]}

  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    // for any other case, return "database not found" and abort the replication
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");
  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "transaction id value is missing in slice");
  }

  // transaction id
  // note: this is the remote transaction id!
  TransactionId tid{NumberUtils::atoi_zero<TransactionId::BaseType>(
      id.data(), id.data() + id.size())};

  auto it = _ongoingTransactions.find(tid);

  if (it != _ongoingTransactions.end()) {
    // found a previous version of the same transaction - should not happen...
    _ongoingTransactions.erase(it);
  }

  TRI_ASSERT(tid.isSet());

  LOG_TOPIC("e39dc", TRACE, Logger::REPLICATION)
      << "starting replication transaction " << tid.id();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(countOngoingTransactions(slice) == 0);
#endif

  auto trx = std::make_unique<ReplicationTransaction>(
      *vocbase,
      transaction::OperationOriginInternal{"replication transaction"});
  Result res = trx->begin();

  if (res.ok()) {
    _ongoingTransactions[tid] = std::move(trx);
  }

  return res;
}

/// @brief aborts a transaction, based on the VelocyPack provided
Result TailingSyncer::abortTransaction(VPackSlice const& slice) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");

  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "transaction id is missing in slice");
  }

  // transaction id
  // note: this is the remote transaction id!
  TransactionId const tid{NumberUtils::atoi_zero<TransactionId::BaseType>(
      id.data(), id.data() + id.size())};

  auto it = _ongoingTransactions.find(tid);

  if (it == _ongoingTransactions.end() || (*it).second == nullptr) {
    // invalid state, no transaction was started.
    return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION);
  }

  TRI_ASSERT(tid.isSet());

  LOG_TOPIC("19551", TRACE, Logger::REPLICATION)
      << "aborting replication transaction " << tid.id();

  _ongoingTransactions.erase(it);
  return Result();
}

/// @brief commits a transaction, based on the VelocyPack provided
Result TailingSyncer::commitTransaction(VPackSlice const& slice) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");

  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "transaction id is missing in slice");
  }

  // transaction id
  // note: this is the remote transaction id!
  TransactionId const tid{NumberUtils::atoi_zero<TransactionId::BaseType>(
      id.data(), id.data() + id.size())};

  auto it = _ongoingTransactions.find(tid);

  if (it == _ongoingTransactions.end() || (*it).second == nullptr) {
    return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION);
  }

  TRI_ASSERT(tid.isSet());

  LOG_TOPIC("fb331", TRACE, Logger::REPLICATION)
      << "committing replication transaction " << tid.id();

  std::unique_ptr<ReplicationTransaction>& trx = (*it).second;
  Result res = trx->commit();

  _ongoingTransactions.erase(it);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(countOngoingTransactions(slice) == 0);
#endif
  return res;
}

/// @brief renames a collection, based on the VelocyPack provided
Result TailingSyncer::renameCollection(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "rename slice is not an object");
  }

  VPackSlice collection = slice.get("collection");
  if (!collection.isObject()) {
    collection = slice.get("data");
  }

  if (!collection.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection slice is not an object");
  }

  std::string const name =
      VelocyPackHelper::getStringValue(collection, "name", "");

  if (name.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "name attribute is missing in rename slice");
  }

  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::shared_ptr<arangodb::LogicalCollection> col;

  if (slice.hasKey("cuid")) {
    col = resolveCollection(*vocbase, slice);

    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "unknown cuid");
    }
  } else if (collection.hasKey("oldName")) {
    col = vocbase->lookupCollection(collection.get("oldName").copyString());

    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    "unknown old collection name");
    }
  } else {
    TRI_ASSERT(col == nullptr);
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "unable to identify collection");
  }

  if (col->system()) {
    LOG_TOPIC("36d7e", WARN, Logger::REPLICATION)
        << "Renaming system collection " << col->name();
  }

  return vocbase->renameCollection(col->id(), name);
}

/// @brief changes the properties of a collection,
/// based on the VelocyPack provided
Result TailingSyncer::changeCollection(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection slice is no object");
  }

  VPackSlice data = slice.get("collection");
  if (!data.isObject()) {
    data = slice.get("data");
  }

  if (!data.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "data slice is no object");
  }

  VPackSlice d = data.get("deleted");
  bool const isDeleted = (d.isBool() && d.getBool());

  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    if (isDeleted) {
      // not a problem if a collection that is going to be deleted anyway
      // does not exist on follower
      return Result();
    }

    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto col = resolveCollection(*vocbase, slice);

  if (!col) {
    if (isDeleted) {
      // not a problem if a collection that is going to be deleted anyway
      // does not exist on follower
      return Result();
    }

    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  arangodb::CollectionGuard guard(vocbase, col->id());

  return guard.collection()->properties(data);
}

/// @brief truncate a collections. Assumes no trx are running
Result TailingSyncer::truncateCollection(
    arangodb::velocypack::Slice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection slice is no object");
  }

  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  auto col = resolveCollection(*vocbase, slice);
  if (col == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  if (!_ongoingTransactions.empty()) {
    const char* msg =
        "Encountered truncate but still have ongoing transactions";
    LOG_TOPIC("9e39e", ERR, Logger::REPLICATION) << msg;
    return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION, msg);
  }

  uint64_t count = 0;
  Result res;
  {
    auto operationOrigin = transaction::OperationOriginInternal{
        "truncating collection for replication"};
    SingleCollectionTransaction trx(
        transaction::StandaloneContext::create(*vocbase, operationOrigin), *col,
        AccessMode::Type::EXCLUSIVE);
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
    trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
    Result res = trx.begin();
    if (!res.ok()) {
      return res;
    }

    OperationOptions opts;
    OperationResult opRes =
        trx.count(col->name(), transaction::CountType::kNormal, opts);
    if (opRes.ok() && opRes.slice().isNumber()) {
      count = opRes.slice().getNumber<uint64_t>();
    }

    opts.isRestore = true;
    opRes = trx.truncate(col->name(), opts);

    if (opRes.fail()) {
      return opRes.result;
    }

    res = trx.finish(opRes.result);
  }

  if (res.ok() && count >= 4 * 1024) {
    // only compact if the collection contained a substantial amount of
    // documents before truncation
    col->compact();
  }

  return res;
}

/// @brief changes the properties of a view,
/// based on the VelocyPack provided
Result TailingSyncer::changeView(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return {TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            "view marker slice is no object"};
  }

  VPackSlice data = slice.get("data");

  if (!data.isObject()) {
    return {TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            "data slice is no object in view change marker"};
  }

  VPackSlice d = data.get("deleted");
  bool const isDeleted = (d.isBool() && d.getBool());

  TRI_vocbase_t* vocbase = resolveVocbase(slice);

  if (vocbase == nullptr) {
    if (isDeleted) {
      // not a problem if a view that is going to be deleted anyway
      // does not exist on follower
      return {};
    }
    return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND};
  }

  VPackSlice guidSlice = data.get(StaticStrings::DataSourceGuid);

  if (!guidSlice.isString() || guidSlice.getStringLength() == 0) {
    return {TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            "no guid specified for view"};
  }

  auto view = vocbase->lookupView(guidSlice.copyString());

  if (view == nullptr) {
    if (isDeleted) {
      // not a problem if a collection that is going to be deleted anyway
      // does not exist on follower
      return {};
    }

    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  VPackSlice nameSlice = data.get(StaticStrings::DataSourceName);

  if (nameSlice.isString() && !nameSlice.isEqualString(view->name())) {
    auto res = view->rename(nameSlice.copyString());

    if (!res.ok()) {
      return res;
    }
  }

  // do a partial update only for views of type "arangosearch".
  // for "search-alias" views, always do a full update.
  bool const partialUpdate =
      data.get(StaticStrings::DataSourceType).stringView() !=
      iresearch::StaticStrings::ViewSearchAliasType;
  return view->properties(data, false, partialUpdate);
}

/// @brief apply a single marker from the continuous log
Result TailingSyncer::applyLogMarker(VPackSlice const& slice,
                                     ApplyStats& applyStats,
                                     TRI_voc_tick_t /*firstRegularTick*/,
                                     TRI_voc_tick_t /*markerTick*/,
                                     TRI_replication_operation_e type) {
  auto sg = arangodb::scopeGuard([&]() noexcept {
    if (_usersModified) {
      // This function can drop/create Databases/Collections
      // this needs an up-to-date UserManager as we could run in to conflicts
      // with changes that are not yet loaded to the internal cache.
      reloadUsers();
      _usersModified = false;
    }
  });

  // handle marker type
  if (type == REPLICATION_MARKER_DOCUMENT ||
      type == REPLICATION_MARKER_REMOVE) {
    if (type == REPLICATION_MARKER_DOCUMENT) {
      ++applyStats.processedDocuments;
    } else {
      ++applyStats.processedRemovals;
    }
    try {
      return processDocument(type, slice);
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL, "unknown exception in processDocument");
    }
  }

  if (type == REPLICATION_TRANSACTION_START) {
    return startTransaction(slice);
  }

  if (type == REPLICATION_TRANSACTION_ABORT) {
    return abortTransaction(slice);
  }

  if (type == REPLICATION_TRANSACTION_COMMIT) {
    return commitTransaction(slice);
  }

  if (type == REPLICATION_COLLECTION_CREATE) {
    if (_ignoreRenameCreateDrop) {
      LOG_TOPIC("ff01b", DEBUG, Logger::REPLICATION)
          << "Ignoring collection marker";
      return Result();
    }

    TRI_vocbase_t* vocbase = resolveVocbase(slice);

    if (vocbase == nullptr) {
      LOG_TOPIC("627db", WARN, Logger::REPLICATION)
          << "Did not find database for " << slice.toJson();
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    if (slice.get("collection").isObject()) {
      return createCollection(*vocbase, slice.get("collection"), nullptr);
    }

    return createCollection(*vocbase, slice.get("data"), nullptr);
  }

  if (type == REPLICATION_COLLECTION_DROP) {
    if (_ignoreRenameCreateDrop) {
      return TRI_ERROR_NO_ERROR;
    }

    return dropCollection(slice, false);
  }

  if (type == REPLICATION_COLLECTION_RENAME) {
    if (_ignoreRenameCreateDrop) {
      // do not execute rename operations
      return Result();
    }

    return renameCollection(slice);
  }

  if (type == REPLICATION_COLLECTION_CHANGE) {
    return changeCollection(slice);
  }

  if (type == REPLICATION_COLLECTION_TRUNCATE) {
    return truncateCollection(slice);
  }

  if (type == REPLICATION_INDEX_CREATE) {
    return createIndex(slice);
  }

  if (type == REPLICATION_INDEX_DROP) {
    return dropIndex(slice);
  }

  if (type == REPLICATION_VIEW_CREATE) {
    if (_ignoreRenameCreateDrop) {
      LOG_TOPIC("846a7", DEBUG, Logger::REPLICATION)
          << "Ignoring view create marker";
      return Result();
    }

    TRI_vocbase_t* vocbase = resolveVocbase(slice);

    if (vocbase == nullptr) {
      LOG_TOPIC("028ef", WARN, Logger::REPLICATION)
          << "Did not find database for " << slice.toJson();
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    return createView(*vocbase, slice.get("data"));
  }

  if (type == REPLICATION_VIEW_DROP) {
    if (_ignoreRenameCreateDrop) {
      LOG_TOPIC("9ea21", DEBUG, Logger::REPLICATION)
          << "Ignoring view drop marker";
      return Result();
    }

    return dropView(slice, false);
  }

  if (type == REPLICATION_VIEW_CHANGE) {
    return changeView(slice);
  }

  if (type == REPLICATION_DATABASE_CREATE ||
      type == REPLICATION_DATABASE_DROP) {
    if (_ignoreDatabaseMarkers) {
      LOG_TOPIC("4254e", DEBUG, Logger::REPLICATION)
          << "Ignoring database marker";
      return Result();
    }

    return processDBMarker(type, slice);
  }

  return Result(TRI_ERROR_REPLICATION_UNEXPECTED_MARKER,
                absl::StrCat("unexpected marker type ", type));
}

/// @brief apply the data from the continuous log
Result TailingSyncer::applyLog(SimpleHttpResult* response,
                               TRI_voc_tick_t firstRegularTick,
                               ApplyStats& applyStats,
                               arangodb::velocypack::Builder& builder,
                               uint64_t& ignoreCount) {
  bool isVPack = replutils::isVelocyPack(*response);
  // reload users if they were modified
  _usersModified = false;
  _analyzersModified.clear();
  auto reloader = [this]() {
    if (_usersModified) {
      // reload users after initial dump
      reloadUsers();
      _usersModified = false;
    }
    if (!_analyzersModified.empty()) {
      TRI_ASSERT(*_analyzersModified.begin());
      auto& begin = *_analyzersModified.begin();
      auto& server = begin->server();
      if (server.hasFeature<iresearch::IResearchAnalyzerFeature>()) {
        auto& analyzersFeature =
            server.getFeature<iresearch::IResearchAnalyzerFeature>();
        for (auto* vocbase : _analyzersModified) {
          TRI_ASSERT(vocbase);
          // we need to trigger cache invalidation
          // because single server has no revisions
          // and never reloads cache from db by itself
          // so new analyzers will be not usable on follower
          analyzersFeature.invalidate(
              *vocbase,
              transaction::OperationOriginInternal{"invalidating analyzers"});
        }
      }
      _analyzersModified.clear();
    }
  };
  auto sg = arangodb::scopeGuard([&]() noexcept { reloader(); });

  StringBuffer& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  // buffer must end with a NUL byte
  TRI_ASSERT(*end == '\0');

  while (p < end) {
    char const* lineStart = p;
    size_t lineLength = 0;
    VPackSlice slice;
    if (!isVPack) {
      char const* q = static_cast<char const*>(memchr(p, '\n', (end - p)));

      if (q == nullptr) {
        q = end;
      }

      size_t const lineLength = q - p;

      if (lineLength < 2) {
        // we are done
        return Result();
      }

      TRI_ASSERT(q <= end);

      builder.clear();
      try {
        VPackParser parser(builder);
        parser.parse(p, static_cast<size_t>(q - p));
      } catch (std::exception const& ex) {
        return Result(TRI_ERROR_HTTP_CORRUPTED_JSON, ex.what());
      } catch (...) {
        return Result(TRI_ERROR_OUT_OF_MEMORY);
      }

      p = q + 1;
      slice = builder.slice();
    } else {
      slice = VPackSlice{reinterpret_cast<uint8_t const*>(p)};
      lineLength = slice.byteSize();
      p += lineLength;
    }
    applyStats.processedMarkers++;

    if (!slice.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "received invalid JSON data");
    }

    int typeValue = VelocyPackHelper::getNumericValue<int>(slice, "type", 0);
    TRI_replication_operation_e markerType =
        static_cast<TRI_replication_operation_e>(typeValue);
    TRI_voc_tick_t markerTick = 0;

    VPackSlice tickSlice = slice.get(::tickRef);

    if (tickSlice.isString() && tickSlice.getStringLength() > 0) {
      VPackValueLength len = 0;
      char const* str = tickSlice.getStringUnchecked(len);
      markerTick = NumberUtils::atoi_zero<TRI_voc_tick_t>(str, str + len);
    }

    // entry is skipped?
    bool skipped = skipMarker(firstRegularTick, slice, markerTick, markerType);

    if (!skipped) {
      Result res = applyLogMarker(slice, applyStats, firstRegularTick,
                                  markerTick, markerType);

      if (res.fail()) {
        // apply error
        auto errorMsg = std::string{res.errorMessage()};

        if (ignoreCount == 0) {
          if (lineLength > 1024) {
            errorMsg +=
                ", offending marker: " + std::string(lineStart, 1024) + "...";
          } else {
            errorMsg +=
                ", offending marker: " + std::string(lineStart, lineLength);
          }

          res.reset(res.errorNumber(), errorMsg);
          return res;
        }

        ignoreCount--;
        LOG_TOPIC("c887a", WARN, Logger::REPLICATION)
            << "ignoring replication error for database '"
            << _state.databaseName << "': " << errorMsg;
      }
    }
  }

  // reached the end
  return Result();
}
