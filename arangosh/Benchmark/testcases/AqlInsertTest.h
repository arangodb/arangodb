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

  struct AqlInsertTest : public Benchmark<AqlInsertTest> {
    static std::string name() { return "aqlinsert"; }

    AqlInsertTest(BenchFeature& arangobench) : Benchmark<AqlInsertTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench);
    }

    void tearDown() override {}

    void buildRequest(int threadNumber, size_t threadCounter, 
                      size_t globalCounter, BenchmarkOperation::RequestData& requestData) const override {
      using namespace arangodb::velocypack;
      requestData.url = "/_api/cursor";
      requestData.type = rest::RequestType::POST;
      requestData.payload.openObject();
      requestData.payload.add("query", Value(std::string("INSERT @data INTO " + _arangobench.collection())));
      requestData.payload.add(Value("bindVars"));
      requestData.payload.openObject();
      requestData.payload.add(Value("data"));
      requestData.payload.openObject();
      requestData.payload.add(StaticStrings::KeyString, Value(std::string("test") + std::to_string((int64_t)globalCounter)));
      uint64_t const n = _arangobench.complexity();
      for (uint64_t i = 1; i <= n; ++i) {
        requestData.payload.add(std::string("value") + std::to_string(i), Value(true));
      }
      requestData.payload.close();
      requestData.payload.close();
      requestData.payload.close();
    }

    char const* getDescription() const noexcept override {
      return "performs AQL queries that insert one document per query. The --complexity parameter controls the number of attributes per document. The attribute values for the inserted documents will be hard-coded, except _key. The total number of documents to be inserted is equal to the value of --requests.";
    }

    bool isDeprecated() const noexcept override {
      return false;
    }
  };

}  // namespace arangodb::arangobench
