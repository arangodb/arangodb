////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_GLOBAL_INITIAL_SYNCER_H
#define ARANGOD_REPLICATION_GLOBAL_INITIAL_SYNCER_H 1

#include "Replication/InitialSyncer.h"

namespace arangodb {

/// Meta Syncer driving multiple initial syncer
class GlobalInitialSyncer final : public InitialSyncer {
 public:
  explicit GlobalInitialSyncer(ReplicationApplierConfiguration const&);

  ~GlobalInitialSyncer();

 public:
  /// @brief run method, performs a full synchronization
  /// public method, catches exceptions
  arangodb::Result run(bool incremental) override;

  /// @brief fetch the server's inventory, public method
  Result getInventory(arangodb::velocypack::Builder& builder);

 private:
 private:
  /// @brief run method, performs a full synchronization
  /// internal method, may throw exceptions
  arangodb::Result runInternal(bool incremental);

  /// @brief check whether the initial synchronization should be aborted
  bool checkAborted();

  /// @brief fetch the server's inventory
  Result fetchInventory(arangodb::velocypack::Builder& builder);

  /// @brief add or remove databases such that the local inventory mirrors the
  /// masters
  Result updateServerInventory(velocypack::Slice const& masterDatabases);
};
}  // namespace arangodb

#endif
