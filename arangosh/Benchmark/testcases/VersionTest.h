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
#include <velocypack/ValueType.h>
#include <string>

namespace arangodb::arangobench {

struct VersionTest : public Benchmark<VersionTest> {
  static std::string name() { return "version"; }

  VersionTest(BenchFeature& arangobench) 
      : Benchmark<VersionTest>(arangobench),
        _url("/_api/version") {}

  bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
    return true;
  }

  void tearDown() override {}

  void buildRequest(size_t threadNumber, size_t threadCounter,
                    size_t globalCounter, BenchmarkOperation::RequestData& requestData) const override {
    requestData.url = _url;
    requestData.type = rest::RequestType::GET;
  }

  char const* getDescription() const noexcept override {
    return "queries the server version and then instantly returns. In a cluster, this means that Coordinators instantly respond to the requests without ever accessing DB-Servers. This test can be used to establish a baseline for single server or Coordinator throughput. The --complexity parameter is not used.";
  }

  bool isDeprecated() const noexcept override {
    return false;
  }

 private:
  std::string const _url;
};

}  // namespace arangodb::arangobench
