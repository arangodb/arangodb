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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Options.h"

#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Cluster/ServerState.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb::transaction;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
uint64_t Options::defaultMaxTransactionSize =
    std::numeric_limits<decltype(Options::defaultMaxTransactionSize)>::max();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
uint64_t Options::defaultIntermediateCommitSize =
    std::uint64_t{512} * 1024 * 1024;  // 1 << 29
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
uint64_t Options::defaultIntermediateCommitCount = 1'000'000;

Options::Options() {
  // if we are a coordinator, fill in our own server id/reboot id.
  // the data is passed to DB servers when the transaction is started
  // there. the DB servers use this data to abort the transaction
  // timely should the coordinator die or be rebooted.
  // in the DB server case, we leave the origin empty in the beginning,
  // because the coordinator id will be sent via JSON and thus will be
  // picked up inside fromVelocyPack()
  if (ServerState::instance()->isCoordinator()) {
    // cluster transactions always originate on a coordinator
    origin.serverId = ServerState::instance()->getId();
    origin.rebootId = ServerState::instance()->getRebootId();
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // patch intermediateCommitCount for testing
  adjustIntermediateCommitCount(*this);
#endif
}

Options Options::replicationDefaults() {
  Options options;
  // this is important, because when we get a "transaction begin" marker
  // we don't know which collections will participate in the transaction later.
  options.allowImplicitCollectionsForWrite = true;
  options.waitForSync = false;
  return options;
}

void Options::setLimits(uint64_t maxTransactionSize,
                        uint64_t intermediateCommitSize,
                        uint64_t intermediateCommitCount) {
  defaultMaxTransactionSize = maxTransactionSize;
  defaultIntermediateCommitSize = intermediateCommitSize;
  defaultIntermediateCommitCount = intermediateCommitCount;
}

bool Options::isIntermediateCommitEnabled() const noexcept {
  return intermediateCommitSize != UINT64_MAX ||
         intermediateCommitCount != UINT64_MAX;
}

void Options::fromVelocyPack(arangodb::velocypack::Slice slice) {
  if (auto value = slice.get("lockTimeout"); value.isNumber()) {
    lockTimeout = value.getNumber<double>();
  }
  if (auto value = slice.get("maxTransactionSize"); value.isNumber()) {
    maxTransactionSize = value.getNumber<uint64_t>();
  }
  if (auto value = slice.get("intermediateCommitSize"); value.isNumber()) {
    intermediateCommitSize = value.getNumber<uint64_t>();
  }
  if (auto value = slice.get("intermediateCommitCount"); value.isNumber()) {
    intermediateCommitCount = value.getNumber<uint64_t>();
  }
  // simon: 'allowImplicit' is due to naming in 'db._executeTransaction(...)'
  if (auto value = slice.get("allowImplicit"); value.isBool()) {
    allowImplicitCollectionsForRead = value.isTrue();
  }
#ifdef USE_ENTERPRISE
  if (auto value = slice.get("skipInaccessibleCollections"); value.isBool()) {
    skipInaccessibleCollections = value.isTrue();
  }
#endif
  if (auto value = slice.get(StaticStrings::WaitForSyncString);
      value.isBool()) {
    waitForSync = value.isTrue();
  }
  if (auto value = slice.get("fillBlockCache"); value.isBool()) {
    fillBlockCache = value.isTrue();
  }
  if (auto value = slice.get("allowDirtyReads"); value.isBool()) {
    allowDirtyReads = value.isTrue();
  } else {
    TRI_IF_FAILURE("TransactionState::dirtyReadsAreDefault") {
      allowDirtyReads = true;
    }
  }
  if (auto value = slice.get("skipFastLockRound"); value.isBool()) {
    skipFastLockRound = value.isTrue();
  }

  if (!ServerState::instance()->isSingleServer()) {
    if (auto value = slice.get("isFollowerTransaction"); value.isBool()) {
      isFollowerTransaction = value.isTrue();
    }

    // pick up the originating coordinator's id. note: this can be
    // empty if the originating coordinator is an ArangoDB 3.7.
    if (auto value = slice.get("origin"); value.isObject()) {
      origin.serverId =
          value.get(StaticStrings::AttrCoordinatorId).stringView();
      origin.rebootId =
          RebootId{value.get(StaticStrings::AttrCoordinatorRebootId)
                       .getNumber<uint64_t>()};
    }
  }
  // we are intentionally *not* reading allowImplicitCollectionForWrite here.
  // this is an internal option only used in replication

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // patch intermediateCommitCount for testing
  adjustIntermediateCommitCount(*this);
#endif
}

/// @brief add the options to an opened vpack builder
void Options::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  builder.add("lockTimeout", VPackValue(lockTimeout));
  builder.add("maxTransactionSize", VPackValue(maxTransactionSize));
  builder.add("intermediateCommitSize", VPackValue(intermediateCommitSize));
  builder.add("intermediateCommitCount", VPackValue(intermediateCommitCount));
  builder.add("allowImplicit", VPackValue(allowImplicitCollectionsForRead));
#ifdef USE_ENTERPRISE
  builder.add("skipInaccessibleCollections",
              VPackValue(skipInaccessibleCollections));
#endif
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  builder.add("fillBlockCache", VPackValue(fillBlockCache));
  // we are intentionally *not* writing allowImplicitCollectionForWrite here.
  // this is an internal option only used in replication
  builder.add("allowDirtyReads", VPackValue(allowDirtyReads));

  builder.add("skipFastLockRound", VPackValue(skipFastLockRound));

  // serialize data for cluster-wide collections
  if (!ServerState::instance()->isSingleServer()) {
    builder.add("isFollowerTransaction", VPackValue(isFollowerTransaction));

    // serialize the server id/reboot id of the originating server (which must
    // be a coordinator id if set)
    if (!origin.serverId.empty()) {
      builder.add(VPackValue("origin"));
      builder.openObject();
      builder.add(StaticStrings::AttrCoordinatorId,
                  VPackValue{origin.serverId});
      builder.add(StaticStrings::AttrCoordinatorRebootId,
                  VPackValue{origin.rebootId.value()});
      builder.close();
    }
  }
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief patch intermediateCommitCount for testing
/*static*/ void Options::adjustIntermediateCommitCount(Options& options) {
  TRI_IF_FAILURE("TransactionState::intermediateCommitCount100") {
    options.intermediateCommitCount = 100;
  }
  TRI_IF_FAILURE("TransactionState::intermediateCommitCount1000") {
    options.intermediateCommitCount = 1000;
  }
  TRI_IF_FAILURE("TransactionState::intermediateCommitCount10000") {
    options.intermediateCommitCount = 10000;
  }
}
#endif
