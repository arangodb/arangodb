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

#include "Benchmark.h"
#include "helpers.h"
#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <string>

namespace arangodb::arangobench {

  struct TransactionDeadlockTest : public Benchmark<TransactionDeadlockTest> {
    static std::string name() { return "deadlocktrx"; }

    TransactionDeadlockTest(BenchFeature& arangobench) : Benchmark<TransactionDeadlockTest>(arangobench) {}

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

    void payload(int threadNumber, size_t threadCounter,
        size_t globalCounter, std::string& buffer) override {
      size_t mod = globalCounter % 2;
      using namespace arangodb::velocypack;
      Builder b;
      b(Value(ValueType::Object));
      b.add("collections", Value(ValueType::Object));
      b.add("write", Value(ValueType::Array));
      std::string actionValue = std::string("function () { var c = require(\"internal\").db[\"");
      if(mod == 0) {
        b.add(Value(_c1));
        actionValue += _c2;
      } else {
        b.add(Value(_c2));
        actionValue += _c1;
      }
      b.close();
      b.close();
      actionValue += std::string("\"]; c.any(); }");
      b.add("action", Value(actionValue));
      b.close();
      buffer = b.toJson();
    }

    std::string _c1;
    std::string _c2;
  };

}  // namespace arangodb::arangobench
