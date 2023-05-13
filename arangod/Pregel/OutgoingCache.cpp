////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/voc-errors.h"
#include "Inspection/VPackWithErrorT.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/SenderMessage.h"

#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDMessage.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/EffectiveCloseness/HLLCounter.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

template<typename M>
OutCache<M>::OutCache(std::shared_ptr<WorkerConfig const> state,
                      MessageFormat<M> const* format)
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
    this->_localCache->storeMessageNoLock(shard, key, data);
    this->_sendCount++;
  } else {
    _shardMap[shard][std::string(key)].push_back(data);
    if (++(this->_containedMessages) >= this->_batchSize) {
      flushMessages();
    }
  }
}
template<typename M>
auto ArrayOutCache<M>::messagesToVPack(
    containers::NodeHashMap<std::string, std::vector<M>> const&
        messagesForVertices) -> std::tuple<size_t, VPackBuilder> {
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

  // LOG_TOPIC("7af7f", INFO, Logger::PREGEL) << "Beginning to send messages to
  // other machines";
  uint64_t gss = this->_config->globalSuperstep();
  auto& server = this->_config->vocbase()->server();
  auto const& nf = server.template getFeature<arangodb::NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  network::RequestOptions reqOpts;
  reqOpts.database = this->_config->database();
  reqOpts.skipScheduler = true;

  std::vector<futures::Future<network::Response>> responses;
  for (auto const& [shard, vertexMessageMap] : _shardMap) {
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    auto [shardMessageCount, messages] = messagesToVPack(vertexMessageMap);
    auto pregelMessage = worker::message::PregelMessage{
        .executionNumber = this->_config->executionNumber(),
        .gss = gss,
        .shard = shard,
        .messages = messages};
    auto serialized = inspection::serializeWithErrorT(pregelMessage);
    if (!serialized.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "Cannot serialize PregelMessage");
    }
    VPackBuffer<uint8_t> buffer;
    buffer.append(serialized.get().slice().begin(),
                  serialized.get().slice().byteSize());
    responses.emplace_back(network::sendRequest(
        pool, "shard:" + this->_config->globalShardID(shard),
        fuerte::RestVerb::Post, this->_baseUrl + Utils::messagesPath,
        std::move(buffer), reqOpts));

    this->_sendCount += shardMessageCount;
  }

  futures::collectAll(responses).wait();

  this->_removeContainedMessages();
}

// ================= CombiningOutCache ==================

template<typename M>
CombiningOutCache<M>::CombiningOutCache(
    std::shared_ptr<WorkerConfig const> state, MessageFormat<M> const* format,
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
    this->_localCache->storeMessageNoLock(shard, key, data);
    this->_sendCount++;
  } else {
    auto& vertexMap = _shardMap[shard];
    auto it = vertexMap.find(key);
    if (it != vertexMap.end()) {  // more than one message
      auto& ref = (*it).second;   // will be modified by combine(...)
      _combiner->combine(ref, data);
    } else {  // first message for this vertex
      vertexMap.try_emplace(key, data);

      if (++(this->_containedMessages) >= this->_batchSize) {
        flushMessages();
      }
    }
  }
}

template<typename M>
auto CombiningOutCache<M>::messagesToVPack(
    containers::NodeHashMap<std::string_view, M> const& messagesForVertices)
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

  auto& server = this->_config->vocbase()->server();
  auto const& nf = server.template getFeature<arangodb::NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  network::RequestOptions reqOpts;
  reqOpts.database = this->_config->database();
  reqOpts.timeout = network::Timeout(180);
  reqOpts.skipScheduler = true;

  std::vector<futures::Future<network::Response>> responses;
  for (auto const& [shard, vertexMessageMap] : _shardMap) {
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    auto pregelMessage = worker::message::PregelMessage{
        .executionNumber = this->_config->executionNumber(),
        .gss = gss,
        .shard = shard,
        .messages = messagesToVPack(vertexMessageMap)};
    auto serialized = inspection::serializeWithErrorT(pregelMessage);
    if (!serialized.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "Cannot serialize PregelMessage");
    }
    VPackBuffer<uint8_t> buffer;
    buffer.append(serialized.get().slice().begin(),
                  serialized.get().slice().byteSize());
    responses.emplace_back(network::sendRequest(
        pool, "shard:" + this->_config->globalShardID(shard),
        fuerte::RestVerb::Post, this->_baseUrl + Utils::messagesPath,
        std::move(buffer), reqOpts));

    this->_sendCount += vertexMessageMap.size();
  }

  futures::collectAll(responses).wait();

  _removeContainedMessages();
}

// ================= ArrayOutActorCache ==================

template<typename M>
void ArrayOutActorCache<M>::_removeContainedMessages() {
  for (auto& pair : _shardMap) {
    pair.second.clear();
  }
  this->_containedMessages = 0;
}

template<typename M>
void ArrayOutActorCache<M>::appendMessage(PregelShard shard,
                                          std::string_view const& key,
                                          M const& data) {
  _sendCountPerActor[_responsibleActorPerShard
                         [this->_config->globalShardIDs()[shard.value]]]++;
  if (this->_config->isLocalVertexShard(shard)) {
    this->_localCache->storeMessageNoLock(shard, key, data);
    this->_sendCount++;
  } else {
    _shardMap[shard][std::string(key)].push_back(data);
    if (++(this->_containedMessages) >= this->_batchSize) {
      flushMessages();
    }
  }
}
template<typename M>
auto ArrayOutActorCache<M>::messagesToVPack(
    containers::NodeHashMap<std::string, std::vector<M>> const&
        messagesForVertices) -> std::tuple<size_t, VPackBuilder> {
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
void ArrayOutActorCache<M>::flushMessages() {
  if (this->_containedMessages == 0) {
    return;
  }

  uint64_t gss = this->_config->globalSuperstep();

  for (auto const& [shard, vertexMessageMap] : _shardMap) {
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    auto [shardMessageCount, messages] = messagesToVPack(vertexMessageMap);
    auto pregelMessage = worker::message::PregelMessage{
        .executionNumber = this->_config->executionNumber(),
        .gss = gss,
        .shard = shard,
        .messages = messages};
    auto actor =
        _responsibleActorPerShard[this->_config->globalShardIDs()[shard.value]];
    _dispatch(actor, pregelMessage);

    this->_sendCount += shardMessageCount;
  }
  this->_removeContainedMessages();
}

// ================= CombiningOutActorCache ==================

template<typename M>
CombiningOutActorCache<M>::~CombiningOutActorCache() = default;

template<typename M>
void CombiningOutActorCache<M>::_removeContainedMessages() {
  for (auto& pair : _shardMap) {
    pair.second.clear();
  }
  this->_containedMessages = 0;
}

template<typename M>
void CombiningOutActorCache<M>::appendMessage(PregelShard shard,
                                              std::string_view const& key,
                                              M const& data) {
  if (this->_config->isLocalVertexShard(shard)) {
    _sendCountPerActor[_responsibleActorPerShard
                           [this->_config->globalShardIDs()[shard.value]]]++;
    this->_localCache->storeMessageNoLock(shard, key, data);
    this->_sendCount++;
  } else {
    auto& vertexMap = _shardMap[shard];
    auto it = vertexMap.find(key);
    if (it != vertexMap.end()) {  // more than one message
      auto& ref = (*it).second;   // will be modified by combine(...)
      _combiner->combine(ref, data);
    } else {  // first message for this vertex
      _sendCountPerActor[_responsibleActorPerShard
                             [this->_config->globalShardIDs()[shard.value]]]++;
      vertexMap.try_emplace(key, data);

      if (++(this->_containedMessages) >= this->_batchSize) {
        flushMessages();
      }
    }
  }
}

template<typename M>
auto CombiningOutActorCache<M>::messagesToVPack(
    containers::NodeHashMap<std::string_view, M> const& messagesForVertices)
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
void CombiningOutActorCache<M>::flushMessages() {
  if (this->_containedMessages == 0) {
    return;
  }

  uint64_t gss = this->_config->globalSuperstep();

  for (auto const& [shard, vertexMessageMap] : _shardMap) {
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    auto pregelMessage = worker::message::PregelMessage{
        .executionNumber = this->_config->executionNumber(),
        .gss = gss,
        .shard = shard,
        .messages = messagesToVPack(vertexMessageMap)};
    auto actor =
        _responsibleActorPerShard[this->_config->globalShardIDs()[shard.value]];
    _dispatch(actor, pregelMessage);

    this->_sendCount += vertexMessageMap.size();
  }
  this->_removeContainedMessages();
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
template class arangodb::pregel::ArrayOutActorCache<int64_t>;
template class arangodb::pregel::ArrayOutActorCache<uint64_t>;
template class arangodb::pregel::ArrayOutActorCache<float>;
template class arangodb::pregel::ArrayOutActorCache<double>;
template class arangodb::pregel::CombiningOutActorCache<int64_t>;
template class arangodb::pregel::CombiningOutActorCache<uint64_t>;
template class arangodb::pregel::CombiningOutActorCache<float>;
template class arangodb::pregel::CombiningOutActorCache<double>;

// algo specific
template class arangodb::pregel::OutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::ArrayOutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::CombiningOutCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::ArrayOutActorCache<SenderMessage<uint64_t>>;
template class arangodb::pregel::CombiningOutActorCache<
    SenderMessage<uint64_t>>;

template class arangodb::pregel::OutCache<SenderMessage<double>>;
template class arangodb::pregel::ArrayOutCache<SenderMessage<double>>;
template class arangodb::pregel::CombiningOutCache<SenderMessage<double>>;
template class arangodb::pregel::ArrayOutActorCache<SenderMessage<double>>;
template class arangodb::pregel::CombiningOutActorCache<SenderMessage<double>>;

template class arangodb::pregel::OutCache<DMIDMessage>;
template class arangodb::pregel::ArrayOutCache<DMIDMessage>;
template class arangodb::pregel::CombiningOutCache<DMIDMessage>;
template class arangodb::pregel::ArrayOutActorCache<DMIDMessage>;
template class arangodb::pregel::CombiningOutActorCache<DMIDMessage>;

template class arangodb::pregel::OutCache<HLLCounter>;
template class arangodb::pregel::ArrayOutCache<HLLCounter>;
template class arangodb::pregel::CombiningOutCache<HLLCounter>;
template class arangodb::pregel::ArrayOutActorCache<HLLCounter>;
template class arangodb::pregel::CombiningOutActorCache<HLLCounter>;

template class arangodb::pregel::OutCache<ColorPropagationMessageValue>;
template class arangodb::pregel::ArrayOutCache<ColorPropagationMessageValue>;
template class arangodb::pregel::CombiningOutCache<
    ColorPropagationMessageValue>;
template class arangodb::pregel::ArrayOutActorCache<
    ColorPropagationMessageValue>;
template class arangodb::pregel::CombiningOutActorCache<
    ColorPropagationMessageValue>;
