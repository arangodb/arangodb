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

#include "BenchmarkOperation.h"

namespace arangodb::arangobench {

std::map<std::string, BenchmarkOperation::BenchmarkFactory>& BenchmarkOperation::allBenchmarks() {
  // this is a static inline variable to avoid issues with initialization order.
  static std::map<std::string, BenchmarkFactory> benchmarks;
  return benchmarks;
}

std::unique_ptr<BenchmarkOperation> BenchmarkOperation::createBenchmark(std::string const& name, BenchFeature& arangobench) {
  auto it = allBenchmarks().find(name);
  if (it != allBenchmarks().end()) {
    return it->second.operator()(arangobench);
  }
  return nullptr;
}

void BenchmarkOperation::registerBenchmark(std::string name, BenchmarkFactory factory) {
  allBenchmarks().emplace(std::move(name), std::move(factory));
}

}  // namespace arangodb::arangobench
