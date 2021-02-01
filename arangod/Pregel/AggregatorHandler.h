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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_AGGREGATOR_HANDLER_H
#define ARANGODB_PREGEL_AGGREGATOR_HANDLER_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>
#include <map>
#include "Basics/ReadWriteLock.h"
#include "Pregel/Aggregator.h"

namespace arangodb {
namespace pregel {

struct IAlgorithm;

/// Thread safe wrapper around handles
class AggregatorHandler {
  const IAlgorithm* _algorithm;
  std::map<std::string, IAggregator*> _values;
  mutable basics::ReadWriteLock _lock;

 public:
  explicit AggregatorHandler(IAlgorithm const* c) : _algorithm(c) {}
  ~AggregatorHandler();

  IAggregator* getAggregator(std::string const& name);

  /// aggregate this value
  void aggregate(std::string const& name, const void* valuePtr);
  /// aggregates all values from this aggregator
  void aggregateValues(AggregatorHandler const& workerValues);
  /// aggregates all values from this aggregator
  void aggregateValues(VPackSlice const& workerValues);

  /// return true if there are values in this Slice
  void setAggregatedValues(VPackSlice const& workerValues);
  // void setAggregatedValue(std::string const& name, const void* valuePtr);

  /// get the pointer to an aggregator value
  const void* getAggregatedValue(std::string const& name);

  /// calls reset on every aggregator
  void resetValues();

  /// return true if there values in this aggregator which were serialized
  bool serializeValues(VPackBuilder& b, bool onlyConverging = false) const;
  size_t size() const;
};
}  // namespace pregel
}  // namespace arangodb
#endif
