////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/StaticStrings.h"
#include "Network/Utils.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>

#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::network;

TEST(NetworkUtilsTest, errorFromBody) {
  const char* str = "{\"errorNum\":1337, \"errorMessage\":\"abc\"}";
  auto res = network::resultFromBody(VPackParser::fromJson(str), 0);
  ASSERT_EQ(res.errorNumber(), 1337);
  ASSERT_EQ(res.errorMessage(), "abc");
}

TEST(NetworkUtilsTest, errorCodeFromBody) {
  const char* str = "{\"errorNum\":1337, \"errorMessage\":\"abc\"}";
  auto body = VPackParser::fromJson(str);
  auto res = network::errorCodeFromBody(body->slice());
  ASSERT_EQ(res, 1337);
}

TEST(NetworkUtilsTest, errorCodesFromHeaders) {
  network::Headers headers;
  headers[StaticStrings::ErrorCodes] = "{\"5\":2}";
  
  std::unordered_map<int, size_t> errorCounter;
  network::errorCodesFromHeaders(headers, errorCounter, true);
  ASSERT_EQ(errorCounter.size(), 1);
  ASSERT_EQ(errorCounter.begin()->first, 5);
  ASSERT_EQ(errorCounter.begin()->second, 2);
}
