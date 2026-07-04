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

#include <atomic>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Benchmark/BenchFeatureOptions.h"
#include "Benchmark/BenchmarkThread.h"
#include "Benchmark/BenchmarkStats.h"

namespace arangodb {
namespace arangobench {
struct BenchmarkStats;
}

class ClientFeature;

struct BenchRunResult {
  double _time;
  uint64_t _failures;
  uint64_t _incomplete;
  double _requestTime;

  void update(double time, uint64_t failures, uint64_t incomplete,
              double requestTime) {
    _time = time;
    _failures = failures;
    _incomplete = incomplete;
    _requestTime = requestTime;
  }
};

class BenchFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Bench"; }

  BenchFeature(application_features::ApplicationServer& server, int* result,
               BenchFeatureOptions options);
  BenchFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override final;
  void start() override final;

  bool async() const { return _options.async; }
  uint64_t threadCount() const { return _options.threadCount; }
  uint64_t operations() const { return _options.operations; }
  bool createCollection() const { return _options.createCollection; }
  bool keepAlive() const { return _options.keepAlive; }
  std::string const& collection() const { return _options.collection; }
  std::string const& testCase() const { return _options.testCase; }
  uint64_t complexity() const { return _options.complexity; }
  bool delay() const { return _options.delay; }
  bool progress() const { return _options.progress; }
  bool verbose() const { return _options.verbose; }
  bool quiet() const { return _options.quiet; }
  uint64_t runs() const { return _options.runs; }
  std::string const& junitReportFile() const {
    return _options.junitReportFile;
  }
  uint64_t replicationFactor() const { return _options.replicationFactor; }
  uint64_t numberOfShards() const { return _options.numberOfShards; }
  bool waitForSync() const { return _options.waitForSync; }
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  std::string const& customQuery() const { return _options.customQuery; }
  std::string const& customQueryFile() const {
    return _options.customQueryFile;
  }
  std::shared_ptr<VPackBuilder> customQueryBindVars() const {
    return _customQueryBindVarsBuilder;
  }

 private:
  void status(std::string const& value);
  void report(ClientFeature& client, std::vector<BenchRunResult> const& results,
              arangobench::BenchmarkStats const& stats,
              std::string const& histogram, VPackBuilder& builder);
  void printResult(BenchRunResult const& result, VPackBuilder& builder);
  bool writeJunitReport(BenchRunResult const& result);
  void setupHistogram(std::stringstream& pp);
  void updateStatsValues(
      std::stringstream& pp, VPackBuilder& builder,
      std::vector<
          std::unique_ptr<arangodb::arangobench::BenchmarkThread>> const&
          threads,
      arangodb::arangobench::BenchmarkStats& totalStats);

  BenchFeatureOptions _options;

  uint64_t _realOperations = 0;
  std::shared_ptr<VPackBuilder> _customQueryBindVarsBuilder;

  int* _result;

  static void updateStartCounter();
  static int getStartCounter();

  static std::atomic<int> _started;
};

}  // namespace arangodb
