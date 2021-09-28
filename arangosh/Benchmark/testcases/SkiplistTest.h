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

  struct SkiplistTest : public Benchmark<SkiplistTest> {
    static std::string name() { return "skiplist"; }

    SkiplistTest(BenchFeature& arangobench) : Benchmark<SkiplistTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench) &&
        CreateIndex(client, _arangobench.collection(), "skiplist",
            "[\"value\"]");
    }

    void tearDown() override {}

    void buildRequest(int threadNumber, size_t threadCounter,
                      size_t globalCounter, BenchmarkOperation::RequestData& requestData) const override {
      size_t keyId = static_cast<size_t>(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      size_t const mod = globalCounter % 4;
      if (mod == 0) {
        requestData.url = std::string("/_api/document?collection=" + _arangobench.collection()) + "&silent=true";
        requestData.type = rest::RequestType::POST;
      } else {
        requestData.url = std::string("/_api/document/" + _arangobench.collection() + "/" + key);
        requestData.type = (mod == 2) ? rest::RequestType::PATCH : rest::RequestType::GET;
      }
      if (mod == 0 || mod == 2) {
        using namespace arangodb::velocypack;
        requestData.payload.openObject();
        requestData.payload.add(StaticStrings::KeyString, Value(key));
        requestData.payload.add("value", Value(static_cast<uint32_t>(threadCounter)));
        requestData.payload.close();
      }
    }

    char const* getDescription() const noexcept override {
      return "identical to the hash test case nowadays.";
    }

    bool isDeprecated() const noexcept override {
      return true;
    }

  };

}  // namespace arangodb::arangobench
