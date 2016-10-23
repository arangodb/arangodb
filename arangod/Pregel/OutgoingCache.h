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

#include <string>

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Cluster/ClusterInfo.h"


namespace arangodb {
namespace pregel {

  class WorkerContext;
  template<typename M>
  class Combiner;
  
    
/* In the longer run, maybe write optimized implementations for certain use cases. For example threaded
 processing */
  template<typename M>
class OutgoingCache {
    friend class WorkerJob;
public:
  OutgoingCache(std::shared_ptr<WorkerContext> context, Combiner<M> const& combiner);
  ~OutgoingCache();
  
  void sendMessageTo(std::string const& toValue, M const& data);
  void clear();
  size_t count() const {return _numVertices;}
    
protected:
  void sendMessages();
  
private:
  const Combiner<M> *_combiner;
  /// @brief two stage map: shard -> vertice -> message
  std::unordered_map<ShardID, std::unordered_map<std::string, M>> _map;
  ClusterInfo *_ci;
  std::shared_ptr<WorkerContext> _ctx;
  std::shared_ptr<LogicalCollection> _collInfo;
  std::string _baseUrl;
  /// @brief current number of vertices stored
  size_t _numVertices;
};

}}
#endif
