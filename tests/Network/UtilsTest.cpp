////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Network/Utils.cpp
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Basics/StaticStrings.h"
#include "Network/Utils.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>

#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::network;

TEST_CASE("network utils", "[network]") {

  
  SECTION("resolve destinations") {
    
  }
  
  SECTION("errorFromBody") {
    const char* str = "{\"errorNum\":1337, \"errorMessage\":\"abc\"}";
    auto res = network::errorFromBody(VPackParser::fromJson(str), 0);
    CHECK(res.errorNumber() == 1337);
    CHECK(res.errorMessage() == "abc");
  }
  
  SECTION("errorCodeFromBody") {
    const char* str = "{\"errorNum\":1337, \"errorMessage\":\"abc\"}";
    auto body = VPackParser::fromJson(str);
    auto res = network::errorCodeFromBody(body->slice());
    CHECK(res == 1337);
  }
  
  SECTION("errorCodesFromHeaders") {
    network::Headers headers;
    headers[StaticStrings::ErrorCodes] = "{\"5\":2}";
    
    std::unordered_map<int, size_t> errorCounter;
    network::errorCodesFromHeaders(headers, errorCounter, true);
    CHECK(errorCounter.size() == 1);
    CHECK(errorCounter.begin()->first == 5);
    CHECK(errorCounter.begin()->second == 2);
  }
  
}

