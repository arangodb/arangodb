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

#include "OutgoingCache.h"
#include "IncomingCache.h"
#include "Utils.h"
#include "WorkerConfig.h"

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
    : _state(state), _format(cache->format()), _localCache(cache) {
  _baseUrl = Utils::baseUrl(_state->database());
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
  if (this->_state->isLocalVertexShard(shard)) {
    this->_localCache->setDirect(shard, key, data);
    // LOG(INFO) << "Worker: Got messages for myself " << key << " <- " << data;
    this->_sendMessages++;
  } else {
    _shardMap[shard][key].push_back(data);
    if (this->_containedMessages++ > this->_batchSize) {
      flushMessages();
    }
  }
}

template <typename M>
void ArrayOutCache<M>::flushMessages() {
  LOG(INFO) << "Beginning to send messages to other machines";
  uint64_t gss = this->_state->globalSuperstep();
  if (this->_nextPhase) {
    gss += 1;
  }

  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _shardMap) {
    prgl_shard_t shard = it.first;
    std::unordered_map<std::string, std::vector<M>> const& vertexMessageMap =
        it.second;
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    VPackBuilder package;
    package.openObject();
    package.add(Utils::messagesKey, VPackValue(VPackValueType::Array));
    for (auto const& vertexMessagePair : vertexMessageMap) {
      package.add(VPackValue(VPackValueType::Array));
      package.add(VPackValue(shard));
      package.add(VPackValue(vertexMessagePair.first));
      for (M const& val : vertexMessagePair.second) {
        this->_format->addValue(package, val);
        this->_sendMessages++;
      }
      package.close();
    }
    package.close();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey,
                VPackValue(this->_state->executionNumber()));
    package.add(Utils::globalSuperstepKey, VPackValue(gss));
    package.close();
    // add a request
    ShardID const& shardId = this->_state->globalShardIDs()[shard];
    auto body = std::make_shared<std::string>(package.toJson());
    requests.emplace_back("shard:" + shardId, rest::RequestType::POST,
                          this->_baseUrl + Utils::messagesPath, body);

    LOG(INFO) << "Worker: Sending data to other Shard: " << shardId
              << ". Message: " << package.toJson();
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
  if (this->_state->isLocalVertexShard(shard)) {
    this->_localCache->setDirect(shard, key, data);
    // LOG(INFO) << "Worker: Got messages for myself " << key << " <- " << data;
    this->_sendMessages++;
  } else {
    std::unordered_map<std::string, M>& vertexMap = _shardMap[shard];
    auto it = vertexMap.find(key);
    if (it != vertexMap.end()) {  // more than one message
      _combiner->combine(vertexMap[key], data);
    } else {  // first message for this vertex
      vertexMap.emplace(key, data);
    }

    if (this->_containedMessages++ > this->_batchSize) {
      flushMessages();
    }
  }
}

template <typename M>
void CombiningOutCache<M>::flushMessages() {
  LOG(INFO) << "Beginning to send messages to other machines";
  uint64_t gss = this->_state->globalSuperstep();
  if (this->_nextPhase) {
    gss += 1;
  }
  
  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _shardMap) {
    prgl_shard_t shard = it.first;
    std::unordered_map<std::string, M> const& vertexMessageMap = it.second;
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;

    VPackBuilder package(&options);
    package.openObject();
    package.add(Utils::messagesKey, VPackValue(VPackValueType::Array));
    for (auto const& vertexMessagePair : vertexMessageMap) {
      package.add(VPackValue(shard));
      package.add(VPackValue(vertexMessagePair.first));
      this->_format->addValue(package, vertexMessagePair.second);
      this->_sendMessages++;
    }
    package.close();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey,
                VPackValue(this->_state->executionNumber()));
    package.add(Utils::globalSuperstepKey, VPackValue(gss));
    package.close();
    // add a request
    ShardID const& shardId = this->_state->globalShardIDs()[shard];
    auto body = std::make_shared<std::string>(package.toJson());
    requests.emplace_back("shard:" + shardId, rest::RequestType::POST,
                          this->_baseUrl + Utils::messagesPath, body);

    LOG(INFO) << "Worker: Sending data to other Shard: " << shardId
              << ". Message: " << package.toJson();
  }
  size_t nrDone = 0;
  ClusterComm::instance()->performRequests(requests, 180, nrDone,
                                           LogTopic("Pregel message transfer"));
  Utils::printResponses(requests);
  this->clear();
}

// template types to create
template class arangodb::pregel::OutCache<int64_t>;
template class arangodb::pregel::OutCache<float>;
template class arangodb::pregel::ArrayOutCache<int64_t>;
template class arangodb::pregel::ArrayOutCache<float>;
template class arangodb::pregel::CombiningOutCache<int64_t>;
template class arangodb::pregel::CombiningOutCache<float>;
