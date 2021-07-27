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
#include <string>

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

    void payload(int threadNumber, size_t threadCounter, 
                 size_t globalCounter, std::string& buffer) const override {
      size_t mod = globalCounter % 8;
      std::string values;
      if (mod == 0) {
        values = "FOR c IN " + _c1; 
      } else if (mod == 1) {
        values = "FOR c IN " + _c2;
      } else if (mod == 2) {
        values = "FOR c IN " + _c3;
      } else if (mod ==3) { 
        values = "FOR c1 IN " + _c1 + " FOR c2 IN " + _c2;
      } else if (mod == 4) {
        values = "FOR c2 IN " + _c2 + " FOR c1 IN " + _c1;
      } else if (mod == 5) {
        values = "FOR c3 IN " + _c3 + " FOR c1 IN " + _c1;
      } else if (mod == 6) {
        values = "FOR c2 IN " + _c2 + " FOR c3 IN " + _c3;
      } else if (mod == 7) {
        values = "FOR c1 IN " + _c1 + " FOR c2 IN " + _c2 + " FOR c3 IN " + _c3;
      }
      values += " RETURN 1";
      using namespace arangodb::velocypack;
      Builder b;
      b.openObject();
      b.add("query", Value(values));
      b.close();
      buffer = b.toJson();
    }

    std::string _c1;
    std::string _c2;
    std::string _c3;
  };

}  // namespace arangodb::arangobench
