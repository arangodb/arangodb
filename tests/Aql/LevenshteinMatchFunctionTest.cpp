////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {

AqlValue evaluate(AqlValue const& lhs,
                  AqlValue const& rhs,
                  AqlValue const& distance,
                  AqlValue const* transpositions = nullptr) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();
  fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([](int, char const*){ });

  fakeit::Mock<transaction::Context> trxCtxMock;
  fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysDo([](){
    static VPackOptions options;
    return &options;
  });
  transaction::Context& trxCtx = trxCtxMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysDo([&trxCtx](){ return &trxCtx; });
  transaction::Methods& trx = trxMock.get();

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{arena};
  params.reserve(3 + (transpositions ? 2 : 0));
  params.emplace_back(lhs);
  params.emplace_back(rhs);
  params.emplace_back(distance);
  if (transpositions) {
    params.emplace_back(*transpositions);
    params.emplace_back(VPackSlice::nullSlice()); // redundant argument
  }

  AqlValue result = Functions::LevenshteinMatch(&expressionContext, &trx, params);

  // explicitly call cleanup on the mocked transaction context because
  // for whatever reason the context's dtor does not fire and thus we
  // risk leaking memory (only during tests)
  trxCtx.cleanup();

  return result;
}

void assertLevenshteinMatchFail(AqlValue const& lhs,
                                AqlValue const& rhs,
                                AqlValue const& distance,
                                AqlValue const* transpositions = nullptr) {
  ASSERT_TRUE(evaluate(lhs, rhs, distance, transpositions).isNull(false));
  ASSERT_TRUE(evaluate(rhs, lhs, distance, transpositions).isNull(false));
}

void assertLevenshteinMatch(bool expectedValue,
                            AqlValue const& lhs,
                            AqlValue const& rhs,
                            AqlValue const& distance,
                            AqlValue const* transpositions = nullptr) {
  auto assertLevenshteinMatchValue = [](
      bool expectedValue,
      AqlValue const& lhs,
      AqlValue const& rhs,
      AqlValue const& distance,
      AqlValue const* transpositions = nullptr) {
    auto const value = evaluate(lhs, rhs, distance, transpositions);
    ASSERT_TRUE(value.isBoolean());
    ASSERT_EQ(expectedValue, value.toBoolean());
  };

  assertLevenshteinMatchValue(expectedValue, lhs, rhs, distance, transpositions);
  assertLevenshteinMatchValue(expectedValue, rhs, lhs, distance, transpositions);
}

}

TEST(LevenshteinMatchFunctionTest, test) {
  AqlValue const Damerau{AqlValueHintBool{true}};
  AqlValue const Levenshtein{AqlValueHintBool{false}};
  AqlValue const InvalidNull{AqlValueHintNull{}};
  AqlValue const InvalidInt{AqlValueHintInt{1}};
  AqlValue const InvalidArray{AqlValueHintEmptyArray{}};
  AqlValue const InvalidObject{AqlValueHintEmptyObject{}};

  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{0}), &Levenshtein);
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{0}));
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{0}), &Damerau);
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{1}), &Levenshtein);
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{1}));
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{1}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{2}), &Levenshtein);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{2}));
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{2}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintDouble{2}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintDouble{2.5}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{3}), &Levenshtein);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{3}));
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{3}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{4}), &Levenshtein);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{4}));
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{4}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}));
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}), &Damerau);
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{6}));
  assertLevenshteinMatch(true,  AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{6}), &Damerau);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintNull{}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Levenshtein);
  assertLevenshteinMatch(false, AqlValue(AqlValueHintEmptyArray{}), AqlValue("aa"), AqlValue(AqlValueHintInt{1}), &Levenshtein);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintEmptyObject{}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Levenshtein);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintInt{1}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Levenshtein);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintDouble{1.}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Levenshtein);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintBool{false}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Levenshtein);
  assertLevenshteinMatch(false, AqlValue(AqlValueHintNull{}), AqlValue("aa"), AqlValue(AqlValueHintInt{1}), &Damerau);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintEmptyArray{}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Damerau);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintEmptyObject{}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Damerau);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintInt{1}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Damerau);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintDouble{1.}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Damerau);
  assertLevenshteinMatch(true, AqlValue(AqlValueHintBool{false}), AqlValue("aa"), AqlValue(AqlValueHintInt{2}), &Damerau);
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{1}), &Levenshtein);
  assertLevenshteinMatch(false, AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintDouble{1.}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintNull{}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintEmptyArray{}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintEmptyObject{}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintBool{false}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}), &InvalidNull);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}), &InvalidInt);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}), &InvalidArray);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}), &InvalidObject);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{-1}));
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{-1}), &Damerau);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{-1}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{5}), &Levenshtein);
  assertLevenshteinMatchFail(AqlValue("aa"), AqlValue("aaaa"), AqlValue(AqlValueHintInt{6}), &Levenshtein);
}
