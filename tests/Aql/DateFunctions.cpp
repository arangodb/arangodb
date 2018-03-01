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
#include "Aql/Functions.h"
#include "Aql/Query.h"
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
  fakeit::Mock<Query> queryMock;
  Query& query = queryMock.get();

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
            Functions::IsDatestring(&query, &trx, params);
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
  fakeit::Mock<Query> queryMock;
  Query& query = queryMock.get();

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
            Functions::DateCompare(&query, &trx, params);
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
}  // date_functions_aql
}  // tests
}  // arangodb
