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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestAqlHandler.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngine.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

RestAqlHandler::RestAqlHandler(application_features::ApplicationServer& server,
                               GeneralRequest* request, GeneralResponse* response,
                               std::pair<QueryRegistry*, traverser::TraverserEngineRegistry*>* registries)
    : RestVocbaseBaseHandler(server, request, response),
      _queryRegistry(registries->first),
      _traverserRegistry(registries->second),
      _query(nullptr),
      _qId(0) {
  TRI_ASSERT(_queryRegistry != nullptr);
  TRI_ASSERT(_traverserRegistry != nullptr);
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

std::pair<double, std::shared_ptr<VPackBuilder>> RestAqlHandler::getPatchedOptionsWithTTL(
    VPackSlice const& optionsSlice) const {
  auto options = std::make_shared<VPackBuilder>();
  double ttl = _queryRegistry->defaultTTL();
  {
    VPackObjectBuilder guard(options.get());
    TRI_ASSERT(optionsSlice.isObject());
    for (auto pair : VPackObjectIterator(optionsSlice)) {
      if (pair.key.isEqualString("ttl")) {
        ttl = VelocyPackHelper::getNumericValue<double>(optionsSlice, "ttl", ttl);
        ttl = _request->parsedValue<double>("ttl", ttl);
        if (ttl <= 0) {
          ttl = _queryRegistry->defaultTTL();
        }
        options->add("ttl", VPackValue(ttl));
      } else {
        options->add(pair.key.stringRef(), pair.value);
      }
    }
  }
  return std::make_pair(ttl, options);
}

void RestAqlHandler::setupClusterQuery() {
  // We should not intentionally call this method
  // on the wrong server. So fail during maintanence.
  // On user setup reply gracefully.
  TRI_ASSERT(ServerState::instance()->isDBServer());
  if (!ServerState::instance()->isDBServer()) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
    return;
  }

  // ---------------------------------------------------
  // SECTION:                            body validation
  // ---------------------------------------------------
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
  // If we have a new format then it has to be included here.
  // If not default to classic (old coordinator will not send it)
  SerializationFormat format = static_cast<SerializationFormat>(
      VelocyPackHelper::getNumericValue<int>(querySlice, StaticStrings::SerializationFormat,
                                             static_cast<int>(SerializationFormat::CLASSIC)));
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

  std::shared_ptr<VPackBuilder> options;
  double ttl;
  std::tie(ttl, options) = getPatchedOptionsWithTTL(optionsSlice);

  // Build the collection information
  VPackBuilder collectionBuilder;
  collectionBuilder.openArray();
  for (auto const& lockInf : VPackObjectIterator(lockInfoSlice)) {
    if (!lockInf.value.isArray()) {
      LOG_TOPIC("1dc00", ERR, arangodb::Logger::AQL)
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
        LOG_TOPIC("9e29f", ERR, arangodb::Logger::AQL)
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
    }
  }
  collectionBuilder.close();

  // creates a StandaloneContext or a leased context
  auto ctx = createTransactionContext();

  VPackBuilder answerBuilder;
  answerBuilder.openObject();
  bool needToLock = true;
  bool res = registerSnippets(snippetsSlice, collectionBuilder.slice(), variablesSlice,
                              options, ctx, ttl, format, needToLock, answerBuilder);
  if (!res) {
    // TODO we need to trigger cleanup here??
    // Registering the snippets failed.
    return;
  }

  if (!traverserSlice.isNone()) {
    res = registerTraverserEngines(traverserSlice, ctx, ttl, needToLock, answerBuilder);

    if (!res) {
      // TODO we need to trigger cleanup here??
      // Registering the traverser engines failed.
      return;
    }
  }

  answerBuilder.close();

  generateOk(rest::ResponseCode::OK, answerBuilder.slice());
}

bool RestAqlHandler::registerSnippets(
    VPackSlice const snippetsSlice, VPackSlice const collectionSlice,
    VPackSlice const variablesSlice, std::shared_ptr<VPackBuilder> const& options,
    std::shared_ptr<transaction::Context> const& ctx, double const ttl,
    SerializationFormat format, bool& needToLock, VPackBuilder& answerBuilder) {
  TRI_ASSERT(answerBuilder.isOpenObject());
  answerBuilder.add(VPackValue("snippets"));
  answerBuilder.openObject();
  // NOTE: We need to clean up all engines if we bail out during the following
  // loop
  for (auto it : VPackObjectIterator(snippetsSlice)) {
    auto planBuilder = std::make_shared<VPackBuilder>();
    planBuilder->openObject();
    planBuilder->add(VPackValue("collections"));
    planBuilder->add(collectionSlice);

    planBuilder->add(VPackValue("nodes"));
    planBuilder->add(it.value.get("nodes"));

    planBuilder->add(VPackValue("variables"));
    planBuilder->add(variablesSlice);
    planBuilder->close();  // base-object

    // All snippets know all collections.
    // The first snippet will provide proper locking
    auto query = std::make_unique<Query>(false, _vocbase, planBuilder, options,
                                         (needToLock ? PART_MAIN : PART_DEPENDENT));

    // enables the query to get the correct transaction
    query->setTransactionContext(ctx);

    try {
      query->prepare(_queryRegistry, format);
    } catch (std::exception const& ex) {
      LOG_TOPIC("c1ce7", ERR, arangodb::Logger::AQL)
          << "failed to instantiate the query: " << ex.what();
      generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN, ex.what());
      return false;
    } catch (...) {
      LOG_TOPIC("aae22", ERR, arangodb::Logger::AQL)
          << "failed to instantiate the query";
      generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN);
      return false;
    }

    try {
      if (needToLock) {
        // Directly try to lock only the first snippet is required to be locked.
        // For all others locking is pointless
        needToLock = false;

        try {
          int res = query->trx()->lockCollections();
          if (res != TRI_ERROR_NO_ERROR) {
            generateError(rest::ResponseCode::SERVER_ERROR, res, TRI_errno_string(res));
            return false;
          }
        } catch (basics::Exception const& e) {
          generateError(rest::ResponseCode::SERVER_ERROR, e.code(), e.message());
          return false;
        } catch (...) {
          generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                        "Unable to lock all collections.");
          return false;
        }
        // If we get here we successfully locked the collections.
        // If we bail out up to this point nothing is kept alive.
        // No need to cleanup...
      }

      QueryId qId = query->id();  // not true in general
      TRI_ASSERT(qId > 0);
      _queryRegistry->insert(qId, query.get(), ttl, true, false);
      query.release();
      answerBuilder.add(it.key);
      answerBuilder.add(VPackValue(arangodb::basics::StringUtils::itoa(qId)));
    } catch (...) {
      LOG_TOPIC("e7ea6", ERR, arangodb::Logger::AQL)
          << "could not keep query in registry";
      generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                    "could not keep query in registry");
      return false;
    }
  }
  answerBuilder.close();  // Snippets

  return true;
}

bool RestAqlHandler::registerTraverserEngines(VPackSlice const traverserEngines,
                                              std::shared_ptr<transaction::Context> const& ctx,
                                              double ttl, bool& needToLock,
                                              VPackBuilder& answerBuilder) {
  TRI_ASSERT(traverserEngines.isArray());

  TRI_ASSERT(answerBuilder.isOpenObject());
  answerBuilder.add(VPackValue("traverserEngines"));
  answerBuilder.openArray();

  for (auto const& te : VPackArrayIterator(traverserEngines)) {
    try {
      auto id = _traverserRegistry->createNew(_vocbase, ctx, te, ttl, needToLock);

      needToLock = false;
      TRI_ASSERT(id != 0);
      answerBuilder.add(VPackValue(id));
    } catch (basics::Exception const& e) {
      LOG_TOPIC("f257c", ERR, arangodb::Logger::AQL)
          << "Failed to instanciate traverser engines. Reason: " << e.message();
      generateError(rest::ResponseCode::SERVER_ERROR, e.code(), e.message());
      return false;
    } catch (...) {
      LOG_TOPIC("4087b", ERR, arangodb::Logger::AQL)
          << "Failed to instanciate traverser engines.";
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                    "Unable to instanciate traverser engines");
      return false;
    }
  }
  answerBuilder.close();  // traverserEngines
  // Everything went well
  return true;
}

// DELETE method for /_api/aql/kill/<queryId>, (internal)
bool RestAqlHandler::killQuery(std::string const& idString) {
  _qId = arangodb::basics::StringUtils::uint64(idString);
  return _queryRegistry->kill(&_vocbase, _qId);
}

// PUT method for /_api/aql/<operation>/<queryId>, (internal)
// this is using the part of the cursor API with side effects.
// <operation>: can be "lock" or "getSome" or "skip" or "initializeCursor" or
// "shutdown".
// The body must be a Json with the following attributes:
// For the "getSome" operation one has to give:
//   "atMost": must be a positive integer, the cursor returns never
//             more than "atMost" items. The result is the JSON representation
//             of an AqlItemBlock.
//             If "atMost" is not given it defaults to
//             ExecutionBlock::DefaultBatchSize.
// For the "skipSome" operation one has to give:
//   "atMost": must be a positive integer, the cursor skips never
//             more than "atMost" items. The result is a JSON object with a
//             single attribute "skipped" containing the number of
//             skipped items.
//             If "atMost" is not given it defaults to
//             ExecutionBlock::DefaultBatchSize.
// For the "skip" operation one should give:
//   "number": must be a positive integer, the cursor skips as many items,
//             possibly exhausting the cursor.
//             The result is a JSON with the attributes "error" (boolean),
//             "errorMessage" (if applicable) and "done" (boolean)
//             to indicate whether or not the cursor is exhausted.
//             If "number" is not given it defaults to 1.
// For the "initializeCursor" operation, one has to bind the following
// attributes:
//   "items": This is a serialized AqlItemBlock with usually only one row
//            and the correct number of columns.
//   "pos":   The number of the row in "items" to take, usually 0.
// For the "shutdown" and "lock" operations no additional arguments are
// required and an empty JSON object in the body is OK.
// All operations allow to set the HTTP header "Shard-ID:". If this is
// set, then the root block of the stored query must be a ScatterBlock
// and the shard ID is given as an additional argument to the ScatterBlock's
// special API.
RestStatus RestAqlHandler::useQuery(std::string const& operation, std::string const& idString) {
  bool success = false;
  VPackSlice querySlice = this->parseVPackBody(success);
  if (!success) {
    return RestStatus::DONE;
  }

  if (!_query) {  // the PUT verb
    TRI_ASSERT(this->state() == RestHandler::HandlerState::EXECUTE);

    _query = findQuery(idString);
    if (!_query) {
      return RestStatus::DONE;
    }
    std::shared_ptr<SharedQueryState> ss = _query->sharedState();
    ss->setWakeupHandler(
        [self = shared_from_this()] { return self->wakeupHandler(); });
  }

  TRI_ASSERT(_qId > 0);
  TRI_ASSERT(_query != nullptr);
  TRI_ASSERT(_query->engine() != nullptr);

  if (_query->queryOptions().profile >= PROFILE_LEVEL_TRACE_1) {
    LOG_TOPIC("1bf67", INFO, Logger::QUERIES)
        << "[query#" << _qId << "] remote request received: " << operation
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
      } else {
        if (killQuery(suffixes[1])) {
          VPackBuilder answerBody;
          {
            VPackObjectBuilder guard(&answerBody);
            answerBody.add(StaticStrings::Error, VPackValue(false));
          }
          generateResult(rest::ResponseCode::OK, answerBody.slice());
        } else {
          generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_QUERY_NOT_FOUND);
        }
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
    TRI_ASSERT(_query != nullptr);
    return useQuery(suffixes[0], suffixes[1]);
  }
  generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                "continued non-continuable method for /_api/aql");

  return RestStatus::DONE;
}

void RestAqlHandler::shutdownExecute(bool isFinalized) noexcept {
  try {
    if (isFinalized) {
      if (_query) {
        _query->sharedState()->resetWakeupHandler();
      }
      if (_qId != 0) {
        _queryRegistry->close(&_vocbase, _qId);
      }
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
Query* RestAqlHandler::findQuery(std::string const& idString) {
  TRI_ASSERT(_qId == 0);
  _qId = arangodb::basics::StringUtils::uint64(idString);

  // sleep for 10ms each time, wait for at most 30 seconds...
  static int64_t const SingleWaitPeriod = 10 * 1000;
  static int64_t const MaxIterations =
      static_cast<int64_t>(30.0 * 1000000.0 / (double)SingleWaitPeriod);

  int64_t iterations = 0;

  Query* q = nullptr;
  // probably need to cycle here until we can get hold of the query
  while (++iterations < MaxIterations) {
    try {
      q = _queryRegistry->open(&_vocbase, _qId);
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
        << "Timeout waiting for query " << _qId;
    _qId = 0;
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_QUERY_NOT_FOUND);
    return q;
  }

  TRI_ASSERT(_qId > 0);

  return q;
}

// handle for useQuery
RestStatus RestAqlHandler::handleUseQuery(std::string const& operation,
                                          VPackSlice const querySlice) {
  std::string const& shardId = _request->header("shard-id");

  // upon first usage, the "initializeCursor" method must be called
  // note: if the operation is "initializeCursor" itself, we do not initialize
  // the cursor here but let the case for "initializeCursor" process it.
  // this is because the request may contain additional data
  if ((operation == "getSome" || operation == "skipSome") &&
      !_query->engine()->initializeCursorCalled()) {
    TRI_IF_FAILURE("RestAqlHandler::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    auto res = _query->engine()->initializeCursor(nullptr, 0);
    if (res.first == ExecutionState::WAITING) {
      return RestStatus::WAITING;
    }
    if (!res.second.ok()) {
      generateError(GeneralResponse::responseCode(res.second.errorNumber()),
                    res.second.errorNumber(),
                    "cannot initialize cursor for AQL query");
      return RestStatus::DONE;
    }
  }

  auto transactionContext = _query->trx()->transactionContext();

  VPackBuffer<uint8_t> answerBuffer;
  VPackBuilder answerBuilder(answerBuffer);
  answerBuilder.openObject(/*unindexed*/ true);

  if (operation == "getSome") {
    TRI_IF_FAILURE("RestAqlHandler::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    auto atMost =
        VelocyPackHelper::getNumericValue<size_t>(querySlice, "atMost",
                                                  ExecutionBlock::DefaultBatchSize());
    SharedAqlItemBlockPtr items;
    ExecutionState state;
    if (shardId.empty()) {
      std::tie(state, items) = _query->engine()->getSome(atMost);
      if (state == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
    } else {
      auto block = dynamic_cast<BlocksWithClients*>(_query->engine()->root());
      if (block == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected node type");
      }
      TRI_ASSERT(block->getPlanNode()->getType() == ExecutionNode::SCATTER ||
                 block->getPlanNode()->getType() == ExecutionNode::DISTRIBUTE);
      std::tie(state, items) = block->getSomeForShard(atMost, shardId);
      if (state == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
    }
    // Used in 3.4.0 onwards.
    answerBuilder.add("done", VPackValue(state == ExecutionState::DONE));
    answerBuilder.add(StaticStrings::Code, VPackValue(TRI_ERROR_NO_ERROR));
    if (items.get() == nullptr) {
      // Backwards Compatibility
      answerBuilder.add(StaticStrings::Error, VPackValue(false));
    } else {
      items->toVelocyPack(_query->trx()->transactionContextPtr()->getVPackOptions(),
                          answerBuilder);
    }
  } else if (operation == "skipSome") {
    auto atMost =
        VelocyPackHelper::getNumericValue<size_t>(querySlice, "atMost",
                                                  ExecutionBlock::DefaultBatchSize());
    size_t skipped;
    if (shardId.empty()) {
      auto tmpRes = _query->engine()->skipSome(atMost);
      if (tmpRes.first == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
      skipped = tmpRes.second;
    } else {
      auto block = dynamic_cast<BlocksWithClients*>(_query->engine()->root());
      if (block == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected node type");
      }
      TRI_ASSERT(block->getPlanNode()->getType() == ExecutionNode::SCATTER ||
                 block->getPlanNode()->getType() == ExecutionNode::DISTRIBUTE);

      auto tmpRes = block->skipSomeForShard(atMost, shardId);
      if (tmpRes.first == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
      skipped = tmpRes.second;
    }
    answerBuilder.add("skipped", VPackValue(skipped));
    answerBuilder.add(StaticStrings::Error, VPackValue(false));
  } else if (operation == "initializeCursor") {
    auto pos = VelocyPackHelper::getNumericValue<size_t>(querySlice, "pos", 0);
    Result res;
    if (VelocyPackHelper::getBooleanValue(querySlice, "done", true)) {
      auto tmpRes = _query->engine()->initializeCursor(nullptr, 0);
      if (tmpRes.first == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
      res = tmpRes.second;
    } else {
      auto items = _query->engine()->itemBlockManager().requestAndInitBlock(
          querySlice.get("items"));
      auto tmpRes = _query->engine()->initializeCursor(std::move(items), pos);
      if (tmpRes.first == ExecutionState::WAITING) {
        return RestStatus::WAITING;
      }
      res = tmpRes.second;
    }
    answerBuilder.add(StaticStrings::Error, VPackValue(res.fail()));
    answerBuilder.add(StaticStrings::Code, VPackValue(res.errorNumber()));
  } else if (operation == "shutdown") {
    int errorCode = VelocyPackHelper::getNumericValue<int>(querySlice, StaticStrings::Code,
                                                           TRI_ERROR_INTERNAL);

    ExecutionState state;
    Result res;
    std::tie(state, res) = _query->engine()->shutdown(errorCode);
    if (state == ExecutionState::WAITING) {
      return RestStatus::WAITING;
    }

    // return statistics
    answerBuilder.add(VPackValue("stats"));
    _query->getStats(answerBuilder);

    // return warnings if present
    _query->addWarningsToVelocyPack(answerBuilder);
    _query->sharedState()->resetWakeupHandler();
    _query = nullptr;

    // return the query to the registry
    _queryRegistry->close(&_vocbase, _qId);

    // delete the query from the registry
    _queryRegistry->destroy(_vocbase.name(), _qId, errorCode, false);
    _qId = 0;
    answerBuilder.add(StaticStrings::Error, VPackValue(res.fail()));
    answerBuilder.add(StaticStrings::Code, VPackValue(res.errorNumber()));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  answerBuilder.close();
  generateResult(rest::ResponseCode::OK, std::move(answerBuffer), transactionContext);

  return RestStatus::DONE;
}
