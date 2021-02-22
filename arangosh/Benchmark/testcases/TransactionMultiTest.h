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

#ifndef ARANGODB_BENCHMARK_TESTCASES_TRANSACTION_MULTI_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_TRANSACTION_MULTI_TEST_H

#include "Benchmark.h"
#include "helpers.h"

namespace arangodb::arangobench {

struct TransactionMultiTest : public Benchmark<TransactionMultiTest> {
  static std::string name() { return "multitrx"; }

  TransactionMultiTest(BenchFeature& arangobench) : Benchmark<TransactionMultiTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    _c1 = std::string(_arangobench.collection() + "1");
    _c2 = std::string(_arangobench.collection() + "2");

    return DeleteCollection(client, _c1) &&
           DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2, _arangobench) &&
           CreateCollection(client, _c2, 2, _arangobench) &&
           CreateDocument(client, _c2, "{ \"_key\": \"sum\", \"count\": 0 }");
  }

  void tearDown() override {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) override {
    return std::string("/_api/transaction");
  }

  rest::RequestType type(int const threadNumber, size_t const threadCounter,
                         size_t const globalCounter) override {
    return rest::RequestType::POST;
  }

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    size_t const mod = globalCounter % 2;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { ");

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "\"exclusive\": [ \"");
    } else {
      TRI_AppendStringStringBuffer(buffer, "\"read\": [ \"");
    }

    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(buffer, "\", \"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer,
                                 "\" ] }, \"action\": \"function () { ");
    TRI_AppendStringStringBuffer(buffer,
                                 "var c1 = require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(
        buffer, "\\\"]; var c2 = require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer, "\\\"]; ");

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer,
                                   "var n = Math.floor(Math.random() * 25) + "
                                   "1; c1.save({ count: n }); var d = "
                                   "c2.document(\\\"sum\\\"); c2.update(d, { "
                                   "count: d.count + n });");
    } else {
      TRI_AppendStringStringBuffer(
          buffer,
          "var r1 = 0; c1.toArray().forEach(function "
          "(d) { r1 += d.count }); var r2 = "
          "c2.document(\\\"sum\\\").count; if (r1 !== "
          "r2) { throw \\\"error, counters deviate!\\\"; }");
    }

    TRI_AppendStringStringBuffer(buffer, " }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(buffer);

    return (char const*)ptr;
  }

  std::string _c1;
  std::string _c2;
};

}  // namespace arangodb::arangobench
#endif
