////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class BenchFeature final : public application_features::ApplicationFeature {
 public:
  BenchFeature(application_features::ApplicationServer* server, int* result);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void start() override final;
  void unprepare() override final;

 public:
  bool async() const { return _async; }
  uint64_t const& concurrency() const { return _concurreny; }
  uint64_t const& operations() const { return _operations; }
  uint64_t const& batchSize() const { return _batchSize; }
  bool keepAlive() const { return _keepAlive; }
  std::string const& collection() const { return _collection; }
  std::string const& testCase() const { return _testCase; }
  uint64_t const& complexity() const { return _complexity; }
  bool delay() const { return _delay; }
  bool progress() const { return _progress; }
  bool verbose() const { return _verbose; }
  bool quit() const { return _quiet; }

 private:
  void status(std::string const& value);

 private:
  bool _async;
  uint64_t _concurreny;
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

 private:
  int* _result;

 private:
  static void updateStartCounter();
  static int getStartCounter();

 private:
  static std::atomic<int> _started;
};
}

#endif
