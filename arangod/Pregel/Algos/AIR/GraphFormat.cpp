////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "GraphFormat.h"
#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

// Graph Format
GraphFormat::GraphFormat(application_features::ApplicationServer& server,
                         std::string const& resultField,
                         AccumulatorsDeclaration const& globalAccumulatorDeclarations,
                         AccumulatorsDeclaration const& vertexAccumulatorDeclarations,
                         CustomAccumulatorDefinitions customDefinitions)
    : graph_format(server),
      _resultField(resultField),
      _globalAccumulatorDeclarations(globalAccumulatorDeclarations),
      _vertexAccumulatorDeclarations(vertexAccumulatorDeclarations),
      _customDefinitions(std::move(customDefinitions)) {}

size_t GraphFormat::estimatedVertexSize() const { return sizeof(vertex_type); }
size_t GraphFormat::estimatedEdgeSize() const { return sizeof(edge_type); }

// Extract vertex data from vertex document into target
void GraphFormat::copyVertexData(std::string const& documentId,
                                 arangodb::velocypack::Slice vertexDocument,
                                 vertex_type& targetPtr) {
  targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,  documentId, vertexDocument, _vertexIdRange++);
}

void GraphFormat::copyEdgeData(arangodb::velocypack::Slice edgeDocument, edge_type& targetPtr) {
  targetPtr.reset(edgeDocument);
}

bool GraphFormat::buildVertexDocument(arangodb::velocypack::Builder& b,
                                      const vertex_type* ptr, size_t size) const {
  VPackObjectBuilder guard(&b, _resultField);
  for (auto&& acc : ptr->_vertexAccumulators) {
    b.add(VPackValue(acc.first));
    if (auto res = acc.second->finalizeIntoBuilder(b); res.fail()) {
      LOG_DEVEL << "finalize program failed: " << res.error().toString();
      TRI_ASSERT(false);
    }
  }
  return true;
}

bool GraphFormat::buildEdgeDocument(arangodb::velocypack::Builder& b,
                                    const edge_type* ptr, size_t size) const {
  // FIXME
  // std::abort();
  return false;
}

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
