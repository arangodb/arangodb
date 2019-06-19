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

#ifndef TESTS_AQL_AQLHELPER_H
#define TESTS_AQL_AQLHELPER_H

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"

namespace arangodb {
namespace aql {

std::ostream& operator<<(std::ostream&, arangodb::aql::ExecutionStats const&);
std::ostream& operator<<(std::ostream&, arangodb::aql::AqlItemBlock const&);

bool operator==(arangodb::aql::ExecutionStats const&, arangodb::aql::ExecutionStats const&);
bool operator==(arangodb::aql::AqlItemBlock const&, arangodb::aql::AqlItemBlock const&);

}
}

#endif  // TESTS_AQL_AQLHELPER_H
