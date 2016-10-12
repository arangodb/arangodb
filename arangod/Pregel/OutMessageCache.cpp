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

#include "OutMessageCache.h"
#include "Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

OutMessageCache::OutMessageCache(CollectionID &vertexCollection, std::string baseUrl) : _collection(vertexCollection) {
  _ci = ClusterInfo::instance();
  
  auto shardMap = _ci->getShardList(vertexCollection);
  for (ShardID const &it : *shardMap) {
    _map[it];
  }
}

OutMessageCache::~OutMessageCache() {
  for (auto const &it : _map) {
    for (auto const &it2 : it.second) {
      delete(it2.second);
    }
  }
  _map.clear();
}

void OutMessageCache::clean() {
  //TODO better way?
  for (auto const &it : _map) {
    for (auto const &it2 : it.second) {
      it2.second->clear();// clears VPackBuilder
    }
  }
  _map.clear();
}

void OutMessageCache::addMessage(std::string key, VPackSlice slice) {
  LOG(INFO) << "Adding messages to out queue\n";
  
  ShardID responsibleShard;
  bool usesDefaultShardingAttributes;
  VPackBuilder keyDoc;
  keyDoc.openObject();
  keyDoc.add(StaticStrings::KeyString, VPackValue(key));
  keyDoc.close();
  int res = _ci->getResponsibleShard(_collection, keyDoc.slice(), true,
                                     responsibleShard, usesDefaultShardingAttributes);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, "OutMessageCache could not resolve the responsible shard");
  }
  TRI_ASSERT(usesDefaultShardingAttributes);// should be true anyway
    
  //std::unordered_map<std::string, VPackBuilder*> vertexMap =;
  auto it = _map[responsibleShard].find(key);
  if (it != _map[responsibleShard].end()) {// more than one message
    VPackBuilder *b = it->second;
    
    // hardcoded combiner
    int64_t oldValue = b->slice().get("value").getInt();
    int64_t newValue = slice.get("value").getInt();
    if (newValue < oldValue) {
      b->clear();
      b->add(slice);
    }
    
  } else {// first message for this vertex
     std::unique_ptr<VPackBuilder> b(new VPackBuilder());
    b->add(slice);
    _map[responsibleShard][key] = b.get();
    b.release();
  }
}

void OutMessageCache::getMessages(ShardID const& shardId, VPackBuilder &outBuilder) {
  auto shardIt = _map.find(shardId);
  outBuilder.openArray();
  if (shardIt != _map.end()) {
    //auto vertices = *shardIt;
    for (auto messagesPair : shardIt->second) {
      outBuilder.add(VPackArrayIterator(messagesPair.second->slice()));      
    }
    //return ArrayIterator(vmsg->second->slice())
  }
  outBuilder.close();
  //else return VPackSlice();
}
