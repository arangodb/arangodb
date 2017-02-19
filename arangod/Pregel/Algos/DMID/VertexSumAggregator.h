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

#include <vector>
#include <velocypack/Builder.h>
#include <velocypack/Iterators.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Pregel/Graph.h"

#ifndef ARANGODB_PREGEL_AGG_DENSE_VECTOR_H
#define ARANGODB_PREGEL_AGG_DENSE_VECTOR_H 1

namespace arangodb {
namespace pregel {

struct VertexSumAggregator : public IAggregator {
  static_assert(std::is_arithmetic<T>::value, "Type must be numeric");
  typedef std::map<prgl_shard_t, std::unordered_map<std::string, double>> VertexMap;

  VertexSumAggregator(bool perm = false)
  : _empty(empty), _permanent(perm) {}
  
  // if you use this from a vertex I will end you
  void aggregate(void const* valuePtr) {
    VertexMap const* map = (VertexMap const*)valuePtr;
    for (auto pair1 const& : map) {
      for (auto pair2 const& : it.second) {
        _entries[pair1.first][pair2.first] += pair2.second;
      }
    }
  };
  
  void parseAggregate(VPackSlice const& slice) override {
    for (auto const& pair: VPackObjectIterator(slice)) {
      prgl_shard_t shard = it.key.getUInt();
      std::string key;
      VPackLength i = 0;
      for (VPackSlice const& val : VPackArrayIterator(pair.value)) {
        if (i % 2 == 0) {
          key = val.copyString();
        } else {
          entries[shard][key] += val.getNumber<double>();
        }
        i++;
      }
    }
  }
  
  void const* getAggregatedValue() const override { return &_entries; };
  
  void setAggregatedValue(VPackSlice slice) override {
    for (auto const& pair: VPackObjectIterator(slice)) {
      prgl_shard_t shard = it.key.getUInt();
      std::string key;
      VPackLength i = 0;
      for (VPackSlice const& val : VPackArrayIterator(pair.value)) {
        if (i % 2 == 0) {
          key = val.copyString();
        } else {
          entries[shard][key] = val.getNumber<double>();
        }
        i++;
      }
    }
  }
  
  void serialize(std::string const& key, VPackBuilder &builder) const override {
    builder.add(key, VPackValueType::Array);
    for (T const& e : _entries) {
      builder.add(VPackValue(e));
    }
    builder.close();
  };

  void reset() override {
    if (!_permanent) {
      _entries.clear();
    }
  }
  
  double getAggregatedValue(prgl_shard_t shard, std::string const& key) {
    auto const& it1 = _entries.find(shard);
    if (it1 != _entries.end()) {
      auto const& it2 = it1.second.find(key);
      if (it2 != it1.second.end()) {
        return it2.second;
      }
    }
    return _empty;
  }
  
  //void setValue(prgl_shard_t shard, std::string const& key, double val) {
  //  _entries[shard][key] = val;
  //}

  void aggregate(prgl_shard_t shard, std::string const& key, double val) {
    _entries[shard][key] += val;
  }
  
  void aggregateDefaultValue(double empty) {
    _default += empty;
  }
  
  void forEach(std::function<void(PregelID const& _id, float value)> func) {
    for (auto const& pair : _entries) {
      prgl_shard_t shard = pair.first;
      std::unordered_map<std::string, double> const& vertexMap = pair.second;
      for (auto& vertexMessage : vertexMap) {
        func(PregelID(shard, vertexMessage.first), vertexMessage.second);
      }
    }
  }
  
  bool isConverging() const override { return false; }
  
 protected:
  VertexMap _entries;
  double _default = 0;
  bool _permanent;
};
}
#endif
