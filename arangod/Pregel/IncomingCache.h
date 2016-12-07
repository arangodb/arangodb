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

#ifndef ARANGODB_IN_MESSAGE_CACHE_H
#define ARANGODB_IN_MESSAGE_CACHE_H 1

#include <velocypack/velocypack-aliases.h>
#include <velocypack/vpack.h>
#include <string>

#include "Basics/Common.h"
#include "Basics/Mutex.h"

#include "Pregel/GraphStore.h"
#include "Pregel/Iterators.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"

namespace arangodb {
namespace pregel {

/* In the longer run, maybe write optimized implementations for certain use
cases. For example threaded
processing */
template <typename M>
class InCache {
 protected:
  mutable Mutex _writeLock;
  size_t _receivedMessageCount = 0;
  MessageFormat<M> const* _format;

  InCache(MessageFormat<M> const* format)
      : _receivedMessageCount(0), _format(format) {}

 public:
  virtual ~InCache() {};
  
  MessageFormat<M> const* format() const {return _format;}
  void parseMessages(VPackSlice messages);

  /// clear cache
  virtual void clear() = 0;
  /// @brief internal method to direclty set the messages for a vertex. Only
  /// valid with already combined messages
  virtual void setDirect(prgl_shard_t shard, std::string const& vertexId,
                         M const& data) = 0;
  virtual void mergeCache(InCache<M> const* otherCache) = 0;
  /// @brief get messages for vertex id. (Don't use keys from _from or _to
  /// directly, they contain the collection name)
  virtual MessageIterator<M> getMessages(prgl_shard_t shard, std::string const& key) = 0;

  size_t receivedMessageCount() const { return _receivedMessageCount; }
};

template <typename M>
class ArrayInCache : public InCache<M> {
  std::map<prgl_shard_t, std::unordered_map<std::string, std::vector<M>>> _shardMap;

 public:
  ArrayInCache(MessageFormat<M> const* format) : InCache<M>(format) {}

  void clear() override;
  void setDirect(prgl_shard_t shard, std::string const& vertexId,
                 M const& data) override;
  void mergeCache(InCache<M> const* otherCache) override;
  MessageIterator<M> getMessages(prgl_shard_t shard, std::string const& key) override;
};

template <typename M>
class CombiningInCache : public InCache<M> {
  MessageCombiner<M> const* _combiner;
  std::map<prgl_shard_t, std::unordered_map<std::string, M>> _shardMap;

 public:
  CombiningInCache(MessageFormat<M> const* format,
                   MessageCombiner<M> const* combiner)
  : InCache<M>(format), _combiner(combiner) {}
  
  MessageCombiner<M> const* combiner() const {return _combiner;}

  void clear() override;
  void setDirect(prgl_shard_t shard, std::string const& vertexId,
                 M const& data) override;
  void mergeCache(InCache<M> const* otherCache) override;
  MessageIterator<M> getMessages(prgl_shard_t shard, std::string const& key) override;
};
}
}
#endif
