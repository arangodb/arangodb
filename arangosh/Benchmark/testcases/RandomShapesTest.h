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

#ifndef ARANGODB_BENCHMARK_TESTCASES_RANDOM_SHAPES_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_RANDOM_SHAPES_TEST_H

#include "Benchmark.h"
#include "Random/RandomGenerator.h"

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

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      uint64_t const n = _arangobench.complexity();
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 3);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendString2StringBuffer(buffer, key.c_str(), key.size());
      TRI_AppendStringStringBuffer(buffer, "\"");

      uint32_t const t = _randomValue % (uint32_t)(globalCounter + threadNumber + 1);

      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer, (uint64_t)(globalCounter + i));
        if (t % 3 == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        } else if (t % 3 == 1) {
          TRI_AppendStringStringBuffer(buffer, "\":null");
        } else
          TRI_AppendStringStringBuffer(
              buffer,
              "\":\"some bogus string value to fill up the datafile...\"");
      }

      TRI_AppendStringStringBuffer(buffer, "}");

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(buffer);

      return (char const*)ptr;
    } else {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    }
  }

  uint32_t _randomValue;
};

}  // namespace arangodb::arangobench
#endif
