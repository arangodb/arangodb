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

#ifndef ARANGODB_BENCHMARK_TESTCASES_DOCUMENT_CREATION_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_DOCUMENT_CREATION_TEST_H

#include "Benchmark.h"
#include "helpers.h"

namespace arangodb::arangobench {

struct DocumentCreationTest : public Benchmark<DocumentCreationTest> {
  static std::string name() { return "document"; }

  DocumentCreationTest(BenchFeature& arangobench)
      : Benchmark<DocumentCreationTest>(arangobench),
        _url("/_api/document?collection=" + _arangobench.collection()),
        _buffer(nullptr) {
    uint64_t const n = _arangobench.complexity();

    _buffer = TRI_CreateSizedStringBuffer(4096);
    TRI_AppendCharStringBuffer(_buffer, '{');

    for (uint64_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(_buffer, "\"test");
      TRI_AppendUInt64StringBuffer(_buffer, i);
      TRI_AppendStringStringBuffer(_buffer, "\":\"some test value\"");
      if (i != n) {
        TRI_AppendCharStringBuffer(_buffer, ',');
      }
    }

    TRI_AppendCharStringBuffer(_buffer, '}');

    _length = TRI_LengthStringBuffer(_buffer);
  }

  ~DocumentCreationTest() { TRI_FreeStringBuffer(_buffer); }

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return DeleteCollection(client, _arangobench.collection()) &&
           CreateCollection(client, _arangobench.collection(), 2, _arangobench);
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
    *mustFree = false;
    *length = _length;
    return (char const*)_buffer->_buffer;
  }

  std::string _url;

  TRI_string_buffer_t* _buffer;

  size_t _length;
};

}  // namespace arangodb::arangobench
#endif
