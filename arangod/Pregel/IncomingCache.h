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

#ifndef ARANGODB_IN_MESSAGE_CACHE_H
#define ARANGODB_IN_MESSAGE_CACHE_H 1

#include <velocypack/Slice.h>

#include <atomic>
#include <string>
#include <map>

#include "Basics/Common.h"

#include "Pregel/GraphStore.h"
#include "Pregel/Iterators.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"

namespace arangodb {
namespace pregel {

class WorkerConfig;

/* In the longer run, maybe write optimized implementations for certain use
cases. For example threaded
processing */
template <typename M>
class InCache {
 protected:
  mutable std::map<PregelShard, std::mutex> _bucketLocker;
  std::atomic<uint64_t> _containedMessageCount;
  MessageFormat<M> const* _format;

  /// Initialize format and mutex map.
  /// @param config can be null if you don't want locks
  explicit InCache(MessageFormat<M> const* format);
  virtual void _set(PregelShard shard, velocypack::StringRef const& vertexId, M const& data) = 0;

 public:
  virtual ~InCache() = default;

  MessageFormat<M> const* format() const { return _format; }
  uint64_t containedMessageCount() const { return _containedMessageCount; }

  void parseMessages(VPackSlice const& messages);

  /// @brief Store a single message.
  /// Only ever call when you are sure this is a thread local store
  void storeMessageNoLock(PregelShard shard, velocypack::StringRef const& vertexId, M const& data);
  /// @brief  Store a single message
  void storeMessage(PregelShard shard, velocypack::StringRef const& vertexId, M const& data);

  virtual void mergeCache(WorkerConfig const& config, InCache<M> const* otherCache) = 0;
  /// @brief get messages for vertex id. (Don't use keys from _from or _to
  /// directly, they contain the collection name)
  virtual MessageIterator<M> getMessages(PregelShard shard, velocypack::StringRef const& key) = 0;
  /// clear cache
  virtual void clear() = 0;

  /// Deletes one entry. DOES NOT LOCK
  virtual void erase(PregelShard shard, velocypack::StringRef const& key) = 0;

  /// Calls function for each entry. DOES NOT LOCK
  virtual void forEach(std::function<void(PregelShard, velocypack::StringRef const&, M const&)> func) = 0;
};

/// Cache version which stores a std::vector<M> for each pregel id
/// containing all messages for this vertex
template <typename M>
class ArrayInCache : public InCache<M> {
  typedef std::unordered_map<std::string, std::vector<M>> HMap;
  std::map<PregelShard, HMap> _shardMap;

 protected:
  void _set(PregelShard shard, velocypack::StringRef const& vertexId, M const& data) override;

 public:
  ArrayInCache(WorkerConfig const* config, MessageFormat<M> const* format);

  void mergeCache(WorkerConfig const& config, InCache<M> const* otherCache) override;
  MessageIterator<M> getMessages(PregelShard shard, velocypack::StringRef const& key) override;
  void clear() override;
  void erase(PregelShard shard, velocypack::StringRef const& key) override;
  void forEach(std::function<void(PregelShard shard, velocypack::StringRef const& key, M const& val)> func) override;
};

/// Cache which stores one value per vertex id
template <typename M>
class CombiningInCache : public InCache<M> {
  typedef std::unordered_map<std::string, M> HMap;

  MessageCombiner<M> const* _combiner;
  std::map<PregelShard, HMap> _shardMap;

 protected:
  void _set(PregelShard shard, velocypack::StringRef const& vertexId,
            M const& data) override;

 public:
  CombiningInCache(WorkerConfig const* config, MessageFormat<M> const* format,
                   MessageCombiner<M> const* combiner);

  MessageCombiner<M> const* combiner() const { return _combiner; }

  void mergeCache(WorkerConfig const& config, InCache<M> const* otherCache) override;
  MessageIterator<M> getMessages(PregelShard shard, velocypack::StringRef const& key) override;
  void clear() override;
  void erase(PregelShard shard, velocypack::StringRef const& key) override;
  void forEach(std::function<void(PregelShard, velocypack::StringRef const&, M const&)> func) override;
};
}  // namespace pregel
}  // namespace arangodb
#endif
