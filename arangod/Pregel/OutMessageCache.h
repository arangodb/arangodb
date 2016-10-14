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

    class InMessageCache;
    class WorkerContext;
    
/* In the longer run, maybe write optimized implementations for certain use cases. For example threaded
 processing */
class OutMessageCache {
    friend class WorkerJob;
public:
  OutMessageCache(std::shared_ptr<WorkerContext> context);
  ~OutMessageCache();
  
  void sendMessageTo(std::string const& toValue, VPackSlice mData);
    
  void clear();
    
protected:
  void sendMessages();
  
private:
  // two stage map: shard -> vertice -> message
  std::unordered_map<ShardID, std::unordered_map<std::string, VPackBuilder*>> _map;
  ClusterInfo *_ci;
    std::shared_ptr<WorkerContext> _ctx;
  std::shared_ptr<LogicalCollection> _collInfo;
  std::string _baseUrl;
  
  size_t _numVertices;
};

}}
#endif
