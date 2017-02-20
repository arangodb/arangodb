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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef CLUSTER_AGENCYCALLACKREGISTRY_H
#define CLUSTER_AGENCYCALLACKREGISTRY_H 1

#include "Cluster/AgencyCallback.h"
#include "Basics/ReadWriteLock.h"

namespace arangodb {

class AgencyCallbackRegistry {
public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief ctor
  //////////////////////////////////////////////////////////////////////////////
  explicit AgencyCallbackRegistry(std::string const&);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief dtor
  //////////////////////////////////////////////////////////////////////////////
  ~AgencyCallbackRegistry();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief register a callback 
  //////////////////////////////////////////////////////////////////////////////
  bool registerCallback(std::shared_ptr<AgencyCallback>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unregister a callback
  //////////////////////////////////////////////////////////////////////////////
  bool unregisterCallback(std::shared_ptr<AgencyCallback>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a callback by its key
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<AgencyCallback> getCallback(uint32_t);

private:
  std::string getEndpointUrl(uint32_t);

  AgencyComm _agency;

  arangodb::basics::ReadWriteLock _lock;

  std::string const _callbackBasePath;

  std::unordered_map<uint32_t, std::shared_ptr<AgencyCallback>> _endpoints;
};

}

#endif
