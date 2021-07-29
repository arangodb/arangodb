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

  struct CollectionCreationTest : public Benchmark<CollectionCreationTest> {
    static std::string name() { return "collection"; }

    CollectionCreationTest(BenchFeature& arangobench)
      : Benchmark<CollectionCreationTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      //unfortunately, we don't know how many collections we would need to clean up here
      //because the user can choose a timed execution, so there would be no way to find out
      //how many collections would be created in a timed execution.
      return true;
    }

    void tearDown() override {}

    void buildRequest(int threadNumber, size_t threadCounter,
                      size_t globalCounter, BenchmarkOperation::RequestData& requestData) const override {
      requestData.url = "/_api/collection";
      requestData.type = rest::RequestType::POST;
      using namespace arangodb::velocypack;
      requestData.payload.openObject();
      requestData.payload.add("name", Value(_arangobench.collection() + std::to_string(++_counter)));
      requestData.payload.close();
    }

    char const* getDescription() const noexcept override {
      return "creates as many separate (empty) collections as provided in the value of --requests.";
    }

    bool isDeprecated() const noexcept override {
      return false;
    }

    static std::atomic<uint64_t> _counter;

  };

  std::atomic<uint64_t> CollectionCreationTest::_counter(0);

}  // namespace arangodb::arangobench
