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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_PATHMANAGEMENT_PATHSTORE_TRACER_H
#define ARANGOD_GRAPH_PATHMANAGEMENT_PATHSTORE_TRACER_H 1

#include "Graph/EdgeDocumentToken.h"
#include "Graph/Helpers/TraceEntry.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/TypeAliases.h"

#include "Basics/ResourceUsage.h"

#include <unordered_map>
#include <vector>

namespace arangodb {

namespace aql {
class QueryContext;
}

namespace graph {

class ValidationResult;

template <class PathStoreImpl>
class PathStoreTracer {
 public:
  using Step = typename PathStoreImpl::Step;

 public:
  explicit PathStoreTracer(arangodb::ResourceMonitor& resourceMonitor);
  ~PathStoreTracer();

  // @brief reset
  void reset();

  // @brief Method adds a new element to the schreier vector
  // returns the index of inserted element
  size_t append(Step step);

  // @brief returns the current vector size
  size_t size() const;

  template <class ProviderType>
  auto buildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const -> void;

  template <class ProviderType>
  auto reverseBuildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const
      -> void;

  auto visitReversePath(Step const& step, std::function<bool(Step const&)> const& visitor) const
      -> bool;

 private:
  PathStoreImpl _impl;

  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable std::unordered_map<std::string, TraceEntry> _stats;
};

}  // namespace graph
}  // namespace arangodb

#endif
