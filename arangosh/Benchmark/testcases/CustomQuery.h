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

#ifndef ARANGODB_BENCHMARK_TESTCASES_CUSTOM_QUERY_TEST_H
#define ARANGODB_BENCHMARK_TESTCASES_CUSTOM_QUERY_TEST_H

#include "Benchmark.h"

#include "Basics/ScopeGuard.h"
#include "Basics/files.h"
#include "Basics/StringBuffer.h"

namespace arangodb::arangobench {

struct CustomQueryTest : public Benchmark<CustomQueryTest> {
  static std::string name() { return "custom-query"; }

  CustomQueryTest(BenchFeature& arangobench)
      : Benchmark<CustomQueryTest>(arangobench) {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    _query = _arangobench.customQuery();
    if (_query.empty()) {
      auto file = _arangobench.customQueryFile();
      size_t length;
      auto* p = TRI_SlurpFile(file.c_str(), &length);
      if (p != nullptr) {
        auto guard = scopeGuard([&p]() { TRI_Free(p); });
        _query = std::string(p, length);
      }
    }

    if (_query.empty()) {
      LOG_TOPIC("79cce", FATAL, arangodb::Logger::FIXME)
          << "custom benchmark requires --custom-query or --custom-query-file to "
             "be specified";
      return false;
    }
    
    basics::StringBuffer buff;
    buff.appendText("{\"query\":");
    buff.appendJsonEncoded(_query.c_str(), _query.size());
    buff.appendChar('}');
    _query = buff.toString();
    
    return true;
  }

  void tearDown() override {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) override {
    return std::string("/_api/cursor");
  }

  rest::RequestType type(int const threadNumber, size_t const threadCounter,
                         size_t const globalCounter) override {
    return rest::RequestType::POST;
  }

  char const* payload(size_t* length, int const threadNumber, size_t const threadCounter,
                      size_t const globalCounter, bool* mustFree) override {
    *mustFree = false;
    *length = _query.size();
    return _query.c_str();
  }

 private:
  std::string _query;
};

}  // namespace arangodb::arangobench
#endif
