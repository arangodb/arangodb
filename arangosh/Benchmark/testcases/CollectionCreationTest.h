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

    void payload(int threadNumber, size_t threadCounter, 
                 size_t globalCounter, std::string& buffer) const override {
      using namespace arangodb::velocypack;
      Builder b;
      b.openObject();
      b.add("name", Value(_arangobench.collection() + std::to_string(++_counter)));
      b.close();
      buffer = b.toJson();
    }

    static std::atomic<uint64_t> _counter;

    std::string _url;
  };

  std::atomic<uint64_t> CollectionCreationTest::_counter(0);

}  // namespace arangodb::arangobench
