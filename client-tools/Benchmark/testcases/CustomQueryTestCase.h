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

#include "Basics/ScopeGuard.h"
#include "Basics/files.h"
#include "Basics/StringBuffer.h"
#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <string>

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
        auto guard = scopeGuard([&p]() noexcept { TRI_Free(p); });
        _query = std::string(p, length);
      }
    }

    if (_query.empty()) {
      LOG_TOPIC("79cce", FATAL, arangodb::Logger::BENCH)
          << "custom benchmark requires --custom-query or --custom-query-file "
             "to "
             "be specified";
      return false;
    }
    _queryBindVars = _arangobench.customQueryBindVars();

    return true;
  }

  void tearDown() override {}

  void buildRequest(
      size_t threadNumber, size_t threadCounter, size_t globalCounter,
      BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = "/_api/cursor";
    requestData.type = rest::RequestType::POST;
    using namespace arangodb::velocypack;
    requestData.payload.openObject();
    requestData.payload.add("query", Value(_query));
    if (_queryBindVars != nullptr) {
      requestData.payload.add("bindVars", _queryBindVars->slice());
    }
    requestData.payload.close();
  }
  char const* getDescription() const noexcept override {
    return "executes a custom AQL query, that can be specified either via the "
           "--custom-query option or be read from a file specified via the "
           "--custom-query-file option. The query will be executed as many "
           "times as the value of --requests. The --complexity parameter is "
           "not used.";
  }

  bool isDeprecated() const noexcept override { return false; }

 private:
  std::string _query;
  std::shared_ptr<VPackBuilder> _queryBindVars;
};

}  // namespace arangodb::arangobench
