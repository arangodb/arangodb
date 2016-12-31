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
#include "Pregel/Algos/PageRank.h"
#include "Pregel/Algos/SSSP.h"
#include "Pregel/Algos/ShortestPath.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::pregel;

IAlgorithm* AlgoRegistry::createAlgorithm(std::string const& algorithm,
                                          VPackSlice userParams) {
  if (algorithm == "sssp") {
    return new algos::SSSPAlgorithm(userParams);
  } else if (algorithm == "pagerank") {
    return new algos::PageRankAlgorithm(userParams);
  } else if (algorithm == "shortestpath") {
    return new algos::ShortestPathAlgorithm(userParams);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
  return nullptr;
}

template <typename V, typename E, typename M>
IWorker* AlgoRegistry::createWorker(TRI_vocbase_t* vocbase,
                                    Algorithm<V, E, M>* algo, VPackSlice body) {
  return new Worker<V, E, M>(vocbase, algo, body);
}

IWorker* AlgoRegistry::createWorker(TRI_vocbase_t* vocbase, VPackSlice body) {
  VPackSlice algorithm = body.get(Utils::algorithmKey);
  if (!algorithm.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }

  VPackSlice userParams = body.get(Utils::userParametersKey);
  if (algorithm.compareString("SSSP") == 0) {
    return createWorker(vocbase, new algos::SSSPAlgorithm(userParams), body);
  } else if (algorithm.compareString("PageRank") == 0) {
    return createWorker(vocbase, new algos::PageRankAlgorithm(userParams),
                        body);
  } else if (algorithm.compareString("ShortestPath") == 0) {
    return createWorker(vocbase, new algos::ShortestPathAlgorithm(userParams),
                        body);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
  return nullptr;
};
