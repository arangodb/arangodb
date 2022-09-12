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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/OutgoingCache.h"
#include "Pregel/Algos/AIR/AIR.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/Graph.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/NetworkConnection.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker/WorkerConfig.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Pregel/WorkerConductorMessages.h"
#include "VocBase/LogicalCollection.h"
#include "velocypack/Builder.h"

#include <velocypack/Iterator.h>
#include <cstddef>
#include <cstdint>

using namespace arangodb;
using namespace arangodb::pregel;

template<typename M>
OutCache<M>::OutCache(WorkerConfig* state, MessageFormat<M> const* format)
    : _config(state),
      _format(format),
      _baseUrl(Utils::baseUrl(Utils::workerPrefix)) {}

// ================= ArrayOutCache ==================

template<typename M>
ArrayOutCache<M>::~ArrayOutCache() = default;

template<typename M>
void ArrayOutCache<M>::_removeContainedMessages() {
  for (auto& pair : _shardMap) {
    pair.second.clear();
  }
  this->_containedMessages = 0;
}

template<typename M>
void ArrayOutCache<M>::appendMessage(PregelShard shard,
                                     std::string_view const& key,
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
    _shardMap[shard][std::string(key)].push_back(data);
    if (++(this->_containedMessages) >= this->_batchSize) {
      flushMessages();
    }
  }
}

template<typename M>
auto ArrayOutCache<M>::messagesToVPack(
    std::unordered_map<std::string, std::vector<M>> const& messagesForVertices)
    -> std::tuple<size_t, VPackBuilder> {
  VPackBuilder messagesVPack;
  size_t messageCount = 0;
  {
    VPackArrayBuilder ab(&messagesVPack);
    for (auto const& [vertex, messages] : messagesForVertices) {
      messagesVPack.add(VPackValue(vertex));  // key
      {
        VPackArrayBuilder ab2(&messagesVPack);
        for (M const& message : messages) {
          this->_format->addValue(messagesVPack, message);
          messageCount++;
        }
      }
    }
  }
  return {messageCount, messagesVPack};
}

template<typename M>
void ArrayOutCache<M>::flushMessages() {
  if (this->_containedMessages == 0) {
    return;
  }

  uint64_t gss = this->_config->globalSuperstep();
  if (this->_sendToNextGSS) {
    gss += 1;
  }

  network::RequestOptions reqOpts;
  reqOpts.database = this->_config->database();
  reqOpts.skipScheduler = true;
  auto connection = Connection::create(this->_baseUrl, std::move(reqOpts),
                                       *this->_config->vocbase());

  std::vector<futures::Future<Result>> responses;
  for (auto const& [shard, vertexMessageMap] : _shardMap) {
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    auto [shardMessageCount, messageVPack] = messagesToVPack(vertexMessageMap);
    responses.emplace_back(connection.send(
        Destination{Destination::Type::shard,
                    this->_config->globalShardIDs()[shard]},
        ModernMessage{.executionNumber = this->_config->executionNumber(),
                      .payload = PregelMessage{
                          .senderId = ServerState::instance()->getId(),
                          .gss = gss,
                          .shard = shard,
                          .messages = messageVPack}}));

    if (this->_sendToNextGSS) {
      this->_sendCountNextGSS += shardMessageCount;
    } else {
      this->_sendCount += shardMessageCount;
    }
  }

  futures::collectAll(responses).wait();

  this->_removeContainedMessages();
}

// ================= CombiningOutCache ==================

template<typename M>
CombiningOutCache<M>::CombiningOutCache(WorkerConfig* state,
                                        MessageFormat<M> const* format,
                                        MessageCombiner<M> const* combiner)
    : OutCache<M>(state, format), _combiner(combiner) {}

template<typename M>
CombiningOutCache<M>::~CombiningOutCache() = default;

template<typename M>
void CombiningOutCache<M>::_removeContainedMessages() {
  for (auto& pair : _shardMap) {
    pair.second.clear();
  }
  this->_containedMessages = 0;
}

template<typename M>
void CombiningOutCache<M>::appendMessage(PregelShard shard,
                                         std::string_view const& key,
                                         M const& data) {
  if (this->_config->isLocalVertexShard(shard)) {
    if (this->_sendToNextGSS) {
      this->_localCacheNextGSS->storeMessage(shard, key, data);
      this->_sendCountNextGSS++;
    } else {
      this->_localCache->storeMessageNoLock(shard, key, data);
      this->_sendCount++;
    }
  } else {
    std::unordered_map<std::string_view, M>& vertexMap = _shardMap[shard];
    auto it = vertexMap.find(key);
    if (it != vertexMap.end()) {  // more than one message
      auto& ref = (*it).second;   // will be modified by combine(...)
      _combiner->combine(ref, data);
    } else {  // first message for this vertex
      vertexMap.try_emplace(key, data);

      if (++(this->_containedMessages) >= this->_batchSize) {
        // LOG_TOPIC("23bc7", INFO, Logger::PREGEL) << "Hit buffer limit";
        flushMessages();
      }
    }
  }
}

template<typename M>
auto CombiningOutCache<M>::messagesToVPack(
    std::unordered_map<std::string_view, M> const& messagesForVertices)
    -> VPackBuilder {
  VPackBuilder messagesVPack;
  {
    VPackArrayBuilder ab(&messagesVPack);
    for (auto const& [vertex, message] : messagesForVertices) {
      messagesVPack.add(VPackValuePair(vertex.data(), vertex.size(),
                                       VPackValueType::String));  // key
      this->_format->addValue(messagesVPack, message);            // value
    }
  }
  return messagesVPack;
}
template<typename M>
void CombiningOutCache<M>::flushMessages() {
  if (this->_containedMessages == 0) {
    return;
  }

  uint64_t gss = this->_config->globalSuperstep();

  network::RequestOptions reqOpts;
  reqOpts.database = this->_config->database();
  reqOpts.timeout = network::Timeout(180);
  reqOpts.skipScheduler = true;
  auto connection = Connection::create(this->_baseUrl, std::move(reqOpts),
                                       *this->_config->vocbase());

  std::vector<futures::Future<Result>> responses;
  for (auto const& [shard, vertexMessageMap] : _shardMap) {
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    responses.emplace_back(connection.send(
        Destination{Destination::Type::shard,
                    this->_config->globalShardIDs()[shard]},
        ModernMessage{.executionNumber = this->_config->executionNumber(),
                      .payload = PregelMessage{
                          .senderId = ServerState::instance()->getId(),
                          .gss = gss,
                          .shard = shard,
                          .messages = messagesToVPack(vertexMessageMap)}}));

    if (this->_sendToNextGSS) {
      this->_sendCountNextGSS += vertexMessageMap.size();
    } else {
      this->_sendCount += vertexMessageMap.size();
    }
  }

  futures::collectAll(responses).wait();

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

using namespace arangodb::pregel::algos::accumulators;
template class arangodb::pregel::OutCache<MessageData>;
template class arangodb::pregel::ArrayOutCache<MessageData>;
template class arangodb::pregel::CombiningOutCache<MessageData>;
