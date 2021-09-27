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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Cluster/ServerState.h>
#include <VocBase/VocbaseInfo.h>
#include <velocypack/Slice.h>
#include "Basics/Result.h"
#include "CollectionCreationInfo.h"

using namespace arangodb;

class CollectionValidator {
 public:
  CollectionValidator(CollectionCreationInfo const& info, ServerState* const serverState,
                      TRI_vocbase_t const& vocbase, bool isSingleServerSmartGraph,
                      bool enforceReplicationFactor, bool isLocalCollection, bool isSystemName)
      : _info(info),
        _serverState(serverState),
        _vocbase(vocbase),
        _isSingleServerSmartGraph(isSingleServerSmartGraph),
        _enforceReplicationFactor(enforceReplicationFactor),
        _isLocalCollection(isLocalCollection),
        _isSystemName(isSystemName) {}

  Result validateCreationInfo();

 private:
  CollectionCreationInfo const& _info;
  ServerState* _serverState;
  TRI_vocbase_t const& _vocbase;
  bool _isSingleServerSmartGraph;
  bool _enforceReplicationFactor;
  bool _isLocalCollection;
  bool _isSystemName;
};

