////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedLogFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/application-exit.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "ProgramOptions/Parameters.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "RestServer/MetricsFeature.h"

#include <memory>

using namespace arangodb::options;
using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

ReplicatedLogFeature::ReplicatedLogFeature(ApplicationServer& server)
    : ApplicationFeature(server, "ReplicatedLog"),
      _replicatedLogMetrics(std::make_shared<ReplicatedLogMetrics>(
          server.getFeature<MetricsFeature>())),
      _options(std::make_shared<ReplicatedLogOptions>()) {
  setOptional(true);
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<DatabaseFeaturePhase>();
}

auto ReplicatedLogFeature::metrics() const noexcept
    -> std::shared_ptr<replication2::replicated_log::ReplicatedLogMetrics> const& {
  return _replicatedLogMetrics;
}

auto ReplicatedLogFeature::options() const noexcept
    -> std::shared_ptr<replication2::ReplicatedLogOptions const> {
  return _options;
}

void ReplicatedLogFeature::prepare() {
  if (ServerState::instance()->isCoordinator() || ServerState::instance()->isAgent()) {
    setEnabled(false);
    return;
  }
}

void ReplicatedLogFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("replicatedlog", "Options for replicated logs");

  options->addOption("--replicatedlog.max-network-batch-size", "",
                     new SizeTParameter(&_options->_maxNetworkBatchSize));
  options->addOption("--replicatedlog.max-rocksdb-write-batch-size", "",
                     new SizeTParameter(&_options->_maxRocksDBWriteBatchSize));
}

void ReplicatedLogFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_options->_maxNetworkBatchSize < ReplicatedLogOptions::minNetworkBatchSize) {
    LOG_TOPIC("e83c3", FATAL, arangodb::Logger::REPLICATION2)
        << "Invalid value for `--max-network-batch-size`. The value must be at "
           "least "
        << ReplicatedLogOptions::minNetworkBatchSize;
    FATAL_ERROR_EXIT();
  }
  if (_options->_maxRocksDBWriteBatchSize < ReplicatedLogOptions::minRocksDBWriteBatchSize) {
    LOG_TOPIC("e83c3", FATAL, arangodb::Logger::REPLICATION2)
        << "Invalid value for `--max-network-batch-size`. The value must be at "
           "least "
        << ReplicatedLogOptions::minRocksDBWriteBatchSize;
    FATAL_ERROR_EXIT();
  }
}

ReplicatedLogFeature::~ReplicatedLogFeature() = default;
