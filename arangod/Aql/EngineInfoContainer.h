////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ENGINE_INFO_CONTAINER_H
#define ARANGOD_AQL_ENGINE_INFO_CONTAINER_H 1

#include "Basics/Common.h"
#include "VocBase/AccessMode.h"

namespace arangodb {
namespace aql {

class ExecutionNode;

class EngineInfoContainer {
 private:
  struct EngineInfo {
   public:
    EngineInfo(size_t id, std::vector<ExecutionNode*>&& nodes,
               size_t idOfRemoteNode);
    ~EngineInfo();

   private:
    size_t const _id;
    std::vector<ExecutionNode*> _nodes;
    size_t _idOfRemoteNode;  // id of the remote node
  };

 public:
  EngineInfoContainer();

  ~EngineInfoContainer();

  // Add a new query part to be executed in this location
  // This will have the following effect:
  // * It creates a new EngineInfo from the given information
  // * It adds the collections to the map
  // This intentionally copies the vector of nodes, the caller reuses
  // the given vector.

  void addQuerySnippet(std::vector<ExecutionNode*> nodes, size_t idOfRemoteNode);

 private:
  // @brief List of EngineInfos to distribute accross the cluster
  std::vector<EngineInfo> _engines;

  // @brief Mapping of used collection names to lock type required
  std::unordered_map<std::string, AccessMode::Type> _collections;
};

}  // aql
}  // arangodb
#endif
