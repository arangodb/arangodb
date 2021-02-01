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

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_GRAPHFORMAT_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_GRAPHFORMAT_H 1

#include "AIR.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct GraphFormat final : public graph_format {
  explicit GraphFormat(application_features::ApplicationServer& server,
                       std::string resultField,
                       AccumulatorsDeclaration globalAccumulatorDeclarations,
                       AccumulatorsDeclaration vertexAccumulatorDeclarations,
                       CustomAccumulatorDefinitions customDefinitions,
                       DataAccessDefinition dataAccess);

  std::string const _resultField;

  // We use these accumulatorDeclarations to setup VertexData in
  // copyVertexData
  AccumulatorsDeclaration _globalAccumulatorDeclarations;
  AccumulatorsDeclaration _vertexAccumulatorDeclarations;
  CustomAccumulatorDefinitions _customDefinitions;
  DataAccessDefinitions _dataAccess;

  size_t estimatedVertexSize() const override;
  size_t estimatedEdgeSize() const override;

  void copyVertexData(arangodb::velocypack::Options const& vpackOptions,
                      std::string const& documentId, arangodb::velocypack::Slice rawDocument,
                      ProgrammablePregelAlgorithm::vertex_type& targetPtr,
                      uint64_t& vertexIdRange) override;

  void copyEdgeData(arangodb::velocypack::Options const& vpackOptions,
                    arangodb::velocypack::Slice rawDocument,
                    ProgrammablePregelAlgorithm::edge_type& targetPtr) override;

  greenspun::EvalResult buildVertexDocumentWithResult(
      arangodb::velocypack::Builder& b, const ProgrammablePregelAlgorithm::vertex_type* ptr) const override;

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           VertexData const* targetPtr) const override {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

 protected:
  std::atomic<uint64_t> _vertexIdRange = 0;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
