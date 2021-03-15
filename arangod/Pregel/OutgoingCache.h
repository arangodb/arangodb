////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_OUT_MESSAGE_CACHE_H
#define ARANGODB_OUT_MESSAGE_CACHE_H 1

#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/voc-types.h"

#include "Pregel/GraphStore.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/WorkerConfig.h"

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

/// None of the current implementations use locking
/// Therefore only ever use this thread locally
/// We expect the local cache to be thread local too,
/// next GSS cache may be a global cache
template <typename M>
class OutCache {
 protected:
  WorkerConfig const* _config;
  MessageFormat<M> const* _format;
  InCache<M>* _localCache = nullptr;
  InCache<M>* _localCacheNextGSS = nullptr;
  std::string _baseUrl;
  uint32_t _batchSize = 1000;
  bool _sendToNextGSS = false;

  /// @brief current number of vertices stored
  size_t _containedMessages = 0;
  size_t _sendCount = 0;
  size_t _sendCountNextGSS = 0;
  virtual void _removeContainedMessages() = 0;

 public:
  OutCache(WorkerConfig* state, MessageFormat<M> const* format);
  virtual ~OutCache() = default;

  size_t sendCount() const { return _sendCount; }
  size_t sendCountNextGSS() const { return _sendCountNextGSS; }
  uint32_t batchSize() const { return _batchSize; }
  void setBatchSize(uint32_t bs) { _batchSize = bs; }
  inline void setLocalCache(InCache<M>* cache) { _localCache = cache; }
  inline void setLocalCacheNextGSS(InCache<M>* cache) {
    _localCacheNextGSS = cache;
  }

  void sendToNextGSS(bool np) {
    if (np != _sendToNextGSS) {
      flushMessages();
      _sendToNextGSS = np;
    }
  }

  void clear() {
    _sendCount = 0;
    _sendCountNextGSS = 0;
    _removeContainedMessages();
  }
  virtual void appendMessage(PregelShard shard, velocypack::StringRef const& key, M const& data) = 0;
  virtual void flushMessages() = 0;
};

template <typename M>
class ArrayOutCache : public OutCache<M> {
  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<PregelShard, std::unordered_map<std::string, std::vector<M>>> _shardMap;

  void _removeContainedMessages() override;

 public:
  ArrayOutCache(WorkerConfig* state, MessageFormat<M> const* format)
      : OutCache<M>(state, format) {}
  ~ArrayOutCache();

  void appendMessage(PregelShard shard, velocypack::StringRef const& key, M const& data) override;
  void flushMessages() override;
};

template <typename M>
class CombiningOutCache : public OutCache<M> {
  MessageCombiner<M> const* _combiner;

  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<PregelShard, std::unordered_map<velocypack::StringRef, M>> _shardMap;
  void _removeContainedMessages() override;

 public:
  CombiningOutCache(WorkerConfig* state, MessageFormat<M> const* format,
                    MessageCombiner<M> const* combiner);
  ~CombiningOutCache();

  void appendMessage(PregelShard shard, velocypack::StringRef const& key, M const& data) override;
  void flushMessages() override;
};
}  // namespace pregel
}  // namespace arangodb
#endif
