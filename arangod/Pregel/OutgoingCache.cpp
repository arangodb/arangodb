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
#include "Pregel/IncomingCache.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
#include "Pregel/CommonFormats.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename M>
OutCache<M>::OutCache(WorkerConfig* state, InCache<M>* cache)
    : _config(state), _format(cache->format()), _localCache(cache) {
  _baseUrl = Utils::baseUrl(_config->database());
}

template <typename M>
OutCache<M>::OutCache(WorkerConfig* state, InCache<M>* cache,
                      InCache<M>* nextGSS)
    : _config(state),
      _format(cache->format()),
      _localCache(cache),
      _localCacheNextGSS(nextGSS) {
  _baseUrl = Utils::baseUrl(_config->database());
}

// ================= ArrayOutCache ==================

template <typename M>
ArrayOutCache<M>::~ArrayOutCache() {
  clear();
}

template <typename M>
void ArrayOutCache<M>::clear() {
  _shardMap.clear();
  this->_containedMessages = 0;
}

template <typename M>
void ArrayOutCache<M>::appendMessage(prgl_shard_t shard, std::string const& key,
                                     M const& data) {
  if (this->_config->isLocalVertexShard(shard)) {
    if (this->_sendToNextGSS) {
      this->_localCacheNextGSS->setDirect(shard, key, data);
      this->_sendCountNextGSS++;
    } else {
      this->_localCache->setDirect(shard, key, data);
      this->_sendCount++;
    }
  } else {
    _shardMap[shard][key].push_back(data);
    if (this->_containedMessages++ > this->_batchSize) {
      flushMessages();
    }
  }
}

template <typename M>
void ArrayOutCache<M>::flushMessages() {
  //LOG(INFO) << "Beginning to send messages to other machines";
  uint64_t gss = this->_config->globalSuperstep();
  if (this->_sendToNextGSS) {
    gss += 1;
  }
  VPackOptions options = VPackOptions::Defaults;
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _shardMap) {
    prgl_shard_t shard = it.first;
    std::unordered_map<std::string, std::vector<M>> const& vertexMessageMap =
        it.second;
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    VPackBuilder data;
    data.openObject(&options);
    data.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    data.add(Utils::executionNumberKey,
                VPackValue(this->_config->executionNumber()));
    data.add(Utils::globalSuperstepKey, VPackValue(gss));
    data.add(Utils::shardIdKey, VPackValue(shard));
    data.add(Utils::messagesKey, VPackValue(VPackValueType::Array, true));
    for (auto const& vertexMessagePair : vertexMessageMap) {
      data.add(VPackValue(vertexMessagePair.first));// key
      data.add(VPackValue(VPackValueType::Array, true));// message array
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

    //LOG(INFO) << "Worker: Sending data to other Shard: " << shardId;
    //<< ". Message: " << package.toJson();
  }
  size_t nrDone = 0;
  ClusterComm::instance()->performRequests(requests, 120, nrDone,
                                           LogTopic("Pregel message transfer"));
  // readResults(requests);
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      LOG(INFO) << res.answer->payload().toJson();
    }
  }
  this->clear();
}

// ================= CombiningOutCache ==================

template <typename M>
CombiningOutCache<M>::CombiningOutCache(WorkerConfig* state,
                                        CombiningInCache<M>* cache)
    : OutCache<M>(state, cache), _combiner(cache->combiner()) {}

template <typename M>
CombiningOutCache<M>::CombiningOutCache(WorkerConfig* state,
                                        CombiningInCache<M>* cache,
                                        InCache<M>* nextPhase)
    : OutCache<M>(state, cache, nextPhase), _combiner(cache->combiner()) {}

template <typename M>
CombiningOutCache<M>::~CombiningOutCache() {
  clear();
}

template <typename M>
void CombiningOutCache<M>::clear() {
  _shardMap.clear();
  this->_containedMessages = 0;
}

template <typename M>
void CombiningOutCache<M>::appendMessage(prgl_shard_t shard,
                                         std::string const& key,
                                         M const& data) {
  if (this->_config->isLocalVertexShard(shard)) {
    if (this->_sendToNextGSS) {
      this->_localCacheNextGSS->setDirect(shard, key, data);
      this->_sendCountNextGSS++;
    } else {
      this->_localCache->setDirect(shard, key, data);
      this->_sendCount++;
    }
  } else {
    std::unordered_map<std::string, M>& vertexMap = _shardMap[shard];
    auto it = vertexMap.find(key);
    if (it != vertexMap.end()) {  // more than one message
      _combiner->combine(vertexMap[key], data);
    } else {  // first message for this vertex
      vertexMap.emplace(key, data);
      
      if (this->_containedMessages++ > this->_batchSize) {
        LOG(INFO) << "Hit buffer limit";
        flushMessages();
      }
    }
  }
}

template <typename M>
void CombiningOutCache<M>::flushMessages() {
  //LOG(INFO) << "Beginning to send messages to other machines";
  uint64_t gss = this->_config->globalSuperstep();
  if (this->_sendToNextGSS && this->_config->asynchronousMode()) {
    gss += 1;
  }
  VPackOptions options = VPackOptions::Defaults;
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _shardMap) {
    prgl_shard_t shard = it.first;
    std::unordered_map<std::string, M> const& vertexMessageMap = it.second;
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
      data.add(VPackValue(vertexMessagePair.first));// key
      this->_format->addValue(data, vertexMessagePair.second); // value
      
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

    //LOG(INFO) << "Worker: Sending data to other Shard: " << shardId;
    //          << ". Message: " << package.toJson();
  }
  size_t nrDone = 0;
  ClusterComm::instance()->performRequests(requests, 180, nrDone,
                                           LogTopic("Pregel"));
  Utils::printResponses(requests);
  this->clear();
}

// template types to create

template class arangodb::pregel::OutCache<int64_t>;
template class arangodb::pregel::OutCache<float>;
template class arangodb::pregel::OutCache<double>;
template class arangodb::pregel::ArrayOutCache<int64_t>;
template class arangodb::pregel::ArrayOutCache<float>;
template class arangodb::pregel::ArrayOutCache<double>;
template class arangodb::pregel::CombiningOutCache<int64_t>;
template class arangodb::pregel::CombiningOutCache<float>;
template class arangodb::pregel::CombiningOutCache<double>;

// algo specific
template class arangodb::pregel::OutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::ArrayOutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::CombiningOutCache<SenderMessage<uint64_t>>;
