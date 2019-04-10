////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algos/AsyncSCC.h"
#include "Pregel/Algos/ConnectedComponents.h"
#include "Pregel/Algos/DMID/DMID.h"
#include "Pregel/Algos/EffectiveCloseness/EffectiveCloseness.h"
#include "Pregel/Algos/HITS.h"
#include "Pregel/Algos/LabelPropagation.h"
#include "Pregel/Algos/LineRank.h"
#include "Pregel/Algos/PageRank.h"
#include "Pregel/Algos/RecoveringPageRank.h"
#include "Pregel/Algos/SCC.h"
#include "Pregel/Algos/SLPA.h"
#include "Pregel/Algos/SSSP.h"
#include "Pregel/Algos/ShortestPath.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::pregel;

IAlgorithm* AlgoRegistry::createAlgorithm(std::string const& algorithm, VPackSlice userParams) {
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
  } else if (algorithm == "asyncscc") {
    return new algos::AsyncSCC(userParams);
  } else if (algorithm == "hits") {
    return new algos::HITS(userParams);
  } else if (algorithm == "labelpropagation") {
    return new algos::LabelPropagation(userParams);
  } else if (algorithm == "slpa") {
    return new algos::SLPA(userParams);
  } else if (algorithm == "dmid") {
    return new algos::DMID(userParams);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
  return nullptr;
}

template <typename V, typename E, typename M>
/*static*/ std::shared_ptr<IWorker> AlgoRegistry::createWorker(TRI_vocbase_t& vocbase,
                                                               Algorithm<V, E, M>* algo,
                                                               VPackSlice body) {
  return std::make_shared<Worker<V, E, M>>(vocbase, algo, body);
}

/*static*/ std::shared_ptr<IWorker> AlgoRegistry::createWorker(TRI_vocbase_t& vocbase,
                                                               VPackSlice body) {
  VPackSlice algoSlice = body.get(Utils::algorithmKey);

  if (!algoSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }

  VPackSlice userParams = body.get(Utils::userParametersKey);
  std::string algorithm = algoSlice.copyString();
  std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(), ::tolower);

  if (algorithm == "sssp") {
    return createWorker(vocbase, new algos::SSSPAlgorithm(userParams), body);
  } else if (algorithm == "pagerank") {
    return createWorker(vocbase, new algos::PageRank(userParams), body);
  } else if (algorithm == "recoveringpagerank") {
    return createWorker(vocbase, new algos::RecoveringPageRank(userParams), body);
  } else if (algorithm == "shortestpath") {
    return createWorker(vocbase, new algos::ShortestPathAlgorithm(userParams), body);
  } else if (algorithm == "linerank") {
    return createWorker(vocbase, new algos::LineRank(userParams), body);
  } else if (algorithm == "effectivecloseness") {
    return createWorker(vocbase, new algos::EffectiveCloseness(userParams), body);
  } else if (algorithm == "connectedcomponents") {
    return createWorker(vocbase, new algos::ConnectedComponents(userParams), body);
  } else if (algorithm == "scc") {
    return createWorker(vocbase, new algos::SCC(userParams), body);
  } else if (algorithm == "asyncscc") {
    return createWorker(vocbase, new algos::AsyncSCC(userParams), body);
  } else if (algorithm == "hits") {
    return createWorker(vocbase, new algos::HITS(userParams), body);
  } else if (algorithm == "labelpropagation") {
    return createWorker(vocbase, new algos::LabelPropagation(userParams), body);
  } else if (algorithm == "slpa") {
    return createWorker(vocbase, new algos::SLPA(userParams), body);
  } else if (algorithm == "dmid") {
    return createWorker(vocbase, new algos::DMID(userParams), body);
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "Unsupported algorithm");
}
