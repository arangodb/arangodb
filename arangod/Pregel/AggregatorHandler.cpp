////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/AggregatorHandler.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"

using namespace arangodb;
using namespace arangodb::pregel;

AggregatorHandler::~AggregatorHandler() {
  WRITE_LOCKER(guard, _lock);
  for (auto const& it : _values) {
    delete it.second;
  }
  _values.clear();
}

IAggregator* AggregatorHandler::getAggregator(AggregatorID const& name) {
  {
    READ_LOCKER(guard, _lock);
    auto it = _values.find(name);
    if (it != _values.end()) {
      return it->second;
    }
  }
  // aggregator doesn't exists, create it
  std::unique_ptr<IAggregator> agg(_algorithm->aggregator(name));
  if (agg) {
    WRITE_LOCKER(guard, _lock);
    auto result = _values.insert({name, agg.get()});
    if (result.second) {
      return agg.release();
    }
    return result.first->second;
  } else {
    return nullptr;
  }
}

void AggregatorHandler::aggregate(AggregatorID const& name, const void* valuePtr) {
  IAggregator* agg = getAggregator(name);
  if (agg) {
    agg->aggregate(valuePtr);
  }
}

void AggregatorHandler::aggregateValues(AggregatorHandler const& workerValues) {
  for (auto const& pair : workerValues._values) {
    AggregatorID const& name = pair.first;
    IAggregator* agg = getAggregator(name);
    if (agg) {
      agg->aggregate(pair.second->getAggregatedValue());
    }
  }
}

void AggregatorHandler::aggregateValues(VPackSlice const& workerValues) {
  VPackSlice values = workerValues.get(Utils::aggregatorValuesKey);
  if (values.isObject()) {
    for (auto const& keyValue : VPackObjectIterator(values)) {
      AggregatorID name = keyValue.key.copyString();
      IAggregator* agg = getAggregator(name);
      if (agg) {
        agg->parseAggregate(keyValue.value);
      }
    }
  }
}

void AggregatorHandler::setAggregatedValues(VPackSlice const& workerValues) {
  VPackSlice values = workerValues.get(Utils::aggregatorValuesKey);
  if (values.isObject()) {
    for (auto const& keyValue : VPackObjectIterator(values)) {
      AggregatorID name = keyValue.key.copyString();
      IAggregator* agg = getAggregator(name);
      if (agg) {
        agg->setAggregatedValue(keyValue.value);
      }
    }
  }
}

const void* AggregatorHandler::getAggregatedValue(AggregatorID const& name) {
  IAggregator* agg = getAggregator(name);
  return agg != nullptr ? agg->getAggregatedValue() : nullptr;
}

void AggregatorHandler::resetValues() {
  for (auto& it : _values) {
    it.second->reset();
  }
}

bool AggregatorHandler::serializeValues(VPackBuilder& b, bool onlyConverging) const {
  bool hasValues = false;
  b.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
  READ_LOCKER(guard, _lock);
  for (auto const& pair : _values) {
    AggregatorID const& name = pair.first;
    IAggregator* agg = pair.second;
    if (!onlyConverging || agg->isConverging()) {
      agg->serialize(name, b);
      hasValues = true;
    }
  }
  guard.unlock();
  b.close();
  return hasValues;
}

size_t AggregatorHandler::size() const {
  READ_LOCKER(guard, _lock);
  return _values.size();
}
