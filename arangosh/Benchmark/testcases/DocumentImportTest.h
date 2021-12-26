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

struct DocumentImportTest : public Benchmark<DocumentImportTest> {
  static std::string name() { return "import-document"; }

  DocumentImportTest(BenchFeature& arangobench)
      : Benchmark<DocumentImportTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
  }

  void tearDown() override {}

  void buildRequest(size_t threadNumber, size_t threadCounter, size_t globalCounter,
                    BenchmarkOperation::RequestData& requestData) const override {
    requestData.type = rest::RequestType::POST;
    requestData.url = std::string("/_api/import?collection=" + _arangobench.collection() +
                                  "&type=documents");
    uint64_t const n = _arangobench.complexity();
    using namespace arangodb::velocypack;
    requestData.payload.openArray();
    for (uint64_t i = 0; i < n; ++i) {
      requestData.payload.openObject();
      requestData.payload.add("key1", Value(i));
      requestData.payload.add("key2", Value(i));
      requestData.payload.close();
    }
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "performs multi-document imports using the specialized import API "
           "(in contrast to performing inserts via generic AQL). Each inserted "
           "document will have two attributes. The --complexity parameter "
           "controls the number of documents per import request. The total "
           "number of documents to be inserted is equal to the value of "
           "--requests times the value of --complexity.";
  }

  bool isDeprecated() const noexcept override { return false; }
};

}  // namespace arangodb::arangobench
