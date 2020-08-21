////////////////////////////////////////////////////////////////////////////////
///
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
///
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_GRAPHFORMAT_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_GRAPHFORMAT_H 1

#include "AIR.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct GraphFormat final : public graph_format {
  // FIXME: passing of options? Maybe a struct? The complete struct that comes
  //        out of the deserializer, or just a view of it?
  explicit GraphFormat(application_features::ApplicationServer& server,
                       std::string const& resultField,
                       AccumulatorsDeclaration const& globalAccumulatorDeclarations,
                       AccumulatorsDeclaration const& vertexAccumulatorDeclarations,
                       CustomAccumulatorDefinitions customDefinitions,
                       DataAccessDefinition const& dataAccess);

  std::string const _resultField;

  // We use these accumulatorDeclarations to setup VertexData in
  // copyVertexData
  AccumulatorsDeclaration _globalAccumulatorDeclarations;
  AccumulatorsDeclaration _vertexAccumulatorDeclarations;
  CustomAccumulatorDefinitions _customDefinitions;
  DataAccessDefinitions _dataAccess;

  size_t estimatedVertexSize() const override;
  size_t estimatedEdgeSize() const override;

  void copyVertexData(std::string const& documentId, arangodb::velocypack::Slice document,
                      VertexAccumulators::vertex_type& targetPtr) override;

  void copyEdgeData(arangodb::velocypack::Slice document,
                    VertexAccumulators::edge_type& targetPtr) override;

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const VertexAccumulators::vertex_type* ptr,
                           size_t size) const override;
  bool buildEdgeDocument(arangodb::velocypack::Builder& b,
                         const VertexAccumulators::edge_type* ptr, size_t size) const override;

 protected:
  std::atomic<uint64_t> _vertexIdRange = 0;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
