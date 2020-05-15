////////////////////////////////////////////////////////////////////////////////
/// @brief test sr AQL Datefunctions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <cmath>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace arangodb {
namespace tests {
namespace date_functions_aql {

struct TestDateModifierFlagFactory {
 public:
  enum FLAGS { INVALID, MILLI, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH, YEAR };

  static std::vector<std::string> createAllFlags(FLAGS const& e) {
    switch (e) {
      case INVALID:
        return {"abc"};
      case MILLI:
        return {"f", "millisecond", "milliseconds", "MiLLiSeCOnd"};
      case SECOND:
        return {"s", "second", "seconds", "SeCoNd"};
      case MINUTE:
        return {"i", "minute", "minutes", "MiNutEs"};
      case HOUR:
        return {"h", "hour", "hours", "HoUr"};
      case DAY:
        return {"d", "day", "days", "daYs"};
      case WEEK:
        return {"w", "week", "weeks", "WeEkS"};
      case MONTH:
        return {"m", "month", "months", "mOnTHs"};
      case YEAR:
        return {"y", "year", "years", "yeArS"};
    }
    return {"abc"};
  }

  static std::string createFlag(FLAGS const& e) {
    switch (e) {
      case INVALID:
        return "abc";
      case MILLI:
        return "f";
      case SECOND:
        return "s";
      case MINUTE:
        return "i";
      case HOUR:
        return "h";
      case DAY:
        return "d";
      case WEEK:
        return "w";
      case MONTH:
        return "m";
      case YEAR:
        return "y";
    }
  }
};

namespace is_datestring {
struct TestDate {
 public:
  TestDate(std::string const json, bool v) : _date(nullptr), _isValid(v) {
    // Make sure to only insert valid JSON.
    // We are not testing the parser here.
    _date = arangodb::velocypack::Parser::fromJson(json);
  }

  std::string const testName() const {
    return _date->toJson() + " => " + (_isValid ? "true" : "false");
  }

  void buildParams(VPackFunctionParameters& input) const {
    input.emplace_back(*_date);
  }

  void validateResult(AqlValue const& result) const {
    ASSERT_TRUE(result.isBoolean());
    ASSERT_EQ(result.toBoolean(), _isValid);
  }

 private:
  std::shared_ptr<arangodb::velocypack::Builder> _date;
  bool _isValid;
};

TEST(DateFunctionsTest, IS_DATESTRING) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  std::vector<TestDate> testees = {
#include "IS_DATESTRING.testcases"
  };

  for (auto const& testee : testees) {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};
    testee.buildParams(params);
    AqlValue res = Functions::IsDatestring(&expressionContext, &trx, params);
    testee.validateResult(res);

    // Free input parameters
    for (auto& it : params) {
      it.destroy();
    }
  }
}

}  // namespace is_datestring

namespace date_compare {
struct TestDate {
 public:
  TestDate(std::vector<std::string> const args, bool v) : _isValid(v) {
    _argBuilder.openArray();
    for (auto const& it : args) {
      _argBuilder.add(VPackValue(it));
    }
    _argBuilder.close();
  }

  std::string const testName() const {
    return "Input: " + _argBuilder.toJson() + " => " + (_isValid ? "true" : "false");
  }

  void buildParams(VPackFunctionParameters& input) const {
    for (VPackSlice it : VPackArrayIterator(_argBuilder.slice())) {
      input.emplace_back(it);
    }
  }

  void validateResult(AqlValue const& result) const {
    ASSERT_TRUE(result.isBoolean());
    ASSERT_EQ(result.toBoolean(), _isValid);
  }

 private:
  arangodb::velocypack::Builder _argBuilder;
  bool _isValid;
};

TEST(DateFunctionsTest, DATE_COMPARE) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  std::vector<TestDate> testees = {
#include "DATE_COMPARE.testcases"
  };

  for (auto const& testee : testees) {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};
    testee.buildParams(params);
    AqlValue res = Functions::DateCompare(&expressionContext, &trx, params);
    testee.validateResult(res);
    // Free input parameters
    for (auto& it : params) {
      it.destroy();
    }
  }
}

}  // namespace date_compare

namespace date_diff {
class DateFunctionsTestDateDiff : public ::testing::Test {
 protected:
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext;
  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx;

  // These dates differ by:
  // 1 year
  // 2 months
  // 1 week
  // 12 days
  // 4 hours
  // 5 minutes
  // 6 seconds
  // 123 milliseconds
  std::string const earlierDate;
  std::string const laterDate;
  // Exact milisecond difference
  double dateDiffMillis;
  // Average number of days per month in the given dates
  double avgDaysPerMonth;
  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params;
  VPackBuilder dateBuilder;
  VPackBuilder flagBuilder;
  VPackBuilder switchBuilder;

  DateFunctionsTestDateDiff()
      : expressionContext(expressionContextMock.get()),
        trx(trxMock.get()),
        earlierDate("2000-04-01T02:48:42.123"),
        laterDate("2001-06-13T06:53:48.246"),
        dateDiffMillis(37857906123),
        avgDaysPerMonth(365.0/12.0),
        params(arena) {
    dateBuilder.openArray();
    dateBuilder.add(VPackValue(earlierDate));
    dateBuilder.add(VPackValue(laterDate));
    dateBuilder.close();
  }

  void testCombinations(std::string const& f, double expected) {
      {
        double eps = 0.05;
        params.clear();
        flagBuilder.clear();
        flagBuilder.add(VPackValue(f));
        params.emplace_back(dateBuilder.slice().at(0));
        params.emplace_back(dateBuilder.slice().at(1));
        params.emplace_back(flagBuilder.slice());
        switchBuilder.add(VPackValue(true));
        params.emplace_back(switchBuilder.slice());
        AqlValue res = Functions::DateDiff(&expressionContext, &trx, params);
        ASSERT_TRUE(res.isNumber());
        double out = res.toDouble();
        ASSERT_GE(out, expected - eps);
        ASSERT_LE(out, expected + eps);
        for (auto& it : params) {
          it.destroy();
        }
      }
      {
        params.clear();
        flagBuilder.clear();
        flagBuilder.add(VPackValue(f));
        params.emplace_back(dateBuilder.slice().at(0));
        params.emplace_back(dateBuilder.slice().at(1));
        params.emplace_back(flagBuilder.slice());
        switchBuilder.add(VPackValue(false));
        params.emplace_back(switchBuilder.slice());
        AqlValue res = Functions::DateDiff(&expressionContext, &trx, params);
        ASSERT_TRUE(res.isNumber());
        ASSERT_EQ(std::round(res.toDouble()), std::round(expected));
        for (auto& it : params) {
          it.destroy();
        }
      }
      {
        double eps = 0.05;
        params.clear();
        flagBuilder.clear();
        flagBuilder.add(VPackValue(f));
        params.emplace_back(dateBuilder.slice().at(1));
        params.emplace_back(dateBuilder.slice().at(0));
        params.emplace_back(flagBuilder.slice());
        switchBuilder.add(VPackValue(true));
        params.emplace_back(switchBuilder.slice());
        AqlValue res = Functions::DateDiff(&expressionContext, &trx, params);
        ASSERT_TRUE(res.isNumber());
        double out = res.toDouble();
        ASSERT_GE(out, -(expected + eps));
        ASSERT_LE(out, -(expected - eps));
        for (auto& it : params) {
          it.destroy();
        }
      }
      {
        params.clear();
        flagBuilder.clear();
        flagBuilder.add(VPackValue(f));
        params.emplace_back(dateBuilder.slice().at(1));
        params.emplace_back(dateBuilder.slice().at(0));
        params.emplace_back(flagBuilder.slice());
        switchBuilder.add(VPackValue(false));
        params.emplace_back(switchBuilder.slice());
        AqlValue res = Functions::DateDiff(&expressionContext, &trx, params);
        ASSERT_TRUE(res.isNumber());
        ASSERT_EQ(std::round(res.toDouble()), -std::round(expected));
        for (auto& it : params) {
          it.destroy();
        }
      }

  }
};

TEST_F(DateFunctionsTestDateDiff, checking_millis) {
  double expectedDiff = dateDiffMillis;
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::MILLI);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_seconds) {
  double expectedDiff = dateDiffMillis / 1000;
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::SECOND);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_minutes) {
  double expectedDiff = dateDiffMillis / (1000 * 60);
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::MINUTE);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_hours) {
  double expectedDiff = dateDiffMillis / (1000 * 60 * 60);
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::HOUR);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_days) {
  double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24);
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::DAY);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_weeks) {
  double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24 * 7);
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::WEEK);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_months) {
  double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24) / avgDaysPerMonth;
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::MONTH);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_years) {
  double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24) / 365;
  auto allFlags = TestDateModifierFlagFactory::createAllFlags(
      TestDateModifierFlagFactory::FLAGS::YEAR);
  for (auto const& f : allFlags) {
    testCombinations(f, expectedDiff);
  }
}

TEST_F(DateFunctionsTestDateDiff, checking_leap_days) {
  // TODO!
}

}  // namespace date_diff

namespace date_subtract {

struct TestDate {
 public:
  TestDate(std::string const& json, std::string const& v)
      : _input(nullptr), _result(v) {
    // Make sure to only insert valid JSON.
    // We are not testing the parser here.
    _input = arangodb::velocypack::Parser::fromJson(json);
  }

  std::string const testName() const {
    return _input->toJson() + " => " + _result;
  }

  void buildParams(VPackFunctionParameters& input) const {
    VPackSlice s = _input->slice();
    for (VPackSlice it : VPackArrayIterator(s)) {
      input.emplace_back(it);
    }
  }

  void validateResult(AqlValue const& result) const {
    ASSERT_TRUE(result.isString());
    auto res = result.slice();
    std::string ref = res.copyString();  // Readability in test Tool
    ASSERT_EQ(ref, _result);
  }

 private:
  std::shared_ptr<arangodb::velocypack::Builder> _input;
  std::string const _result;
};

TEST(DateFunctionsTest, DATE_SUBTRACT) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  std::vector<TestDate> testees = {
#include "DATE_SUBTRACT.testcases"
  };

  for (auto const& testee : testees) {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};
    testee.buildParams(params);
    AqlValue res = Functions::DateSubtract(&expressionContext, &trx, params);
    testee.validateResult(res);
    res.destroy();
    // Free input parameters
    for (auto& it : params) {
      it.destroy();
    }
  }
}

}  // namespace date_subtract

}  // namespace date_functions_aql
}  // namespace tests
}  // namespace arangodb
