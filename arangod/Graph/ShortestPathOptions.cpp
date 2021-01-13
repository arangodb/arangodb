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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShortestPathOptions.h"

#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Cluster/ClusterMethods.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/SingleServerEdgeCursor.h"
#include "Indexes/Index.h"
#include "Transaction/Helpers.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;
using namespace arangodb::traverser;
using VPackHelper = arangodb::basics::VelocyPackHelper;

ShortestPathOptions::ShortestPathOptions(aql::QueryContext& query)
    : BaseOptions(query),
      minDepth(1),
      maxDepth(1),
      weightAttribute(""),
      defaultWeight(1),
      bidirectional(true),
      multiThreaded(true) {}

ShortestPathOptions::ShortestPathOptions(aql::QueryContext& query, VPackSlice const& info)
    : ShortestPathOptions(query) {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice type = info.get("type");
  TRI_ASSERT(type.isString());
  TRI_ASSERT(type.isEqualString("shortestPath"));
#endif
  minDepth = VPackHelper::getNumericValue<uint64_t>(info, "minDepth", 1);
  maxDepth = VPackHelper::getNumericValue<uint64_t>(info, "maxDepth", 1);
  weightAttribute =
      VelocyPackHelper::getStringValue(info, "weightAttribute", "");
  defaultWeight =
      VelocyPackHelper::getNumericValue<double>(info, "defaultWeight", 1);
}

ShortestPathOptions::ShortestPathOptions(aql::QueryContext& query,
                                         VPackSlice info, VPackSlice collections)
    : BaseOptions(query, info, collections),
      weightAttribute(""),
      defaultWeight(1),
      bidirectional(true),
      multiThreaded(true) {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice type = info.get("type");
  TRI_ASSERT(type.isString());
  TRI_ASSERT(type.isEqualString("shortestPath"));
#endif
  minDepth = VPackHelper::getNumericValue<uint64_t>(info, "minDepth", 1);
  maxDepth = VPackHelper::getNumericValue<uint64_t>(info, "maxDepth", 1);
  weightAttribute =
      VelocyPackHelper::getStringValue(info, "weightAttribute", "");
  defaultWeight =
      VelocyPackHelper::getNumericValue<double>(info, "defaultWeight", 1);
  VPackSlice read = info.get("reverseLookupInfos");
  if (!read.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a reverseLookupInfos");
  }

  size_t length = read.length();
  TRI_ASSERT(read.length() == collections.length());
  _reverseLookupInfos.reserve(length);
  for (size_t j = 0; j < length; ++j) {
    _reverseLookupInfos.emplace_back(query, read.at(j), collections.at(j));
  }
}

ShortestPathOptions::~ShortestPathOptions() = default;

void ShortestPathOptions::buildEngineInfo(VPackBuilder& result) const {
  result.openObject();
  injectEngineInfo(result);
  result.add("type", VPackValue("shortestPath"));
  result.add("defaultWeight", VPackValue(defaultWeight));
  result.add("weightAttribute", VPackValue(weightAttribute));
  result.add(VPackValue("reverseLookupInfos"));
  result.openArray();
  for (auto const& it : _reverseLookupInfos) {
    it.buildEngineInfo(result);
  }
  result.close();

  result.close();
}

bool ShortestPathOptions::useWeight() const { return !weightAttribute.empty(); }

void ShortestPathOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("minDepth", VPackValue(minDepth));
  builder.add("maxDepth", VPackValue(maxDepth));
  builder.add("weightAttribute", VPackValue(weightAttribute));
  builder.add("defaultWeight", VPackValue(defaultWeight));
  builder.add("type", VPackValue("shortestPath"));
  builder.add(StaticStrings::GraphRefactorFlag, VPackValue(refactor()));
}

void ShortestPathOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      it2->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics,
                                                  Index::Serialize::Estimates));
    }
  }
  builder.close();
}

double ShortestPathOptions::estimateCost(size_t& nrItems) const {
  size_t baseCreateItems = 0;
  double baseCost = costForLookupInfoList(_baseLookupInfos, baseCreateItems);
  // We use the "seven-degrees-of-seperation" rule.
  // This theory asumes that the shortest path is at most 7 steps of length

  nrItems = static_cast<size_t>(std::pow(baseCreateItems, 7));
  return std::pow(baseCost, 7);
}

void ShortestPathOptions::addReverseLookupInfo(aql::ExecutionPlan* plan,
                                               std::string const& collectionName,
                                               std::string const& attributeName,
                                               aql::AstNode* condition) {
  injectLookupInfoInList(_reverseLookupInfos, plan, collectionName, attributeName, condition);
}

double ShortestPathOptions::weightEdge(VPackSlice edge) const {
  TRI_ASSERT(useWeight());
  return arangodb::basics::VelocyPackHelper::getNumericValue<double>(
      edge, weightAttribute.c_str(), defaultWeight);
}

std::unique_ptr<EdgeCursor> ShortestPathOptions::buildCursor(bool backward) {
  ensureCache();

  if (_isCoordinator) {
    return std::make_unique<ClusterShortestPathEdgeCursor>(this, backward);
  }

  return std::make_unique<SingleServerEdgeCursor>(this, _tmpVar, nullptr,
                                                  backward ? _reverseLookupInfos
                                                           : _baseLookupInfos);
}

template <typename ListType>
void ShortestPathOptions::fetchVerticesCoordinator(ListType const& vertexIds) {
  if (!arangodb::ServerState::instance()->isCoordinator()) {
    return;
  }

  // In Coordinator all caches are ClusterTraverserCache instances
  auto ch = reinterpret_cast<ClusterTraverserCache*>(cache());
  TRI_ASSERT(ch != nullptr);
  // get the map of _ids into the datalake
  graph::ClusterTraverserCache::Cache& cache = ch->cache();

  std::unordered_set<arangodb::velocypack::HashedStringRef> fetch;
  for (auto const& it : vertexIds) {
    arangodb::velocypack::HashedStringRef hashedId(it.data(),
                                                   static_cast<uint32_t>(it.length()));
    if (cache.find(hashedId) == cache.end()) {
      // We do not have this vertex
      fetch.emplace(hashedId);
    }
  }
  if (!fetch.empty()) {
    fetchVerticesFromEngines(_trx, *ch, fetch, cache, /*forShortestPath*/ true);
  }
}

auto ShortestPathOptions::estimateDepth() const noexcept -> uint64_t {
  // We certainly have no clue how the depth actually is.
  // So we return a "random" number here.
  // By the six degrees of seperation rule, which defines most vertices in a naturally created graph
  // are 6 steps away from each other, 7 seems to be a quite good worst-case estimate.
  return 7;
}

ShortestPathOptions::ShortestPathOptions(ShortestPathOptions const& other,
                                         bool const allowAlreadyBuiltCopy)
    : BaseOptions(other, allowAlreadyBuiltCopy),
      minDepth(other.minDepth),
      maxDepth(other.maxDepth),
      start{other.start},
      end{other.end},
      weightAttribute{other.weightAttribute},
      defaultWeight{other.defaultWeight},
      bidirectional{other.bidirectional},
      multiThreaded{other.multiThreaded},
      _reverseLookupInfos{other._reverseLookupInfos} {}

template void ShortestPathOptions::fetchVerticesCoordinator<std::deque<arangodb::velocypack::StringRef>>(
    std::deque<arangodb::velocypack::StringRef> const& vertexIds);
template void ShortestPathOptions::fetchVerticesCoordinator<std::vector<arangodb::velocypack::HashedStringRef>>(
    std::vector<arangodb::velocypack::HashedStringRef> const& vertexIds);
