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
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "Basics/ResultT.h"

using namespace arangodb;
using namespace arangodb::wasm;

struct WasmFunctionCreation : public ::testing::Test {
  void expectWasmFunction(std::string&& string, WasmFunction&& wasmFunction) {
    auto result = arangodb::wasm::velocypackToWasmFunction(
        VPackParser::fromJson(string)->slice());
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.get(), wasmFunction);
  }

  void expectError(std::string&& string) {
    auto result = arangodb::wasm::velocypackToWasmFunction(
        VPackParser::fromJson(string)->slice());
    EXPECT_TRUE(result.fail());
    EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER);
  }
};

TEST_F(WasmFunctionCreation,
       wasm_function_is_created_from_velocypack_with_byte_array) {
  expectWasmFunction(
      R"({"name": "Anne", "code": [1, 2, 255], "isDeterministic": true})",
      WasmFunction{"Anne", {{1, 2, 255}}, true});
}

TEST_F(WasmFunctionCreation,
       WasmFunction_is_created_from_velocypack_with_base64_string) {
  expectWasmFunction(
      R"({"name": "Anne", "code": "AQL/", "isDeterministic": true})",
      WasmFunction{"Anne", {{1, 2, 255}}, true});
}

TEST_F(WasmFunctionCreation, uses_false_as_isDeterministic_default) {
  expectWasmFunction(R"({"name": "Anne", "code": [43, 8]})",
                     WasmFunction{"Anne", {{43, 8}}, false});
}

TEST_F(WasmFunctionCreation, returns_error_when_name_is_not_given) {
  expectError(R"({"code": [43, 8]})");
}

TEST_F(WasmFunctionCreation, returns_error_when_code_is_not_given) {
  expectError(R"({"name": "test"})");
}

TEST_F(WasmFunctionCreation, returns_error_when_velocypack_is_not_an_object) {
  expectError(R"([])");
}

TEST_F(WasmFunctionCreation, gives_error_for_unknown_key) {
  expectError(R"({"name": "test", "code": [8, 9, 0], "banane": 5})");
}

TEST_F(WasmFunctionCreation, gives_error_when_name_is_not_a_string) {
  expectError(R"({"name": 1, "code": [0]})");
}

TEST_F(WasmFunctionCreation, gives_error_when_code_is_a_number) {
  expectError(R"({"name": "some_function", "code": 1})");
}

TEST_F(WasmFunctionCreation,
       gives_error_when_code_byte_array_includes_not_only_bytes) {
  expectError(R"({"name": "some_function", "code": [1000]})");
}

TEST_F(WasmFunctionCreation,
       gives_error_when_code_string_is_not_a_base64_string) {
  expectError(R"({"name": "some_function", "code": "121ü"})");
}

TEST_F(WasmFunctionCreation,
       gives_error_when_isDeterministic_is_not_a_boolean) {
  expectError(
      R"({"name": "some_function", "code": [0, 1], "isDeterministic":
      "ABC"})");
}

TEST(WasmFunctionConversion, converts_wasm_function_to_velocypack) {
  VPackBuilder velocypackBuilder;
  arangodb::wasm::wasmFunctionToVelocypack(
      WasmFunction{"function_name", {{3, 233}}, false}, velocypackBuilder);
  EXPECT_EQ(
      velocypackBuilder.toJson(),
      VPackParser::fromJson(
          R"({"name": "function_name", "code": [3, 233], "isDeterministic": false})")
          ->slice()
          .toJson());
}

TEST(ParameterCreation, extracts_parameters_from_velocypack) {
  auto velocypack = VPackParser::fromJson(R"({"a": 3, "b": 982})")->slice();
  auto result = arangodb::wasm::velocypackToParameters(velocypack);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.get(), (Parameters{3, 982}));
}
