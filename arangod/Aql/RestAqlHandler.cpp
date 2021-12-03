////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestAqlHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlExecuteResult.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/ClusterQuery.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Cluster/CallbackGuard.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/RebootTracker.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngine.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

namespace {
constexpr std::string_view writeKey("write");
constexpr std::string_view exclusiveKey("exclusive");
} // namespace

RestAqlHandler::RestAqlHandler(application_features::ApplicationServer& server,
                               GeneralRequest* request, GeneralResponse* response,
                               QueryRegistry* qr)
    : RestVocbaseBaseHandler(server, request, response),
      _queryRegistry(qr),
      _engine(nullptr) {
  TRI_ASSERT(_queryRegistry != nullptr);
}

// POST method for /_api/aql/setup (internal)
// Only available on DBServers in the Cluster.
// This route sets-up all the query engines required
// for a complete query on this server.
// Furthermore it directly locks all shards for this query.
// So after this route the query is ready to go.
// NOTE: As this Route LOCKS the collections, the caller
// is responsible to destroy those engines in a timely
// manner, if the engines are not called for a period
// of time, they will be garbage-collected and unlocked.
// The body is a VelocyPack with the following layout:
//  {
//    lockInfo: {
//      NONE: [<collections to not-lock],
//      READ: [<collections to read-lock],
//      WRITE: [<collections to write-lock],
//      EXCLUSIVE: [<collections with exclusive-lock]
//    },
//    options: { < query options > },
//    snippets: {
//      <queryId: {nodes: [ <nodes>]}>
//    },
//    traverserEngines: [ <infos for traverser engines> ],
//    variables: [ <variables> ]
//  }
void RestAqlHandler::setupClusterQuery() {
  // We should not intentionally call this method
  // on the wrong server. So fail during maintanence.
  // On user setup reply gracefully.
  TRI_ASSERT(ServerState::instance()->isDBServer());
  if (ADB_UNLIKELY(!ServerState::instance()->isDBServer())) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
    return;
  }

  TRI_IF_FAILURE("Query::setupTimeout") {
    // intentionally delay the request
    std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
  }

  TRI_IF_FAILURE("Query::setupTimeoutFailSequence") {
    // simulate lock timeout during query setup
    uint32_t r = 100;
    TRI_IF_FAILURE("Query::setupTimeoutFailSequenceRandom") {
      r = RandomGenerator::interval(uint32_t(100));
    }
    if (r >= 96) {
      std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
  }

  bool success = false;
  VPackSlice querySlice = this->parseVPackBody(success);
  if (!success) {
    // if no success here, generateError will have been called already
    LOG_TOPIC("ef4ca", ERR, arangodb::Logger::AQL)
        << "Failed to setup query. Could not "
           "parse the transmitted plan. "
           "Aborting query.";
    return;
  }

  QueryId clusterQueryId = 0;
  // this is an optional attribute that 3.8 coordinators will send, but
  // older versions won't send.
  // if set, it is the query id that will be used for this particular query
  VPackSlice queryIdSlice = querySlice.get("clusterQueryId");
  if (queryIdSlice.isNumber()) {
    clusterQueryId = queryIdSlice.getNumber<QueryId>();
    TRI_ASSERT(clusterQueryId > 0);
  }

  VPackSlice lockInfoSlice = querySlice.get("lockInfo");

  if (!lockInfoSlice.isObject()) {
    LOG_TOPIC("19e7e", ERR, arangodb::Logger::AQL)
        << "Invalid VelocyPack: \"lockInfo\" is required but not an object.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"lockInfo\"");
    return;
  }

  VPackSlice optionsSlice = querySlice.get("options");
  if (!optionsSlice.isObject()) {
    LOG_TOPIC("1a8a1", ERR, arangodb::Logger::AQL)
        << "Invalid VelocyPack: \"options\" attribute missing.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"options\"");
    return;
  }

  VPackSlice snippetsSlice = querySlice.get("snippets");
  if (!snippetsSlice.isObject()) {
    LOG_TOPIC("5bd07", ERR, arangodb::Logger::AQL)
        << "Invalid VelocyPack: \"snippets\" attribute missing.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"snippets\"");
    return;
  }

  VPackSlice traverserSlice = querySlice.get("traverserEngines");
  if (!traverserSlice.isNone() && !traverserSlice.isArray()) {
    LOG_TOPIC("69f64", ERR, arangodb::Logger::AQL)
        << "Invalid VelocyPack: \"traverserEngines\" attribute is not an "
           "array.";
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
        "if \"traverserEngines\" is set in the body, it has to be an array");
    return;
  }

  VPackSlice variablesSlice = querySlice.get("variables");
  if (!variablesSlice.isArray()) {
    LOG_TOPIC("6f9dc", ERR, arangodb::Logger::AQL)
        << "Invalid VelocyPack: \"variables\" attribute missing.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"variables\"");
    return;
  }

  LOG_TOPIC("f9e30", DEBUG, arangodb::Logger::AQL)
    << "Setting up cluster AQL with " << querySlice.toJson();

  VPackSlice coordinatorRebootIdSlice = querySlice.get(StaticStrings::AttrCoordinatorRebootId);
  VPackSlice coordinatorIdSlice = querySlice.get(StaticStrings::AttrCoordinatorId);
  RebootId rebootId(0);
  std::string coordinatorId;
  if (!coordinatorRebootIdSlice.isNone() || !coordinatorIdSlice.isNone()) {
    bool good = false;
    if (coordinatorRebootIdSlice.isInteger() && coordinatorIdSlice.isString()) {
      coordinatorId = coordinatorIdSlice.copyString();
      try {
        // The following will throw for negative numbers, which should not happen:
        rebootId = RebootId(coordinatorRebootIdSlice.getUInt());
        good = true;
      } catch (...) {
      }
    }
    if (!good) {
      LOG_TOPIC("4251a", ERR, arangodb::Logger::AQL)
          << "Invalid VelocyPack: \"" << StaticStrings::AttrCoordinatorRebootId
          << "\" needs to be a positive number and \""
          << StaticStrings::AttrCoordinatorId << "\" needs to be a non-empty string";
      generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                    "body must be an object with attribute \"" + StaticStrings::AttrCoordinatorRebootId +
                        "\" and \"" + StaticStrings::AttrCoordinatorId + "\"");
      return;
    }
  }
  // Valid to not exist for upgrade scenarios!

  // Now we need to create shared_ptr<VPackBuilder>
  // That contains the old-style cluster snippet in order
  // to prepare create a Query object.
  // This old snippet is created as follows:
  //
  // {
  //   collections: [ { name: "xyz", type: "READ" }, {name: "abc", type:
  //   "WRITE"} ], initialize: false, nodes: <one of snippets[*].value>,
  //   variables: <variables slice>
  // }

  QueryOptions options(optionsSlice);
  if (options.ttl <= 0) { // patch TTL value
    options.ttl = _queryRegistry->defaultTTL();
  }

  AccessMode::Type access = AccessMode::Type::READ;

  // TODO: technically we could change the code in prepareClusterQuery to parse
  //       the collection info directly
  // Build the collection information
  VPackBuilder collectionBuilder;
  collectionBuilder.openArray();
  for (auto const& lockInf : VPackObjectIterator(lockInfoSlice)) {
    if (!lockInf.value.isArray()) {
      LOG_TOPIC("1dc00", WARN, arangodb::Logger::AQL)
          << "Invalid VelocyPack: \"lockInfo." << lockInf.key.copyString()
          << "\" is required but not an array.";
      generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                    "body must be an object with attribute: \"lockInfo." +
                        lockInf.key.copyString() +
                        "\" is required but not an array.");
      return;
    }
    for (VPackSlice col : VPackArrayIterator(lockInf.value)) {
      if (!col.isString()) {
        LOG_TOPIC("9e29f", WARN, arangodb::Logger::AQL)
            << "Invalid VelocyPack: \"lockInfo." << lockInf.key.copyString()
            << "\" is required but not an array.";
        generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                      "body must be an object with attribute: \"lockInfo." +
                          lockInf.key.copyString() +
                          "\" is required but not an array.");
        return;
      }
      collectionBuilder.openObject();
      collectionBuilder.add("name", col);
      collectionBuilder.add("type", lockInf.key);
      collectionBuilder.close();

      if (!AccessMode::isWriteOrExclusive(access) && lockInf.key.isEqualString(::writeKey)) {
        access = AccessMode::Type::WRITE;
      } else if (!AccessMode::isExclusive(access) && lockInf.key.isEqualString(::exclusiveKey)) {
        access = AccessMode::Type::EXCLUSIVE;
      }
    }
  }
  collectionBuilder.close();

  double const ttl = options.ttl;
  // creates a StandaloneContext or a leased context
  auto q = ClusterQuery::create(clusterQueryId,
                                createTransactionContext(access),
                                std::move(options));
  TRI_ASSERT(clusterQueryId == 0 || clusterQueryId == q->id());

  VPackBufferUInt8 buffer;
  VPackBuilder answerBuilder(buffer);
  answerBuilder.openObject();
  answerBuilder.add(StaticStrings::Error, VPackValue(false));
  answerBuilder.add(StaticStrings::Code, VPackValue(static_cast<int>(rest::ResponseCode::OK)));

  answerBuilder.add(StaticStrings::AqlRemoteResult, VPackValue(VPackValueType::Object));
  if (clusterQueryId == 0) {
    // only return this attribute if we didn't get a query ID as input from
    // the coordinator. this will be the case for setup requests from 3.7
    // coordinators
    answerBuilder.add("queryId", VPackValue(q->id()));
  }
  // send back our own reboot id
  answerBuilder.add(StaticStrings::RebootId, VPackValue(ServerState::instance()->getRebootId().value()));

  QueryAnalyzerRevisions analyzersRevision;
  auto revisionRes = analyzersRevision.fromVelocyPack(querySlice);
  if(ADB_UNLIKELY(revisionRes.fail())) {
    LOG_TOPIC("b2a37", ERR, arangodb::Logger::AQL)
      << "Failed to read ArangoSearch analyzers revision " << revisionRes.errorMessage();
    generateError(revisionRes);
    return;
  }
  q->prepareClusterQuery(querySlice, collectionBuilder.slice(),
                         variablesSlice, snippetsSlice,
                         traverserSlice, answerBuilder, analyzersRevision);

  answerBuilder.close(); // result
  answerBuilder.close();

  cluster::CallbackGuard rGuard;

  // Now set an alarm for the case that the coordinator is restarted which
  // initiated this query. In that case, we want to drop our piece here:
  if (rebootId.initialized()) {
    LOG_TOPIC("42512", TRACE, Logger::AQL)
        << "Setting RebootTracker on coordinator " << coordinatorId
        << " for query with id " << q->id();
    auto& clusterFeature = _server.getFeature<ClusterFeature>();
    auto& clusterInfo = clusterFeature.clusterInfo();
    rGuard = clusterInfo.rebootTracker().callMeOnChange(
        cluster::RebootTracker::PeerState(coordinatorId, rebootId),
        [queryRegistry = _queryRegistry, vocbaseName = _vocbase.name(),
         queryId = q->id()]() {
          queryRegistry->destroyQuery(vocbaseName, queryId, TRI_ERROR_TRANSACTION_ABORTED);
          LOG_TOPIC("42511", DEBUG, Logger::AQL)
              << "Query snippet destroyed as consequence of "
                 "RebootTracker for coordinator, db="
              << vocbaseName << " queryId=" << queryId;
        },
        "Query aborted since coordinator rebooted or failed.");
  }

  _queryRegistry->insertQuery(std::move(q), ttl, std::move(rGuard));

  generateResult(rest::ResponseCode::OK, std::move(buffer));
}

// PUT method for /_api/aql/<operation>/<queryId>, (internal)
// see comment in header for details
RestStatus RestAqlHandler::useQuery(std::string const& operation, std::string const& idString) {
  bool success = false;
  VPackSlice querySlice = this->parseVPackBody(success);
  if (!success) {
    return RestStatus::DONE;
  }

  if (!_engine) {  // the PUT verb
    TRI_ASSERT(this->state() == RestHandler::HandlerState::EXECUTE);

    _engine = findEngine(idString);
    if (!_engine) {
      return RestStatus::DONE;
    }
    std::shared_ptr<SharedQueryState> ss = _engine->sharedState();
    ss->setWakeupHandler(
        withLogContext([self = shared_from_this()] {
          return self->wakeupHandler();
        }));
  }

  TRI_ASSERT(_engine != nullptr);
  TRI_ASSERT(std::to_string(_engine->engineId()) == idString);

  if (_engine->getQuery().queryOptions().profile >= ProfileLevel::TraceOne) {
    LOG_TOPIC("1bf67", INFO, Logger::QUERIES)
        << "[query#" << _engine->getQuery().id() << "] remote request received: " << operation
        << " registryId=" << idString;
  }

  try {
    return handleUseQuery(operation, querySlice);
  } catch (arangodb::basics::Exception const& ex) {
    generateError(rest::ResponseCode::SERVER_ERROR, ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC("d1266", ERR, arangodb::Logger::AQL)
        << "failed during use of Query: " << ex.what();

    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                  ex.what());
  } catch (...) {
    LOG_TOPIC("5a2e8", ERR, arangodb::Logger::AQL)
        << "failed during use of Query: Unknown exception occurred";

    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                  "an unknown exception occurred");
  }
  return RestStatus::DONE;
}

// executes the handler
RestStatus RestAqlHandler::execute() {
  if (ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_HTTP_NOT_IMPLEMENTED,
                  "this endpoint is only available in clusters");
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  // extract the sub-request type
  rest::RequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::POST: {
      if (suffixes.size() != 1) {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      } else if (suffixes[0] == "setup") {
        setupClusterQuery();
      } else {
        std::string msg("Unknown POST API: ");
        msg += arangodb::basics::StringUtils::join(suffixes, '/');
        LOG_TOPIC("b7507", ERR, arangodb::Logger::AQL) << msg;
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                      std::move(msg));
      }
      break;
    }
    case rest::RequestType::PUT: {
      if (suffixes.size() != 2) {
        std::string msg("Unknown PUT API: ");
        msg += arangodb::basics::StringUtils::join(suffixes, '/');
        LOG_TOPIC("9880a", ERR, arangodb::Logger::AQL) << msg;
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                      std::move(msg));
      } else {
        auto status = useQuery(suffixes[0], suffixes[1]);
        if (status == RestStatus::WAITING) {
          return status;
        }
      }
      break;
    }
    case rest::RequestType::DELETE_REQ: {
      if (suffixes.size() != 2) {
        std::string msg("Unknown DELETE API: ");
        msg += arangodb::basics::StringUtils::join(suffixes, '/');
        LOG_TOPIC("f1993", ERR, arangodb::Logger::AQL) << msg;
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                      std::move(msg));
        return RestStatus::DONE;
      }
      if (suffixes[0] == "finish") {
        return handleFinishQuery(suffixes[1]);
      } else {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_QUERY_NOT_FOUND,
                      "query with id " + suffixes[1] + " not found");
      }

      break;
    }

    default: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal method for /_api/aql");
      break;
    }
  }

  return RestStatus::DONE;
}

RestStatus RestAqlHandler::continueExecute() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  // extract the sub-request type
  rest::RequestType type = _request->requestType();

  if (type == rest::RequestType::PUT) {
    // This cannot be changed!
    TRI_ASSERT(suffixes.size() == 2);
    TRI_ASSERT(_engine != nullptr);
    return useQuery(suffixes[0], suffixes[1]);
  } else if (type == rest::RequestType::DELETE_REQ && suffixes[0] == "finish") {
    return RestStatus::DONE; // uses futures
  }
  generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                "continued non-continuable method for /_api/aql");

  return RestStatus::DONE;
}

void RestAqlHandler::shutdownExecute(bool isFinalized) noexcept {
  try {
    if (isFinalized && _engine != nullptr) {
      auto qId = _engine->engineId();
      _engine->sharedState()->resetWakeupHandler();
      _engine = nullptr;

      _queryRegistry->closeEngine(qId);
    }
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC("f73b8", INFO, Logger::FIXME)
        << "Ignoring exception during rest handler shutdown: "
        << "[" << ex.code() << "] " << ex.message();
  } catch (std::exception const& ex) {
    LOG_TOPIC("b7335", INFO, Logger::FIXME)
        << "Ignoring exception during rest handler shutdown: " << ex.what();
  } catch (...) {
    LOG_TOPIC("c4db4", INFO, Logger::FIXME)
        << "Ignoring unknown exception during rest handler shutdown.";
  }
}

// dig out the query from ID, handle errors
ExecutionEngine* RestAqlHandler::findEngine(std::string const& idString) {
  TRI_ASSERT(_engine == nullptr);
  uint64_t qId = arangodb::basics::StringUtils::uint64(idString);

  // sleep for 10ms each time, wait for at most 30 seconds...
  static int64_t const SingleWaitPeriod = 10 * 1000;
  static int64_t const MaxIterations =
      static_cast<int64_t>(30.0 * 1000000.0 / static_cast<double>(SingleWaitPeriod));

  int64_t iterations = 0;

  ExecutionEngine* q = nullptr;
  // probably need to cycle here until we can get hold of the query
  while (++iterations < MaxIterations) {
    if (server().isStopping()) {
      // don't loop for long here if we are shutting down anyway
      generateError(ResponseCode::BAD, TRI_ERROR_SHUTTING_DOWN);
      break;
    }
    try {
      q = _queryRegistry->openExecutionEngine(qId);
      // we got the query (or it was not found - at least no one else
      // can now have access to the same query)
      break;
    } catch (...) {
      // we can only get here if the query is currently used by someone
      // else. in this case we sleep for a while and re-try
      std::this_thread::sleep_for(std::chrono::microseconds(SingleWaitPeriod));
    }
  }

  if (q == nullptr) {
    LOG_TOPIC_IF("baef6", ERR, Logger::AQL, iterations == MaxIterations)
        << "Timeout waiting for query " << qId;
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_QUERY_NOT_FOUND,
                  "query ID " + idString + " not found");
  }

  TRI_ASSERT(q == nullptr || q->engineId() == qId);

  return q;
}

class AqlExecuteCall {
 public:
  // Deserializing factory
  static auto fromVelocyPack(VPackSlice slice) -> ResultT<AqlExecuteCall>;

  auto callStack() const noexcept -> AqlCallStack const& { return _callStack; }

 private:
  AqlExecuteCall(AqlCallStack&& callStack) : _callStack(std::move(callStack)) {}

  AqlCallStack _callStack;
};

namespace {
// hack for MSVC
auto getStringView(velocypack::Slice slice) -> std::string_view {
  std::string_view ref = slice.stringView();
  return std::string_view(ref.data(), ref.size());
}
}  // namespace

// TODO Use the deserializer when available
auto AqlExecuteCall::fromVelocyPack(VPackSlice const slice) -> ResultT<AqlExecuteCall> {
  if (ADB_UNLIKELY(!slice.isObject())) {
    using namespace std::string_literals;
    return Result(TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                  "When deserializating AqlExecuteCall: Expected object, got "s +
                      slice.typeName());
  }

  auto expectedPropertiesFound = std::map<std::string_view, bool>{};
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteCallStack, false);

  std::optional<AqlCallStack> callStack;

  for (auto const it : VPackObjectIterator(slice)) {
    auto const keySlice = it.key;
    if (ADB_UNLIKELY(!keySlice.isString())) {
      return Result(TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                    "When deserializating AqlExecuteCall: Key is not a string");
    }
    auto const key = getStringView(keySlice);

    if (auto propIt = expectedPropertiesFound.find(key);
        ADB_LIKELY(propIt != expectedPropertiesFound.end())) {
      if (ADB_UNLIKELY(propIt->second)) {
        return Result(
            TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "When deserializating AqlExecuteCall: Encountered duplicate key");
      }
      propIt->second = true;
    }

    if (key == StaticStrings::AqlRemoteCallStack) {
      auto maybeCallStack = AqlCallStack::fromVelocyPack(it.value);
      if (ADB_UNLIKELY(maybeCallStack.fail())) {
        auto message = std::string{
            "When deserializating AqlExecuteCall: failed to deserialize "};
        message += StaticStrings::AqlRemoteCallStack;
        message += ": ";
        message += maybeCallStack.errorMessage();
        return Result(TRI_ERROR_CLUSTER_AQL_COMMUNICATION, std::move(message));
      }

      callStack = maybeCallStack.get();
    } else {
      LOG_TOPIC("0dd42", WARN, Logger::AQL)
          << "When deserializating AqlExecuteCall: Encountered unexpected key " << key;
      // If you run into this assertion during rolling upgrades after adding a
      // new attribute, remove it in the older version.
      TRI_ASSERT(false);
    }
  }

  for (auto const& it : expectedPropertiesFound) {
    if (ADB_UNLIKELY(!it.second)) {
      auto message =
          std::string{"When deserializating AqlExecuteCall: missing key "};
      message += it.first;
      return Result(TRI_ERROR_CLUSTER_AQL_COMMUNICATION, std::move(message));
    }
  }

  TRI_ASSERT(callStack.has_value());

  return {AqlExecuteCall{std::move(callStack).value()}};
}

// handle for useQuery
RestStatus RestAqlHandler::handleUseQuery(std::string const& operation,
                                          VPackSlice querySlice) {
  bool found;
  std::string const& shardId = _request->header(StaticStrings::AqlShardIdHeader, found);

  auto const rootNodeType = _engine->root()->getPlanNode()->getType();

  VPackBuffer<uint8_t> answerBuffer;
  VPackBuilder answerBuilder(answerBuffer);
  answerBuilder.openObject(/*unindexed*/ true);

  if (operation == StaticStrings::AqlRemoteExecute) {
    auto maybeExecuteCall = AqlExecuteCall::fromVelocyPack(querySlice);
    if (maybeExecuteCall.fail()) {
      generateError(std::move(maybeExecuteCall).result());
      return RestStatus::DONE;
    }
    TRI_IF_FAILURE("RestAqlHandler::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    auto& executeCall = maybeExecuteCall.get();

    auto items = SharedAqlItemBlockPtr{};
    auto skipped = SkipResult{};
    auto state = ExecutionState::HASMORE;

    // shardId is set IFF the root node is scatter or distribute
    TRI_ASSERT(shardId.empty() != (rootNodeType == ExecutionNode::SCATTER ||
                                   rootNodeType == ExecutionNode::DISTRIBUTE));
    if (shardId.empty()) {
      std::tie(state, skipped, items) =
          _engine->execute(executeCall.callStack());
    } else {
      std::tie(state, skipped, items) =
          _engine->executeForClient(executeCall.callStack(), shardId);
    }

    if (state == ExecutionState::WAITING) {
      return RestStatus::WAITING;
    }

    auto result = AqlExecuteResult{state, skipped, std::move(items)};
    answerBuilder.add(VPackValue(StaticStrings::AqlRemoteResult));
    result.toVelocyPack(answerBuilder, &_engine->getQuery().vpackOptions());
    answerBuilder.add(StaticStrings::Code, VPackValue(TRI_ERROR_NO_ERROR));
  } else if (operation == "initializeCursor") {
    auto pos = VelocyPackHelper::getNumericValue<size_t>(querySlice, "pos", 0);
    Result res;
    if (VelocyPackHelper::getBooleanValue(querySlice, "done", true)) {
      auto tmpRes = _engine->initializeCursor(nullptr, 0);
      if (tmpRes.first == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
      res = tmpRes.second;
    } else {
      auto items = _engine->itemBlockManager().requestAndInitBlock(
          querySlice.get("items"));
      auto tmpRes = _engine->initializeCursor(std::move(items), pos);
      if (tmpRes.first == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
      res = tmpRes.second;
    }
    answerBuilder.add(StaticStrings::Error, VPackValue(res.fail()));
    answerBuilder.add(StaticStrings::Code, VPackValue(res.errorNumber()));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  answerBuilder.close();

  VPackOptions const* opts = &VPackOptions::Defaults;
  if (_engine) { // might be destroyed on shutdown
    opts = &_engine->getQuery().vpackOptions();
  }

  generateResult(rest::ResponseCode::OK, std::move(answerBuffer), opts);

  return RestStatus::DONE;
}

// handle query finalization for all engines
RestStatus RestAqlHandler::handleFinishQuery(std::string const& idString) {
  auto qid = arangodb::basics::StringUtils::uint64(idString);
  bool success = false;
  VPackSlice querySlice = this->parseVPackBody(success);
  if (!success) {
    return RestStatus::DONE;
  }

  auto errorCode =
      VelocyPackHelper::getNumericValue<ErrorCode>(querySlice, StaticStrings::Code,
                                                   TRI_ERROR_INTERNAL);
  std::shared_ptr<ClusterQuery> query = _queryRegistry->destroyQuery(_vocbase.name(), qid, errorCode);
  if (!query) {
    // this may be a race between query garbage collection and the client
    // shutting down the query. it is debatable whether this is an actual error
    // if we only want to abort the query...
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  auto f = query->finalizeClusterQuery(errorCode);

  return waitForFuture(std::move(f)
                       .thenValue([me = shared_from_this(), this,
                                   q = std::move(query)](Result res) {
    VPackBufferUInt8 buffer;
    VPackBuilder answerBuilder(buffer);
    answerBuilder.openObject(/*unindexed*/true);
    answerBuilder.add(VPackValue("stats"));
    q->executionStats().toVelocyPack(answerBuilder, q->queryOptions().fullCount);
    q->warnings().toVelocyPack(answerBuilder);
    answerBuilder.add(StaticStrings::Error, VPackValue(res.fail()));
    answerBuilder.add(StaticStrings::Code, VPackValue(res.errorNumber()));
    answerBuilder.close();

    generateResult(rest::ResponseCode::OK, std::move(buffer));
  }));
}

RequestLane RestAqlHandler::lane() const {
  if (ServerState::instance()->isCoordinator()) {
    return RequestLane::CLUSTER_AQL_INTERNAL_COORDINATOR;
  } else {
    return RequestLane::CLUSTER_AQL;
  }
}
