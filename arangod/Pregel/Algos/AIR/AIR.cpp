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

#include "AIR.h"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"

#include "Pregel/Worker.h"

#include "Logger/Logger.h"

#include "Pregel/Algos/AIR/GraphFormat.h"
#include "Pregel/Algos/AIR/MasterContext.h"
#include "Pregel/Algos/AIR/MessageFormat.h"
#include "Pregel/Algos/AIR/VertexComputation.h"
#include "Pregel/Algos/AIR/WorkerContext.h"

#include "Pregel/Algos/AIR/AccumulatorOptionsDeserializer.h"

#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"

#include <Pregel/Algos/AIR/Greenspun/Primitives.h>

#include <set>

#define LOG_VERTEXACC(logId, level) \
  LOG_DEVEL << "[" << pregel_algorithm_name << "] "

// LOG_TOPIC(logId, level, Logger::QUERIES) << "[AIR] "

using namespace arangodb;
using namespace arangodb::pregel;

namespace arangodb::pregel::algos::accumulators {

VertexAccumulators::VertexAccumulators(application_features::ApplicationServer& server,
                                       VPackSlice userParams)
    : Algorithm(server, pregel_algorithm_name) {
  parseUserParams(userParams);
}

bool VertexAccumulators::supportsAsyncMode() const { return false; }
bool VertexAccumulators::supportsCompensation() const { return false; }

auto VertexAccumulators::createComputation(WorkerConfig const* config) const
    -> vertex_computation* {
  return new VertexComputation(*this);
}

auto VertexAccumulators::inputFormat() const -> graph_format* {
  // TODO: pass *this - (i.e. a reference to the algorithm object, and make accessor functions for the declarations in there.
  return new GraphFormat(_server, _options.resultField, _options.globalAccumulators,
                         _options.vertexAccumulators, _options.customAccumulators, _options.dataAccess);
}

message_format* VertexAccumulators::messageFormat() const {
  return new MessageFormat();
}

message_combiner* VertexAccumulators::messageCombiner() const {
  return nullptr;
}

void VertexAccumulators::parseUserParams(VPackSlice userParams) {
  LOG_VERTEXACC("", DEBUG) << "parsing user params: " << userParams;
  auto result = parseVertexAccumulatorOptions(userParams);
  if (result) {
    _options = std::move(result).get();

    LOG_VERTEXACC("", DEBUG) << "declared accumulators";
    for (auto&& acc : _options.vertexAccumulators) {
      LOG_VERTEXACC("", DEBUG) << acc.first << " " << acc.second;
    }
    LOG_VERTEXACC("", DEBUG) << "declared global accumulators";
    for(auto&& acc : _options.globalAccumulators) {
      LOG_VERTEXACC("", DEBUG) << acc.first << " " << acc.second;
    }
  } else {
    // What do we do on error? std::terminate()
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, result.error().as_string());
  }
  LOG_DEVEL << "done constructing";
}

VertexAccumulatorOptions const& VertexAccumulators::options() const {
  return _options;
}

bool VertexAccumulators::getBindParameter(std::string_view name, VPackBuilder& into) const {
  std::string nameStr(name);  // TODO remove this in c++20 (then this method will be noexcept)
  if (auto iter = options().bindings.find(nameStr); iter != std::end(options().bindings)) {
    into.add(iter->second.slice());
    return true;
  }

  return false;
}

::arangodb::pregel::MasterContext* VertexAccumulators::masterContext(VPackSlice userParams) const {
  return new MasterContext(this);
}

::arangodb::pregel::WorkerContext* VertexAccumulators::workerContext(VPackSlice userParams) const {
  return new WorkerContext(this);
}

IAggregator* VertexAccumulators::aggregator(const std::string& name) const {
  if (name == "phase") {  // permanent value
    return new OverwriteAggregator<uint32_t>(0, true);
  } else if (name == "phase-first-step") {
    return new OverwriteAggregator<uint64_t>(0, true);
  }
  return nullptr;
}

};  // namespace arangodb::pregel::algos::accumulators

using namespace arangodb::pregel::algos::accumulators;
template class arangodb::pregel::Worker<vertex_type, edge_type, message_type>;
