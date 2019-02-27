////////////////////////////////////////////////////////////////////////////////
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "shared.hpp"
#include "IResearch/IResearchLinkHelper.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkHelperSetup {
  IResearchLinkHelperSetup() {
  }

  ~IResearchLinkHelperSetup() {
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchLinkHelperTest", "[iresearch][iresearch-link]") {
  IResearchLinkHelperSetup s;
  UNUSED(s);

SECTION("test_equals") {
  // test slice not both object
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("123");
    auto rhs = arangodb::velocypack::Parser::fromJson("{}");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test view id same type (validate only meta)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    CHECK((true == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((true == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test view id not same type (at least one non-string)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test view id prefix (up to /) not equal (at least one empty)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test view id prefix (up to /) not equal (shorter does not end with '/')
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test view id prefix (up to /) not equal (shorter ends with '/' but not a prefix of longer)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"ab/c\" }");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test view id prefix (up to /) equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/bc\" }");
    CHECK((true == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((true == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test meta init fail
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/bc\", \"includeAllFields\": 42 }");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test meta not equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\", \"includeAllFields\": false }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/bc\", \"includeAllFields\": true }");
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }

  // test equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\", \"includeAllFields\": false }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/bc\", \"includeAllFields\": false }");
    CHECK((true == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(), rhs->slice())));
    CHECK((true == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(), lhs->slice())));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------