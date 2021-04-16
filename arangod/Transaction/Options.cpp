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

#include "Options.h"

#include "Basics/debugging.h"
#include "Cluster/ServerState.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::transaction;

uint64_t Options::defaultMaxTransactionSize = UINT64_MAX;
uint64_t Options::defaultIntermediateCommitSize = 512 * 1024 * 1024;
uint64_t Options::defaultIntermediateCommitCount = 1 * 1000 * 1000;

Options::Options()
    : lockTimeout(defaultLockTimeout),
      maxTransactionSize(defaultMaxTransactionSize),
      intermediateCommitSize(defaultIntermediateCommitSize),
      intermediateCommitCount(defaultIntermediateCommitCount),
      allowImplicitCollectionsForRead(true),
      allowImplicitCollectionsForWrite(false),
#ifdef USE_ENTERPRISE
      skipInaccessibleCollections(false),
#endif
      waitForSync(false),
      isFollowerTransaction(false),
      origin("", arangodb::RebootId(0)) {

  // if we are a coordinator, fill in our own server id/reboot id.
  // the data is passed to DB servers when the transaction is started
  // there. the DB servers use this data to abort the transaction
  // timely should the coordinator die or be rebooted.
  // in the DB server case, we leave the origin empty in the beginning,
  // because the coordinator id will be sent via JSON and thus will be
  // picked up inside fromVelocyPack()
  if (ServerState::instance()->isCoordinator()) {
    // cluster transactions always originate on a coordinator
    origin = arangodb::cluster::RebootTracker::PeerState(
        ServerState::instance()->getId(), 
        ServerState::instance()->getRebootId()
    );
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

void Options::setLimits(uint64_t maxTransactionSize, uint64_t intermediateCommitSize,
                        uint64_t intermediateCommitCount) {
  defaultMaxTransactionSize = maxTransactionSize;
  defaultIntermediateCommitSize = intermediateCommitSize;
  defaultIntermediateCommitCount = intermediateCommitCount;
}

void Options::fromVelocyPack(arangodb::velocypack::Slice const& slice) {
  VPackSlice value;

  value = slice.get("lockTimeout");
  if (value.isNumber()) {
    lockTimeout = value.getNumber<double>();
  }
  value = slice.get("maxTransactionSize");
  if (value.isNumber()) {
    maxTransactionSize = value.getNumber<uint64_t>();
  }
  value = slice.get("intermediateCommitSize");
  if (value.isNumber()) {
    intermediateCommitSize = value.getNumber<uint64_t>();
  }
  value = slice.get("intermediateCommitCount");
  if (value.isNumber()) {
    intermediateCommitCount = value.getNumber<uint64_t>();
  }
  // simon: 'allowImplicit' is due to naming in 'db._executeTransaction(...)'
  value = slice.get("allowImplicit");
  if (value.isBool()) {
    allowImplicitCollectionsForRead = value.getBool();
  }
#ifdef USE_ENTERPRISE
  value = slice.get("skipInaccessibleCollections");
  if (value.isBool()) {
    skipInaccessibleCollections = value.getBool();
  }
#endif
  value = slice.get("waitForSync");
  if (value.isBool()) {
    waitForSync = value.getBool();
  }
  
  if (!ServerState::instance()->isSingleServer()) {
    value = slice.get("isFollowerTransaction");
    if (value.isBool()) {
      isFollowerTransaction = value.getBool();
    }

    // pick up the originating coordinator's id. note: this can be
    // empty if the originating coordinator is an ArangoDB 3.7.
    value = slice.get("origin");
    if (value.isObject()) {
      origin = cluster::RebootTracker::PeerState::fromVelocyPack(value);
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
  builder.add("skipInaccessibleCollections", VPackValue(skipInaccessibleCollections));
#endif
  builder.add("waitForSync", VPackValue(waitForSync));
  // we are intentionally *not* writing allowImplicitCollectionForWrite here.
  // this is an internal option only used in replication

  // serialize data for cluster-wide collections
  if (!ServerState::instance()->isSingleServer()) {
    builder.add("isFollowerTransaction", VPackValue(isFollowerTransaction));
    
    // serialize the server id/reboot id of the originating server (which must
    // be a coordinator id if set)
    if (!origin.serverId().empty()) {
      builder.add(VPackValue("origin"));
      origin.toVelocyPack(builder);
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
