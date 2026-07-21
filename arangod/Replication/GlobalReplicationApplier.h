////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication/ReplicationApplier.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class StorageEngine;

/// @brief server-global replication applier for all databases
class GlobalReplicationApplier final : public ReplicationApplier {
  friend class GlobalTailingSyncer;

 public:
  explicit GlobalReplicationApplier(
      application_features::ApplicationServer& server, StorageEngine& engine);

  ~GlobalReplicationApplier();

  /// @brief whether or not the applier is the global one
  bool isGlobal() const override { return true; }

  /// @brief execute the check condition
  bool applies() const override { return true; }

  /// @brief load a persisted configuration for the applier
  static ReplicationApplierConfiguration loadConfiguration(
      application_features::ApplicationServer& server, StorageEngine& engine);

 protected:
  std::string getStateFilename() const override;

 private:
  StorageEngine& _engine;
};

}  // namespace arangodb
