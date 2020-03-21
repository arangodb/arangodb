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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <string>
#include <vector>
#include "Pregel/Graph.h"

#ifndef ARANGODB_PREGEL_AGG_DENSE_VECTOR_H
#define ARANGODB_PREGEL_AGG_DENSE_VECTOR_H 1

namespace arangodb {
namespace pregel {

struct VertexSumAggregator : public IAggregator {
  typedef std::map<PregelShard, std::unordered_map<std::string, double>> VertexMap;
  typedef std::pair<PregelShard, std::unordered_map<std::string, double>> MyPair;

  VertexSumAggregator(bool perm = false) : _permanent(perm) {}

  // if you use this from a vertex I will end you
  void aggregate(void const* valuePtr) override {
    VertexMap const* map = (VertexMap const*)valuePtr;
    for (auto const& pair1 : *map) {
      for (auto const& pair2 : pair1.second) {
        _entries[pair1.first][pair2.first] += pair2.second;
      }
    }
  };

  void parseAggregate(VPackSlice const& slice) override {
    for (auto const& pair : VPackObjectIterator(slice)) {
      PregelShard shard = std::stoi(pair.key.copyString());
      std::string key;
      VPackValueLength i = 0;
      for (VPackSlice const& val : VPackArrayIterator(pair.value)) {
        if (i % 2 == 0) {
          key = val.copyString();
        } else {
          _entries[shard][key] += val.getNumber<double>();
        }
        i++;
      }
    }
  }

  void const* getAggregatedValue() const override { return &_entries; };

  void setAggregatedValue(VPackSlice const& slice) override {
    for (auto const& pair : VPackObjectIterator(slice)) {
      PregelShard shard = std::stoi(pair.key.copyString());
      std::string key;
      VPackValueLength i = 0;
      for (VPackSlice const& val : VPackArrayIterator(pair.value)) {
        if (i % 2 == 0) {
          key = val.copyString();
        } else {
          _entries[shard][key] = val.getNumber<double>();
        }
        i++;
      }
    }
  }

  void serialize(std::string const& key, VPackBuilder& builder) const override {
    builder.add(key, VPackValue(VPackValueType::Object));
    for (auto const& pair1 : _entries) {
      builder.add(std::to_string(pair1.first), VPackValue(VPackValueType::Array));
      for (auto const& pair2 : pair1.second) {
        builder.add(VPackValuePair(pair2.first.data(), pair2.first.size(),
                                   VPackValueType::String));
        builder.add(VPackValue(pair2.second));
      }
      builder.close();
    }
    builder.close();
  };

  void reset() override {
    if (!_permanent) {
      _entries.clear();
    }
  }

  double getAggregatedValue(PregelShard shard, std::string const& key) const {
    auto const& it1 = _entries.find(shard);
    if (it1 != _entries.end()) {
      auto const& it2 = it1->second.find(key);
      if (it2 != it1->second.end()) {
        return it2->second;
      }
    }
    return _default;
  }

  // void setValue(PregelShard shard, std::string const& key, double val) {
  //  _entries[shard][key] = val;
  //}

  void aggregate(PregelShard shard, std::string const& key, double val) {
    _entries[shard][key] += val;
  }

  void aggregateDefaultValue(double empty) { _default += empty; }

  void forEach(std::function<void(PregelID const& _id, double value)> func) const {
    for (auto const& pair : _entries) {
      PregelShard shard = pair.first;
      std::unordered_map<std::string, double> const& vertexMap = pair.second;
      for (auto const& vertexMessage : vertexMap) {
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
}  // namespace pregel
}  // namespace arangodb
#endif
