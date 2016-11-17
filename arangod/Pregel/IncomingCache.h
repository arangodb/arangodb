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

#include "MessageCombiner.h"
#include "MessageFormat.h"
#include "MessageIterator.h"

namespace arangodb {
namespace pregel {

/* In the longer run, maybe write optimized implementations for certain use
cases. For example threaded
processing */
template <typename M>
class IncomingCache {
 public:
  IncomingCache(MessageFormat<M> const* format, MessageCombiner<M> const* combiner)
      : _format(format), _combiner(combiner), _receivedMessageCount(0) {}
  ~IncomingCache();

  void parseMessages(VPackSlice messages);
  /// @brief get messages for vertex id. (Don't use keys from _from or _to
  /// directly, they contain the collection name)
  MessageIterator<M> getMessages(std::string const& vertexId);
  void clear();

  /// @brief internal method to direclty set the messages for a vertex. Only
  /// valid with already combined messages
  void setDirect(std::string const& vertexId, M const& data);
  void mergeCache(IncomingCache<M> const& otherCache);

  size_t receivedMessageCount() { return _receivedMessageCount; }

 private:
  mutable Mutex _writeLock;
  std::unordered_map<std::string, M> _messages;
  size_t _receivedMessageCount = 0;

  MessageFormat<M> const* _format;
  MessageCombiner<M> const* _combiner;
};
}
}
#endif
