////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "PathStoreTracer.h"

#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/Providers/SingleServerProvider.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class PathStoreImpl, class Step>
PathStoreTracer<PathStoreImpl, Step>::PathStoreTracer() : _impl{} {}

template <class PathStoreImpl, class Step>
PathStoreTracer<PathStoreImpl, Step>::~PathStoreTracer() {
  LOG_TOPIC("f39e8", INFO, Logger::GRAPHS) << "PathStore Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("f39e9", INFO, Logger::GRAPHS) << "  " << name << ": " << trace;
  }
}

template <class PathStoreImpl, class Step>
bool PathStoreTracer<PathStoreImpl, Step>::testPath(Step step) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["testPath"].addTiming(TRI_microtime() - start));
  return _impl.testPath(step);
}

template <class PathStoreImpl, class Step>
void PathStoreTracer<PathStoreImpl, Step>::reset() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["reset"].addTiming(TRI_microtime() - start));
  return _impl.reset();
}

template <class PathStoreImpl, class Step>
void PathStoreTracer<PathStoreImpl, Step>::setStartVertex(Step step) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["setStartVertex"].addTiming(TRI_microtime() - start));
  return _impl.setStartVertex(step);
}

template <class PathStoreImpl, class Step>
size_t PathStoreTracer<PathStoreImpl, Step>::append(Step step) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["append"].addTiming(TRI_microtime() - start));
  return _impl.append(step);
}

template <class PathStoreImpl, class Step>
size_t PathStoreTracer<PathStoreImpl, Step>::size() const {
  double start = TRI_microtime();
  TRI_DEFER(_stats["size"].addTiming(TRI_microtime() - start));
  return _impl.size();
}

template class ::arangodb::graph::PathStoreTracer<PathStore<SingleServerProvider::Step>, SingleServerProvider::Step>;
