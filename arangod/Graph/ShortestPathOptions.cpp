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

#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;

ShortestPathOptions::ShortestPathOptions(transaction::Methods* trx)
    : BaseOptions(trx),
      direction("outbound"),
      weightAttribute(""),
      defaultWeight(1),
      bidirectional(true),
      multiThreaded(true) {}

ShortestPathOptions::ShortestPathOptions(transaction::Methods* trx,
                                         VPackSlice const& info)
    : BaseOptions(trx),
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

ShortestPathOptions::~ShortestPathOptions() {}

void ShortestPathOptions::buildEngineInfo(VPackBuilder& result) const {
  // TODO Implement me!
  BaseOptions::buildEngineInfo(result);
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
  // TODO Implement me
}

double ShortestPathOptions::estimateCost(size_t& nrItems) const {
  size_t baseCreateItems = 0;
  double baseCost = costForLookupInfoList(_baseLookupInfos, baseCreateItems);
  // We use the "seven-degrees-of-seperation" rule.
  // This theory asumes that the shortest path is at most 7 steps of length

  nrItems = static_cast<size_t>(std::pow(baseCreateItems, 7));
  return std::pow(baseCost, 7);
}

void ShortestPathOptions::addReverseLookupInfo(
    aql::Ast* ast, std::string const& collectionName,
    std::string const& attributeName, aql::AstNode* condition) {
  injectLookupInfoInList(_reverseLookupInfos, ast, collectionName,
                         attributeName, condition);
}

double ShortestPathOptions::weightEdge(VPackSlice edge) {
  TRI_ASSERT(useWeight());
  return arangodb::basics::VelocyPackHelper::getNumericValue<double>(
      edge, weightAttribute.c_str(), defaultWeight);
}

EdgeCursor* ShortestPathOptions::nextCursor(ManagedDocumentResult* mmdr,
                                            StringRef vid) {
  if (_isCoordinator) {
    return nextCursorCoordinator(vid);
  }
  TRI_ASSERT(mmdr != nullptr);
  return nextCursorLocal(mmdr, vid, _baseLookupInfos);
}

EdgeCursor* ShortestPathOptions::nextReverseCursor(ManagedDocumentResult* mmdr,
                                                   StringRef vid) {
  if (_isCoordinator) {
    return nextReverseCursorCoordinator(vid);
  }
  TRI_ASSERT(mmdr != nullptr);
  return nextCursorLocal(mmdr, vid, _reverseLookupInfos);
}

EdgeCursor* ShortestPathOptions::nextCursorCoordinator(StringRef vid) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

EdgeCursor* ShortestPathOptions::nextReverseCursorCoordinator(StringRef vid) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
