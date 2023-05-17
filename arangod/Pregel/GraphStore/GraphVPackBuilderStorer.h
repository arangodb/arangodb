////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <memory>

#include "Pregel/GraphFormat.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Pregel/GraphStore/GraphStorerBase.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/PregelMetrics.h"
#include "Cluster/ClusterTypes.h"

namespace arangodb::pregel {

class WorkerConfig;

template<class V, class E>
struct GraphFormat;

template<typename V, typename E>
struct GraphVPackBuilderStorer : GraphStorerBase<V, E> {
  explicit GraphVPackBuilderStorer(
      bool withId, std::shared_ptr<WorkerConfig> config,
      std::shared_ptr<GraphFormat<V, E> const> graphFormat,
      std::shared_ptr<PregelMetrics> metrics)
      : result(std::make_unique<VPackBuilder>()),
        withId(withId),
        graphFormat(graphFormat),
        config(config),
        metrics(metrics) {
    result->openArray(/*unindexed*/ true);
  }

  auto store(Magazine<V, E> magazine)
      -> futures::Future<futures::Unit> override;
  auto stealResult() -> std::unique_ptr<VPackBuilder> {
    result->close();
    return std::move(result);
  }

  std::unique_ptr<VPackBuilder> result;
  bool withId{false};

  std::shared_ptr<GraphFormat<V, E> const> graphFormat;
  std::shared_ptr<WorkerConfig const> config;
  std::shared_ptr<PregelMetrics> metrics;
};

}  // namespace arangodb::pregel
