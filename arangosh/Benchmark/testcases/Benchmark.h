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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BENCHMARK_TESTCASES_BENCHMARK_H
#define ARANGODB_BENCHMARK_TESTCASES_BENCHMARK_H

#include "../BenchmarkOperation.h"
#include "../BenchFeature.h"

namespace arangodb::arangobench {

template <class Derived>
struct Benchmark : public BenchmarkOperation {
  explicit Benchmark(BenchFeature& arangobench) : BenchmarkOperation(arangobench) {}

  static void registerTestcase() {
    registerBenchmark(Derived::name(), [](BenchFeature& arangobench) {
      return std::make_unique<Derived>(arangobench);
    });
  }
};

}  // namespace arangodb::arangobench
#endif
