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

#ifndef ARANGOD_GRAPH_ALGORITHM_ALIASES_H
#define ARANGOD_GRAPH_ALGORITHM_ALIASES_H 1

#include <Graph/PathManagement/PathValidator.h>
#include <Graph/Providers/ProviderTracer.h>
#include <Graph/Types/UniquenessLevel.h>

namespace arangodb {
namespace graph {

// K_PATH implementation
template <class Provider>
using KPathEnumerator =
    TwoSidedEnumerator<FifoQueue<typename Provider::Step>, PathStore<typename Provider::Step>, Provider,
                       PathValidator<PathStore<typename Provider::Step>, VertexUniquenessLevel::PATH>>;

// K_PATH implementation using Tracing
template <class Provider>
using TracedKPathEnumerator =
    TwoSidedEnumerator<QueueTracer<FifoQueue<typename Provider::Step>>,
                       PathStoreTracer<PathStore<typename Provider::Step>>, ProviderTracer<Provider>,
                       PathValidator<PathStoreTracer<PathStore<typename Provider::Step>>, VertexUniquenessLevel::PATH>>;
}  // namespace graph
}  // namespace arangodb

#endif