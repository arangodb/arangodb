////////////////////////////////////////////////////////////////////////////////
///
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
///
////////////////////////////////////////////////////////////////////////////////

#include "AccumulatorAggregator.h"

namespace arangodb::pregel::algos::accumulators {

VertexAccumulatorAggregator::VertexAccumulatorAggregator(AccumulatorOptions const& opts,
                                                         bool persists)
    : fake(), accumulator(instantiateAccumulator(fake, opts)), permanent(persists) {
  if (accumulator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Failed to create global vertex accumulator.");
  }
}

/// @brief Used when updating aggregator value locally
void VertexAccumulatorAggregator::aggregate(void const* valuePtr)  {
  accumulator->updateValueFromPointer(valuePtr);
}

/// @brief Used when updating aggregator value from remote
void VertexAccumulatorAggregator::parseAggregate(arangodb::velocypack::Slice const& slice)  {
  accumulator->updateByMessageSlice(slice);
}

void const* VertexAccumulatorAggregator::getAggregatedValue() const {
  return accumulator->getValuePointer();
}

/// @brief Value from superstep S-1 supplied by the conductor
void VertexAccumulatorAggregator::setAggregatedValue(arangodb::velocypack::Slice const& slice)  {
  accumulator->setBySlice(slice);
}

void VertexAccumulatorAggregator::serialize(std::string const& key,
                                            arangodb::velocypack::Builder& builder) const  {
  VPackBuilder local;
  accumulator->getValueIntoBuilder(local);
  builder.add(VPackValue(key));
  builder.add(local.slice());
}

void VertexAccumulatorAggregator::reset()  {
  if (!permanent) {
    accumulator->clear();
  }
}

bool VertexAccumulatorAggregator::isConverging() const  {
  return false;
}

[[nodiscard]] AccumulatorBase& VertexAccumulatorAggregator::getAccumulator() const {
  return *accumulator;
}

}  // namespace arangodb::pregel::algos::accumulators
