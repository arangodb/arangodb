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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SimpleHttpClient/SimpleHttpClient.h"

#include <map>
#include <memory>
#include <velocypack/Builder.h>

namespace arangodb {

  class BenchFeature;

  namespace arangobench {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief simple interface for benchmark operations
    ////////////////////////////////////////////////////////////////////////////////

    struct BenchmarkOperation {

      struct RequestData { //name to RequestData
        std::string url;
        arangodb::rest::RequestType type;
        arangodb::velocypack::Builder payload;
      };

      //////////////////////////////////////////////////////////////////////////////
      /// @brief ctor, derived class can implemented something sensible
      //////////////////////////////////////////////////////////////////////////////

      explicit BenchmarkOperation(BenchFeature& arangobench) : _arangobench(arangobench) {}

      //////////////////////////////////////////////////////////////////////////////
      /// @brief dtor, derived class can implemented something sensible
      //////////////////////////////////////////////////////////////////////////////

      virtual ~BenchmarkOperation() = default;

      //////////////////////////////////////////////////////////////////////////////
      /// @brief setup
      //////////////////////////////////////////////////////////////////////////////

      virtual bool setUp(arangodb::httpclient::SimpleHttpClient*) = 0;

      //////////////////////////////////////////////////////////////////////////////
      /// @brief teardown
      //////////////////////////////////////////////////////////////////////////////

      virtual void tearDown() = 0;

      //////////////////////////////////////////////////////////////////////////////
      /// @brief build the HTTP request and set it's values (url, type and payload) inside RequestData.
      //  The caller must provide it and clean its values before the call
      //////////////////////////////////////////////////////////////////////////////
  
      virtual void buildRequest(int threadNumber, size_t threadCounter, size_t globalCounter, RequestData&) const;

      using BenchmarkFactory = std::function<std::unique_ptr<BenchmarkOperation>(BenchFeature&)>;

      /// @brief return the the map of all available benchmarks
      static std::map<std::string, BenchmarkFactory>& allBenchmarks();

      /// @brief register a benchmark with the given name and factory function
      static void registerBenchmark(std::string name, BenchmarkFactory factory);

      /// @brief return the benchmark for a name
      static std::unique_ptr<BenchmarkOperation> createBenchmark(std::string const& name, BenchFeature& arangobench);
  
      /// @brief prints the description of the testcase
      virtual char const* getDescription() const noexcept = 0;
      
      /// @brief returns wether testcase is deprecated or not 
      virtual bool isDeprecated() const noexcept = 0;

      protected:
      arangodb::BenchFeature& _arangobench;
    };

  }  // namespace arangobench
}  // namespace arangodb

