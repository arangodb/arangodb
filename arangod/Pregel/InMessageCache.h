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

#include <string>
#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Cluster/ClusterInfo.h"


namespace arangodb {
  //class Mutex;
  
namespace pregel {

/* In the longer run, maybe write optimized implementations for certain use cases. For example threaded
 processing */
class InMessageCache {
public:
  InMessageCache() {}
  ~InMessageCache();
  
  void addMessages(VPackArrayIterator messages);
  arangodb::velocypack::ArrayIterator getMessages(ShardID const& shardId);
  void clean();
  
private:
  std::unordered_map<std::string, VPackBuilder*> _messages;
  Mutex writeMutex;
};

}}
#endif
