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
#include <string>
#include <vector>

#include "Basics/NumberOfCores.h"

namespace arangodb {

struct BenchFeatureOptions {
  uint64_t threadCount = NumberOfCores::getValue();
  uint64_t operations = 1000;
  uint64_t duration = 0;
  std::string collection = "ArangoBenchmark";
  std::string testCase = "version";
  uint64_t complexity = 1;
  bool async = false;
  bool keepAlive = true;
  bool createDatabase = false;
  bool createCollection = true;
  bool delay = false;
  bool progress = true;
  bool verbose = false;
  bool quiet = false;
  bool waitForSync = false;
  bool generateHistogram = false;
  uint64_t runs = 1;
  std::string junitReportFile;
  std::string jsonReportFile;
  uint64_t replicationFactor = 1;
  uint64_t numberOfShards = 1;
  std::string customQuery;
  std::string customQueryFile;
  std::string customQueryBindVars;
  uint64_t histogramNumIntervals = 1000;
  double histogramIntervalSize = 0.0;
  std::vector<double> percentiles = {50.0, 80.0, 85.0, 90.0, 95.0, 99.0, 99.99};
};

}  // namespace arangodb
