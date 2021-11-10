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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "VertexData.h"

#include <Pregel/Algos/AIR/AbstractAccumulator.h>

#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb::pregel::algos::accumulators;

void VertexData::reset(AccumulatorsDeclaration const& vertexAccumulatorsDeclaration,
                       CustomAccumulatorDefinitions const& customDefinitions,
                       std::string documentId, VPackSlice const& doc, std::size_t vertexId) {
  _documentId = std::move(documentId);
  _document.clear();
  _document.add(doc);
  _vertexId = vertexId;

  for (auto&& acc : vertexAccumulatorsDeclaration) {
    _vertexAccumulators.emplace(acc.first, instantiateAccumulator(acc.second, customDefinitions));
  }
}

std::unique_ptr<AccumulatorBase> const& VertexData::accumulatorByName(std::string_view name) const {
  return _vertexAccumulators.at(std::string{name}); // C++20 use string_view to lookup in map
}
