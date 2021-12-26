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

#pragma once

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <string>
#include "Benchmark.h"
#include "helpers.h"

namespace arangodb::arangobench {

struct TransactionMultiTest : public Benchmark<TransactionMultiTest> {
  static std::string name() { return "multitrx"; }

  TransactionMultiTest(BenchFeature& arangobench)
      : Benchmark<TransactionMultiTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    _c1 = std::string(_arangobench.collection() + "1");
    _c2 = std::string(_arangobench.collection() + "2");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2, _arangobench) &&
           CreateCollection(client, _c2, 2, _arangobench) &&
           CreateDocument(client, _c2, "{ \"_key\": \"sum\", \"count\": 0 }");
  }

  void tearDown() override {}

  void buildRequest(
      size_t threadNumber, size_t threadCounter, size_t globalCounter,
      BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = "/_api/transaction";
    requestData.type = rest::RequestType::POST;
    size_t mod = globalCounter % 2;
    using namespace arangodb::velocypack;
    requestData.payload(Value(ValueType::Object));
    requestData.payload.add("collections", Value(ValueType::Object));
    if (mod == 0) {
      requestData.payload.add("exclusive", Value(ValueType::Array));
    } else {
      requestData.payload.add("read", Value(ValueType::Array));
    }
    requestData.payload.add(Value(_c1));
    requestData.payload.add(Value(_c2));
    requestData.payload.close();
    requestData.payload.close();
    std::string actionValue =
        std::string("function () { var c1 = require('internal').db['") + _c1 +
        std::string("']; var c2 = require('internal').db['") + _c2 +
        std::string("'];");
    if (mod == 0) {
      actionValue += std::string(
          "var n = Math.floor(Math.random() * 25) + 1; c1.save({count : n}); "
          "var d = c2.document('sum'); c2.update(d, { count: d.count + n }); "
          "}");
    } else {
      actionValue += std::string(
          "var r1 = 0; c1.toArray().forEach(function (d) { r1 += d.count }); "
          "var r2 = c2.document('sum').count; if (r1 !== r2) { throw 'error, "
          "counters deviate!'; } }");
    }
    requestData.payload.add("action", Value(actionValue));
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "creates two collections and then executes JavaScript Transactions "
           "that read from and write to both collections. There will be as "
           "many JavaScript Transactions as --requests. The --complexity "
           "parameter is ignored.";
  }

  bool isDeprecated() const noexcept override { return true; }

  std::string _c1;
  std::string _c2;
};

}  // namespace arangodb::arangobench
