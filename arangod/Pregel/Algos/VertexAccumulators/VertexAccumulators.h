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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_H 1

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/VertexComputation.h"

#include "Pregel/Algos/VertexAccumulators/AbstractAccumulator.h"
#include "Pregel/Algos/VertexAccumulators/AccumulatorOptionsDeserializer.h"
#include "Pregel/Algos/VertexAccumulators/Accumulators.h"

namespace arangodb {
namespace pregel {
namespace algos {

// Vertex data has to be default constructible m(
class VertexData {
 public:
  std::string toString() const { return "vertexAkkum"; };

  void reset(AccumulatorsDeclaration const& accumulatorsDeclaration,
             std::string documentId, VPackSlice const& doc);

  std::map<std::string, std::unique_ptr<AccumulatorBase>, std::less<>> _accumulators;

  std::string _documentId;
  // FIXME: YOLO. we copy the whole document, which is
  //        probably super expensive.
  VPackBuilder _document;
};

std::ostream& operator<<(std::ostream&, VertexData const&);

struct EdgeData {
  void reset(VPackSlice const& doc);
  // FIXME: YOLO. we copy the whole document, which is
  //        probably super expensive.
  // FIXME. VERDAMMT.
  VPackBuilder _document;

  // At the moment it's only important that message is sent
  // to the correct neighbour
  std::string _toId;
};

struct MessageData {
  void reset(std::string accumulatorName, VPackSlice const& value, std::string const& sender);

  std::string _accumulatorName;

  // We copy the value :/ is this necessary?
  VPackBuilder _value;
  std::string _sender;
};

struct VertexAccumulators : public Algorithm<VertexData, EdgeData, MessageData> {
  struct GraphFormat final : public graph_format {
    // FIXME: passing of options? Maybe a struct? The complete struct that comes
    //        out of the deserializer, or just a view of it?
    explicit GraphFormat(application_features::ApplicationServer& server,
                         std::string const& resultField,
                         std::map<std::string, AccumulatorOptions> const& accumulatorDeclarations);

    std::string const _resultField;

    // We use these accumulatorDeclarations to setup VertexData in
    // copyVertexData
    std::map<std::string, AccumulatorOptions> _accumulatorDeclarations;

    size_t estimatedVertexSize() const override;
    size_t estimatedEdgeSize() const override;

    void copyVertexData(std::string const& documentId, arangodb::velocypack::Slice document,
                        vertex_type& targetPtr) override;

    void copyEdgeData(arangodb::velocypack::Slice document, edge_type& targetPtr) override;

    bool buildVertexDocument(arangodb::velocypack::Builder& b,
                             const vertex_type* ptr, size_t size) const override;
    bool buildEdgeDocument(arangodb::velocypack::Builder& b,
                           const edge_type* ptr, size_t size) const override;
  };

  struct MessageFormat : public message_format {
    MessageFormat();

    void unwrapValue(VPackSlice s, message_type& message) const override;
    void addValue(VPackBuilder& arrayBuilder, message_type const& message) const override;
  };

  struct VertexComputation : public vertex_computation {
    explicit VertexComputation(VertexAccumulators const& algorithm);
    void compute(MessageIterator<message_type> const& messages) override;
    VertexAccumulators const& algorithm() const { return _algorithm; }
   private:
    VertexAccumulators const& _algorithm;
  };

 public:
  explicit VertexAccumulators(application_features::ApplicationServer& server,
                              VPackSlice userParams);

  bool supportsAsyncMode() const override;
  bool supportsCompensation() const override;

  graph_format* inputFormat() const override;

  message_format* messageFormat() const override { return new MessageFormat(); }
  message_combiner* messageCombiner() const override { return nullptr; }
  vertex_computation* createComputation(WorkerConfig const*) const override;

  bool getBindParameter(std::string_view, VPackBuilder& into) const;

  MasterContext* masterContext(VPackSlice userParams) const override;

  IAggregator* aggregator(std::string const& name) const override;

  VertexAccumulatorOptions const& options() const;
 private:
  void parseUserParams(VPackSlice userParams);


 private:
  VertexAccumulatorOptions _options;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
