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

struct TransactionMultiCollectionTest
    : public Benchmark<TransactionMultiCollectionTest> {
  static std::string name() { return "multi-collection"; }

  TransactionMultiCollectionTest(BenchFeature& arangobench)
      : Benchmark<TransactionMultiCollectionTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    _c1 = std::string(_arangobench.collection() + "1");
    _c2 = std::string(_arangobench.collection() + "2");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2, _arangobench) &&
           CreateCollection(client, _c2, 2, _arangobench);
  }

  void tearDown() override {}

  void buildRequest(
      size_t threadNumber, size_t threadCounter, size_t globalCounter,
      BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = "/_api/transaction";
    requestData.type = rest::RequestType::POST;
    using namespace arangodb::velocypack;
    requestData.payload(Value(ValueType::Object));
    requestData.payload.add("collections", Value(ValueType::Object));
    requestData.payload.add("write", Value(ValueType::Array));
    requestData.payload.add(Value(_c1));
    requestData.payload.add(Value(_c2));
    requestData.payload.close();
    requestData.payload.close();
    std::string actionValue =
        std::string("function () { var c1 = require('internal').db['") + _c1 +
        std::string("']; var c2 = require('internal').db['") + _c2 +
        std::string("']; var doc = {");
    uint64_t const n = _arangobench.complexity();
    for (uint64_t i = 0; i < n; ++i) {
      if (i > 0) {
        actionValue += ", ";
      }
      actionValue +=
          std::string("value") + std::to_string(i) + ": " + std::to_string(i);
    }
    actionValue += " }; c1.save(doc); c2.save(doc); }";
    requestData.payload.add("action", Value(actionValue));
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "creates two collections and then executes JavaScript Transactions "
           "that first write into one and then the other collection. The "
           "documents written into both collections are identical, and the "
           "number of their attributes can be controlled via the --complexity "
           "parameter. There will be as many JavaScript Transactions as "
           "--requests, and twice the number of documents inserted.";
  }

  bool isDeprecated() const noexcept override { return true; }

  std::string _c1;
  std::string _c2;
};

}  // namespace arangodb::arangobench
