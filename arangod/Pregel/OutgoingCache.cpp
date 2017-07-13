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

#include "Pregel/OutgoingCache.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename M>
OutCache<M>::OutCache(WorkerConfig* state, MessageFormat<M> const* format)
    : _config(state), _format(format) {
  _baseUrl = Utils::baseUrl(_config->database(), Utils::workerPrefix);
}

// ================= ArrayOutCache ==================

template <typename M>
ArrayOutCache<M>::~ArrayOutCache() {}

template <typename M>
void ArrayOutCache<M>::_removeContainedMessages() {
  for (auto& pair : _shardMap) {
    pair.second.clear();
  }
  this->_containedMessages = 0;
}

template <typename M>
void ArrayOutCache<M>::appendMessage(PregelShard shard, PregelKey const& key,
                                     M const& data) {
  if (this->_config->isLocalVertexShard(shard)) {
    if (this->_sendToNextGSS) {  // I use the global cache, we need locking
      this->_localCacheNextGSS->storeMessage(shard, key, data);
      this->_sendCountNextGSS++;
    } else {  // the local cache is always thread local
      this->_localCache->storeMessageNoLock(shard, key, data);
      this->_sendCount++;
    }
  } else {
    _shardMap[shard][key].push_back(data);
    if (++(this->_containedMessages) >= this->_batchSize) {
      flushMessages();
    }
  }
}

template <typename M>
void ArrayOutCache<M>::flushMessages() {
  if (this->_containedMessages == 0) {
    return;
  }

  // LOG_TOPIC(INFO, Logger::PREGEL) << "Beginning to send messages to other
  // machines";
  uint64_t gss = this->_config->globalSuperstep();
  if (this->_sendToNextGSS) {
    gss += 1;
  }
  VPackOptions options = VPackOptions::Defaults;
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _shardMap) {
    PregelShard shard = it.first;
    std::unordered_map<PregelKey, std::vector<M>> const& vertexMessageMap =
        it.second;
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    VPackBuilder data(&options);
    data.openObject();
    data.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    data.add(Utils::executionNumberKey,
             VPackValue(this->_config->executionNumber()));
    data.add(Utils::globalSuperstepKey, VPackValue(gss));
    data.add(Utils::shardIdKey, VPackValue(shard));
    data.add(Utils::messagesKey, VPackValue(VPackValueType::Array, true));
    for (auto const& vertexMessagePair : vertexMessageMap) {
      data.add(VPackValue(vertexMessagePair.first));      // key
      data.add(VPackValue(VPackValueType::Array, true));  // message array
      for (M const& val : vertexMessagePair.second) {
        this->_format->addValue(data, val);
        if (this->_sendToNextGSS) {
          this->_sendCountNextGSS++;
        } else {
          this->_sendCount++;
        }
      }
      data.close();
    }
    data.close();
    data.close();
    // add a request
    ShardID const& shardId = this->_config->globalShardIDs()[shard];
    auto body = std::make_shared<std::string>(data.toJson());
    requests.emplace_back("shard:" + shardId, rest::RequestType::POST,
                          this->_baseUrl + Utils::messagesPath, body);
  }
  size_t nrDone = 0;
  ClusterComm::instance()->performRequests(requests, 120, nrDone,
                                           LogTopic("Pregel message transfer"),
                                           false);
  Utils::printResponses(requests);
  this->_removeContainedMessages();
}

// ================= CombiningOutCache ==================

template <typename M>
CombiningOutCache<M>::CombiningOutCache(WorkerConfig* state,
                                        MessageFormat<M> const* format,
                                        MessageCombiner<M> const* combiner)
    : OutCache<M>(state, format), _combiner(combiner) {}

template <typename M>
CombiningOutCache<M>::~CombiningOutCache() {}

template <typename M>
void CombiningOutCache<M>::_removeContainedMessages() {
  for (auto& pair : _shardMap) {
    pair.second.clear();
  }
  this->_containedMessages = 0;
}

template <typename M>
void CombiningOutCache<M>::appendMessage(PregelShard shard,
                                         PregelKey const& key, M const& data) {
  if (this->_config->isLocalVertexShard(shard)) {
    if (this->_sendToNextGSS) {
      this->_localCacheNextGSS->storeMessage(shard, key, data);
      this->_sendCountNextGSS++;
    } else {
      this->_localCache->storeMessageNoLock(shard, key, data);
      this->_sendCount++;
    }
  } else {
    std::unordered_map<PregelKey, M>& vertexMap = _shardMap[shard];
    auto it = vertexMap.find(key);
    if (it != vertexMap.end()) {  // more than one message
      _combiner->combine(vertexMap[key], data);
    } else {  // first message for this vertex
      vertexMap.emplace(key, data);

      if (++(this->_containedMessages) >= this->_batchSize) {
        // LOG_TOPIC(INFO, Logger::PREGEL) << "Hit buffer limit";
        flushMessages();
      }
    }
  }
}

template <typename M>
void CombiningOutCache<M>::flushMessages() {
  if (this->_containedMessages == 0) {
    return;
  }

  uint64_t gss = this->_config->globalSuperstep();
  if (this->_sendToNextGSS && this->_config->asynchronousMode()) {
    gss += 1;
  }
  VPackOptions options = VPackOptions::Defaults;
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _shardMap) {
    PregelShard shard = it.first;
    std::unordered_map<PregelKey, M> const& vertexMessageMap = it.second;
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    VPackBuilder data(&options);
    data.openObject();
    data.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    data.add(Utils::executionNumberKey,
             VPackValue(this->_config->executionNumber()));
    data.add(Utils::globalSuperstepKey, VPackValue(gss));
    data.add(Utils::shardIdKey, VPackValue(shard));
    data.add(Utils::messagesKey, VPackValue(VPackValueType::Array, true));
    for (auto const& vertexMessagePair : vertexMessageMap) {
      data.add(VPackValue(vertexMessagePair.first));            // key
      this->_format->addValue(data, vertexMessagePair.second);  // value

      if (this->_sendToNextGSS) {
        this->_sendCountNextGSS++;
      } else {
        this->_sendCount++;
      }
    }
    data.close();
    data.close();
    // add a request
    ShardID const& shardId = this->_config->globalShardIDs()[shard];
    auto body = std::make_shared<std::string>(data.toJson());
    requests.emplace_back("shard:" + shardId, rest::RequestType::POST,
                          this->_baseUrl + Utils::messagesPath, body);
  }
  size_t nrDone = 0;
  ClusterComm::instance()->performRequests(requests, 180, nrDone,
                                           LogTopic("Pregel"), false);
  Utils::printResponses(requests);
  _removeContainedMessages();
}

// template types to create

template class arangodb::pregel::OutCache<int64_t>;
template class arangodb::pregel::OutCache<uint64_t>;
template class arangodb::pregel::OutCache<float>;
template class arangodb::pregel::OutCache<double>;
template class arangodb::pregel::ArrayOutCache<int64_t>;
template class arangodb::pregel::ArrayOutCache<uint64_t>;
template class arangodb::pregel::ArrayOutCache<float>;
template class arangodb::pregel::ArrayOutCache<double>;
template class arangodb::pregel::CombiningOutCache<int64_t>;
template class arangodb::pregel::CombiningOutCache<uint64_t>;
template class arangodb::pregel::CombiningOutCache<float>;
template class arangodb::pregel::CombiningOutCache<double>;

// algo specific
template class arangodb::pregel::OutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::ArrayOutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::CombiningOutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::OutCache<SenderMessage<double>>;
template class arangodb::pregel::ArrayOutCache<SenderMessage<double>>;
template class arangodb::pregel::CombiningOutCache<SenderMessage<double>>;
template class arangodb::pregel::OutCache<DMIDMessage>;
template class arangodb::pregel::ArrayOutCache<DMIDMessage>;
template class arangodb::pregel::CombiningOutCache<DMIDMessage>;
template class arangodb::pregel::OutCache<HLLCounter>;
template class arangodb::pregel::ArrayOutCache<HLLCounter>;
template class arangodb::pregel::CombiningOutCache<HLLCounter>;
