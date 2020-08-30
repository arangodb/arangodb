////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BENCHMARK_BENCH_FEATURE_H
#define ARANGODB_BENCHMARK_BENCH_FEATURE_H 1

#include <atomic>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class ClientFeature;

struct BenchRunResult {
  double time;
  size_t failures;
  size_t incomplete;
  double requestTime;

  void update(double _time, size_t _failures, size_t _incomplete, double _requestTime) {
    time = _time;
    failures = _failures;
    incomplete = _incomplete;
    requestTime = _requestTime;
  }
};

class BenchFeature final : public application_features::ApplicationFeature {
 public:
  BenchFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void start() override final;
  void unprepare() override final;

  bool async() const { return _async; }
  uint64_t concurrency() const { return _concurrency; }
  uint64_t operations() const { return _operations; }
  uint64_t batchSize() const { return _batchSize; }
  bool keepAlive() const { return _keepAlive; }
  std::string const& collection() const { return _collection; }
  std::string const& testCase() const { return _testCase; }
  uint64_t complexity() const { return _complexity; }
  bool delay() const { return _delay; }
  bool progress() const { return _progress; }
  bool verbose() const { return _verbose; }
  bool quit() const { return _quiet; }
  uint64_t runs() const { return _runs; }
  std::string const& junitReportFile() const { return _junitReportFile; }
  uint64_t replicationFactor() const { return _replicationFactor; }
  uint64_t numberOfShards() const { return _numberOfShards; }
  bool waitForSync() const { return _waitForSync; }

 private:
  void status(std::string const& value);
  bool report(ClientFeature&, std::vector<BenchRunResult>, double minTime, double maxTime, double avgTime, std::string const& histogram, VPackBuilder& builder);
  void printResult(BenchRunResult const& result, VPackBuilder& builder);
  bool writeJunitReport(BenchRunResult const& result);

  bool _async;
  uint64_t _concurrency;
  uint64_t _operations;
  uint64_t _batchSize;
  bool _keepAlive;
  std::string _collection;
  std::string _testCase;
  uint64_t _complexity;
  bool _delay;
  bool _progress;
  bool _verbose;
  bool _quiet;
  uint64_t _runs;
  std::string _junitReportFile;
  std::string _jsonReportFile;
  uint64_t _replicationFactor;
  uint64_t _numberOfShards;
  bool _waitForSync;

  int* _result;

  uint64_t _histogramNumIntervals;
  double _histogramIntervalSize;
  std::vector<double> _percentiles;
  
  static void updateStartCounter();
  static int getStartCounter();

  static std::atomic<int> _started;
};

}  // namespace arangodb

#endif
