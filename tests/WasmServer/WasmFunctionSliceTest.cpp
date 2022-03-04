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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "WasmServer/WasmCommon.h"
#include "velocypack/Parser.h"

using namespace arangodb;
using namespace arangodb::wasm;

struct WasmFunctionSliceTest : ::testing::Test {};

TEST_F(WasmFunctionSliceTest, converts_from_velocypack) {
  auto input = VPackParser::fromJson(
      R"({"name": "Anne", "code": "ABC", "isDeterministic": true})");
  auto result = WasmFunction::fromVelocyPack(input->slice());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.get(), (WasmFunction{"Anne", "ABC", true}));
}

TEST_F(WasmFunctionSliceTest, uses_false_as_isDeterministic_default) {
  auto input = VPackParser::fromJson(R"({"name": "Anne", "code": "ABC"})");
  auto result = WasmFunction::fromVelocyPack(input->slice());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.get(), (WasmFunction{"Anne", "ABC", false}));
}

TEST_F(WasmFunctionSliceTest, requires_name_field) {
  auto input = VPackParser::fromJson(R"({"code": "ABC"})");
  auto result = WasmFunction::fromVelocyPack(input->slice());
  EXPECT_TRUE(result.fail());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER);
}

TEST_F(WasmFunctionSliceTest, requires_code_field) {
  auto input = VPackParser::fromJson(R"({"name": "test"})");
  auto result = WasmFunction::fromVelocyPack(input->slice());
  EXPECT_TRUE(result.fail());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER);
}

TEST_F(WasmFunctionSliceTest, gives_error_for_invalid_jason) {
  auto input = VPackParser::fromJson(R"([])");
  auto result = WasmFunction::fromVelocyPack(input->slice());
  EXPECT_TRUE(result.fail());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER);
}

TEST_F(WasmFunctionSliceTest, gives_error_for_invalid_key) {
  auto input = VPackParser::fromJson(
      R"({"name": "Klaus", "code": "XSAWE", "babane": 5})");
  auto result = WasmFunction::fromVelocyPack(input->slice());
  EXPECT_TRUE(result.fail());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER);
}
