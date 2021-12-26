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
#include <string>
#include "Benchmark.h"
#include "helpers.h"

namespace arangodb::arangobench {

struct AqlV8Test : public Benchmark<AqlV8Test> {
  static std::string name() { return "aqlv8"; }

  AqlV8Test(BenchFeature& arangobench) : Benchmark<AqlV8Test>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
  }

  void tearDown() override {}

  void buildRequest(size_t threadNumber, size_t threadCounter, size_t globalCounter,
                    BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = "/_api/cursor";
    requestData.type = rest::RequestType::POST;
    using namespace arangodb::velocypack;
    uint64_t const n = _arangobench.complexity();
    std::string values = "INSERT { _key: 'test" +
                         std::to_string(static_cast<int64_t>(globalCounter)) +
                         "'";
    for (uint64_t i = 1; i <= n; ++i) {
      values += ", value" + std::to_string(i) + ": RAND()";
      values += ", test" + std::to_string(i) + ": RANDOM_TOKEN(32)";
    }
    values += "} INTO " + _arangobench.collection();
    requestData.payload.openObject();
    requestData.payload.add("query", Value(values));
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "performs AQL queries that insert one document per query. The "
           "--complexity parameter controls the number of attributes per "
           "document. The attribute values for the inserted documents are "
           "generated using AQL functions RAND() and RANDOM_TOKEN(). The total "
           "number of documents to be inserted is equal to the value of "
           "--requests.";
  }

  bool isDeprecated() const noexcept override { return true; }
};

}  // namespace arangodb::arangobench
