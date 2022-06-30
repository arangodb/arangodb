////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "PathEnumeratorInterface.h"

#include "Aql/QueryContext.h"
#include "Graph/algorithm-aliases.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/ClusterProviderStep.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/PathManagement/PathValidatorOptions.h"


#include <memory>

using namespace arangodb;
using namespace arangodb::graph;


template<class ProviderName>
auto PathEnumeratorInterface::createEnumerator(
    aql::QueryContext& query,
    typename ProviderName::Options&& forwardProviderOptions,
    typename ProviderName::Options&& backwardProviderOptions,
    TwoSidedEnumeratorOptions enumeratorOptions,
    PathValidatorOptions validatorOptions, PathEnumeratorType type,
    bool useTracing) -> std::unique_ptr<PathEnumeratorInterface> {
  switch (type) {
    case PathEnumeratorType::K_PATH: {
      return std::make_unique<KPathEnumerator<ProviderName>>(
          ProviderName{query, std::move(forwardProviderOptions),
                   query.resourceMonitor()},
          ProviderName{query, std::move(backwardProviderOptions),
                   query.resourceMonitor()},
          std::move(enumeratorOptions), std::move(validatorOptions),
          query.resourceMonitor());
    }
    default: {}
  }
  TRI_ASSERT(false);
  return nullptr;
}

template auto PathEnumeratorInterface::createEnumerator<SingleServerProvider<arangodb::graph::SingleServerProviderStep>>(
    arangodb::aql::QueryContext& query,
    SingleServerProvider<arangodb::graph::SingleServerProviderStep>::Options&& forwardProviderOptions,
    SingleServerProvider<arangodb::graph::SingleServerProviderStep>::Options&& backwardProviderOptions,
    arangodb::graph::TwoSidedEnumeratorOptions enumeratorOptions,
    arangodb::graph::PathValidatorOptions validatorOptions, PathEnumeratorType type,
    bool useTracing) -> std::unique_ptr<arangodb::graph::PathEnumeratorInterface>;

template auto PathEnumeratorInterface::createEnumerator<ClusterProvider<arangodb::graph::ClusterProviderStep>>(
    arangodb::aql::QueryContext& query,
    ClusterProvider<arangodb::graph::ClusterProviderStep>::Options&& forwardProviderOptions,
    ClusterProvider<arangodb::graph::ClusterProviderStep>::Options&& backwardProviderOptions,
    arangodb::graph::TwoSidedEnumeratorOptions enumeratorOptions,
    arangodb::graph::PathValidatorOptions validatorOptions, PathEnumeratorType type,
    bool useTracing) -> std::unique_ptr<arangodb::graph::PathEnumeratorInterface>;
