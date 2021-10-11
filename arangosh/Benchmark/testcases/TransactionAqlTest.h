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

    void buildRequest(size_t threadNumber, size_t threadCounter,
                      size_t globalCounter, BenchmarkOperation::RequestData& requestData) const override {
      requestData.url = "/_api/cursor";
      requestData.type = rest::RequestType::POST;
      size_t mod = globalCounter % 8;
      std::string query;
      if (mod == 0) {
        query = "FOR c IN " + _c1;
      } else if (mod == 1) {
        query = "FOR c IN " + _c2;
      } else if (mod == 2) {
        query = "FOR c IN " + _c3;
      } else if (mod == 3) {
        query = "FOR c1 IN " + _c1 + " FOR c2 IN " + _c2;
      } else if (mod == 4) {
        query = "FOR c2 IN " + _c2 + " FOR c1 IN " + _c1;
      } else if (mod == 5) {
        query = "FOR c3 IN " + _c3 + " FOR c1 IN " + _c1;
      } else if (mod == 6) {
        query = "FOR c2 IN " + _c2 + " FOR c3 IN " + _c3;
      } else if (mod == 7) {
        query = "FOR c1 IN " + _c1 + " FOR c2 IN " + _c2 + " FOR c3 IN " + _c3;
      }
      query += " RETURN 1";
      using namespace arangodb::velocypack;
      requestData.payload.openObject();
      requestData.payload.add("query", Value(query));
      requestData.payload.close();
    }

    char const* getDescription() const noexcept override {
      return "creates 3 empty collections and then performs different AQL read queries on these collections, partially using joins. This test was once used to test shard locking, but is now largely obsolete. In a cluster, it still provides a little value because it effectively measures query setup and shutdown times for concurrent AQL queries.";
    }

    bool isDeprecated() const noexcept override {
      return true;
    }

    std::string _c1;
    std::string _c2;
    std::string _c3;
  };

}  // namespace arangodb::arangobench
