////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <Logger/LogMacros.h>
#include <Logger/Logger.h>

#include "WorkerContext.h"

namespace arangodb::pregel::algos::accumulators {

WorkerContext::WorkerContext(VertexAccumulators const* algorithm)
    : _algo(algorithm) {
  (void)_algo;

  auto const& customDefinitions = algorithm->options().customDefinitions;
  auto const& globalAccumulatorsDeclarations = algorithm->options().globalAccumulatorsDeclarations;

  for (auto&& acc : globalAccumulatorsDeclaration) {
    _globalAccumulators.emplace(acc.first, instantiateAccumulator(*this, acc.second, customDefinitions));
  }
}

AccumulatorMap const& WorkerContext::globalAccumulators() {
  return _globalAccumulators;
}

void WorkerContext::preGlobalSuperstep(uint64_t gss) {
  for (auto&& acc : globalAccumulators()) {
    auto res = acc.second.clearWithResult();
    // TODO: report error if res.ok is false
    (void)res;
  }
}

void WorkerContext::getUpdateMessagesIntoBuilder(VPackBuilder& builder) override {
  VPackObjectBuilder guard(builder);
  for (auto&& acc : globalAccumulators()) {
    builder.add(VPackValue(acc.first));
    res = acc.second.serializeWithResult(builder);
  }
}

}  // namespace arangodb::pregel::algos::accumulators
