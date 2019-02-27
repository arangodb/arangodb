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

#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Basics/SmallVector.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

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
        return {"f" ,"millisecond", "milliseconds", "MiLLiSeCOnd"};
      case SECOND:
        return {"s" ,"second", "seconds", "SeCoNd"};
      case MINUTE:
        return {"i" ,"minute", "minutes", "MiNutEs"};
      case HOUR:
        return {"h" ,"hour", "hours", "HoUr"};
      case DAY:
        return {"d" ,"day", "days", "daYs"};
      case WEEK:
        return {"w" ,"week", "weeks", "WeEkS"};
      case MONTH:
        return {"m" ,"month", "months", "mOnTHs"};
      case YEAR:
        return {"y" ,"year", "years", "yeArS"};
    }
    return {"abc"};
  }

  static std::string createFlag(FLAGS const&e) {
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
    input.emplace_back(_date.get());
  }

  void validateResult(AqlValue const& result) const {
    REQUIRE(result.isBoolean());
    REQUIRE(result.toBoolean() == _isValid);
  }

 private:
  std::shared_ptr<arangodb::velocypack::Builder> _date;
  bool _isValid;
};

SCENARIO("Testing IS_DATESTRING", "[AQL][DATE]") {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  GIVEN("the non error case") {
    std::vector<TestDate> testees = {
#include "IS_DATESTRING.testcases"
    };

    for (auto const& testee : testees) {
      THEN("Validating: " + testee.testName()) {
        SmallVector<AqlValue>::allocator_type::arena_type arena;
        SmallVector<AqlValue> params{arena};
        testee.buildParams(params);
        AqlValue res =
            Functions::IsDatestring(&expressionContext, &trx, params);
        testee.validateResult(res);

        // Free input parameters
        for (auto& it : params) {
          it.destroy();
        }
      }
    }
  }
}

}  // is_datestring

namespace date_compare {
struct TestDate {
 public:
  TestDate(std::vector<std::string> const args,
            bool v)
      : _isValid(v) {
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
    for (auto const& it : VPackArrayIterator(_argBuilder.slice())) {
      input.emplace_back(it);
    }
  }

  void validateResult(AqlValue const& result) const {
    REQUIRE(result.isBoolean());
    REQUIRE(result.toBoolean() == _isValid);
  }

 private:
  arangodb::velocypack::Builder _argBuilder;
  bool _isValid;
};

SCENARIO("Testing DATE_COMPARE", "[AQL][DATE]") {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  GIVEN("the non error case") {
    std::vector<TestDate> testees = {
#include "DATE_COMPARE.testcases"
    };

    for (auto const& testee : testees) {
      THEN("Validating: " + testee.testName()) {
        SmallVector<AqlValue>::allocator_type::arena_type arena;
        SmallVector<AqlValue> params{arena};
        testee.buildParams(params);
        AqlValue res =
            Functions::DateCompare(&expressionContext, &trx, params);
        testee.validateResult(res);
        // Free input parameters
        for (auto& it : params) {
          it.destroy();
        }
      }
    }
  }
}
 
}  // date_compare

namespace date_diff {
SCENARIO("Testing DATE_DIFF", "[AQL][DATE]") {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking all modifier Flags") {
    // These dates differ by:
    // 1 year
    // 2 months
    // 1 week
    // 12 days
    // 4 hours
    // 5 minutes
    // 6 seconds
    // 123 milliseconds
    std::string const earlierDate = "2000-04-01T02:48:42.123"; 
    std::string const laterDate = "2001-06-13T06:53:48.246";
    // Exact milisecond difference
    double dateDiffMillis = 37857906123;
    // Average number of days per month in the given dates
    double avgDaysPerMonth = 31*8+30*5+28;
    avgDaysPerMonth /= 14; // We have 14 months
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};
    VPackBuilder dateBuilder;
    dateBuilder.openArray();
    dateBuilder.add(VPackValue(earlierDate));
    dateBuilder.add(VPackValue(laterDate));
    dateBuilder.close();
    VPackBuilder flagBuilder;
    VPackBuilder switchBuilder;

    auto testCombinations = [&](std::string const& f, double expected) -> void {
      double eps = 0.05;
      params.clear();
      flagBuilder.clear();
      flagBuilder.add(VPackValue(f));
      WHEN("using "  + earlierDate + ", " + laterDate + ", " + f) {
        params.emplace_back(dateBuilder.slice().at(0));
        params.emplace_back(dateBuilder.slice().at(1));
        params.emplace_back(flagBuilder.slice());
        THEN("returning float") {
          switchBuilder.add(VPackValue(true));
          params.emplace_back(switchBuilder.slice());
          AqlValue res =
              Functions::DateDiff(&expressionContext, &trx, params);
          REQUIRE(res.isNumber());
          double out = res.toDouble();
          REQUIRE(out >= expected - eps);
          REQUIRE(out <= expected + eps);
        }
        THEN("returning integer") {
          switchBuilder.add(VPackValue(false));
          params.emplace_back(switchBuilder.slice());
          AqlValue res =
              Functions::DateDiff(&expressionContext, &trx, params);
          REQUIRE(res.isNumber());
          REQUIRE(res.toDouble() == std::round(expected));
        }
      }
      WHEN("using "  + laterDate + ", " + earlierDate + ", " + f) {
        params.emplace_back(dateBuilder.slice().at(1));
        params.emplace_back(dateBuilder.slice().at(0));
        params.emplace_back(flagBuilder.slice());
        THEN("returning float") {
          switchBuilder.add(VPackValue(true));
          params.emplace_back(switchBuilder.slice());
          AqlValue res =
              Functions::DateDiff(&expressionContext, &trx, params);
          REQUIRE(res.isNumber());
          double out = res.toDouble();
          REQUIRE(out >= -(expected + eps));
          REQUIRE(out <= -(expected - eps));
        }
        THEN("returning integer") {
          switchBuilder.add(VPackValue(false));
          params.emplace_back(switchBuilder.slice());
          AqlValue res =
              Functions::DateDiff(&expressionContext, &trx, params);
          REQUIRE(res.isNumber());
          REQUIRE(res.toDouble() == -std::round(expected));
        }
      }
 
      for (auto& it : params) {
        it.destroy();
      }

    };

    WHEN("checking Millis") {
      double expectedDiff = dateDiffMillis;
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::MILLI);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Seconds") {
      double expectedDiff = dateDiffMillis / 1000;
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::SECOND);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Minutes") {
      double expectedDiff = dateDiffMillis / (1000 * 60);
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::MINUTE);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Hours") {
      double expectedDiff = dateDiffMillis / (1000 * 60 * 60);
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::HOUR);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Days") {
      double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24);
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::DAY);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Weeks") {
      double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24 * 7);
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::WEEK);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Months") {
      double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24) / avgDaysPerMonth;
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::MONTH);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

    WHEN("checking Years") {
      double expectedDiff = dateDiffMillis / (1000 * 60 * 60 * 24) / 365;
      auto allFlags = TestDateModifierFlagFactory::createAllFlags(TestDateModifierFlagFactory::FLAGS::YEAR);
      for (auto const& f : allFlags) {
        testCombinations(f, expectedDiff);
      }
    }

  }

  WHEN("checking leap days") {
    // TODO!
  }
}
} // date_diff


namespace date_subtract {

struct TestDate {
 public:
  TestDate(std::string const& json, std::string const& v) : _input(nullptr), _result(v) {
    // Make sure to only insert valid JSON.
    // We are not testing the parser here.
    _input = arangodb::velocypack::Parser::fromJson(json);
  }

  std::string const testName() const {
    return _input->toJson() + " => " + _result;
  }

  void buildParams(VPackFunctionParameters& input) const {
    VPackSlice s = _input->slice();
    for (auto const& it : VPackArrayIterator(s)) {
      input.emplace_back(it);
    }
  }

  void validateResult(AqlValue const& result) const {
    REQUIRE(result.isString());
    auto res = result.slice();
    std::string ref = res.copyString(); // Readability in test Tool
    REQUIRE(ref == _result);
  }

 private:
  std::shared_ptr<arangodb::velocypack::Builder> _input;
  std::string const _result;
};


SCENARIO("Testing DATE_SUBTRACT", "[AQL][DATE]") {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  GIVEN("the non error case") {
    std::vector<TestDate> testees = {
#include "DATE_SUBTRACT.testcases"
    };

    for (auto const& testee : testees) {
      THEN("Validating: " + testee.testName()) {
        SmallVector<AqlValue>::allocator_type::arena_type arena;
        SmallVector<AqlValue> params{arena};
        testee.buildParams(params);
        AqlValue res =
            Functions::DateSubtract(&expressionContext, &trx, params);
        testee.validateResult(res);
        res.destroy();
        // Free input parameters
        for (auto& it : params) {
          it.destroy();
        }
      }
    }
  }
}
 
} // date_subtract

}  // date_functions_aql
}  // tests
}  // arangodb
