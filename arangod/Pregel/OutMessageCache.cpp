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

OutMessageCache::OutMessageCache(CollectionID vertexCollection) : _vertexCollection(vertexCollection) {
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
  ShardID responsibleShard;
  bool usesDefaultShardingAttributes;
  VPackBuilder keyDoc;
  keyDoc.openObject();
  keyDoc.add(StaticStrings::KeyString, VPackValue(key));
  keyDoc.close();
  int res = _ci->getResponsibleShard(_vertexCollection, keyDoc.slice(), true,
                                     responsibleShard, usesDefaultShardingAttributes);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
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
/*
void OutMessageCache::addMessages(VPackArrayIterator incomingMessages) {
  
  //unordered_map<string, vector<VPackSlice>> messageBucket;
  //VPackSlice messages = data.get(Utils::messagesKey);
  for (auto const &it : incomingMessages) {
    std::string vertexId = it.get(StaticStrings::ToString).copyString();
    
    auto vmsg = _messages.find(vertexId);
    if (vmsg != _messages.end()) {
      
      // if no combiner
      // vmsg->add(it.slice())
      
      // TODO do not hardcode combiner
      int64_t old = vmsg->second->slice().get("value").getInt();
      int64_t nw = it.get("value").getInt();
      if (nw < old) {
        vmsg->second->clear();
        vmsg->second->add(it);
      }
    } else {
      // assuming we have a combiner
      std::unique_ptr<VPackBuilder> b(new VPackBuilder());
      b->add(it);
      _messages[vertexId] = b.get();
      b.release();
      
      // if no combiner
      // VPackBuilder *arr = new VPackBuilder(it);
      // arr->openArray();
      // arr->add(it)
      // _messages[vertexId] = arr;
    }
  }
}*/

void OutMessageCache::getMessages(ShardID const& shardId, VPackBuilder &outBuilder) {
  auto shardIt = _map.find(shardId);
  if (shardIt != _map.end()) {
    //auto vertices = *shardIt;
    outBuilder.openArray();
    for (auto messagesPair : shardIt->second) {
      outBuilder.add(VPackArrayIterator(messagesPair.second->slice()));      
    }
    outBuilder.close();
    //return ArrayIterator(vmsg->second->slice())
  
  }
  //else return VPackSlice();
}
