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

struct ShapesTest : public Benchmark<ShapesTest> {
  static std::string name() { return "shapes"; }

  ShapesTest(BenchFeature& arangobench) : Benchmark<ShapesTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
  }

  void tearDown() override {}

  void buildRequest(
      size_t threadNumber, size_t threadCounter, size_t globalCounter,
      BenchmarkOperation::RequestData& requestData) const override {
    size_t keyId = static_cast<size_t>(globalCounter / 3);
    std::string const key = "testkey" + StringUtils::itoa(keyId);
    size_t const mod = globalCounter % 3;
    if (mod == 0) {
      requestData.url = std::string("/_api/document?collection=" +
                                    _arangobench.collection()) +
                        "&silent=true";
      requestData.type = rest::RequestType::POST;

      using namespace arangodb::velocypack;
      requestData.payload.openObject();
      requestData.payload.add(StaticStrings::KeyString, Value(key));
      uint64_t n = _arangobench.complexity();
      for (uint64_t i = 1; i <= n; ++i) {
        uint64_t mod = _arangobench.operations() / 10;
        if (mod < 100) {
          mod = 100;
        }
        requestData.payload.add(
            std::string("value") + std::to_string(static_cast<uint64_t>(
                                       (globalCounter + i) % mod)),
            Value("some bogus string value to fill up the datafile..."));
      }
      requestData.payload.close();
    } else {
      requestData.url = std::string("/_api/document/" +
                                    _arangobench.collection() + "/" + key);
      requestData.type =
          (mod == 1) ? rest::RequestType::GET : rest::RequestType::DELETE_REQ;
    }
  }

  char const* getDescription() const noexcept override {
    return "will perform a mix of insert, get and remove operations for "
           "documents with different, but predictable attribute names. 33% of "
           "the operations will be single-document inserts, 33% of the "
           "operations will be single-document reads, and 33% of the "
           "operations are single-document removals. There will be a total of "
           "--requests operations. The --complexity parameter can be used to "
           "control the number of attributes for the inserted documents.";
  }

  bool isDeprecated() const noexcept override { return true; }
};

}  // namespace arangodb::arangobench
