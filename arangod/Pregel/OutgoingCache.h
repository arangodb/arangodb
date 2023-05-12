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

#pragma once

#include "Actor/ActorPID.h"
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Containers/FlatHashSet.h"
#include "Pregel/Worker/Messages.h"
#include "VocBase/voc-types.h"

#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/Worker/WorkerConfig.h"

namespace arangodb {
namespace pregel {
/* In the longer run, maybe write optimized implementations for certain use
 cases. For example threaded
 processing */
template<typename M>
class InCache;

template<typename M>
class CombiningInCache;

template<typename M>
class ArrayInCache;

/// None of the current implementations use locking
/// Therefore only ever use this thread locally
/// We expect the local cache to be thread local too,
/// next GSS cache may be a global cache
template<typename M>
class OutCache {
 protected:
  std::shared_ptr<WorkerConfig const> _config;
  containers::FlatHashSet<PregelShard> _localShards;
  MessageFormat<M> const* _format;
  InCache<M>* _localCache = nullptr;
  InCache<M>* _localCacheNextGSS = nullptr;
  std::string _baseUrl;
  uint32_t _batchSize = 1000;

  /// @brief current number of vertices stored
  size_t _containedMessages = 0;
  size_t _sendCount = 0;
  virtual void _removeContainedMessages() = 0;
  virtual auto _clearSendCountPerActor() -> void{};

  bool isLocalShard(PregelShard pregelShard) {
    return _localShards.contains(pregelShard);
  }

 public:
  OutCache(std::shared_ptr<WorkerConfig const> state,
           containers::FlatHashSet<PregelShard> localShards,
           MessageFormat<M> const* format);
  virtual ~OutCache() = default;

  size_t sendCount() const { return _sendCount; }
  uint32_t batchSize() const { return _batchSize; }
  void setBatchSize(uint32_t bs) { _batchSize = bs; }
  inline void setLocalCache(InCache<M>* cache) { _localCache = cache; }

  void clear() {
    _sendCount = 0;
    _removeContainedMessages();
    _clearSendCountPerActor();
  }

  virtual void setDispatch(
      std::function<void(actor::ActorPID receiver,
                         worker::message::PregelMessage message)>
          dispatch){};
  virtual void setResponsibleActorPerShard(
      std::unordered_map<ShardID, actor::ActorPID> _responsibleActorPerShard){};
  virtual void appendMessage(PregelShard shard, std::string_view const& key,
                             M const& data) = 0;
  virtual void flushMessages() = 0;
  virtual auto sendCountPerActor() const
      -> std::unordered_map<actor::ActorPID, uint64_t> {
    return {};
  }
};

template<typename M>
class ArrayOutCache : public OutCache<M> {
  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<PregelShard,
                     std::unordered_map<std::string, std::vector<M>>>
      _shardMap;
  std::unordered_map<ShardID, uint64_t> _sendCountPerShard;

  void _removeContainedMessages() override;
  auto messagesToVPack(std::unordered_map<std::string, std::vector<M>> const&
                           messagesForVertices)
      -> std::tuple<size_t, VPackBuilder>;

 public:
  ArrayOutCache(std::shared_ptr<WorkerConfig const> state,
                containers::FlatHashSet<PregelShard> localShards,
                MessageFormat<M> const* format)
      : OutCache<M>(std::move(state), std::move(localShards), format) {}
  ~ArrayOutCache() = default;

  void appendMessage(PregelShard shard, std::string_view const& key,
                     M const& data) override;
  void flushMessages() override;
};

template<typename M>
class CombiningOutCache : public OutCache<M> {
  MessageCombiner<M> const* _combiner;

  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<PregelShard, std::unordered_map<std::string_view, M>>
      _shardMap;
  std::unordered_map<ShardID, uint64_t> _sendCountPerShard;

  void _removeContainedMessages() override;
  auto messagesToVPack(
      std::unordered_map<std::string_view, M> const& messagesForVertices)
      -> VPackBuilder;

 public:
  CombiningOutCache(std::shared_ptr<WorkerConfig const> state,
                    containers::FlatHashSet<PregelShard> localShards,
                    MessageFormat<M> const* format,
                    MessageCombiner<M> const* combiner)
      : OutCache<M>(std::move(state), std::move(localShards), format),
        _combiner(combiner) {}

  ~CombiningOutCache() = default;

  void appendMessage(PregelShard shard, std::string_view const& key,
                     M const& data) override;
  void flushMessages() override;
};

template<typename M>
class ArrayOutActorCache : public OutCache<M> {
  std::unordered_map<ShardID, actor::ActorPID> _responsibleActorPerShard;
  std::function<void(actor::ActorPID receiver,
                     worker::message::PregelMessage message)>
      _dispatch;

  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<PregelShard,
                     std::unordered_map<std::string, std::vector<M>>>
      _shardMap;
  std::unordered_map<actor::ActorPID, uint64_t> _sendCountPerActor;

  void _removeContainedMessages() override;

  auto _clearSendCountPerActor() -> void override {
    _sendCountPerActor.clear();
  }
  auto messagesToVPack(std::unordered_map<std::string, std::vector<M>> const&
                           messagesForVertices)
      -> std::tuple<size_t, VPackBuilder>;

 public:
  ArrayOutActorCache(std::shared_ptr<WorkerConfig const> state,
                     containers::FlatHashSet<PregelShard> localShards,
                     MessageFormat<M> const* format)
      : OutCache<M>{std::move(state), std::move(localShards), format} {};
  ~ArrayOutActorCache() = default;

  void setDispatch(std::function<void(actor::ActorPID receiver,
                                      worker::message::PregelMessage message)>
                       dispatch) override {
    _dispatch = std::move(dispatch);
  }
  void setResponsibleActorPerShard(std::unordered_map<ShardID, actor::ActorPID>
                                       responsibleActorPerShard) override {
    _responsibleActorPerShard = std::move(responsibleActorPerShard);
  }
  void appendMessage(PregelShard shard, std::string_view const& key,
                     M const& data) override;
  void flushMessages() override;
  auto sendCountPerActor() const
      -> std::unordered_map<actor::ActorPID, uint64_t> override {
    return _sendCountPerActor;
  }
};

template<typename M>
class CombiningOutActorCache : public OutCache<M> {
  MessageCombiner<M> const* _combiner;
  std::unordered_map<ShardID, actor::ActorPID> _responsibleActorPerShard;
  std::function<void(actor::ActorPID receiver,
                     worker::message::PregelMessage message)>
      _dispatch;

  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<PregelShard, std::unordered_map<std::string_view, M>>
      _shardMap;
  std::unordered_map<actor::ActorPID, uint64_t> _sendCountPerActor;

  void _removeContainedMessages() override;

  auto _clearSendCountPerActor() -> void override {
    _sendCountPerActor.clear();
  }
  auto messagesToVPack(
      std::unordered_map<std::string_view, M> const& messagesForVertices)
      -> VPackBuilder;

 public:
  CombiningOutActorCache(std::shared_ptr<WorkerConfig const> state,
                         containers::FlatHashSet<PregelShard> localShards,
                         MessageFormat<M> const* format,
                         MessageCombiner<M> const* combiner)
      : OutCache<M>{state, std::move(localShards), format},
        _combiner(combiner){};
  ~CombiningOutActorCache();

  void setDispatch(std::function<void(actor::ActorPID receiver,
                                      worker::message::PregelMessage message)>
                       dispatch) override {
    _dispatch = std::move(dispatch);
  }
  void setResponsibleActorPerShard(std::unordered_map<ShardID, actor::ActorPID>
                                       responsibleActorPerShard) override {
    _responsibleActorPerShard = std::move(responsibleActorPerShard);
  }
  void appendMessage(PregelShard shard, std::string_view const& key,
                     M const& data) override;
  void flushMessages() override;
  auto sendCountPerActor() const
      -> std::unordered_map<actor::ActorPID, uint64_t> override {
    return _sendCountPerActor;
  }
};

}  // namespace pregel
}  // namespace arangodb
