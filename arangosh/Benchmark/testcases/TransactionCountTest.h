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
#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <string>

namespace arangodb::arangobench {

  struct TransactionCountTest : public Benchmark<TransactionCountTest> {
    static std::string name() { return "counttrx"; }

    TransactionCountTest(BenchFeature& arangobench) : Benchmark<TransactionCountTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench);
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
      using namespace arangodb::velocypack;
      Builder b;
      b(Value(ValueType::Object));
      b.add("collections", Value(ValueType::Object));
      b.add("write", Value(_arangobench.collection()));
      b.close();
      b.add("action", Value(std::string("function () { var c = require(\"internal\").db[\"") + _arangobench.collection() + std::string("\"]; var startcount = c.count(); for (var i = 0; i < 50; ++i) { if (startcount + i !== c.count()) { throw \"error, counters deviate!\"; } c.save({ }); } }")));   
      b.close();
      buffer = b.toJson();
      LOG_DEVEL << "buffer is receiving " << buffer;
    }

  };

}  // namespace arangodb::arangobench
