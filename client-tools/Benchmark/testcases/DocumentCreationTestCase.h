////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include <string_view>

namespace arangodb::arangobench {

struct DocumentCreationTest : public Benchmark<DocumentCreationTest> {
  static std::string name() { return "document"; }
  DocumentCreationTest(BenchFeature& arangobench)
      : Benchmark<DocumentCreationTest>(arangobench),
        _url(std::string("/_api/document?collection=") +
             _arangobench.collection() + "&silent=true") {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
  }

  void tearDown() override {}

  void buildRequest(
      size_t threadNumber, size_t threadCounter, size_t globalCounter,
      BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = _url;
    requestData.type = rest::RequestType::POST;
    using namespace arangodb::velocypack;
    requestData.payload.openObject();
    uint64_t const n = _arangobench.complexity();
    for (uint64_t i = 1; i <= n; ++i) {
      requestData.payload.add(std::string("test") + std::to_string(i),
                              Value(std::string_view("some test value")));
    }
    requestData.payload.close();
  }

  char const* getDescription() const noexcept override {
    return "performs single-document insert operations via the specialized "
           "insert API (in contrast to performing inserts via generic AQL). "
           "The --complexity parameter controls the number of attributes per "
           "document. The attribute values for the inserted documents will be "
           "hard-coded. The total number of documents to be inserted is equal "
           "to the value of --requests.";
  }

  bool isDeprecated() const noexcept override { return false; }

 private:
  std::string const _url;
};

}  // namespace arangodb::arangobench
