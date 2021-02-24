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

#ifndef ARANGODB_BENCHMARK_TESTCASES_TRANSACTION_AQL_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_TRANSACTION_AQL_TEST_H

#include "Benchmark.h"

namespace arangodb::arangobench {

struct TransactionAqlTest : public Benchmark<TransactionAqlTest> {
  static std::string name() { return "aqltrx"; }

  TransactionAqlTest(BenchFeature& arangobench) : Benchmark<TransactionAqlTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    _c1 = std::string(_arangobench.collection() + "1");
    _c2 = std::string(_arangobench.collection() + "2");
    _c3 = std::string(_arangobench.collection() + "3");

    return DeleteCollection(client, _c1) &&
           DeleteCollection(client, _c2) &&
           DeleteCollection(client, _c3) &&
           CreateCollection(client, _c1, 2, _arangobench) &&
           CreateCollection(client, _c2, 2, _arangobench) &&
           CreateCollection(client, _c3, 2, _arangobench);
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
    size_t const mod = globalCounter % 8;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(256);

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 1) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 2) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 3) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 4) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 5) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 6) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 7) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(buffer);

    return (char const*)ptr;
  }

  std::string _c1;
  std::string _c2;
  std::string _c3;
};

}  // namespace arangodb::arangobench
#endif
