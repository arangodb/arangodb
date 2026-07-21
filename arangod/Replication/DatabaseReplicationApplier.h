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
#include "VocBase/voc-types.h"

namespace arangodb {

struct Database;
class StorageEngine;

/// @brief replication applier for a single database
class DatabaseReplicationApplier final : public ReplicationApplier {
  friend class DatabaseTailingSyncer;
  friend class RestReplicationHandler;

 public:
  explicit DatabaseReplicationApplier(Database& vocbase);

  DatabaseReplicationApplier(
      ReplicationApplierConfiguration const& configuration, Database& vocbase);

  ~DatabaseReplicationApplier();

  /// @brief whether or not the applier is the global one
  bool isGlobal() const override { return false; }

  /// @brief execute the check condition
  bool applies() const override;

  /// @brief factory function for creating a database-specific replication
  /// applier
  static DatabaseReplicationApplier* create(Database& vocbase);

  /// @brief load a persisted configuration for the applier
  static ReplicationApplierConfiguration loadConfiguration(Database& vocbase);

 protected:
  std::string getStateFilename() const override;

 private:
  Database& _vocbase;
};

}  // namespace arangodb
