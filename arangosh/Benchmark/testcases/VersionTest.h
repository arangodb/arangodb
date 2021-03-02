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

#ifndef ARANGODB_BENCHMARK_TESTCASES_VERSION_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_VERSION_TEST_H

#include "Benchmark.h"

namespace arangodb::arangobench {

struct VersionTest : public Benchmark<VersionTest> {
  static std::string name() { return "version"; }

  VersionTest(BenchFeature& arangobench) : Benchmark<VersionTest>(arangobench), _url("/_api/version") {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return true;
  }

  void tearDown() override {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) override {
    return _url;
  }

  rest::RequestType type(int const threadNumber, size_t const threadCounter,
                         size_t const globalCounter) override {
    return rest::RequestType::GET;
  }

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    static char const* payload = "";

    *mustFree = false;
    *length = 0;
    return payload;
  }

  std::string _url;
};

}  // namespace arangodb::arangobench
#endif
