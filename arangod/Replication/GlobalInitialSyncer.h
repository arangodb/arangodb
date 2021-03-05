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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_GLOBAL_INITIAL_SYNCER_H
#define ARANGOD_REPLICATION_GLOBAL_INITIAL_SYNCER_H 1

#include "Replication/InitialSyncer.h"

#include <memory>

namespace arangodb {

/// Meta Syncer driving multiple initial syncer
class GlobalInitialSyncer : public InitialSyncer {
 private:
  // constructor is private, as GlobalInitialSyncer uses shared_from_this() and
  // we must ensure that it is only created via make_shared.
  explicit GlobalInitialSyncer(ReplicationApplierConfiguration const&);
 
 public:
  static std::shared_ptr<GlobalInitialSyncer> create(ReplicationApplierConfiguration const&);

  ~GlobalInitialSyncer();

  /// @brief run method, performs a full synchronization
  /// public method, catches exceptions
  arangodb::Result run(bool incremental, char const* context = nullptr) override;

  /// @brief fetch the server's inventory, public method
  Result getInventory(arangodb::velocypack::Builder& builder);

 private:
  /// @brief run method, performs a full synchronization
  /// internal method, may throw exceptions
  arangodb::Result runInternal(bool incremental, char const* context);

  /// @brief check whether the initial synchronization should be aborted
  bool checkAborted();

  /// @brief fetch the server's inventory
  Result fetchInventory(arangodb::velocypack::Builder& builder);

  /// @brief add or remove databases such that the local inventory mirrors the
  /// leader's
  Result updateServerInventory(velocypack::Slice const& leaderDatabases);
};
}  // namespace arangodb

#endif
