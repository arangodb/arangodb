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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ExecutorTestHelper.h"

std::ostream& arangodb::tests::aql::operator<<(std::ostream& stream,
                                               arangodb::tests::aql::ExecutorCall call) {
  return stream << [call]() {
    switch (call) {
      case ExecutorCall::SKIP_ROWS:
        return "SKIP_ROWS";
      case ExecutorCall::PRODUCE_ROWS:
        return "PRODUCE_ROWS";
      case ExecutorCall::FETCH_FOR_PASSTHROUGH:
        return "FETCH_FOR_PASSTHROUGH";
      case ExecutorCall::EXPECTED_NR_ROWS:
        return "EXPECTED_NR_ROWS";
    }
  }();
}
