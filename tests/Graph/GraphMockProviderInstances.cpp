////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "./MockGraphProvider.h"
#include "Graph/Enumerators/TwoSidedEnumerator.cpp"
#include "Graph/PathManagement/PathResult.cpp"
#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/Queues/FifoQueue.h"

#include "Graph/PathManagement/PathStoreTracer.cpp"
#include "Graph/PathManagement/PathValidator.cpp"
#include "Graph/Queues/QueueTracer.cpp"

template class ::arangodb::graph::PathResult<::arangodb::tests::graph::MockGraphProvider,
                                             ::arangodb::tests::graph::MockGraphProvider::Step>;

template class ::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>;

template class ::arangodb::graph::PathValidator<
    ::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>, VertexUniquenessLevel::PATH>;

template class ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>>;

template class ::arangodb::graph::PathValidator<
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>>, VertexUniquenessLevel::PATH>;

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::FifoQueue<::arangodb::tests::graph::MockGraphProvider::Step>,
    ::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>, ::arangodb::tests::graph::MockGraphProvider,
    ::arangodb::graph::PathValidator<::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>, VertexUniquenessLevel::PATH>>;

template class ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::tests::graph::MockGraphProvider::Step>>;

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::tests::graph::MockGraphProvider::Step>>,
    ::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>, ::arangodb::tests::graph::MockGraphProvider,
    ::arangodb::graph::PathValidator<::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>, VertexUniquenessLevel::PATH>>;

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::tests::graph::MockGraphProvider::Step>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>>,
    ::arangodb::tests::graph::MockGraphProvider,
    ::arangodb::graph::PathValidator<::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<::arangodb::tests::graph::MockGraphProvider::Step>>,
                                     VertexUniquenessLevel::PATH>>;
