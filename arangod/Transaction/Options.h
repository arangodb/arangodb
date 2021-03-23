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

#ifndef ARANGOD_TRANSACTION_OPTIONS_H
#define ARANGOD_TRANSACTION_OPTIONS_H 1

#include <cstdint>

#include "Basics/Common.h"
#include "Cluster/RebootTracker.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace transaction {

struct Options {
  Options();

  /// @brief returns default options used in tailing sync replication
  static Options replicationDefaults();

  /// @brief adjust the global default values for transactions
  static void setLimits(uint64_t maxTransactionSize, uint64_t intermediateCommitSize,
                        uint64_t intermediateCommitCount);

  /// @brief read the options from a vpack slice
  void fromVelocyPack(arangodb::velocypack::Slice const&);

  /// @brief add the options to an opened vpack builder
  void toVelocyPack(arangodb::velocypack::Builder&) const;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  /// @brief patch intermediateCommitCount for testing
  static void adjustIntermediateCommitCount(Options& options);
#endif

  static constexpr double defaultLockTimeout = 900.0;
  static uint64_t defaultMaxTransactionSize;
  static uint64_t defaultIntermediateCommitSize;
  static uint64_t defaultIntermediateCommitCount;

  /// @brief time (in seconds) that is spent waiting for a lock
  double lockTimeout;
  uint64_t maxTransactionSize;
  uint64_t intermediateCommitSize;
  uint64_t intermediateCommitCount;
  bool allowImplicitCollectionsForRead;
  bool allowImplicitCollectionsForWrite; // replication only!
#ifdef USE_ENTERPRISE
  bool skipInaccessibleCollections;
#endif
  bool waitForSync;
  bool isFollowerTransaction;

  /// @brief originating server of this transaction. will be populated
  /// only in the cluster, and with a coordinator id/coordinator reboot id
  /// then. coordinators fill this in when they start a transaction, and
  /// the info is send with the transaction begin requests to DB servers,
  /// which will also store the coordinator's id. this is so they can
  /// abort the transaction should the coordinator die or be rebooted.
  /// the server id and reboot id are intentionally empty in single server
  /// case.
  arangodb::cluster::RebootTracker::PeerState origin;
};

}  // namespace transaction
}  // namespace arangodb

#endif
