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

  struct TransactionMultiCollectionTest : public Benchmark<TransactionMultiCollectionTest> {
    static std::string name() { return "multi-collection"; }

    TransactionMultiCollectionTest(BenchFeature& arangobench)
      : Benchmark<TransactionMultiCollectionTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      _c1 = std::string(_arangobench.collection() + "1");
      _c2 = std::string(_arangobench.collection() + "2");

      return DeleteCollection(client, _c1) &&
        DeleteCollection(client, _c2) &&
        CreateCollection(client, _c1, 2, _arangobench) &&
        CreateCollection(client, _c2, 2, _arangobench);
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
        size_t globalCounter, std::string& buffer) const override {
      using namespace arangodb::velocypack;
      Builder b;
      b(Value(ValueType::Object));
      b.add("collections", Value(ValueType::Object));
      b.add("write", Value(ValueType::Array));
      b.add(Value(_c1));
      b.add(Value(_c2));
      b.close();
      b.close();
      std::string actionValue = std::string("function () { var c1 = require(\"internal\").db[\"") + _c1 + std::string("\"]; var c2 = require(\"internal\").db[\"") + _c2 + std::string("\"]; var doc = {");
      uint64_t const n = _arangobench.complexity();
      for (uint64_t i = 0; i < n; ++i) {
        if (i > 0) {
          actionValue += ", ";
        }
        actionValue += std::string("value") + std::to_string(i) + ": " + std::to_string(i);
      }
      actionValue += " }; c1.save(doc); c2.save(doc); }";
      b.add("action", Value(actionValue));
      b.close();
      buffer = b.toJson();
    }

    std::string _c1;
    std::string _c2;
  };

}  // namespace arangodb::arangobench
