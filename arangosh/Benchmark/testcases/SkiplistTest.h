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

#ifndef ARANGODB_BENCHMARK_TESTCASES_SKIPLIST_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_SKIPLIST_TEST_H

#include "Benchmark.h"

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

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) override {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + _arangobench.collection());
    } else {
      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + _arangobench.collection() + "/" + key);
    }
  }

  rest::RequestType type(int const threadNumber, size_t const threadCounter,
                         size_t const globalCounter) override {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return rest::RequestType::POST;
    } else if (mod == 1) {
      return rest::RequestType::GET;
    } else if (mod == 2) {
      return rest::RequestType::PATCH;
    } else if (mod == 3) {
      return rest::RequestType::GET;
    } else {
      TRI_ASSERT(false);
      return rest::RequestType::GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    size_t const mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      TRI_AppendStringStringBuffer(buffer, ",\"value\":");
      TRI_AppendUInt32StringBuffer(buffer, (uint32_t)threadCounter);
      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(buffer);

      return (char const*)ptr;
    } else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    } else {
      TRI_ASSERT(false);
      return nullptr;
    }
  }
};

}  // namespace arangodb::arangobench
#endif
