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
#include "Random/RandomGenerator.h"
#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <string>

namespace arangodb::arangobench {

  struct RandomShapesTest : public Benchmark<RandomShapesTest> {
    static std::string name() { return "random-shapes"; }

    RandomShapesTest(BenchFeature& arangobench) : Benchmark<RandomShapesTest>(arangobench) {
      _randomValue = RandomGenerator::interval(UINT32_MAX);
    }

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench);
    }

    void tearDown() override {}

    std::string url(int const threadNumber, size_t const threadCounter,
                    size_t const globalCounter) override {
      size_t const mod = globalCounter % 3;
      if (mod == 0) {
        return std::string("/_api/document?collection=") + _arangobench.collection();
      } else {
        size_t keyId = (size_t)(globalCounter / 3);
        std::string const key = "testkey" + StringUtils::itoa(keyId);
        return std::string("/_api/document/") + _arangobench.collection() +
          std::string("/") + key;
      }
    }

    rest::RequestType type(int const threadNumber, size_t const threadCounter,
                           size_t const globalCounter) override {
      size_t const mod = globalCounter % 3;
      if (mod == 0) {
        return rest::RequestType::POST;
      } else if (mod == 1) {
        return rest::RequestType::GET;
      } else {
        return rest::RequestType::DELETE_REQ;
      }
    }

    void payload(int threadNumber, size_t threadCounter,
                 size_t globalCounter, std::string& buffer) const override {
      size_t mod = globalCounter % 3;
      if (mod == 0) {
        uint64_t n = _arangobench.complexity();
        size_t keyId = static_cast<size_t>(globalCounter / 3);
        using namespace arangodb::velocypack;
        Builder b;
        b.openObject();
        b.add("_key", Value(std::string("testkey") + std::to_string(keyId)));
        uint32_t t = _randomValue % static_cast<uint32_t>(globalCounter + threadNumber + 1);
        for (uint64_t i = 1; i <= n; ++i) {
          std::string fieldName = "value" + std::to_string(static_cast<uint64_t>(globalCounter + i));
          if (t % 3 == 0) {
            b.add(fieldName, Value(true));
          } else if (t % 3 == 1) {
            b.add(fieldName, Value(ValueType::Null));
          } else {
            b.add(fieldName, Value("some bogus string value to fill up the datafile..."));
          }
        }
        b.close();
        buffer = b.toJson();
      }
    }

    uint32_t _randomValue;
  };

}  // namespace arangodb::arangobench
