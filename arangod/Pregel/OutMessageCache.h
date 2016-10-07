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

/* In the longer run, maybe write optimized implementations for certain use cases. For example threaded
 processing */
class OutMessageCache {
public:
  OutMessageCache(CollectionID vertexCollection);
  ~OutMessageCache();
  
  void addMessage(std::string key, VPackSlice slice);
  //void addMessages(VPackArrayIterator messages);
  void getMessages(ShardID const& shardId, VPackBuilder &outBuilder);
  void clean();
  
private:
  // two stage map: shard -> vertice -> message
  CollectionID _vertexCollection;
  std::unordered_map<std::string, std::unordered_map<std::string, VPackBuilder*>> _map;
  ClusterInfo *_ci;
};

}}
#endif
