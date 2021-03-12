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

#include "QueueTracer.h"

#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/FifoQueue.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class QueueImpl>
QueueTracer<QueueImpl>::QueueTracer(arangodb::ResourceMonitor& resourceMonitor)
    : _impl{resourceMonitor} {}

template <class QueueImpl>
QueueTracer<QueueImpl>::~QueueTracer() {
  LOG_TOPIC("4773a", INFO, Logger::GRAPHS) << "Queue Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("fabba", INFO, Logger::GRAPHS) << "  " << name << ": " << trace;
  }
}

template <class QueueImpl>
void QueueTracer<QueueImpl>::clear() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["clear"].addTiming(TRI_microtime() - start));
  return _impl.clear();
}

template <class QueueImpl>
void QueueTracer<QueueImpl>::append(typename QueueImpl::Step step) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["append"].addTiming(TRI_microtime() - start));
  return _impl.append(std::move(step));
}

template <class QueueImpl>
bool QueueTracer<QueueImpl>::hasProcessableElement() const {
  double start = TRI_microtime();
  TRI_DEFER(_stats["hasProcessableElement"].addTiming(TRI_microtime() - start));
  return _impl.hasProcessableElement();
}

template <class QueueImpl>
size_t QueueTracer<QueueImpl>::size() const {
  double start = TRI_microtime();
  TRI_DEFER(_stats["size"].addTiming(TRI_microtime() - start));
  return _impl.size();
}

template <class QueueImpl>
bool QueueTracer<QueueImpl>::isEmpty() const {
  double start = TRI_microtime();
  TRI_DEFER(_stats["isEmpty"].addTiming(TRI_microtime() - start));
  return _impl.isEmpty();
}

template <class QueueImpl>
auto QueueTracer<QueueImpl>::getLooseEnds() -> std::vector<typename QueueImpl::Step*> {
  double start = TRI_microtime();
  TRI_DEFER(_stats["getLooseEnds"].addTiming(TRI_microtime() - start));
  return _impl.getLooseEnds();
}

template <class QueueImpl>
auto QueueTracer<QueueImpl>::pop() -> typename QueueImpl::Step {
  double start = TRI_microtime();
  TRI_DEFER(_stats["pop"].addTiming(TRI_microtime() - start));
  return _impl.pop();
}

/* SingleServerProvider Section */
template class ::arangodb::graph::QueueTracer<arangodb::graph::FifoQueue<arangodb::graph::SingleServerProvider::Step>>;

/* ClusterServerProvider Section */
template class ::arangodb::graph::QueueTracer<arangodb::graph::FifoQueue<arangodb::graph::ClusterProvider::Step>>;