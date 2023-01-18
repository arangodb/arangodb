////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/CollectionInfoCurrent.h"

#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>

namespace arangodb {

CollectionInfoCurrent::CollectionInfoCurrent(uint64_t currentVersion)
    : _currentVersion(currentVersion) {}

CollectionInfoCurrent::~CollectionInfoCurrent() = default;

bool CollectionInfoCurrent::add(std::string_view shardID, VPackSlice slice) {
  auto it = _vpacks.find(shardID);
  if (it == _vpacks.end()) {
    _vpacks.emplace(shardID, std::make_shared<VPackBuilder>(slice));
    return true;
  }
  return false;
}

VPackSlice CollectionInfoCurrent::getIndexes(std::string_view shardID) const {
  auto it = _vpacks.find(shardID);
  if (it != _vpacks.end()) {
    VPackSlice slice = it->second->slice();
    return slice.get("indexes");
  }
  return VPackSlice::noneSlice();
}

bool CollectionInfoCurrent::error(std::string_view shardID) const {
  return getFlag(StaticStrings::Error, shardID);
}

containers::FlatHashMap<ShardID, bool> CollectionInfoCurrent::error() const {
  return getFlag(StaticStrings::Error);
}

int CollectionInfoCurrent::errorNum(std::string_view shardID) const {
  auto it = _vpacks.find(shardID);
  if (it != _vpacks.end()) {
    VPackSlice slice = it->second->slice();
    return arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        slice, StaticStrings::ErrorNum, 0);
  }
  return 0;
}

containers::FlatHashMap<ShardID, int> CollectionInfoCurrent::errorNum() const {
  containers::FlatHashMap<ShardID, int> m;

  for (auto const& it : _vpacks) {
    int s = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        it.second->slice(), StaticStrings::ErrorNum, 0);
    m.insert(std::make_pair(it.first, s));
  }
  return m;
}

std::vector<ServerID> CollectionInfoCurrent::servers(
    std::string_view shardID) const {
  std::vector<ServerID> v;

  auto it = _vpacks.find(shardID);
  if (it != _vpacks.end()) {
    VPackSlice slice = it->second->slice();

    VPackSlice servers = slice.get("servers");
    if (servers.isArray()) {
      for (VPackSlice server : VPackArrayIterator(servers)) {
        if (server.isString()) {
          v.push_back(server.copyString());
        }
      }
    }
  }
  return v;
}

std::vector<ServerID> CollectionInfoCurrent::failoverCandidates(
    std::string_view shardID) const {
  std::vector<ServerID> v;

  auto it = _vpacks.find(shardID);
  if (it != _vpacks.end()) {
    VPackSlice slice = it->second->slice();

    VPackSlice servers = slice.get(StaticStrings::FailoverCandidates);
    if (servers.isArray()) {
      for (VPackSlice server : VPackArrayIterator(servers)) {
        TRI_ASSERT(server.isString());
        if (server.isString()) {
          v.emplace_back(server.copyString());
        }
      }
    }
  }
  return v;
}

std::string CollectionInfoCurrent::errorMessage(
    std::string_view shardID) const {
  auto it = _vpacks.find(shardID);
  if (it != _vpacks.end()) {
    VPackSlice slice = it->second->slice();
    if (slice.isObject() && slice.hasKey(StaticStrings::ErrorMessage)) {
      return slice.get(StaticStrings::ErrorMessage).copyString();
    }
  }
  return std::string();
}

uint64_t CollectionInfoCurrent::getCurrentVersion() const {
  return _currentVersion;
}

bool CollectionInfoCurrent::getFlag(std::string_view name,
                                    std::string_view shardID) const {
  auto it = _vpacks.find(shardID);
  if (it != _vpacks.end()) {
    return arangodb::basics::VelocyPackHelper::getBooleanValue(
        it->second->slice(), name, false);
  }
  return false;
}

containers::FlatHashMap<ShardID, bool> CollectionInfoCurrent::getFlag(
    std::string_view name) const {
  containers::FlatHashMap<ShardID, bool> m;
  for (auto const& it : _vpacks) {
    auto vpack = it.second;
    bool b = arangodb::basics::VelocyPackHelper::getBooleanValue(vpack->slice(),
                                                                 name, false);
    m.emplace(it.first, b);
  }
  return m;
}

}  // end namespace arangodb
