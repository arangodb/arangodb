////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Match/VariableScope.h"
#include "Aql/Variable.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

using namespace arangodb::aql;
using namespace arangodb::aql::match;

namespace {

class VariableScopeTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
};

}  // namespace

TEST_F(VariableScopeTest, resolveWithoutSubstitutionReturnsSameVariable) {
  Variable user("v", 1, false, monitor);
  VariableScope scope;
  EXPECT_EQ(&user, scope.resolve(&user));
  EXPECT_TRUE(scope.map().empty());
}

TEST_F(VariableScopeTest, registerSubstitutionAndResolve) {
  Variable user("v", 1, false, monitor);
  Variable temp("tmp", 2, false, monitor);
  Variable other("w", 3, false, monitor);

  VariableScope scope;
  scope.registerSubstitution(&user, &temp);

  EXPECT_EQ(&temp, scope.resolve(&user));
  EXPECT_EQ(&other, scope.resolve(&other));
  EXPECT_EQ(1U, scope.map().size());
  EXPECT_EQ(&temp, scope.map().at(user.id));
}
