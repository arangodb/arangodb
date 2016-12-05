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

#include "IncomingCache.h"
#include "Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename M>
IncomingCache<M>::~IncomingCache() {
  LOG(INFO) << "~IncomingCache";
  _shardMap.clear();
}

template <typename M>
void IncomingCache<M>::clear() {
  MUTEX_LOCKER(guard, _writeLock);
  _receivedMessageCount = 0;
  _shardMap.clear();
}

template <typename M>
void IncomingCache<M>::parseMessages(VPackSlice incomingMessages) {
  prgl_shard_t shard;
  std::string key;
  VPackValueLength i = 0;
  for (VPackSlice current : VPackArrayIterator(incomingMessages)) {
    if (i % 3 == 0) {
      shard = (prgl_shard_t) current.getUInt();
    } else if (i % 3 == 1) {  // TODO support multiple recipients
      key = current.copyString();
    } else {
      M newValue;
      bool sucess = _format->unwrapValue(current, newValue);
      if (!sucess) {
        LOG(WARN) << "Invalid message format supplied";
        continue;
      }
      setDirect(shard, key, newValue);
    }
    i++;
  }
  
  if (i % 3 != 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
                                   TRI_ERROR_BAD_PARAMETER,
                                   "There must always be a multiple of 3 entries in messages");
  }
}

template <typename M>
void IncomingCache<M>::setDirect(prgl_shard_t shard, std::string const& key, M const& newValue) {
  MUTEX_LOCKER(guard, _writeLock);
  
  _receivedMessageCount++;
  std::unordered_map<std::string, M> &vertexMap = _shardMap[shard];
  auto vmsg = vertexMap.find(key);
  if (vmsg != vertexMap.end()) {  // got a message for the same vertex
    vmsg->second = _combiner->combine(vmsg->second, newValue);
  } else {
    vertexMap.insert(std::make_pair(key, newValue));
  }
}

template <typename M>
void IncomingCache<M>::mergeCache(IncomingCache<M> const& otherCache) {
  MUTEX_LOCKER(guard, _writeLock);
  _receivedMessageCount += otherCache._receivedMessageCount;
  
  // cannot call setDirect since it locks
  for (auto const& pair : otherCache._shardMap) {
    std::unordered_map<std::string, M> &vertexMap = _shardMap[pair.first];
    
    for (auto &vertexMessage : pair.second) {
      auto vmsg = vertexMap.find(vertexMessage.first);
      if (vmsg != vertexMap.end()) {  // got a message for the same vertex
        vmsg->second = _combiner->combine(vmsg->second, vertexMessage.second);
      } else {
        vertexMap.insert(vertexMessage);
      }
    }
  }
}

template <typename M>
MessageIterator<M> IncomingCache<M>::getMessages(prgl_shard_t shard, std::string const& key) {
  std::unordered_map<std::string, M> const& vertexMap = _shardMap[shard];
  auto vmsg = vertexMap.find(key);
  if (vmsg != vertexMap.end()) {
    LOG(INFO) << "Got a message for " << key;
    return MessageIterator<M>(&vmsg->second);
  } else {
    LOG(INFO) << "No message for " << key;
    return MessageIterator<M>();
  }
}

// template types to create
template class arangodb::pregel::IncomingCache<int64_t>;
template class arangodb::pregel::IncomingCache<float>;
