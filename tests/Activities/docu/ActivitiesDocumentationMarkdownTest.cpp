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
////////////////////////////////////////////////////////////////////////////////

#include "activity_declaration.h"
#include "markdown.h"
#include <gtest/gtest.h>

TEST(ActivityDocumentationMarkdownTest, generates_markdown_correctly) {
  EXPECT_EQ(activities_to_markdown(std::vector<ActivityDeclaration>{
                ActivityDeclaration{
                    .owner = "ns::Holder",
                    .type = "ns::Foo",
                    .data_type_definition =
                        {Struct{.name = "ns::FooData",
                                .fields = {Member{.name = "id", .type = "int"},
                                           Member{.name = "label",
                                                  .type = "std::string"}}},
                         Struct{.name = "ns::Bar", .fields = {}}}},
                ActivityDeclaration{.owner = "ns::run",
                                    .type = "ns::Empty",
                                    .data_type_definition = {}}}),
            R""""(# Activities

## ns::Holder
type: ns::Foo

### ns::FooData
| Field | Type        |
|-------|-------------|
| id    | int         |
| label | std::string |

### ns::Bar

## ns::run
type: ns::Empty
)"""");
}
