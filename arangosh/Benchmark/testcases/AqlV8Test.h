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

#ifndef ARANGODB_BENCHMARK_TESTCASES_AQL_V8_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_AQL_V8_TEST_H

#include "Benchmark.h"
#include "helpers.h"

namespace arangodb::arangobench {

struct AqlV8Test : public Benchmark<AqlV8Test> {
  static std::string name() { return "aqlv8"; }

  AqlV8Test(BenchFeature& arangobench) : Benchmark<AqlV8Test>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
  }

  void tearDown() override {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) override {
    return std::string("/_api/cursor");
  }

  rest::RequestType type(int const threadNumber, size_t const threadCounter,
                         size_t const globalCounter) override {
    return rest::RequestType::POST;
  }

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(256);

    TRI_AppendStringStringBuffer(buffer,
                                 "{\"query\":\"INSERT { _key: \\\"test");
    TRI_AppendInt64StringBuffer(buffer, (int64_t)globalCounter);
    TRI_AppendStringStringBuffer(buffer, "\\\"");

    uint64_t const n = _arangobench.complexity();
    for (uint64_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(buffer, ",\\\"value");
      TRI_AppendUInt64StringBuffer(buffer, i);
      TRI_AppendStringStringBuffer(buffer, "\\\":RAND(),\\\"test");
      TRI_AppendUInt64StringBuffer(buffer, i);
      TRI_AppendStringStringBuffer(buffer, "\\\":RANDOM_TOKEN(32)");
    }

    TRI_AppendStringStringBuffer(buffer, " } INTO ");
    TRI_AppendStringStringBuffer(buffer, _arangobench.collection().c_str());
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(buffer);

    return (char const*)ptr;
  }
};

}  // namespace arangodb::arangobench
#endif
