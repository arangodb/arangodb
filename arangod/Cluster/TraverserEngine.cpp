////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraverserEngine.h"
#include "Aql/AqlTransaction.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Transaction/Context.h"
#include "Utils/CollectionNameResolver.h"


#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessMethods.h"
#endif

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

static const std::string OPTIONS = "options";
static const std::string INACCESSIBLE = "inaccessible";
static const std::string SHARDS = "shards";
static const std::string EDGES = "edges";
static const std::string TYPE = "type";
static const std::string VARIABLES = "variables";
static const std::string VERTICES = "vertices";

namespace {
void parseLookupInfo(aql::QueryContext& query, VPackSlice conditionsArray,
                     VPackSlice shards,
                     bool lastInFirstOut,
                     std::vector<IndexAccessor>& vectorToInsert) {
  TRI_ASSERT(conditionsArray.isArray());
  std::vector<IndexAccessor> indexesWithPriority;

  for (size_t i = 0; i < conditionsArray.length(); ++i) {
    auto oneInfo = conditionsArray.at(i);
    transaction::Methods::IndexHandle idx;
    {
      // Parse Condition Member to Update
      bool conditionNeedUpdate =
          arangodb::basics::VelocyPackHelper::getBooleanValue(
              oneInfo, "condNeedUpdate", false);
      std::optional<size_t> memberToUpdate =
          conditionNeedUpdate
              ? std::optional(
                    arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
                        oneInfo, "condMemberToUpdate", 0))
              : std::nullopt;

      // Parse the Condition
      auto cond = oneInfo.get("condition");
      if (!cond.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "Each lookup requires condition to be an object");
      }
      aql::AstNode* condition = query.ast()->createNode(cond);

      VPackSlice read = oneInfo.get("handle");
      if (!read.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "Each lookup requires handle to be an object");
      }

      // Parse the IndexHandle
      read = read.get("id");
      if (!read.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER, "Each handle requires id to be a string");
      }
      std::string idxId = read.copyString();
      aql::Collections const& collections = query.collections();

      TRI_edge_direction_e dir = TRI_EDGE_ANY;
      VPackSlice dirSlice = oneInfo.get(StaticStrings::GraphDirection);
      if (dirSlice.isEqualString(StaticStrings::GraphDirectionInbound)) {
        dir = TRI_EDGE_IN;
      } else if (dirSlice.isEqualString(
                     StaticStrings::GraphDirectionOutbound)) {
        dir = TRI_EDGE_OUT;
      }
      if (dir == TRI_EDGE_ANY) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "Missing or invalid direction attribute in graph definition");
      }

      auto allShards = shards.at(i);
      TRI_ASSERT(allShards.isArray());
      for (auto const it : VPackArrayIterator(allShards)) {
        if (!it.isString()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                         "Shards have to be a list of strings");
        }
        auto* coll = collections.get(it.copyString());
        if (!coll) {
          TRI_ASSERT(false);
          THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
        }

        read = oneInfo.get("expression");
        std::unique_ptr<aql::Expression> exp =
            read.isObject()
                ? std::make_unique<aql::Expression>(query.ast(), read)
                : nullptr;

        read = oneInfo.get("nonConstContainer");
        std::optional<aql::NonConstExpressionContainer> nonConstPart{
            std::nullopt};
        if (read.isObject()) {
          nonConstPart.emplace(aql::NonConstExpressionContainer::fromVelocyPack(
              query.ast(), read));
        }
        if (coll->getCollection()->isLocalSmartEdgeCollection()) {
          indexesWithPriority.emplace_back(
              coll->indexByIdentifier(idxId), condition, memberToUpdate,
              std::move(exp), std::move(nonConstPart), i, dir);
        } else {
          vectorToInsert.emplace_back(coll->indexByIdentifier(idxId), condition,
                                      memberToUpdate, std::move(exp),
                                      std::move(nonConstPart), i, dir);
        }
      }
    }
  }

  if (lastInFirstOut) {
    // Push priority indexes to the end of the list.
    // So they are injected last => We work on them first
    std::move(indexesWithPriority.begin(), indexesWithPriority.end(),
              std::back_inserter(vectorToInsert));
  } else {
    // Push priority indexes to the beginning of the list.
    // So they are injected first => We work on them first

    // As vectors can only append, we append the "non-priorities" into
    // priority, and then swap the vectors back
    std::move(vectorToInsert.begin(), vectorToInsert.end(),
              std::back_inserter(indexesWithPriority));
    vectorToInsert.swap(indexesWithPriority);
  }
}

aql::TraversalStats outputVertex(
    graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>&
        provider,
    VPackBuilder& builder, VPackSlice vertex, size_t depth) {
  TRI_ASSERT(vertex.isString());
  auto start = provider.startVertex(
      arangodb::velocypack::HashedStringRef{vertex}, depth);
  provider.expand(start, 0, [&](SingleServerProviderStep neighbor) {
    provider.insertEdgeIntoResult(neighbor.getEdgeIdentifier(), builder);
  });
  auto stats = provider.stealStats();
  provider.clear();
  return stats;
}

std::unique_ptr<graph::ShortestPathOptions> parseShortestPathOptions(
    arangodb::velocypack::Slice info, aql::QueryContext& query) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires an " + OPTIONS + " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + OPTIONS + " require a " + TYPE + " attribute.");
  }
  TRI_ASSERT(type.isEqualString("shortestPath"));
  auto opts =
      std::make_unique<ShortestPathOptions>(query, optsSlice, edgesSlice);
  // We create the cache, but we do not need any engines.
  opts->activateCache(false, nullptr);
  return opts;
}

std::unique_ptr<traverser::TraverserOptions> parseTraverserPathOptions(
    arangodb::velocypack::Slice info, aql::QueryContext& query) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (!optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires an " + OPTIONS + " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + OPTIONS + " require a " + TYPE + " attribute.");
  }
  TRI_ASSERT(type.isEqualString("traversal"));
  auto opts = std::make_unique<TraverserOptions>(query, optsSlice, edgesSlice);
  // We create the cache, but we do not need any engines.
  opts->activateCache(false, nullptr);
  return opts;
}
}  // namespace

#ifndef USE_ENTERPRISE
/*static*/ std::unique_ptr<BaseEngine> BaseEngine::BuildEngine(
    TRI_vocbase_t& vocbase, aql::QueryContext& query, VPackSlice info) {
  VPackSlice type = info.get(std::vector<std::string>({OPTIONS, TYPE}));

  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires an 'options.type' attribute.");
  }

  if (type.isEqualString("traversal")) {
    return std::make_unique<TraverserEngine>(vocbase, query, info);
  } else if (type.isEqualString("shortestPath")) {
    return std::make_unique<ShortestPathEngine>(vocbase, query, info);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "The 'options.type' attribute either has to "
                                 "be traversal or shortestPath");
}
#endif

BaseEngine::BaseEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
                       VPackSlice info)
    : _engineId(TRI_NewTickServer()), _query(query) {
  VPackSlice shardsSlice = info.get(SHARDS);

  if (shardsSlice.isNone() || !shardsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires a " + SHARDS + " attribute.");
  }

  VPackSlice edgesSlice = shardsSlice.get(EDGES);

  if (edgesSlice.isNone() || !edgesSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + SHARDS + " object requires an " + EDGES + " attribute.");
  }

  VPackSlice vertexSlice = shardsSlice.get(VERTICES);

  if (vertexSlice.isNone() || !vertexSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + SHARDS + " object requires a " + VERTICES + " attribute.");
  }

  // Add all Edge shards to the transaction
  TRI_ASSERT(edgesSlice.isArray());
  for (VPackSlice const shardList : VPackArrayIterator(edgesSlice)) {
    TRI_ASSERT(shardList.isArray());
    for (VPackSlice const shard : VPackArrayIterator(shardList)) {
      TRI_ASSERT(shard.isString());
      _query.collections().add(shard.copyString(), AccessMode::Type::READ,
                               aql::Collection::Hint::Shard);
    }
  }

  // Add all Vertex shards to the transaction
  TRI_ASSERT(vertexSlice.isObject());
  for (auto const& collection : VPackObjectIterator(vertexSlice)) {
    std::vector<std::string> shards;
    TRI_ASSERT(collection.value.isArray());
    for (VPackSlice const shard : VPackArrayIterator(collection.value)) {
      TRI_ASSERT(shard.isString());
      std::string name = shard.copyString();
      _query.collections().add(name, AccessMode::Type::READ,
                               aql::Collection::Hint::Shard);
      shards.emplace_back(std::move(name));
    }
    _vertexShards.try_emplace(collection.key.copyString(), std::move(shards));
  }

#ifdef USE_ENTERPRISE
  if (_query.queryOptions().transactionOptions.skipInaccessibleCollections) {
    _trx = std::make_unique<transaction::IgnoreNoAccessMethods>(
        _query.newTrxContext(), _query.queryOptions().transactionOptions);
  } else {
    _trx = std::make_unique<transaction::Methods>(
        _query.newTrxContext(), _query.queryOptions().transactionOptions);
  }
#else
  _trx = std::make_unique<transaction::Methods>(
      _query.newTrxContext(), _query.queryOptions().transactionOptions);
#endif
}

BaseEngine::~BaseEngine() = default;

std::shared_ptr<transaction::Context> BaseEngine::context() const {
  return _trx->transactionContext();
}

void BaseEngine::getVertexData(VPackSlice vertex, VPackBuilder& builder,
                               bool nestedOutput) {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  std::uint64_t read = 0;
  bool shouldProduceVertices = this->produceVertices();

  auto workOnOneDocument = [&](VPackSlice v) {
    if (v.isNull()) {
      return;
    }

    std::string_view id = v.stringView();
    size_t pos = id.find('/');
    if (pos == std::string::npos || pos + 1 == id.size()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_GRAPH_INVALID_EDGE,
          "edge contains invalid value " + std::string(id));
    }
    std::string shardName = std::string(id.substr(0, pos));
    auto shards = _vertexShards.find(shardName);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "collection not known to traversal: '" +
                                         shardName + "'. please add 'WITH " +
                                         shardName +
                                         "' as the first line in your AQL");
      // The collection is not known here!
      // Maybe handle differently
    }

    if (shouldProduceVertices) {
      std::string_view vertex = id.substr(pos + 1);
      for (std::string const& shard : shards->second) {
        Result res = _trx->documentFastPathLocal(
            shard, vertex, [&](LocalDocumentId const&, VPackSlice doc) {
              // FOUND. short circuit.
              read++;
              builder.add(v);
              if (!options().getVertexProjections().empty()) {
                VPackObjectBuilder guard(&builder);
                options().getVertexProjections().toVelocyPackFromDocument(
                    builder, doc, _trx.get());
              } else {
                builder.add(doc);
              }
              return true;
            });
        if (res.ok()) {
          break;
        }
        if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
          // We are in a very bad condition here...
          THROW_ARANGO_EXCEPTION(res);
        }
      }
    }
  };

  builder.openObject();

  if (nestedOutput) {
    builder.add(VPackValue("vertices"));

    if (vertex.isArray()) {
      builder.openArray(true);
    }
  }

  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      workOnOneDocument(v);
    }
  } else {
    workOnOneDocument(vertex);
  }

  if (nestedOutput) {
    if (vertex.isArray()) {
      builder.close();
    }
    // statistics
    builder.add("readIndex", VPackValue(read));
    builder.add("filtered", VPackValue(0));
    // TODO: wire these counters
    builder.add("cacheHits", VPackValue(0));
    builder.add("cacheMisses", VPackValue(0));
    // intentionally not sending "cursorsCreated" and "cursorsRearmed"
    // here, as we are not using a cursor. the caller can handle that.
  }
  builder.close();
}

std::pair<std::vector<IndexAccessor>,
          std::unordered_map<uint64_t, std::vector<IndexAccessor>>>
BaseEngine::parseIndexAccessors(VPackSlice info,
                                bool lastInFirstOut) const {
  std::vector<IndexAccessor> result;
  std::unordered_map<uint64_t, std::vector<IndexAccessor>> depthLookupInfo;

  auto lookupInfos = info.get("options").get("baseLookupInfos");
  auto depthLookupInfos = info.get("options").get("depthLookupInfo");
  auto shards = info.get("shards").get("edges");

  TRI_ASSERT(lookupInfos.length() == shards.length());
  result.reserve(lookupInfos.length());

  // first, parse the baseLookupInfos (which are globally valid)
  ::parseLookupInfo(_query, lookupInfos, shards, lastInFirstOut, result);

  // now, parse the depthLookupInfo (which are depth based)
  if (!depthLookupInfos.isNone()) {
    if (!depthLookupInfos.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The options require depthLookupInfo to be an object");
    }

    for (auto const& obj : VPackObjectIterator(depthLookupInfos)) {
      TRI_ASSERT(obj.value.isArray());
      TRI_ASSERT(obj.key.isString());
      uint64_t depth = basics::StringUtils::uint64(obj.key.stringView());
      auto [it, emplaced] =
          depthLookupInfo.try_emplace(depth, std::vector<IndexAccessor>());
      TRI_ASSERT(emplaced);
      ::parseLookupInfo(_query, obj.value, shards, lastInFirstOut, depthLookupInfo.at(depth));
    }
  }

  return std::make_pair(std::move(result), std::move(depthLookupInfo));
}

arangodb::graph::SingleServerBaseProviderOptions
BaseEngine::produceProviderOptions(VPackSlice info,
                                   bool lastInFirstOut) {
  return {options().tmpVar(),
          parseIndexAccessors(info, lastInFirstOut),
          options().getExpressionCtx(),
          {},
          _vertexShards,
          options().getVertexProjections(),
          options().getEdgeProjections()};
}

BaseTraverserEngine::BaseTraverserEngine(TRI_vocbase_t& vocbase,
                                         aql::QueryContext& query,
                                         VPackSlice info)
    : BaseEngine(vocbase, query, info),
      _opts{::parseTraverserPathOptions(info, query)},
      _variables(query.ast()->variables()) {}

BaseTraverserEngine::~BaseTraverserEngine() = default;

bool BaseTraverserEngine::produceVertices() const {
  return _opts->produceVertices();
}

aql::VariableGenerator const* BaseTraverserEngine::variables() const {
  return _variables;
}

graph::BaseOptions& BaseTraverserEngine::options() {
  TRI_ASSERT(_opts != nullptr);
  return *_opts;
}

void BaseTraverserEngine::injectVariables(VPackSlice variableSlice) {
  if (variableSlice.isArray()) {
    _opts->clearVariableValues();
    for (auto&& pair : VPackArrayIterator(variableSlice)) {
      if ((!pair.isArray()) || pair.length() != 2) {
        // Invalid communication. Skip
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Internal Traverser communication "
                                       "broken. Failed to inject variables.");
      }
      auto varId =
          arangodb::basics::VelocyPackHelper::getNumericValue<aql::VariableId>(
              pair.at(0), "id", 0);
      aql::Variable* var = variables()->getVariable(varId);
      TRI_ASSERT(var != nullptr);
      // register temporary variables in expression context
      _opts->setVariableValue(
          var, aql::AqlValue{aql::AqlValueHintSliceNoCopy{pair.at(1)}});
    }
    _opts->calculateIndexExpressions(_query.ast());
  }
}

ShortestPathEngine::ShortestPathEngine(TRI_vocbase_t& vocbase,
                                       aql::QueryContext& query,
                                       arangodb::velocypack::Slice info)
    : BaseEngine(vocbase, query, info),
      _opts(::parseShortestPathOptions(info, query)),
      _forwardProvider(query, produceProviderOptions(info, false),
                query.resourceMonitor()),
      _backwardProvider(query, produceReverseProviderOptions(info, false),
                query.resourceMonitor()) {}

ShortestPathEngine::~ShortestPathEngine() = default;

std::pair<std::vector<IndexAccessor>,
          std::unordered_map<uint64_t, std::vector<IndexAccessor>>>
ShortestPathEngine::parseReverseIndexAccessors(VPackSlice info,
                                               bool lastInFirstOut) const {
  std::vector<IndexAccessor> result;
  std::unordered_map<uint64_t, std::vector<IndexAccessor>> depthLookupInfo;

  auto lookupInfos = info.get("options").get("reverseLookupInfos");

  if (!lookupInfos.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a reverseLookupInfos");
  }
  auto depthLookupInfos = info.get("options").get("reversDepthLookupInfos");
  auto shards = info.get("shards").get("edges");

  TRI_ASSERT(lookupInfos.length() == shards.length());
  result.reserve(lookupInfos.length());

  // first, parse the baseLookupInfos (which are globally valid)
  ::parseLookupInfo(_query, lookupInfos, shards, lastInFirstOut, result);

  // now, parse the depthLookupInfo (which are depth based)
  if (!depthLookupInfos.isNone()) {
    if (!depthLookupInfos.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The options require depthLookupInfo to be an object");
    }

    for (auto const& obj : VPackObjectIterator(depthLookupInfos)) {
      TRI_ASSERT(obj.value.isArray());
      TRI_ASSERT(obj.key.isString());
      uint64_t depth = basics::StringUtils::uint64(obj.key.stringView());
      auto [it, emplaced] =
          depthLookupInfo.try_emplace(depth, std::vector<IndexAccessor>());
      TRI_ASSERT(emplaced);
      ::parseLookupInfo(_query, obj.value, shards, lastInFirstOut,
                        depthLookupInfo.at(depth));
    }
  }

  return std::make_pair(std::move(result), std::move(depthLookupInfo));
}

arangodb::graph::SingleServerBaseProviderOptions
ShortestPathEngine::produceReverseProviderOptions(VPackSlice info,
                                                  bool lastInFirstOut) {
  return {options().tmpVar(),
          parseReverseIndexAccessors(info, lastInFirstOut),
          _opts->getExpressionCtx(),
          {},
          _vertexShards,
          options().getVertexProjections(),
          options().getEdgeProjections()};
}

void ShortestPathEngine::getEdges(VPackSlice vertex, bool backward,
                                  VPackBuilder& builder) {
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  auto& provider = backward ? _backwardProvider : _forwardProvider;

  aql::TraversalStats stats{};
  builder.openObject();
  builder.add("edges", VPackValue(VPackValueType::Array));
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      if (!v.isString()) {
        continue;
      }
      stats += ::outputVertex(provider, builder, v, 0);
    }
  } else if (vertex.isString()) {
    stats += ::outputVertex(provider, builder, vertex, 0);
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();

  // statistics
  arangodb::velocypack::serialize(builder, stats);
  builder.close();
}

graph::BaseOptions& ShortestPathEngine::options() {
  TRI_ASSERT(_opts != nullptr);
  return *_opts;
}

TraverserEngine::TraverserEngine(TRI_vocbase_t& vocbase,
                                 aql::QueryContext& query,
                                 arangodb::velocypack::Slice info)
    : BaseTraverserEngine(vocbase, query, info),
      _provider(query, produceProviderOptions(info, false),
                query.resourceMonitor()) {}

TraverserEngine::~TraverserEngine() = default;

void TraverserEngine::getEdges(VPackSlice vertex, size_t depth,
                                   VPackBuilder& builder) {
  aql::TraversalStats stats{};
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  builder.openObject();
  builder.add(VPackValue("edges"));
  builder.openArray(true);
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      stats += ::outputVertex(_provider, builder, v, depth);
    }
  } else if (vertex.isString()) {
    stats += ::outputVertex(_provider, builder, vertex, depth);
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();

  // statistics
  arangodb::velocypack::serialize(builder, stats);
  builder.close();
}

void TraverserEngine::smartSearch(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}

void TraverserEngine::smartSearchUnified(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}
