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
#include "helpers.h"
#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <string>

namespace arangodb::arangobench {

  struct StreamCursorTest : public Benchmark<StreamCursorTest> {
    static std::string name() { return "stream-cursor"; }

    StreamCursorTest(BenchFeature& arangobench) : Benchmark<StreamCursorTest>(arangobench) {}

    bool setUp(arangodb::httpclient::SimpleHttpClient* client) override {
      return DeleteCollection(client, _arangobench.collection()) &&
        CreateCollection(client, _arangobench.collection(), 2, _arangobench);
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

    void payload(int threadNumber, size_t threadCounter,
                 size_t globalCounter, std::string& buffer) const override {
      size_t mod = globalCounter % 2;
      uint64_t n = _arangobench.complexity();
      using namespace arangodb::velocypack;
      Builder b;
      b.openObject();
      std::string values;
      if (globalCounter == 0) {
        values = "FOR i IN 1..500 INSERT { \"_key\": TO_STRING(i)";
        for (uint64_t i = 1; i <= n; ++i) {
          values += ", \"value" + std::to_string(i) + "\": true";
        }
        values += "} INTO " + _arangobench.collection();
        b.add("query", Value(values));
      } else if (mod == 0) {
        values = "UPDATE { \"_key\": \"1\" } WITH { \"foo\": 1";
        for (uint64_t i = 1; i <= n; ++i) {
          values += ", \"value" + std::to_string(i) + "\": true";
        }
        values += " } INTO " + _arangobench.collection() + std::string(" OPTIONS { \"ignoreErrors\": true } ");
        b.add("query", Value(values));
      } else {
        values = "FOR doc IN " + _arangobench.collection() 
          += std::string(" RETURN doc");
        b.add("query", Value(values));
        b.add("options", Value(ValueType::Object));
        b.add("stream", Value(true));
        b.close();
      }
      b.close();
      buffer = b.toJson();
    }

  };

}  // namespace arangodb::arangobench
