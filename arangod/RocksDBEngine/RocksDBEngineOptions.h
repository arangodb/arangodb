////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include "Transaction/Options.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBEngineEEOptions.h"
#endif

namespace arangodb {

struct RocksDBEngineOptions {
  double requiredDiskFreePercentage = 0.01;
  uint64_t requiredDiskFreeBytes = 16 * 1024 * 1024;
  uint64_t maxTransactionSize = transaction::Options::defaultMaxTransactionSize;
  uint64_t intermediateCommitSize =
      transaction::Options::defaultIntermediateCommitSize;
  uint64_t intermediateCommitCount =
      transaction::Options::defaultIntermediateCommitCount;
  uint64_t maxParallelCompactions = 2;
  uint64_t syncInterval = 100;
  uint64_t syncDelayThreshold = 5000;
  double pruneWaitTime = 10.0;
  double pruneWaitTimeInitial = 60.0;
  bool useThrottle = true;
  uint64_t throttleSlots = 120;
  uint64_t throttleFrequency = 1000;
  uint64_t throttleScalingFactor = 17;
  uint64_t throttleMaxWriteRate = 0;
  uint64_t throttleSlowdownWritesTrigger = 8;
  uint64_t throttleLowerBoundBps = 10 * 1024 * 1024;
#ifdef USE_ENTERPRISE
  bool createShaFiles = true;
#else
  bool createShaFiles = false;
#endif
  bool debugLogging = false;
  bool verifySst = false;
  uint64_t maxWalArchiveSizeLimit = 0;
  uint64_t autoFlushMinWalFiles = 20;
  double autoFlushCheckInterval = 60.0 * 30.0;
  bool forceLegacySortingMethod = false;
  bool forceLittleEndianKeys = false;
  bool exportReadWriteMetrics = false;
#ifdef USE_ENTERPRISE
  enterprise::RocksDBEngineEEOptions eeOptions;
#endif
};

}  // namespace arangodb
