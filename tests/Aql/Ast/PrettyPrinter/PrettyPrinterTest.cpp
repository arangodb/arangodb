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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/Parser.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Aql/Ast/PrettyPrinter.h"
#include "Aql/QueryContext.h"
#include "Logger/LogMacros.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include "Mocks/Servers.h"

#include <vector>
#include <ranges>

using namespace arangodb::aql;
using namespace arangodb::velocypack;
using namespace arangodb::tests;

using namespace arangodb::aql::ast::pretty_printer;

struct TestContext {
  explicit TestContext(std::string to_parse)
      : server{},
        query{server.createFakeQuery()},
        trx{server.createFakeTransaction()} {
    // TODO: a lot of magic is happening here that is strictly speaking
    // entirely unnecessary.
    auto qs = QueryString(to_parse);
    auto parser = arangodb::aql::Parser(*query, *query->ast(), qs);
    parser.parse();

    // TODO: find out whether this validateAndOptimize step is necessary (or
    // counterproductive) for these tests
    // query->ast()->validateAndOptimize(*trx, {.optimizeNonCacheable = true});
  }

  auto getTopNode() -> AstNode* {
    return query->ast()->root()->getMemberUnchecked(2)->getMemberUnchecked(0);
  }

  mocks::MockAqlServer server;
  std::shared_ptr<Query> query;
  std::shared_ptr<arangodb::transaction::Methods> trx;
};

TEST(PrettyPrinterTest, prints_pretty) {
  auto expression = TestContext(
      R"=(LET path = [] RETURN path.vertices[* RETURN CURRENT.f == "green"] ALL == true)=");

  auto expected = R"=(- array compare == (65): 
  - expansion (38): 
    - iterator (39): 
      - variable (13): 
      - attribute access (35): vertices
        - reference (45): path
    - reference (45): 3_
    - no-op (50): 
    - no-op (50): 
    - compare == (25): 
      - attribute access (35): f
        - reference (45): 3_
      - value (40): "green"
  - value (40): true
  - quantifier (73): all
)=";

  std::stringstream output;
  toStream(output, expression.getTopNode(), 0);

  std::cerr << output.str();

  ASSERT_EQ(output.str(), expected);
}
