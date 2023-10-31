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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/Conductor/Messages.h"
#include "VocBase/vocbase.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagation.h"
#include "Pregel/Algos/ConnectedComponents/ConnectedComponents.h"
#include "Pregel/Algos/DMID/DMID.h"
#include "Pregel/Algos/EffectiveCloseness/EffectiveCloseness.h"
#include "Pregel/Algos/HITS/HITS.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinberg.h"
#include "Pregel/Algos/LabelPropagation/LabelPropagation.h"
#include "Pregel/Algos/LineRank/LineRank.h"
#include "Pregel/Algos/PageRank/PageRank.h"
#include "Pregel/Algos/RecoveringPageRank/RecoveringPageRank.h"
#include "Pregel/Algos/SCC/SCC.h"
#include "Pregel/Algos/ShortestPath/ShortestPath.h"
#include "Pregel/Algos/SLPA/SLPA.h"
#include "Pregel/Algos/SSSP/SSSP.h"
#include "Pregel/Algos/WCC/WCC.h"
#include "Pregel/Utils.h"
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
#include "Pregel/Algos/ReadWrite/ReadWrite.h"
#endif

using namespace arangodb;
using namespace arangodb::pregel;

IAlgorithm* AlgoRegistry::createAlgorithm(std::string const& algorithm,
                                          VPackSlice userParams) {
  if (algorithm == "sssp") {
    return new algos::SSSPAlgorithm(userParams);
  } else if (algorithm == "pagerank") {
    return new algos::PageRank(userParams);
  } else if (algorithm == "recoveringpagerank") {
    return new algos::RecoveringPageRank(userParams);
  } else if (algorithm == "shortestpath") {
    return new algos::ShortestPathAlgorithm(userParams);
  } else if (algorithm == "linerank") {
    return new algos::LineRank(userParams);
  } else if (algorithm == "effectivecloseness") {
    return new algos::EffectiveCloseness(userParams);
  } else if (algorithm == "connectedcomponents") {
    return new algos::ConnectedComponents(userParams);
  } else if (algorithm == "scc") {
    return new algos::SCC(userParams);
  } else if (algorithm == "hits") {
    return new algos::HITS(userParams);
  } else if (algorithm == "hitskleinberg") {
    return new algos::HITSKleinberg(userParams);
  } else if (algorithm == "labelpropagation") {
    return new algos::LabelPropagation(userParams);
  } else if (algorithm == "slpa") {
    return new algos::SLPA(userParams);
  } else if (algorithm == "dmid") {
    return new algos::DMID(userParams);
  } else if (algorithm == "wcc") {
    return new algos::WCC(userParams);
  } else if (algorithm == "colorpropagation") {
    return new algos::ColorPropagation(userParams);
  }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  else if (algorithm == "readwrite") {
    return new algos::ReadWrite(userParams);
  }
#endif
  else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
}
auto AlgoRegistry::createAlgorithmNew(std::string const& algorithm,
                                      VPackSlice userParams)
    -> std::optional<std::unique_ptr<IAlgorithm>> {
  if (algorithm == "sssp") {
    return std::make_unique<algos::SSSPAlgorithm>(userParams);
  } else if (algorithm == "pagerank") {
    return std::make_unique<algos::PageRank>(userParams);
  } else if (algorithm == "recoveringpagerank") {
    return std::make_unique<algos::RecoveringPageRank>(userParams);
  } else if (algorithm == "shortestpath") {
    return std::make_unique<algos::ShortestPathAlgorithm>(userParams);
  } else if (algorithm == "linerank") {
    return std::make_unique<algos::LineRank>(userParams);
  } else if (algorithm == "effectivecloseness") {
    return std::make_unique<algos::EffectiveCloseness>(userParams);
  } else if (algorithm == "connectedcomponents") {
    return std::make_unique<algos::ConnectedComponents>(userParams);
  } else if (algorithm == "scc") {
    return std::make_unique<algos::SCC>(userParams);
  } else if (algorithm == "hits") {
    return std::make_unique<algos::HITS>(userParams);
  } else if (algorithm == "hitskleinberg") {
    return std::make_unique<algos::HITSKleinberg>(userParams);
  } else if (algorithm == "labelpropagation") {
    return std::make_unique<algos::LabelPropagation>(userParams);
  } else if (algorithm == "slpa") {
    return std::make_unique<algos::SLPA>(userParams);
  } else if (algorithm == "dmid") {
    return std::make_unique<algos::DMID>(userParams);
  } else if (algorithm == "wcc") {
    return std::make_unique<algos::WCC>(userParams);
  } else if (algorithm == "colorpropagation") {
    return std::make_unique<algos::ColorPropagation>(userParams);
  }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  else if (algorithm == "readwrite") {
    return std::make_unique<algos::ReadWrite>(userParams);
  }
#endif
  return std::nullopt;
}

template<typename V, typename E, typename M>
std::shared_ptr<IWorker> AlgoRegistry::createWorker(
    TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algo,
    worker::message::CreateWorker const& parameters, PregelFeature& feature) {
  return std::make_shared<Worker<V, E, M>>(vocbase, algo, parameters, feature);
}

std::shared_ptr<IWorker> AlgoRegistry::createWorker(
    TRI_vocbase_t& vocbase, worker::message::CreateWorker const& parameters,
    PregelFeature& feature) {
  VPackSlice userParams = parameters.userParameters.slice();
  std::string algorithm = parameters.algorithm;
  std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                 ::tolower);

  if (algorithm == "sssp") {
    return createWorker(vocbase, new algos::SSSPAlgorithm(userParams),
                        parameters, feature);
  } else if (algorithm == "pagerank") {
    return createWorker(vocbase, new algos::PageRank(userParams), parameters,
                        feature);
  } else if (algorithm == "recoveringpagerank") {
    return createWorker(vocbase, new algos::RecoveringPageRank(userParams),
                        parameters, feature);
  } else if (algorithm == "shortestpath") {
    return createWorker(vocbase, new algos::ShortestPathAlgorithm(userParams),
                        parameters, feature);
  } else if (algorithm == "linerank") {
    return createWorker(vocbase, new algos::LineRank(userParams), parameters,
                        feature);
  } else if (algorithm == "effectivecloseness") {
    return createWorker(vocbase, new algos::EffectiveCloseness(userParams),
                        parameters, feature);
  } else if (algorithm == "connectedcomponents") {
    return createWorker(vocbase, new algos::ConnectedComponents(userParams),
                        parameters, feature);
  } else if (algorithm == "scc") {
    return createWorker(vocbase, new algos::SCC(userParams), parameters,
                        feature);
  } else if (algorithm == "hits") {
    return createWorker(vocbase, new algos::HITS(userParams), parameters,
                        feature);
  } else if (algorithm == "hitskleinberg") {
    return createWorker(vocbase, new algos::HITSKleinberg(userParams),
                        parameters, feature);
  } else if (algorithm == "labelpropagation") {
    return createWorker(vocbase, new algos::LabelPropagation(userParams),
                        parameters, feature);
  } else if (algorithm == "slpa") {
    return createWorker(vocbase, new algos::SLPA(userParams), parameters,
                        feature);
  } else if (algorithm == "dmid") {
    return createWorker(vocbase, new algos::DMID(userParams), parameters,
                        feature);
  } else if (algorithm == "wcc") {
    return createWorker(vocbase, new algos::WCC(userParams), parameters,
                        feature);
  } else if (algorithm == "colorpropagation") {
    return createWorker(vocbase, new algos::ColorPropagation(userParams),
                        parameters, feature);
  }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  else if (algorithm == "readwrite") {
    return createWorker(vocbase, new algos::ReadWrite(userParams), parameters,
                        feature);
  }
#endif
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "Unsupported algorithm");
}
