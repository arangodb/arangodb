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

  struct ShapesAppendTest : public Benchmark<ShapesAppendTest> {
    static std::string name() { return "shapes-append"; }

    ShapesAppendTest(BenchFeature& arangobench) : Benchmark<ShapesAppendTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench);
    }

    void tearDown() override {}

    std::string url(int const threadNumber, size_t const threadCounter,
        size_t const globalCounter) override {
      size_t const mod = globalCounter % 2;

      if (mod == 0) {
        return std::string("/_api/document?collection=" + _arangobench.collection());
      } else {
        size_t keyId = (size_t)(globalCounter / 2);
        std::string const key = "testkey" + StringUtils::itoa(keyId);

        return std::string("/_api/document/" + _arangobench.collection() + "/" + key);
      }
    }

    rest::RequestType type(int const threadNumber, size_t const threadCounter,
                           size_t const globalCounter) override {
      size_t const mod = globalCounter % 2;

      if (mod == 0) {
        return rest::RequestType::POST;
      }
      return rest::RequestType::GET;
    }

    void payload(int threadNumber, size_t threadCounter,
                 size_t globalCounter, std::string& buffer) const override {
      size_t const mod = globalCounter % 2;
      if (mod == 0) {
        uint64_t const n = _arangobench.complexity();
        using namespace arangodb::velocypack;
        Builder b;
        b.openObject();
        size_t keyId = (size_t)(globalCounter / 2);
        b.add("_key", Value(std::string("testkey") + std::to_string(keyId)));
        for (uint64_t i = 1; i <= n; ++i) {
          uint64_t mod = _arangobench.operations() / 10;
          if (mod < 100) {
            mod = 100;
          }
          b.add(std::string("value") + std::to_string(static_cast<uint64_t>((globalCounter + i) % mod)), Value("some bogus string value to fill up the datafile..."));
        }
        b.close();
        buffer = b.toJson();
      } 
    }

  };

}  // namespace arangodb::arangobench
