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

struct TransactionDeadlockTest : public Benchmark<TransactionDeadlockTest> {
  static std::string name() { return "deadlocktrx"; }

  TransactionDeadlockTest(BenchFeature& arangobench)
      : Benchmark<TransactionDeadlockTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    _c1 = std::string(_arangobench.collection() + "1");
    _c2 = std::string(_arangobench.collection() + "2");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2, _arangobench) &&
           CreateCollection(client, _c2, 2, _arangobench) &&
           CreateDocument(client, _c2, "{ \"_key\": \"sum\", \"count\": 0 }");
  }

  void tearDown() override {}

  void buildRequest(size_t threadNumber, size_t threadCounter, size_t globalCounter,
                    BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = "/_api/transaction";
    requestData.type = rest::RequestType::POST;
    size_t mod = globalCounter % 2;
    using namespace arangodb::velocypack;
    requestData.payload(Value(ValueType::Object));
    requestData.payload.add("collections", Value(ValueType::Object));
    requestData.payload.add("write", Value(ValueType::Array));
    std::string actionValue =
        std::string("function () { var c = require('internal').db['");
    if (mod == 0) {
      requestData.payload.add(Value(_c1));
      actionValue += _c2;
    } else {
      requestData.payload.add(Value(_c2));
      actionValue += _c1;
    }
    requestData.payload.close();
    requestData.payload.close();
    actionValue += std::string("']; c.any(); }");
    requestData.payload.add("action", Value(actionValue));
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "creates two collections and executes JavaScript Transactions that "
           "first access one collection, and then the other. This test was "
           "once used as a means to detect deadlocks caused by collection "
           "locking, but is obsolete nowadays. The --complexity parameter is "
           "not used.";
  }

  bool isDeprecated() const noexcept override { return true; }

  std::string _c1;
  std::string _c2;
};

}  // namespace arangodb::arangobench
