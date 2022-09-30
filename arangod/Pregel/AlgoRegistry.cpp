////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algos/ConnectedComponents.h"
#include "Pregel/Algos/DMID/DMID.h"
#include "Pregel/Algos/EffectiveCloseness/EffectiveCloseness.h"
#include "Pregel/Algos/HITS.h"
#include "Pregel/Algos/HITSKleinberg.h"
#include "Pregel/Algos/LabelPropagation.h"
#include "Pregel/Algos/LineRank.h"
#include "Pregel/Algos/PageRank.h"
#include "Pregel/Algos/RecoveringPageRank.h"
#include "Pregel/Algos/SCC.h"
#include "Pregel/Algos/SLPA.h"
#include "Pregel/Algos/SSSP.h"
#include "Pregel/Algos/ShortestPath.h"
#include "Pregel/Algos/WCC.h"
#include "Pregel/Utils.h"
#include "VocBase/vocbase.h"
#include "Pregel/WorkerConductorMessages.h"
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
#include "Pregel/Algos/ReadWrite.h"
#endif

using namespace arangodb;
using namespace arangodb::pregel;

IAlgorithm* AlgoRegistry::createAlgorithm(
    application_features::ApplicationServer& server,
    std::string const& algorithm, VPackSlice userParams) {
  if (algorithm == "sssp") {
    return new algos::SSSPAlgorithm(server, userParams);
  } else if (algorithm == "pagerank") {
    return new algos::PageRank(server, userParams);
  } else if (algorithm == "recoveringpagerank") {
    return new algos::RecoveringPageRank(server, userParams);
  } else if (algorithm == "shortestpath") {
    return new algos::ShortestPathAlgorithm(server, userParams);
  } else if (algorithm == "linerank") {
    return new algos::LineRank(server, userParams);
  } else if (algorithm == "effectivecloseness") {
    return new algos::EffectiveCloseness(server, userParams);
  } else if (algorithm == "connectedcomponents") {
    return new algos::ConnectedComponents(server, userParams);
  } else if (algorithm == "scc") {
    return new algos::SCC(server, userParams);
  } else if (algorithm == "hits") {
    return new algos::HITS(server, userParams);
  } else if (algorithm == "hitskleinberg") {
    return new algos::HITSKleinberg(server, userParams);
  } else if (algorithm == "labelpropagation") {
    return new algos::LabelPropagation(server, userParams);
  } else if (algorithm == "slpa") {
    return new algos::SLPA(server, userParams);
  } else if (algorithm == "dmid") {
    return new algos::DMID(server, userParams);
  } else if (algorithm == "wcc") {
    return new algos::WCC(server, userParams);
  }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  else if (algorithm == "readwrite") {
    return new algos::ReadWrite(server, userParams);
  }
#endif
  else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
  return nullptr;
}

template<typename V, typename E, typename M>
/*static*/ std::shared_ptr<IWorker> AlgoRegistry::createWorker(
    TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algo, CreateWorker const& body,
    PregelFeature& feature) {
  return std::make_shared<Worker<V, E, M>>(vocbase, algo, body, feature);
}

/*static*/ std::shared_ptr<IWorker> AlgoRegistry::createWorker(
    TRI_vocbase_t& vocbase, CreateWorker const& data, PregelFeature& feature) {
  auto userParams = data.userParameters.slice();
  auto algorithm = data.algorithm;
  std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                 ::tolower);

  auto& server = vocbase.server();
  if (algorithm == "sssp") {
    return createWorker(vocbase, new algos::SSSPAlgorithm(server, userParams),
                        data, feature);
  } else if (algorithm == "pagerank") {
    return createWorker(vocbase, new algos::PageRank(server, userParams), data,
                        feature);
  } else if (algorithm == "recoveringpagerank") {
    return createWorker(vocbase,
                        new algos::RecoveringPageRank(server, userParams), data,
                        feature);
  } else if (algorithm == "shortestpath") {
    return createWorker(vocbase,
                        new algos::ShortestPathAlgorithm(server, userParams),
                        data, feature);
  } else if (algorithm == "linerank") {
    return createWorker(vocbase, new algos::LineRank(server, userParams), data,
                        feature);
  } else if (algorithm == "effectivecloseness") {
    return createWorker(vocbase,
                        new algos::EffectiveCloseness(server, userParams), data,
                        feature);
  } else if (algorithm == "connectedcomponents") {
    return createWorker(vocbase,
                        new algos::ConnectedComponents(server, userParams),
                        data, feature);
  } else if (algorithm == "scc") {
    return createWorker(vocbase, new algos::SCC(server, userParams), data,
                        feature);
  } else if (algorithm == "hits") {
    return createWorker(vocbase, new algos::HITS(server, userParams), data,
                        feature);
  } else if (algorithm == "hitskleinberg") {
    return createWorker(vocbase, new algos::HITSKleinberg(server, userParams),
                        data, feature);
  } else if (algorithm == "labelpropagation") {
    return createWorker(vocbase,
                        new algos::LabelPropagation(server, userParams), data,
                        feature);
  } else if (algorithm == "slpa") {
    return createWorker(vocbase, new algos::SLPA(server, userParams), data,
                        feature);
  } else if (algorithm == "dmid") {
    return createWorker(vocbase, new algos::DMID(server, userParams), data,
                        feature);
  } else if (algorithm == "wcc") {
    return createWorker(vocbase, new algos::WCC(server, userParams), data,
                        feature);
  }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  else if (algorithm == "readwrite") {
    return createWorker(vocbase, new algos::ReadWrite(server, userParams), data,
                        feature);
  }
#endif

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "Unsupported algorithm");
}
