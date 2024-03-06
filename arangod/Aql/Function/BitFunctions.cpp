////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <cstdint>
#include <optional>

using namespace arangodb;

namespace arangodb::aql {
namespace {

template<typename T>
std::optional<T> bitOperationValue(VPackSlice input) {
  if (input.isNumber() && input.getNumber<double>() >= 0) {
    uint64_t result = input.getNumber<uint64_t>();
    if (result <=
        static_cast<uint64_t>(functions::bitFunctionsMaxSupportedValue)) {
      TRI_ASSERT(result <= UINT32_MAX);
      // value is valid
      return std::optional<T>{static_cast<T>(result)};
    }
  }
  // value is not valid
  return {};
}

template<typename T1, typename T2>
std::optional<std::pair<T1, T2>> binaryBitFunctionParameters(
    aql::functions::VPackFunctionParametersView parameters) {
  AqlValue const& value1 =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (value1.isNumber()) {
    auto result1 = bitOperationValue<T1>(value1.slice());
    if (result1.has_value()) {
      AqlValue const& value2 =
          aql::functions::extractFunctionParameterValue(parameters, 1);
      if (value2.isNumber()) {
        auto result2 = bitOperationValue<T2>(value2.slice());
        if (result2.has_value() &&
            result2.value() <=
                static_cast<T2>(functions::bitFunctionsMaxSupportedBits)) {
          return std::optional<std::pair<T1, T2>>{
              {result1.value(), result2.value()}};
        }
      }
    }
  }

  return {};
}

AqlValue handleBitOperation(
    ExpressionContext* expressionContext, AstNode const& node,
    aql::functions::VPackFunctionParametersView parameters,
    std::function<uint64_t(uint64_t, uint64_t)> const& cb) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL == node.type);

  // extract AQL function name
  auto const* impl = static_cast<aql::Function*>(node.getData());
  TRI_ASSERT(impl != nullptr);

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (parameters.size() == 2) {
    // expect two numbers as individual parameters
    if (value.isNumber()) {
      auto result1 = bitOperationValue<uint64_t>(value.slice());
      if (result1.has_value()) {
        AqlValue const& value2 =
            aql::functions::extractFunctionParameterValue(parameters, 1);
        if (value2.isNumber()) {
          auto result2 = bitOperationValue<uint64_t>(value2.slice());
          if (result2.has_value()) {
            uint64_t result = cb(result1.value(), result2.value());
            return AqlValue(AqlValueHintUInt(result));
          }
        }
      }
    }
    aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                   impl->name.c_str());
    return AqlValue(AqlValueHintNull());
  }

  // expect an array of numbers (with null values being ignored)
  if (!value.isArray()) {
    aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                   impl->name.c_str());
    return AqlValue(AqlValueHintNull());
  }

  bool first = true;
  uint32_t result = 0;

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValueMaterializer materializer(vopts);
  VPackSlice s = materializer.slice(value);
  for (VPackSlice v : VPackArrayIterator(s)) {
    // skip null values in the input
    if (v.isNull()) {
      continue;
    }

    auto currentValue = bitOperationValue<uint64_t>(v);
    if (!currentValue.has_value()) {
      aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                     impl->name.c_str());
      return AqlValue(AqlValueHintNull());
    }
    if (first) {
      first = false;
      result = currentValue.value();
    } else {
      result = cb(result, currentValue.value());
    }
  }

  if (first) {
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(AqlValueHintUInt(result));
}

}  // namespace

/// @brief function BIT_AND
AqlValue functions::BitAnd(ExpressionContext* expressionContext,
                           AstNode const& node,
                           VPackFunctionParametersView parameters) {
  return handleBitOperation(
      expressionContext, node, parameters,
      [](uint64_t value1, uint64_t value2) { return value1 & value2; });
}

/// @brief function BIT_OR
AqlValue functions::BitOr(ExpressionContext* expressionContext,
                          AstNode const& node,
                          VPackFunctionParametersView parameters) {
  return handleBitOperation(
      expressionContext, node, parameters,
      [](uint64_t value1, uint64_t value2) { return value1 | value2; });
}

/// @brief function BIT_XOR
AqlValue functions::BitXOr(ExpressionContext* expressionContext,
                           AstNode const& node,
                           VPackFunctionParametersView parameters) {
  return handleBitOperation(
      expressionContext, node, parameters,
      [](uint64_t value1, uint64_t value2) { return value1 ^ value2; });
}

/// @brief function BIT_NEGATE
AqlValue functions::BitNegate(ExpressionContext* expressionContext,
                              AstNode const& node,
                              VPackFunctionParametersView parameters) {
  auto result = binaryBitFunctionParameters<uint64_t, uint64_t>(parameters);
  if (result.has_value()) {
    auto [testee, width] = result.value();
    // mask the lower bits of the result with up to 32 active bits
    uint64_t value = (~testee) & ((uint64_t(1) << width) - 1);
    return AqlValue(AqlValueHintUInt(value));
  }

  static char const* AFN = "BIT_NEGATE";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function BIT_TEST
AqlValue functions::BitTest(ExpressionContext* expressionContext,
                            AstNode const& node,
                            VPackFunctionParametersView parameters) {
  auto result = binaryBitFunctionParameters<uint64_t, uint64_t>(parameters);
  if (result.has_value()) {
    auto [testee, index] = result.value();
    if (index < functions::bitFunctionsMaxSupportedBits) {
      return AqlValue(AqlValueHintBool((testee & (uint64_t(1) << index)) != 0));
    }
  }

  static char const* AFN = "BIT_TEST";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function BIT_SHIFT_LEFT
AqlValue functions::BitShiftLeft(ExpressionContext* expressionContext,
                                 AstNode const& node,
                                 VPackFunctionParametersView parameters) {
  auto result = binaryBitFunctionParameters<uint64_t, uint64_t>(parameters);
  if (result.has_value()) {
    auto [testee, shift] = result.value();
    AqlValue const& value =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (value.isNumber()) {
      auto width = bitOperationValue<uint64_t>(value.slice());
      if (width.has_value() &&
          width.value() <=
              static_cast<uint64_t>(functions::bitFunctionsMaxSupportedBits)) {
        // mask the lower bits of the result with up to 32 active bits
        return AqlValue(AqlValueHintUInt((testee << shift) &
                                         ((uint64_t(1) << width.value()) - 1)));
      }
    }
  }

  static char const* AFN = "BIT_SHIFT_LEFT";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function BIT_SHIFT_RIGHT
AqlValue functions::BitShiftRight(ExpressionContext* expressionContext,
                                  AstNode const& node,
                                  VPackFunctionParametersView parameters) {
  auto result = binaryBitFunctionParameters<uint64_t, uint64_t>(parameters);
  if (result.has_value()) {
    auto [testee, shift] = result.value();
    AqlValue const& value =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (value.isNumber()) {
      auto width = bitOperationValue<uint32_t>(value.slice());
      if (width.has_value() &&
          width.value() <=
              static_cast<uint64_t>(functions::bitFunctionsMaxSupportedBits)) {
        // mask the lower bits of the result with up to 32 active bits
        return AqlValue(AqlValueHintUInt((testee >> shift) &
                                         ((uint64_t(1) << width.value()) - 1)));
      }
    }
  }

  static char const* AFN = "BIT_SHIFT_RIGHT";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function BIT_POPCOUNT
AqlValue functions::BitPopcount(ExpressionContext* expressionContext,
                                AstNode const& node,
                                VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (value.isNumber()) {
    VPackSlice v = value.slice();
    auto result = bitOperationValue<uint64_t>(v);
    if (result.has_value()) {
      uint64_t x = result.value();
      size_t const count = std::popcount(x);
      return AqlValue(AqlValueHintUInt(count));
    }
  }

  static char const* AFN = "BIT_POPCOUNT";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

AqlValue functions::BitConstruct(ExpressionContext* expressionContext,
                                 AstNode const& node,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "BIT_CONSTRUCT";
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (value.isArray()) {
    transaction::Methods* trx = &expressionContext->trx();
    auto* vopts = &trx->vpackOptions();
    AqlValueMaterializer materializer(vopts);
    VPackSlice s = materializer.slice(value);

    uint64_t result = 0;
    for (VPackSlice v : VPackArrayIterator(s)) {
      auto currentValue = bitOperationValue<uint64_t>(v);
      bool valid = currentValue.has_value();
      if (valid) {
        if (currentValue.value() >= functions::bitFunctionsMaxSupportedBits) {
          valid = false;
        }
      }
      if (!valid) {
        registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }

      result |= uint64_t(1) << currentValue.value();
    }

    return AqlValue(AqlValueHintUInt(result));
  }

  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

AqlValue functions::BitDeconstruct(ExpressionContext* expressionContext,
                                   AstNode const& node,
                                   VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (value.isNumber()) {
    VPackSlice v = value.slice();
    auto num = bitOperationValue<uint64_t>(v);
    if (num.has_value()) {
      transaction::Methods* trx = &expressionContext->trx();
      transaction::BuilderLeaser builder(trx);
      builder->openArray();
      uint32_t compare = 1;
      for (uint32_t i = 0; i < functions::bitFunctionsMaxSupportedBits; ++i) {
        // potential improvement: use C++20's std::countr_zero()
        if ((num.value() & compare) != 0) {
          builder->add(VPackValue(i));
        }
        compare <<= 1;
      }
      builder->close();
      return AqlValue(builder->slice(), builder->size());
    }
  }

  char const* AFN = "BIT_DECONSTRUCT";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function BIT_TO_STRING
AqlValue functions::BitToString(ExpressionContext* expressionContext,
                                AstNode const& node,
                                VPackFunctionParametersView parameters) {
  auto result = binaryBitFunctionParameters<uint64_t, uint64_t>(parameters);
  if (result.has_value()) {
    auto [testee, index] = result.value();
    TRI_ASSERT(index <= functions::bitFunctionsMaxSupportedBits);

    char buffer[functions::bitFunctionsMaxSupportedBits];
    char* p = &buffer[0];

    if (index > 0) {
      uint64_t compare = uint64_t(1) << (index - 1);
      while (compare > 0) {
        *p = (testee & compare) ? '1' : '0';
        ++p;
        compare >>= 1;
      }
    }

    return AqlValue(std::string_view{&buffer[0], p});
  }

  static char const* AFN = "BIT_TO_STRING";
  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function BIT_FROM_STRING
AqlValue functions::BitFromString(ExpressionContext* expressionContext,
                                  AstNode const& node,
                                  VPackFunctionParametersView parameters) {
  static char const* AFN = "BIT_FROM_STRING";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (value.isString()) {
    std::string_view v = value.slice().stringView();
    char const* p = v.data();
    char const* e = p + v.size();

    if (static_cast<uint64_t>(e - p) <=
        functions::bitFunctionsMaxSupportedBits) {
      uint64_t result = 0;
      while (p != e) {
        char c = *p;
        if (c == '1') {
          /* only the 1s are interesting for us */
          result += (static_cast<uint64_t>(1) << (e - p - 1));
        } else if (c != '0') {
          // invalid input. abort
          registerInvalidArgumentWarning(expressionContext, AFN);
          return AqlValue(AqlValueHintNull());
        }
        ++p;
      }
      return AqlValue(AqlValueHintUInt(result));
    }
  }

  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

}  // namespace arangodb::aql
