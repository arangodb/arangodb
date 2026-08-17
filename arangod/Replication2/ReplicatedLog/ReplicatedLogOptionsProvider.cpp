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

#include "ReplicatedLogOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::replication2 {

using namespace arangodb::options;

void ReplicatedLogOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts,
    ReplicatedLogGlobalSettings& options) {
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  opts->addSection("replicated-log", "Options for replicated logs");

  opts->addOption(
      "--replicated-log.threshold-network-batch-size",
      "send a batch of log updates early when threshold "
      "(in bytes) is exceeded",
      new SizeTParameter(
          &options.thresholdNetworkBatchSize, /*base*/ 1, /*minValue*/
          ReplicatedLogGlobalSettings::minThresholdNetworkBatchSize));
  opts->addOption(
      "--replicated-log.threshold-rocksdb-write-batch-size",
      "write a batch of log updates to RocksDB early "
      "when threshold (in bytes) is exceeded",
      new SizeTParameter(
          &options.thresholdRocksDBWriteBatchSize, /*base*/ 1, /*minValue*/
          ReplicatedLogGlobalSettings::minThresholdRocksDBWriteBatchSize));
  opts->addOption(
      "--replicated-log.threshold-log-compaction",
      "threshold for log compaction. Number of log entries to wait for before "
      "compacting.",
      new SizeTParameter(&options.thresholdLogCompaction, /*base*/ 1,
                         /*minValue*/ 0));
#endif
}

}  // namespace arangodb::replication2
