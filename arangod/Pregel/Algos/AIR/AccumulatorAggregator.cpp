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
                                                         CustomAccumulatorDefinitions const& defs,
                                                         bool persists)
    : fake(), accumulator(instantiateAccumulator(fake, opts, defs)), permanent(persists) {
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
  LOG_DEVEL << accumulator.get()  << "parseAggregate = " << slice.toJson();
  LOG_DEVEL << "NOT IMPLEMENTED";
  //accumulator->updateByMessageSlice(slice);
}

void const* VertexAccumulatorAggregator::getAggregatedValue() const {
  return accumulator->getValuePointer();
}

/// @brief Value from superstep S-1 supplied by the conductor
void VertexAccumulatorAggregator::setAggregatedValue(arangodb::velocypack::Slice const& slice)  {
  LOG_DEVEL << accumulator.get() << "setAggregatedValue " << slice.toJson();
  //accumulator->setBySlice(slice);
  LOG_DEVEL << "NOT IMPLEMENTED";
}

void VertexAccumulatorAggregator::serialize(std::string const& key,
                                            arangodb::velocypack::Builder& builder) const  {
  VPackBuilder local;
  accumulator->serializeIntoBuilder(local);
  LOG_DEVEL << accumulator.get() << "serialize into key " << key << " with value " << local.toJson();
  builder.add(VPackValue(key));
  builder.add(local.slice());
}

void VertexAccumulatorAggregator::reset()  {
  if (!permanent) {
    LOG_DEVEL << "calling clear on accumulator";
    if (auto res = accumulator->clearWithResult(); res.fail()) {
      LOG_DEVEL << res.error().toString();
    }
  }
}

bool VertexAccumulatorAggregator::isConverging() const  {
  return false;
}

[[nodiscard]] AccumulatorBase& VertexAccumulatorAggregator::getAccumulator() const {
  return *accumulator;
}

}  // namespace arangodb::pregel::algos::accumulators
