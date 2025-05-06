////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestCollectionHandler.h"

#include "Async/async.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/ServerDefaults.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

#include <string_view>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
constexpr std::string_view moduleName("collection management");
}

RestCollectionHandler::RestCollectionHandler(ArangodServer& server,
                                             GeneralRequest* request,
                                             GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RequestLane RestCollectionHandler::lane() const {
  if (_request->requestType() == rest::RequestType::GET) {
    auto const& suffixes = _request->suffixes();
    if (suffixes.size() >= 2 &&
        (suffixes[1] == "shards" || suffixes[1] == "responsibleShard")) {
      // these request types are non-blocking, so we can give them high priority
      return RequestLane::CLUSTER_ADMIN;
    }
  }

  return RequestLane::CLIENT_SLOW;
}

futures::Future<futures::Unit> RestCollectionHandler::executeAsync() {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      co_await handleCommandGet();
      break;
    case rest::RequestType::POST:
      co_await handleCommandPost();
      break;
    case rest::RequestType::PUT:
      co_await handleCommandPut();
      break;
    case rest::RequestType::DELETE_REQ:
      co_await handleCommandDelete();
      break;
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
}

void RestCollectionHandler::shutdownExecute(bool isFinalized) noexcept {
  if (isFinalized) {
    // reset the transactions so they release all locks as early as possible
    _activeTrx.reset();
    _ctxt.reset();
  }
  RestVocbaseBaseHandler::shutdownExecute(isFinalized);
}

async<void> RestCollectionHandler::handleCommandGet() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  // /_api/collection
  if (suffixes.empty()) {
    bool const excludeSystem = _request->parsedValue("excludeSystem", false);

    _builder.openArray();
    for (auto const& collection :
         methods::Collections::getNotDeleted(_vocbase)) {
      TRI_ASSERT(collection);
      bool const canUse = ExecContext::current().canUseCollection(
          collection->name(), auth::Level::RO);

      if (canUse && (!excludeSystem || !collection->system())) {
        // We do not need a transaction here
        methods::Collections::Context ctxt(collection);
        co_await collectionRepresentation(ctxt,
                                          /*showProperties*/ false,
                                          /*showFigures*/ FiguresType::None,
                                          /*showCount*/ CountType::None);
      }
    }

    _builder.close();
    generateOk(rest::ResponseCode::OK, _builder.slice());

    co_return;
  }

  std::string const& name = suffixes[0];
  // /_api/collection/<name>
  if (suffixes.size() == 1) {
    try {
      co_await collectionRepresentation(name, /*showProperties*/ false,
                                        /*showFigures*/ FiguresType::None,
                                        /*showCount*/ CountType::None);
      generateOk(rest::ResponseCode::OK, _builder);
    } catch (basics::Exception const& ex) {  // do not log not found exceptions
      generateError(GeneralResponse::responseCode(ex.code()), ex.code(),
                    ex.what());
    }
    co_return;
  }

  if (suffixes.size() > 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/collection/<collection-name>/<method>");
    co_return;
  }

  _builder.clear();

  std::shared_ptr<LogicalCollection> coll;
  auto res = methods::Collections::lookup(_vocbase, name, coll);
  if (res.fail()) {
    generateError(res);
    co_return;
  }

  TRI_ASSERT(coll);

  std::string const& sub = suffixes[1];

  if (sub == "checksum") {
    // /_api/collection/<identifier>/checksum
    bool withRevisions = _request->parsedValue("withRevisions", false);
    bool withData = _request->parsedValue("withData", false);

    uint64_t checksum;
    RevisionId revId;
    res = co_await methods::Collections::checksum(*coll, withRevisions,
                                                  withData, checksum, revId);

    if (res.ok()) {
      {
        VPackObjectBuilder obj(&_builder, true);
        obj->add("checksum", VPackValue(std::to_string(checksum)));
        obj->add("revision", VPackValue(revId.toString()));

        // We do not need a transaction here
        co_await collectionRepresentation(coll,
                                          /*showProperties*/ false,
                                          /*showFigures*/ FiguresType::None,
                                          /*showCount*/ CountType::None);
      }
      co_return standardResponse();
    }

    generateError(res);
    co_return;
  }
  if (sub == "figures") {
    // /_api/collection/<identifier>/figures
    bool details = _request->parsedValue("details", false);
    _ctxt = std::make_unique<methods::Collections::Context>(coll);
    co_await collectionRepresentation(
        *_ctxt,
        /*showProperties*/ true,
        details ? FiguresType::Detailed : FiguresType::Standard,
        CountType::Standard);
    standardResponse();
    co_return;
  }
  if (sub == "count") {
    // /_api/collection/<identifier>/count
    co_await initializeTransaction(*coll);
    _ctxt =
        std::make_unique<methods::Collections::Context>(coll, _activeTrx.get());

    bool details = _request->parsedValue("details", false);
    bool checkSyncStatus = _request->parsedValue("checkSyncStatus", false);
    // the checkSyncStatus flag is only set in internal requests performed
    // by the ShardDistributionReporter. it is used to determine the shard
    // synchronization status by asking the maintainance. the functionality
    // is only available on DB servers. as this is an internal API, it is ok
    // to make some assumptions about the requests and let everything else
    // fail.
    if (checkSyncStatus) {
      if (!ServerState::instance()->isDBServer()) {
        generateError(Result(TRI_ERROR_NOT_IMPLEMENTED,
                             "syncStatus API is only available on DB servers"));
        co_return;
      }

      // details automatically turned off here, as we will not need them
      details = false;

      // check if a SynchronizeShard job is currently executing for the
      // specified shard
      auto maybeShard = ShardID::shardIdFromString(name);
      // Compatibility, if the shard is not valid we would never find an action
      // for it, so ignore non-shards.
      bool isSyncing = false;
      if (maybeShard.ok()) {
        isSyncing = server().getFeature<MaintenanceFeature>().hasAction(
            maintenance::EXECUTING, maybeShard.get(),
            arangodb::maintenance::SYNCHRONIZE_SHARD);
      }

      // already put some data into the response
      _builder.openObject();
      _builder.add("syncing", VPackValue(isSyncing));
    }

    co_await collectionRepresentation(
        *_ctxt,
        /*showProperties*/ true,
        /*showFigures*/ FiguresType::None,
        /*showCount*/ details ? CountType::Detailed : CountType::Standard);

    if (checkSyncStatus) {
      // checkSyncStatus == true, so we opened the _builder on our own
      // before, and we are now responsible for closing it again.
      _builder.close();
    }
    standardResponse();
    co_return;
  }
  if (sub == "properties") {
    // /_api/collection/<identifier>/properties
    co_await collectionRepresentation(coll,
                                      /*showProperties*/ true,
                                      /*showFigures*/ FiguresType::None,
                                      /*showCount*/ CountType::None);
    co_return standardResponse();
  }
  if (sub == "revision") {
    // /_api/collection/<identifier>/revision

    _ctxt = std::make_unique<methods::Collections::Context>(coll);
    OperationOptions options(_context);
    auto res = co_await methods::Collections::revisionId(*_ctxt, options);
    if (res.fail()) {
      generateTransactionError(coll->name(), res);
      co_return;
    }

    RevisionId rid = RevisionId::fromSlice(res.slice());
    {
      VPackObjectBuilder obj(&_builder, true);
      obj->add("revision", VPackValue(rid.toString()));

      co_await collectionRepresentation(*_ctxt, /*showProperties*/ true,
                                        FiguresType::None, CountType::None);
    }

    standardResponse();
    co_return;
  }
  if (sub == "shards") {
    // /_api/collection/<identifier>/shards
    if (!ServerState::instance()->isRunningInCluster()) {
      this->generateError(Result(TRI_ERROR_NOT_IMPLEMENTED,
                                 "shards API is only available in a cluster"));
      co_return;
    }

    {
      VPackObjectBuilder obj(&_builder, true);  // need to open object

      co_await collectionRepresentation(coll,
                                        /*showProperties*/ true,
                                        FiguresType::None, CountType::None);

      auto shardsMap = coll->shardIds();
      if (_request->parsedValue("details", false)) {
        // with details
        coll->shardMapToVelocyPack(_builder);
      } else {
        // without details
        coll->shardIDsToVelocyPack(_builder);
      }
    }
    co_return standardResponse();
  }

  generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                "expecting one of the resources 'checksum', 'count', "
                "'figures', 'properties', 'responsibleShard', 'revision', "
                "'shards'");
}

// create a collection
async<void> RestCollectionHandler::handleCommandPost() {
  if (ServerState::instance()->isDBServer()) {
    generateError(Result(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR));
    co_return;
  }

  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    events::CreateCollection(_vocbase.name(), StaticStrings::Empty,
                             TRI_ERROR_BAD_PARAMETER);
    co_return;
  }
  auto& cluster = _vocbase.server().getFeature<ClusterFeature>();
  bool waitForSyncReplication = _request->parsedValue(
      "waitForSyncReplication", cluster.createWaitsForSyncReplication());

  bool enforceReplicationFactor =
      _request->parsedValue("enforceReplicationFactor", true);
  auto config = _vocbase.getDatabaseConfiguration();
  config.enforceReplicationFactor = enforceReplicationFactor;

  auto planCollection = CreateCollectionBody::fromCreateAPIBody(body, config);

  if (planCollection.fail()) {
    // error message generated in inspect
    generateError(rest::ResponseCode::BAD, planCollection.errorNumber(),
                  planCollection.errorMessage());
    // Try to get a name for Auditlog, if it is available, otherwise report
    // empty string
    auto collectionName = VelocyPackHelper::getStringValue(
        body, StaticStrings::DataSourceName, StaticStrings::Empty);

    events::CreateCollection(_vocbase.name(), collectionName,
                             planCollection.errorNumber());
    co_return;
  }
  std::vector<CreateCollectionBody> collections{
      std::move(planCollection.get())};

  OperationOptions options(_context);
  auto result = methods::Collections::create(
      _vocbase,  // collection vocbase
      options, collections,
      waitForSyncReplication,    // replication wait flag
      enforceReplicationFactor,  // replication factor flag
      /*isNewDatabase*/ false    // here always false
  );

  std::shared_ptr<LogicalCollection> coll;
  // backwards compatibility transformation:
  Result res;
  if (result.fail()) {
    res = result.result();
  } else {
    TRI_ASSERT(result.get().size() == 1);
    coll = result.get().at(0);
  }

  if (res.ok()) {
    TRI_ASSERT(coll);
    co_await collectionRepresentation(coll->name(),
                                      /*showProperties*/ true,
                                      FiguresType::None, CountType::None);

    generateOk(rest::ResponseCode::OK, _builder);
  } else {
    generateError(res);
  }
}

async<void> RestCollectionHandler::handleCommandPut() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected PUT /_api/collection/<collection-name>/<action>");
    co_return;
  }
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    co_return;
  }

  std::string const& name = suffixes[0];
  std::string const& sub = suffixes[1];

  if (sub != "responsibleShard" && !body.isObject()) {
    // if the caller has sent an empty body. for convenience, let's turn this
    // into an object however, for the "responsibleShard" case we want to
    // distinguish between string values, object values etc. - so we don't do
    // the conversion here
    body = VPackSlice::emptyObjectSlice();
  }

  _builder.clear();

  std::shared_ptr<LogicalCollection> coll;
  Result res = methods::Collections::lookup(_vocbase, name, coll);

  if (res.fail()) {
    generateError(res);
    co_return;
  }
  TRI_ASSERT(coll);

  if (sub == "load") {
    // "load" is a no-op starting with 3.9
    bool cc = VelocyPackHelper::getBooleanValue(body, "count", true);
    co_await collectionRepresentation(
        name, /*showProperties*/ false,
        /*showFigures*/ FiguresType::None,
        /*showCount*/ cc ? CountType::Standard : CountType::None);
    co_return standardResponse();
  }
  if (sub == "unload") {
    bool flush = _request->parsedValue("flush", false);

    if (flush && !coll->deleted()) {
      server().getFeature<EngineSelectorFeature>().engine().flushWal(false,
                                                                     false);
    }

    // apart from WAL flushing, unload is a no-op starting with 3.9
    co_await collectionRepresentation(name, /*showProperties*/ false,
                                      /*showFigures*/ FiguresType::None,
                                      /*showCount*/ CountType::None);
    co_return standardResponse();
  }
  if (sub == "compact") {
    if (ServerState::instance()->isCoordinator()) {
      auto& feature = server().getFeature<ClusterFeature>();
      // while this call is technically blocking, the requests to the
      // DB servers only schedule the compactions, but they do not
      // block until they are completed.
      auto res = compactOnAllDBServers(feature, _vocbase.name(), name);
      if (res.fail()) {
        generateError(res);
        co_return;
      }
    } else {
      coll->compact();
    }

    co_await collectionRepresentation(name, /*showProperties*/ false,
                                      /*showFigures*/ FiguresType::None,
                                      /*showCount*/ CountType::None);
    co_return standardResponse();
  }
  if (sub == "responsibleShard") {
    if (!ServerState::instance()->isCoordinator()) {
      res.reset(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      generateError(res);
      co_return;
    }

    VPackBuilder temp;
    if (body.isString()) {
      temp.openObject();
      temp.add(StaticStrings::KeyString, body);
      temp.close();
      body = temp.slice();
    } else if (body.isNumber()) {
      temp.openObject();
      temp.add(StaticStrings::KeyString,
               VPackValue(std::to_string(body.getNumber<int64_t>())));
      temp.close();
      body = temp.slice();
    }
    if (!body.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting object for responsibleShard");
    }

    auto maybeShard = coll->getResponsibleShard(body, false);

    if (maybeShard.ok()) {
      _builder.openObject();
      _builder.add("shardId", VPackValue(maybeShard.get()));
      _builder.close();
      co_return standardResponse();
    }
    generateError(maybeShard.result());
    co_return;
  }
  if (sub == "truncate") {
    OperationOptions opts(_context);

    opts.waitForSync =
        _request->parsedValue(StaticStrings::WaitForSyncString, false);
    opts.isSynchronousReplicationFrom =
        _request->value(StaticStrings::IsSynchronousReplicationString);
    opts.truncateCompact = _request->parsedValue(StaticStrings::Compact, true);

    _activeTrx = co_await createTransaction(
        coll->name(), AccessMode::Type::EXCLUSIVE, opts,
        transaction::OperationOriginREST{"truncating collection"});
    _activeTrx->addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
    _activeTrx->addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
    res = co_await _activeTrx->beginAsync();
    if (res.fail()) {
      generateTransactionError(coll->name(), OperationResult(res, opts), "");
      _activeTrx.reset();
      co_return;
    }

    if (ServerState::instance()->isDBServer() &&
        (_activeTrx->state()->collection(
             coll->name(), AccessMode::Type::EXCLUSIVE) == nullptr ||
         _activeTrx->state()->isReadOnlyTransaction())) {
      // make sure that the current transaction includes the collection that
      // we want to write into. this is not necessarily the case for follower
      // transactions that are started lazily. in this case, we must reject
      // the request. we _cannot_ do this for follower transactions, where
      // shards may lazily be added (e.g. if servers A and B both replicate
      // their own write ops to follower C one after the after, then C will
      // first see only shards from A and then only from B).
      res.reset(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
                absl::StrCat("Transaction with id '", _activeTrx->tid().id(),
                             "' does not contain collection '", coll->name(),
                             "' with the required access mode."));
      generateError(res);
      _activeTrx.reset();
      co_return;
    }

    // track request only on leader
    if (opts.isSynchronousReplicationFrom.empty() &&
        ServerState::instance()->isDBServer()) {
      _activeTrx->state()->trackShardRequest(
          *_activeTrx->resolver(), _vocbase.name(), coll->name(),
          _request->value(StaticStrings::UserString), AccessMode::Type::WRITE,
          "truncate");
    }

    OperationResult opres =
        co_await _activeTrx->truncateAsync(coll->name(), opts);
    // Will commit if no error occured.
    // or abort if an error occured.
    // result stays valid!
    Result res = co_await _activeTrx->finishAsync(opres.result);
    if (opres.fail()) {
      generateTransactionError(coll->name(), opres);
      co_return;
    }

    if (res.fail()) {
      generateTransactionError(coll->name(),
                               OperationResult(res, opres.options), "");
      co_return;
    }

    _activeTrx.reset();

    if (opts.truncateCompact) {
      // wait for the transaction to finish first. only after that
      // compact the data range(s) for the collection we shouldn't run
      // compact() as part of the transaction, because the compact
      // will be useless inside due to the snapshot the transaction
      // has taken
      coll->compact();
    }

    // no need to use async method, no
    co_await collectionRepresentation(coll,
                                      /*showProperties*/ false,
                                      /*showFigures*/ FiguresType::None,
                                      /*showCount*/ CountType::None);
    standardResponse();
    co_return;
  }
  if (sub == "properties") {
    std::vector<std::string> keep = {
        StaticStrings::WaitForSyncString,    StaticStrings::Schema,
        StaticStrings::ReplicationFactor,
        StaticStrings::MinReplicationFactor,  // deprecated
        StaticStrings::WriteConcern,         StaticStrings::ComputedValues,
        StaticStrings::CacheEnabled};
    VPackBuilder props = VPackCollection::keep(body, keep);

    OperationOptions options(_context);
    res = co_await methods::Collections::updateProperties(*coll, props.slice(),
                                                          options);
    if (res.fail()) {
      generateError(res);
      co_return;
    }

    co_await collectionRepresentation(name, /*showProperties*/ true,
                                      /*showFigures*/ FiguresType::None,
                                      /*showCount*/ CountType::None);
    co_return standardResponse();
  }
  if (sub == "rename") {
    VPackSlice const newNameSlice = body.get(StaticStrings::DataSourceName);
    if (!newNameSlice.isString()) {
      generateError(Result(TRI_ERROR_ARANGO_ILLEGAL_NAME, "name is empty"));
      co_return;
    }

    std::string const newName = newNameSlice.copyString();
    res = methods::Collections::rename(*coll, newName, false);

    if (res.ok()) {
      co_await collectionRepresentation(newName, /*showProperties*/ false,
                                        /*showFigures*/ FiguresType::None,
                                        /*showCount*/ CountType::None);
      co_return standardResponse();
    }

    generateError(res);
    co_return;
  }
  if (sub == "loadIndexesIntoMemory") {
    OperationOptions options(_context);
    auto res = co_await methods::Collections::warmup(_vocbase, *coll);
    if (res.fail()) {
      generateTransactionError(coll->name(), OperationResult(res, options), "");
      co_return;
    }

    {
      VPackObjectBuilder obj(&_builder, true);
      obj->add("result", VPackValue(res.ok()));
    }

    standardResponse();
    co_return;
  }

  auto resExtra = co_await handleExtraCommandPut(coll, sub, _builder);
  if (resExtra.is(TRI_ERROR_NOT_IMPLEMENTED)) {
    resExtra.reset(
        TRI_ERROR_HTTP_NOT_FOUND,
        "expecting one of the actions 'load', 'unload', 'truncate',"
        " 'properties', 'compact', 'rename', 'loadIndexesIntoMemory'");
    generateError(resExtra);
  } else if (resExtra.fail()) {
    generateError(resExtra);
  } else {
    standardResponse();
  }
  co_return;
}

async<void> RestCollectionHandler::handleCommandDelete() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected DELETE /_api/collection/<collection-name>");
    events::DropCollection(_vocbase.name(), "", TRI_ERROR_HTTP_BAD_PARAMETER);
    co_return;
  }

  std::string const& name = suffixes[0];
  bool allowDropSystem =
      _request->parsedValue(StaticStrings::DataSourceSystem, false);
  _builder.clear();

  std::shared_ptr<LogicalCollection> coll;
  Result res = methods::Collections::lookup(_vocbase, name, coll);
  if (res.fail()) {
    events::DropCollection(_vocbase.name(), name, res.errorNumber());
    generateError(res);
    co_return;
  }
  TRI_ASSERT(coll);

  {
    VPackObjectBuilder obj(&_builder, true);

    obj->add("id", VPackValue(std::to_string(coll->id().id())));
    CollectionDropOptions dropOptions{.allowDropSystem = allowDropSystem,
                                      .allowDropGraphCollection = false};
    res = methods::Collections::drop(*coll, dropOptions);
  }

  if (res.fail()) {
    generateError(res);
  } else {
    generateOk(rest::ResponseCode::OK, _builder);
  }
}

/// generate collection info. We lookup the collection again, because in the
/// cluster someinfo is lazily added in loadPlan, which means load, unload,
/// truncate
/// and create will not immediately show the expected results on a collection
/// object.
async<void> RestCollectionHandler::collectionRepresentation(
    std::string const& name, bool showProperties, FiguresType showFigures,
    CountType showCount) {
  std::shared_ptr<LogicalCollection> coll;
  Result r = methods::Collections::lookup(_vocbase, name, coll);
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }
  TRI_ASSERT(coll);
  co_await collectionRepresentation(coll, showProperties, showFigures,
                                    showCount);
}

async<void> RestCollectionHandler::collectionRepresentation(
    std::shared_ptr<LogicalCollection> coll, bool showProperties,
    FiguresType showFigures, CountType showCount) {
  if (showProperties || showCount != CountType::None) {
    // Here we need a transaction
    co_await initializeTransaction(*coll);
    methods::Collections::Context ctxt(coll, _activeTrx.get());

    co_await collectionRepresentation(ctxt, showProperties, showFigures,
                                      showCount);
  } else {
    // We do not need a transaction here
    methods::Collections::Context ctxt(coll);

    co_await collectionRepresentation(ctxt, showProperties, showFigures,
                                      showCount);
  }
}

futures::Future<futures::Unit> RestCollectionHandler::collectionRepresentation(
    methods::Collections::Context& ctxt, bool showProperties,
    FiguresType showFigures, CountType showCount) {
  bool wasOpen = _builder.isOpenObject();
  if (!wasOpen) {
    _builder.openObject();
  }

  auto coll = ctxt.coll();
  TRI_ASSERT(coll != nullptr);

  // `methods::Collections::properties` will filter these out
  _builder.add(StaticStrings::DataSourceId,
               VPackValue(std::to_string(coll->id().id())));
  _builder.add(StaticStrings::DataSourceName, VPackValue(coll->name()));
  // only here for API-compatibility...
  if (coll->deleted()) {
    _builder.add("status", VPackValue(/*TRI_VOC_COL_STATUS_DELETED*/ 5));
  } else {
    _builder.add("status", VPackValue(/*TRI_VOC_COL_STATUS_LOADED*/ 3));
  }
  _builder.add(StaticStrings::DataSourceType, VPackValue(coll->type()));

  if (!showProperties) {
    _builder.add(StaticStrings::DataSourceSystem, VPackValue(coll->system()));
    _builder.add(StaticStrings::DataSourceGuid, VPackValue(coll->guid()));
  } else {
    Result res = co_await methods::Collections::properties(ctxt, _builder);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  OperationOptions options(_context);
  futures::Future<OperationResult> figures =
      futures::makeFuture(OperationResult(Result(), options));
  if (showFigures != FiguresType::None) {
    figures = coll->figures(showFigures == FiguresType::Detailed, options);
  }

  auto figuresRes = co_await std::move(figures);
  if (figuresRes.fail()) {
    THROW_ARANGO_EXCEPTION(figuresRes.result);
  }
  if (figuresRes.buffer) {
    _builder.add("figures", figuresRes.slice());
  }

  auto opRes = OperationResult(Result(), options);
  if (showCount != CountType::None) {
    auto trx = co_await ctxt.trx(AccessMode::Type::READ, true);
    TRI_ASSERT(trx != nullptr);
    opRes = co_await trx->countAsync(coll->name(),
                                     showCount == CountType::Detailed
                                         ? transaction::CountType::kDetailed
                                         : transaction::CountType::kNormal,
                                     options);
  }

  if (opRes.fail()) {
    if (showCount != CountType::None) {
      auto trx = co_await ctxt.trx(AccessMode::Type::READ, true);
      TRI_ASSERT(trx != nullptr);
      std::ignore = co_await trx->finishAsync(opRes.result);
    }
    THROW_ARANGO_EXCEPTION(opRes.result);
  }

  if (showCount != CountType::None) {
    _builder.add("count", opRes.slice());
  }

  if (!wasOpen) {
    _builder.close();
  }
}

void RestCollectionHandler::standardResponse() {
  generateOk(rest::ResponseCode::OK, _builder);
  _response->setHeaderNC(
      StaticStrings::Location,
      absl::StrCat("/_db/", StringUtils::urlEncode(_vocbase.name()),
                   _request->requestPath()));
}

futures::Future<futures::Unit> RestCollectionHandler::initializeTransaction(
    LogicalCollection& coll) {
  try {
    _activeTrx = co_await createTransaction(
        coll.name(), AccessMode::Type::READ, OperationOptions(),
        transaction::OperationOriginREST{::moduleName});
  } catch (basics::Exception const& ex) {
    if (ex.code() == TRI_ERROR_TRANSACTION_NOT_FOUND) {
      // this will happen if the tid of a managed transaction is passed in,
      // but the transaction hasn't yet started on the DB server. in
      // this case, we create an ad-hoc transaction on the underlying
      // collection
      auto origin = transaction::OperationOriginREST{::moduleName};
      _activeTrx = std::make_unique<SingleCollectionTransaction>(
          transaction::StandaloneContext::create(_vocbase, origin), coll.name(),
          AccessMode::Type::READ);
    } else {
      throw;
    }
  }

  TRI_ASSERT(_activeTrx != nullptr);
  Result res = co_await _activeTrx->beginAsync();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}
