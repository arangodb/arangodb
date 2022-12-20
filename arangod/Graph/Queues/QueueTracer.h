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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/Helpers/TraceEntry.h"
#include "Basics/ResourceUsage.h"
#include "Containers/FlatHashMap.h"

#include <unordered_map>
#include <vector>

namespace arangodb {
namespace graph {

template<class QueueImpl>
class QueueTracer {
  using Step = typename QueueImpl::Step;

 public:
  explicit QueueTracer(arangodb::ResourceMonitor& resourceMonitor);
  ~QueueTracer();

  void clear();
  void append(Step step);
  void setStartContent(std::vector<Step> startSteps);
  bool firstIsVertexFetched() const;
  bool hasProcessableElement() const;
  size_t size() const;
  bool isEmpty() const;
  std::vector<Step*> getLooseEnds();

  Step pop();

  // Return all Steps where the Provider needs to call fetchVertices()
  // for in order to be able to process them
  std::vector<Step*> getStepsWithoutFetchedVertex();

  // Return all Steps where the Provider needs to call fetchEdges()
  // for in order to be able to process them.
  // Note here, we hand in the vector of Steps, as in the place
  // we are using this method we have already popped the first
  // Step from the queue, but it cannot be processed.
  void getStepsWithoutFetchedEdges(std::vector<Step*>& stepsToFetch);

 private:
  QueueImpl _impl;

  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable containers::FlatHashMap<std::string, TraceEntry> _stats;
};

}  // namespace graph
}  // namespace arangodb
