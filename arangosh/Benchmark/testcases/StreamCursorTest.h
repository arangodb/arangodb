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
#include <string>

namespace arangodb::arangobench {

  struct StreamCursorTest : public Benchmark<StreamCursorTest> {
    static std::string name() { return "stream-cursor"; }

    StreamCursorTest(BenchFeature& arangobench) : Benchmark<StreamCursorTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench);
    }

    void tearDown() override {}

    void buildRequest(size_t threadNumber, size_t threadCounter,
                      size_t globalCounter, BenchmarkOperation::RequestData& requestData) const override {
      requestData.url = "/_api/cursor";
      requestData.type = rest::RequestType::POST;
      size_t mod = globalCounter % 2;
      uint64_t n = _arangobench.complexity();
      using namespace arangodb::velocypack;
      requestData.payload.openObject();
      std::string query;
      if (globalCounter == 0) {
        query = "FOR i IN 1..500 INSERT { \"_key\": TO_STRING(i)";
        for (uint64_t i = 1; i <= n; ++i) {
          query += ", \"value" + std::to_string(i) + "\": true";
        }
        query += "} INTO " + _arangobench.collection();
      } else if (mod == 0) {
        query = "UPDATE { \"_key\": \"1\" } WITH { \"foo\": 1";
        for (uint64_t i = 1; i <= n; ++i) {
          query += ", \"value" + std::to_string(i) + "\": true";
        }
        query += " } INTO " + _arangobench.collection() + std::string(" OPTIONS { ignoreErrors: true }");
      } else {
        query = "FOR doc IN " + _arangobench.collection() + std::string(" RETURN doc");
        requestData.payload.add("options", Value(ValueType::Object));
        requestData.payload.add("stream", Value(true));
        requestData.payload.close();
      }
      requestData.payload.add("query", Value(query));
      requestData.payload.close();
    }

    char const* getDescription() const noexcept override {
      return "creates 500 documents in a collection, and then performs a mix of AQL update queries (all on the same document) and a streaming AQL query that returns all documents from the collection. The --complexity parameter can be used to control the number of attributes for the inserted documents and the update queries. This test will trigger a lot of write-write conflicts with --concurrency bigger than 2.";
    }

    bool isDeprecated() const noexcept override {
      return true;
    }

  };

}  // namespace arangodb::arangobench
