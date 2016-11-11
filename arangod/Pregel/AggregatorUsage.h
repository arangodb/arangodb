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

#ifndef ARANGODB_PREGEL_AGGRGS_CLLCTR_H
#define ARANGODB_PREGEL_AGGRGS_CLLCTR_H 1

#include <functional>
#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include "Aggregator.h"

namespace arangodb {
namespace pregel {
  
struct IAggregatorCreator;

class AggregatorUsage {
  const IAggregatorCreator *_create;
  /// written by the local worker or thread
  std::map<std::string, Aggregator*> _values;
  /// from the conductor, gathered from the last superstep
  //std::map<std::string, Aggregator*> _global;
  
public:
  
  AggregatorUsage(const IAggregatorCreator *c) : _create(c) {}
  
  ~AggregatorUsage() {
    for (auto const& it : _values) {
      delete it.second;
    }
    _values.clear();
  }
  
  void aggregate(std::string const& name, const void* valuePtr) {
    auto it = _values.find(name);
    if (it !=  _values.end()) {
      it->second->aggregate(valuePtr);
    } else {
      std::unique_ptr<Aggregator> agg(_create->aggregator(name));
      if (agg) {
        agg->aggregate(valuePtr);
        _values[name] = agg.get();
        agg.release();
      }
    }
  }
  
  const void* getAggregatedValue(std::string const& name) const {
    auto const& it = _values.find(name);
    if (it !=  _values.end()) {
      return it->second->getValue();
    }
    return nullptr;
  }
  
  void resetValues() {
    for (auto &it : _values) {
      it.second->reset();
    }
  }
  
  void aggregateValues(AggregatorUsage const& workerValues) {
    for (auto const& pair : workerValues._values) {
      std::string const& name = pair.first;
      auto my = _values.find(name);
      if (my != _values.end()) {
        my->second->aggregate(pair.second->getValue());
      } else {
        std::unique_ptr<Aggregator> agg(_create->aggregator(name));
        if (agg) {
          agg->aggregate(pair.second->getValue());
          _values[name] = agg.get();
          agg.release();
        }
      }
    }
  }

  void aggregateValues(VPackSlice workerValues) {
    for (auto const& keyValue : VPackObjectIterator(workerValues)) {
      std::string name = keyValue.key.copyString();
      auto const &it = _values.find(name);
      if (it != _values.end()) {
        it->second->aggregate(keyValue.value);
      } else {
        std::unique_ptr<Aggregator> agg(_create->aggregator(name));
        if (agg) {
          agg->aggregate(keyValue.value);
          _values[name] = agg.get();
          agg.release();
        }
      }
    }
  }
  
  void serializeValues(VPackBuilder &b) const {
    for (auto const& pair : _values) {
      b.add(pair.first,
            pair.second->vpackValue());
    }
  }
  
  size_t size() {
    return _values.size();
  }
};

}
}
#endif
