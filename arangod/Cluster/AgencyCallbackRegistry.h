////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef CLUSTER_AGENCY_CALLBACK_REGISTRY_H
#define CLUSTER_AGENCY_CALLBACK_REGISTRY_H 1

#include "Basics/ReadWriteLock.h"
#include "Cluster/AgencyCallback.h"
#include "RestServer/Metrics.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class AgencyCallbackRegistry {
 public:
  explicit AgencyCallbackRegistry(application_features::ApplicationServer&,
                                  std::string const&);

  ~AgencyCallbackRegistry();

  /// @brief register a callback
  bool registerCallback(std::shared_ptr<AgencyCallback> cb, bool local = true);

  /// @brief unregister a callback
  bool unregisterCallback(std::shared_ptr<AgencyCallback> cb);

  /// @brief get a callback by its key
  std::shared_ptr<AgencyCallback> getCallback(uint64_t id);

 private:
  std::string getEndpointUrl(uint64_t id) const;

  AgencyComm _agency;

  arangodb::basics::ReadWriteLock _lock;

  std::string const _callbackBasePath;

  std::unordered_map<uint64_t, std::shared_ptr<AgencyCallback>> _callbacks;
  
  /// @brief current number of callbacks registered
  Gauge<uint64_t>& _callbacksCount;
};

}  // namespace arangodb

#endif
