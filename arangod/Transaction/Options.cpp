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
      waitForSync(false) {}
  
void Options::setLimits(uint64_t maxTransactionSize, uint64_t intermediateCommitSize, uint64_t intermediateCommitCount) {
  defaultMaxTransactionSize = maxTransactionSize;
  defaultIntermediateCommitSize = intermediateCommitSize;
  defaultIntermediateCommitCount = intermediateCommitCount;
}
