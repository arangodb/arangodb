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

namespace arangodb::arangobench {

struct TransactionCountTest : public Benchmark<TransactionCountTest> {
  static std::string name() { return "counttrx"; }

  TransactionCountTest(BenchFeature& arangobench)
      : Benchmark<TransactionCountTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
  }

  void tearDown() override {}

  void buildRequest(size_t threadNumber, size_t threadCounter, size_t globalCounter,
                    BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = "/_api/transaction";
    requestData.type = rest::RequestType::POST;
    using namespace arangodb::velocypack;
    requestData.payload(Value(ValueType::Object));
    requestData.payload.add("collections", Value(ValueType::Object));
    requestData.payload.add("write", Value(_arangobench.collection()));
    requestData.payload.close();
    requestData.payload.add(
        "action",
        Value(std::string("function () { var c = require('internal').db['") +
              _arangobench.collection() + std::string("']; var startcount = c.count(); for (var i = 0; i < 50; ++i) { if (startcount + i !== c.count()) { throw 'error, counters deviate!'; } c.save({ }); } }")));
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "executes JavaScript Transactions that each insert 50 (empty) "
           "documents into a collection and validates that collection counts "
           "are as expected. There will be 50 times the number of --requests "
           "documents inserted in total. The --complexity parameter is not "
           "used.";
  }

  bool isDeprecated() const noexcept override { return true; }
};

}  // namespace arangodb::arangobench
