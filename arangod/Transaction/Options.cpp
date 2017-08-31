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

#include "Options.h"

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
      allowImplicitCollections(true),
      waitForSync(false)
#ifdef USE_ENTERPRISE
      ,skipInaccessibleCollections(false)
#endif
{}
  
void Options::setLimits(uint64_t maxTransactionSize, uint64_t intermediateCommitSize, uint64_t intermediateCommitCount) {
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
  value = slice.get("allowImplicitCollections");
  if (value.isBool()) {
    allowImplicitCollections = value.getBool();
  }
  value = slice.get("waitForSync");
  if (value.isBool()) {
    waitForSync = value.getBool();
  }
#ifdef USE_ENTERPRISE
  value = slice.get("skipInaccessibleCollections");
  if (value.isBool()) {
    skipInaccessibleCollections = value.getBool();
  }
#endif
}
 
/// @brief add the options to an opened vpack builder 
void Options::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  builder.add("lockTimeout", VPackValue(lockTimeout));
  builder.add("maxTransactionSize", VPackValue(maxTransactionSize));
  builder.add("intermediateCommitSize", VPackValue(intermediateCommitSize));
  builder.add("intermediateCommitCount", VPackValue(intermediateCommitCount));
  builder.add("allowImplicitCollections", VPackValue(allowImplicitCollections));
  builder.add("waitForSync", VPackValue(waitForSync));
#ifdef USE_ENTERPRISE
  builder.add("skipInaccessibleCollections",
              VPackValue(skipInaccessibleCollections));
#endif
}
