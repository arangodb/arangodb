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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_H 1

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/VertexComputation.h"

#include "Pregel/Algos/AIR/AbstractAccumulator.h"
#include "Pregel/Algos/AIR/AccumulatorOptions.h"
#include "Pregel/Algos/AIR/Accumulators.h"

#include "EdgeData.h"
#include "MessageData.h"
#include "VertexData.h"

namespace arangodb::pregel::algos::accumulators {

constexpr const char pregel_algorithm_name[] = "ppa";

using vertex_type = VertexData;
using edge_type = EdgeData;
using message_type = MessageData;

using graph_format = GraphFormat<vertex_type, edge_type>;
using algorithm = Algorithm<vertex_type, edge_type, message_type>;
using message_format = MessageFormat<message_type>;
using vertex_computation = VertexComputation<vertex_type, edge_type, message_type>;
using message_combiner = MessageCombiner<message_type>;

using AccumulatorMap = std::unordered_map<std::string, std::unique_ptr<AccumulatorBase>, std::less<>>;

struct ProgrammablePregelAlgorithm : public algorithm {
 public:
  explicit ProgrammablePregelAlgorithm(application_features::ApplicationServer& server,
                              VPackSlice userParams);

  bool supportsAsyncMode() const override;
  bool supportsCompensation() const override;

  graph_format* inputFormat() const override;

  message_format* messageFormat() const override;
  message_combiner* messageCombiner() const override;
  vertex_computation* createComputation(WorkerConfig const*) const override;

  bool getBindParameter(std::string_view, VPackBuilder& into) const;

  ::arangodb::pregel::MasterContext* masterContext(VPackSlice userParams) const override;
  ::arangodb::pregel::WorkerContext* workerContext(VPackSlice userParams) const override;

  IAggregator* aggregator(std::string const& name) const override;

  VertexAccumulatorOptions const& options() const;

 private:
  void parseUserParams(VPackSlice userParams);

 private:
  VertexAccumulatorOptions _options;
};

}  // namespace arangodb
#endif
