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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "TailingSyncer.h"

#include <memory>

namespace arangodb {
class GlobalReplicationApplier;

class GlobalTailingSyncer : public TailingSyncer {
 private:
  // constructor is private, as GlobalTailingSyncer uses shared_from_this() and
  // we must ensure that it is only created via make_shared.
  GlobalTailingSyncer(ReplicationApplierConfiguration const&, 
                      TRI_voc_tick_t initialTick,
                      bool useTick);
  
 public:
  static std::shared_ptr<GlobalTailingSyncer> create(ReplicationApplierConfiguration const&, 
                                                     TRI_voc_tick_t initialTick,
                                                     bool useTick);

  /// @brief return the syncer's replication applier
  GlobalReplicationApplier* applier() const {
    return static_cast<GlobalReplicationApplier*>(_applier);
  }

 protected:
  /// @brief resolve to proper base url
  std::string tailingBaseUrl(std::string const& command) override;

  /// @brief save the current applier state
  Result saveApplierState() override;

  bool skipMarker(arangodb::velocypack::Slice slice) override;

 private:
  /// @brief translation between globallyUniqueId and collection name
  std::unordered_map<std::string, std::string> _translations;

  bool _queriedTranslations;
};

}  // namespace arangodb

