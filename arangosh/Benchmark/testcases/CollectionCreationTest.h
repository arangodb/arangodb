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

#ifndef ARANGODB_BENCHMARK_TESTCASES_COLLECTION_CREATION_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_COLLECTION_CREATION_TEST_H

#include "Benchmark.h"

namespace arangodb::arangobench {

struct CollectionCreationTest : public Benchmark<CollectionCreationTest> {
  static std::string name() { return "collection"; }

  CollectionCreationTest(BenchFeature& arangobench)
      : Benchmark<CollectionCreationTest>(arangobench), _url("/_api/collection") {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return true;
  }

  void tearDown() override {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) override {
    return _url;
  }

  rest::RequestType type(int const threadNumber, size_t const threadCounter,
                         size_t const globalCounter) override {
    return rest::RequestType::POST;
  }

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    TRI_string_buffer_t* buffer;
    char* data;

    buffer = TRI_CreateSizedStringBuffer(64);
    if (buffer == nullptr) {
      return nullptr;
    }
    TRI_AppendStringStringBuffer(buffer, "{\"name\":\"");
    TRI_AppendStringStringBuffer(buffer, _arangobench.collection().c_str());
    TRI_AppendUInt64StringBuffer(buffer, ++_counter);
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);

    // this will free the string buffer frame, but not the string
    data = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(buffer);

    *mustFree = true;
    return (char const*)data;
  }

  static std::atomic<uint64_t> _counter;

  std::string _url;
};

std::atomic<uint64_t> CollectionCreationTest::_counter(0);

}  // namespace arangodb::arangobench
#endif