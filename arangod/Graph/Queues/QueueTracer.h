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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_QUEUE_TRACER_H
#define ARANGOD_GRAPH_QUEUE_TRACER_H 1

#include "Graph/Helpers/TraceEntry.h"
#include "Basics/ResourceUsage.h"

#include <unordered_map>
#include <vector>

namespace arangodb {
namespace graph {

template <class QueueImpl>
class QueueTracer {
  using Step = typename QueueImpl::Step;

 public:
  QueueTracer(arangodb::ResourceMonitor& resourceMonitor);
  ~QueueTracer();

  void clear();
  void append(Step step);
  bool hasProcessableElement() const;
  size_t size() const;
  bool isEmpty() const;
  std::vector<Step*> getLooseEnds();

  Step pop();

 private:
  QueueImpl _impl;

  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable std::unordered_map<std::string, TraceEntry> _stats;
};

}  // namespace graph
}  // namespace arangodb

#endif
