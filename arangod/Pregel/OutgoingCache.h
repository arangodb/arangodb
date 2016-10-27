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

#include <velocypack/velocypack-aliases.h>
#include <velocypack/vpack.h>
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Cluster/ClusterInfo.h"

#include "MessageFormat.h"
#include "MessageCombiner.h"

namespace arangodb {
namespace pregel {

template <typename V, typename E, typename M>
class WorkerContext;

/* In the longer run, maybe write optimized implementations for certain use
 cases. For example threaded
 processing */
template <typename V, typename E, typename M>
class OutgoingCache {
 public:
  OutgoingCache(std::shared_ptr<WorkerContext<V, E, M>> context);
  ~OutgoingCache();

  void sendMessageTo(std::string const& toValue, M const& data);
  void clear();
  size_t count() const { return _numVertices; }

  void sendMessages();

 private:
    std::unique_ptr<MessageFormat<M>> _format;
    std::unique_ptr<MessageCombiner<M>> _combiner;
    
  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<ShardID, std::unordered_map<std::string, M>> _map;
  std::shared_ptr<WorkerContext<V, E, M>> _ctx;
  std::shared_ptr<LogicalCollection> _collInfo;
  ClusterInfo* _ci;
  std::string _baseUrl;
  /// @brief current number of vertices stored
  size_t _numVertices;
};
}
}
#endif
