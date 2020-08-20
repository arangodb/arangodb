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
  targetPtr.reset(_globalAccumulatorDeclarations,
                  _vertexAccumulatorDeclarations, _customDefinitions,
                  _dataAccess, documentId, vertexDocument, _vertexIdRange++);
}

void GraphFormat::copyEdgeData(arangodb::velocypack::Slice edgeDocument, edge_type& targetPtr) {
  targetPtr.reset(edgeDocument);
}

bool GraphFormat::buildVertexDocument(arangodb::velocypack::Builder& b,
                                      const vertex_type* ptr, size_t size) const {
  VPackObjectBuilder guard(&b, _resultField);

  // (example: accum-ref -> vertex type (ptr))
  greenspun::Machine m;  // todo: (member graph format)
  InitMachine(m);

  m.setFunction("accum-ref",
                [ptr](greenspun::Machine& ctx, VPackSlice const paramsList,
                      VPackBuilder& result) -> greenspun::EvalResult {
                  // serializeIntoBuilder (method of <abstract> accumulators bsp.: accum-ref -> vertex computation.cpp)
                  // ptr->_vertexAccumulators.at().
                  std::cout << ptr->_vertexId << std::endl;

                  return {};
                });

  std::cout << "Size is: " << ptr->_vertexAccumulators.size() << std::endl;
  if (ptr->_dataAccess.writeVertex.has_value()) {
    std::cout << ptr->_dataAccess.writeVertex->toString() << std::endl;
  }
  // auto res = Evaluate(m, program->slice(), result);

  for (auto&& acc : ptr->_vertexAccumulators) {
    // this will be obsolete soon
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
