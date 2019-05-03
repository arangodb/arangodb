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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShortestPathOptions.h"

#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Cluster/ClusterMethods.h"
#include "Graph/ClusterTraverserCache.h"
#include "Indexes/Index.h"
#include "Transaction/Helpers.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;
using namespace arangodb::traverser;

ShortestPathOptions::ShortestPathOptions(aql::Query* query)
    : BaseOptions(query),
      direction("outbound"),
      weightAttribute(""),
      defaultWeight(1),
      bidirectional(true),
      multiThreaded(true) {}

ShortestPathOptions::ShortestPathOptions(aql::Query* query, VPackSlice const& info)
    : BaseOptions(query),
      direction("outbound"),
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
  weightAttribute =
      VelocyPackHelper::getStringValue(info, "weightAttribute", "");
  defaultWeight =
      VelocyPackHelper::getNumericValue<double>(info, "defaultWeight", 1);
}

ShortestPathOptions::ShortestPathOptions(aql::Query* query, VPackSlice info, VPackSlice collections)
    : BaseOptions(query, info, collections),
      direction("outbound"),
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

ShortestPathOptions::~ShortestPathOptions() {}

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

void ShortestPathOptions::setStart(std::string const& id) {
  start = id;
  startBuilder.clear();
  startBuilder.add(VPackValue(id));
}

void ShortestPathOptions::setEnd(std::string const& id) {
  end = id;
  endBuilder.clear();
  endBuilder.add(VPackValue(id));
}

VPackSlice ShortestPathOptions::getStart() const {
  return startBuilder.slice();
}

VPackSlice ShortestPathOptions::getEnd() const { return endBuilder.slice(); }

bool ShortestPathOptions::useWeight() const { return !weightAttribute.empty(); }

void ShortestPathOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("weightAttribute", VPackValue(weightAttribute));
  builder.add("defaultWeight", VPackValue(defaultWeight));
  builder.add("type", VPackValue("shortestPath"));
}

void ShortestPathOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      it2.getIndex()->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
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

EdgeCursor* ShortestPathOptions::nextCursor(arangodb::velocypack::StringRef vid) {
  if (_isCoordinator) {
    return nextCursorCoordinator(vid);
  }
  return nextCursorLocal(vid, _baseLookupInfos);
}

EdgeCursor* ShortestPathOptions::nextReverseCursor(arangodb::velocypack::StringRef vid) {
  if (_isCoordinator) {
    return nextReverseCursorCoordinator(vid);
  }
  return nextCursorLocal(vid, _reverseLookupInfos);
}

EdgeCursor* ShortestPathOptions::nextCursorCoordinator(arangodb::velocypack::StringRef vid) {
  auto cursor = std::make_unique<ClusterEdgeCursor>(vid, false, this);
  return cursor.release();
}

EdgeCursor* ShortestPathOptions::nextReverseCursorCoordinator(arangodb::velocypack::StringRef vid) {
  auto cursor = std::make_unique<ClusterEdgeCursor>(vid, true, this);
  return cursor.release();
}

void ShortestPathOptions::fetchVerticesCoordinator(
    std::deque<arangodb::velocypack::StringRef> const& vertexIds) {
  // TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  if (!arangodb::ServerState::instance()->isCoordinator()) {
    return;
  }

  // In Coordinator all caches are ClusterTraverserCache instances
  auto ch = reinterpret_cast<ClusterTraverserCache*>(cache());
  TRI_ASSERT(ch != nullptr);
  // get the map of _ids into the datalake
  std::unordered_map<arangodb::velocypack::StringRef, VPackSlice>& cache = ch->cache();

  std::unordered_set<arangodb::velocypack::StringRef> fetch;
  for (auto it : vertexIds) {
    if (cache.find(it) == cache.end()) {
      // We do not have this vertex
      fetch.emplace(it);
    }
  }
  if (!fetch.empty()) {
    transaction::BuilderLeaser leased(trx());

    fetchVerticesFromEngines(trx()->vocbase().name(), ch->engines(), fetch,
                             cache, ch->datalake(), *(leased.get()));
  }
}

void ShortestPathOptions::isQueryKilledCallback() const {
  if (query()->killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}
