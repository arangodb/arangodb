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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_REPLICATION_APPLIER_H
#define ARANGOD_REPLICATION_REPLICATION_APPLIER_H 1

#include "Basics/Common.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationApplierState.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

/// @brief replication applier interface
class ReplicationApplier {
 public:
  explicit ReplicationApplier(ReplicationApplierConfiguration const& configuration) 
      : _configuration(configuration) {}

  virtual ~ReplicationApplier();
  
  ReplicationApplier(ReplicationApplier const&) = delete;
  ReplicationApplier& operator=(ReplicationApplier const&) = delete;

  /// @brief load the applier state from persistent storage
  virtual void loadState() = 0;
  
  /// @brief store the applier state in persistent storage
  virtual void persistState() = 0;
 
  /// @brief store the current applier state in the passed vpack builder 
  virtual void toVelocyPack(arangodb::velocypack::Builder& result) const = 0;

  /// @brief start the applier
  virtual void start() = 0;
  
  /// @brief stop the applier
  virtual void stop() = 0;

 protected:
  ReplicationApplierConfiguration _configuration;

  std::unique_ptr<ReplicationApplierState> _state;
};

}

#endif
