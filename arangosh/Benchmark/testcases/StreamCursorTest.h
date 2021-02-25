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

#ifndef ARANGODB_BENCHMARK_TESTCASES_STREAM_CURSOR_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_STREAM_CURSOR_TEST_H

#include "Benchmark.h"
#include "helpers.h"

namespace arangodb::arangobench {

struct StreamCursorTest : public Benchmark<StreamCursorTest> {
  static std::string name() { return "stream-cursor"; }

  StreamCursorTest(BenchFeature& arangobench) : Benchmark<StreamCursorTest>(arangobench) {}

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

    size_t const mod = globalCounter % 2;

    if (globalCounter == 0) {
      TRI_AppendStringStringBuffer(
          buffer, "{\"query\":\"FOR i IN 1..500 INSERT { _key: TO_STRING(i)");

      uint64_t const n = _arangobench.complexity();
      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\\\"value");
        TRI_AppendUInt64StringBuffer(buffer, i);
        TRI_AppendStringStringBuffer(buffer, "\\\":true");
      }

      TRI_AppendStringStringBuffer(buffer, " } INTO ");
      TRI_AppendStringStringBuffer(buffer, _arangobench.collection().c_str());
      TRI_AppendStringStringBuffer(buffer,
                                   "\"}");  // OPTIONS { ignoreErrors: true }");
    } else if (mod == 0) {
      TRI_AppendStringStringBuffer(
          buffer,
          "{\"query\":\"UPDATE { _key: \\\"1\\\" } WITH { \\\"foo\\\":1");

      uint64_t const n = _arangobench.complexity();
      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\\\"value");
        TRI_AppendUInt64StringBuffer(buffer, i);
        TRI_AppendStringStringBuffer(buffer, "\\\":true");
      }

      TRI_AppendStringStringBuffer(buffer, " } INTO ");
      TRI_AppendStringStringBuffer(buffer, _arangobench.collection().c_str());
      TRI_AppendStringStringBuffer(buffer,
                                   " OPTIONS { ignoreErrors: true }\"}");
    } else {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR doc IN ");
      TRI_AppendStringStringBuffer(buffer, _arangobench.collection().c_str());
      TRI_AppendStringStringBuffer(
          buffer, " RETURN doc\",\"options\":{\"stream\":true}}");
    }

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(buffer);

    return (char const*)ptr;
  }
};

}  // namespace arangodb::arangobench
#endif
