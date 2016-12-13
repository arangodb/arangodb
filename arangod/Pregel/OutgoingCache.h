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

#ifndef ARANGODB_OUT_MESSAGE_CACHE_H
#define ARANGODB_OUT_MESSAGE_CACHE_H 1

#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/voc-types.h"

#include "Pregel/GraphStore.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/WorkerState.h"

namespace arangodb {
namespace pregel {
/* In the longer run, maybe write optimized implementations for certain use
 cases. For example threaded
 processing */
template <typename M>
class InCache;

template <typename M>
class CombiningInCache;

template <typename M>
class ArrayInCache;

template <typename M>
class OutCache {
 protected:
  WorkerState const* _state;
  MessageFormat<M> const* _format;
  InCache<M>* _localCache;
  std::string _baseUrl;
  uint32_t _batchSize = 1000;

  /// @brief current number of vertices stored
  size_t _containedMessages = 0;
  size_t _sendMessages = 0;
  bool shouldFlushCache();

 public:
  OutCache(WorkerState* state, InCache<M>* cache);
  virtual ~OutCache(){};

  size_t sendMessageCount() const { return _sendMessages; }
  uint32_t batchSize() const { return _batchSize; }
  void setBatchSize(uint32_t bs) { _batchSize = bs; }

  virtual void clear() = 0;
  virtual void appendMessage(prgl_shard_t shard, std::string const& key,
                             M const& data) = 0;
  virtual void flushMessages() = 0;
};

template <typename M>
class ArrayOutCache : public OutCache<M> {
  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<prgl_shard_t,
                     std::unordered_map<std::string, std::vector<M>>>
      _shardMap;

 public:
  ArrayOutCache(WorkerState* state, InCache<M>* cache)
      : OutCache<M>(state, cache) {}
  ~ArrayOutCache();

  void clear() override;
  void appendMessage(prgl_shard_t shard, std::string const& key,
                     M const& data) override;
  void flushMessages() override;
};

template <typename M>
class CombiningOutCache : public OutCache<M> {
  MessageCombiner<M> const* _combiner;

  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<prgl_shard_t, std::unordered_map<std::string, M>>
      _shardMap;

 public:
  CombiningOutCache(WorkerState* state, CombiningInCache<M>* cache);
  ~CombiningOutCache();

  void clear() override;
  void appendMessage(prgl_shard_t shard, std::string const& key,
                     M const& data) override;
  void flushMessages() override;
};
}
}
#endif
