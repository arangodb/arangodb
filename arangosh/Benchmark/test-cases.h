////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "testcases/AqlInsertTest.h"
#include "testcases/AqlV8Test.h"
#include "testcases/CustomQuery.h"
#include "testcases/CollectionCreationTest.h"
#include "testcases/DocumentCreationTest.h"
#include "testcases/DocumentCrudAppendTest.h"
#include "testcases/DocumentCrudTest.h"
#include "testcases/DocumentCrudWriteReadTest.h"
#include "testcases/DocumentImportTest.h"
#include "testcases/EdgeCrudTest.h"
#include "testcases/HashTest.h"
#include "testcases/RandomShapesTest.h"
#include "testcases/ShapesAppendTest.h"
#include "testcases/ShapesTest.h"
#include "testcases/SkiplistTest.h"
#include "testcases/StreamCursorTest.h"
#include "testcases/TransactionAqlTest.h"
#include "testcases/TransactionCountTest.h"
#include "testcases/TransactionDeadlockTest.h"
#include "testcases/TransactionMultiCollectionTest.h"
#include "testcases/TransactionMultiTest.h"
#include "testcases/VersionTest.h"
