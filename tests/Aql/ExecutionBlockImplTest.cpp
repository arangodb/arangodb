////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////



#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionBlockImpl.cpp"


#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/FilterExecutor.h"

#include "Transaction/Methods.h"


using namespace arangodb;
using namespace arangodb::aql;


namespace arangodb {
namespace tests {
namespace aql {

 // ExecutionState state;
 // std::unique_ptr<AqlItemBlock> result;
 //
 // // Mock of the ExecutionEngine
 // fakeit::Mock<ExecutionEngine> mockEngine;
 // ExecutionEngine& engine = mockEngine.get();
 //
 // // Mock of the Query
 // fakeit::Mock<Query> mockQuery;
 // Query& query = mockQuery.get();
 //
 // // Mock of the Transaction
 // fakeit::Mock<transaction::Methods> mockTrx;
 // transaction::Methods& trx = mockTrx.get();
 //
 // // This is not used thus far in Base-Clase
 // ExecutionNode const* node = nullptr;
 //
 // // This test is supposed to only test getSome return values,
 // // it is not supposed to test the fetch logic!
 //
 // ExecutionBlockImpl<FilterExecutor> testee(&engine, node);


} // aql
} // tests
} // arangodb
