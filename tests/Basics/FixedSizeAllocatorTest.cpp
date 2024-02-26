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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/AstResources.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/FixedSizeAllocator.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"

#include "gtest/gtest.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

using namespace arangodb;

TEST(FixedSizeAllocatorTest, test_Int) {
  FixedSizeAllocator<int> allocator;

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  allocator.ensureCapacity();
  int* p = allocator.allocate(24);

  // first allocation should be aligned to a cache line
  EXPECT_EQ(0, (uintptr_t(p) % 64));
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(24, *p);
  EXPECT_EQ(1, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.ensureCapacity();
  p = allocator.allocate(42);

  EXPECT_EQ(42, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(2, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.ensureCapacity();
  p = allocator.allocate(23);

  EXPECT_EQ(23, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(3, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());
}

TEST(FixedSizeAllocatorTest, test_UInt64) {
  FixedSizeAllocator<uint64_t> allocator;

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  allocator.ensureCapacity();
  uint64_t* p = allocator.allocate(24);

  // first allocation should be aligned to a cache line
  EXPECT_EQ(0, (uintptr_t(p) % 64));
  EXPECT_EQ(0, (uintptr_t(p) % alignof(uint64_t)));
  EXPECT_EQ(24, *p);
  EXPECT_EQ(1, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.ensureCapacity();
  p = allocator.allocate(42);

  EXPECT_EQ(42, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(2, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.ensureCapacity();
  p = allocator.allocate(23);

  EXPECT_EQ(23, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(3, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());
}

TEST(FixedSizeAllocatorTest, test_Struct) {
  struct Testee {
    Testee() = default;
    Testee(std::string abc, std::string def) : abc(abc), def(def) {}

    std::string abc;
    std::string def;
  };

  FixedSizeAllocator<Testee> allocator;

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  allocator.ensureCapacity();
  Testee* p = allocator.allocate("foo", "bar");

  // first allocation should be aligned to a cache line
  EXPECT_EQ(0, (uintptr_t(p) % 64));
  EXPECT_EQ(0, (uintptr_t(p) % alignof(Testee)));
  EXPECT_EQ("foo", p->abc);
  EXPECT_EQ("bar", p->def);
  EXPECT_EQ(1, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.ensureCapacity();
  p = allocator.allocate("foobar", "baz");
  EXPECT_EQ(0, (uintptr_t(p) % alignof(Testee)));
  EXPECT_EQ("foobar", p->abc);
  EXPECT_EQ("baz", p->def);
  EXPECT_EQ(2, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());
}

TEST(FixedSizeAllocatorTest, test_MassAllocation) {
  FixedSizeAllocator<std::string> allocator;

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  for (size_t i = 0; i < 10 * 1000; ++i) {
    allocator.ensureCapacity();
    std::string* p = allocator.allocate("test" + std::to_string(i));

    EXPECT_EQ("test" + std::to_string(i), *p);
    EXPECT_EQ(i + 1, allocator.numUsed());
  }
  EXPECT_GT(allocator.usedBlocks(), 0);

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());
}

TEST(FixedSizeAllocatorTest, test_Clear) {
  FixedSizeAllocator<uint64_t> allocator;

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  size_t numItemsLeft = 0;
  size_t usedBlocks = 0;
  for (size_t i = 0; i < 10 * 1000; ++i) {
    if (numItemsLeft == 0) {
      numItemsLeft = FixedSizeAllocator<uint64_t>::capacityForBlock(usedBlocks);
      ++usedBlocks;
    }
    allocator.ensureCapacity();
    uint64_t* p = allocator.allocate(i);
    --numItemsLeft;

    EXPECT_EQ(i, *p);
    EXPECT_EQ(i + 1, allocator.numUsed());
    EXPECT_EQ(usedBlocks, allocator.usedBlocks());
  }

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  allocator.ensureCapacity();
  uint64_t* p = allocator.allocate(42);
  EXPECT_EQ(42, *p);
  EXPECT_EQ(1, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());
}

TEST(FixedSizeAllocatorTest, test_ClearMost) {
  FixedSizeAllocator<uint64_t> allocator;

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(0, allocator.usedBlocks());

  size_t numItemsLeft = 0;
  size_t usedBlocks = 0;
  for (size_t i = 0; i < 10 * 1000; ++i) {
    if (numItemsLeft == 0) {
      numItemsLeft = FixedSizeAllocator<uint64_t>::capacityForBlock(usedBlocks);
      ++usedBlocks;
    }
    allocator.ensureCapacity();
    uint64_t* p = allocator.allocate(i);
    --numItemsLeft;

    EXPECT_EQ(i, *p);
    EXPECT_EQ(i + 1, allocator.numUsed());
    EXPECT_EQ(usedBlocks, allocator.usedBlocks());
  }

  allocator.clearMost();

  EXPECT_EQ(0, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());

  allocator.ensureCapacity();
  uint64_t* p = allocator.allocate(42);
  EXPECT_EQ(42, *p);
  EXPECT_EQ(1, allocator.numUsed());
  EXPECT_EQ(1, allocator.usedBlocks());
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST(FixedSizeAllocatorTest, test_AstNodesRollbackDuringCreation) {
  // recursive AstNode structure. the AstNode ctor will throw
  // when it encounters the node with the "throw!" string value,
  // if the failure point is set.
  constexpr std::string_view data(R"(
{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":1,"vTypeID":2},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":2,"vTypeID":2},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":3,"vTypeID":2},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":"throw!","vTypeID":4}]}]}]}]}
  )");

  // whatever query string will do here.
  constexpr std::string_view queryString("RETURN null");

  // create a query object so we have an AST object to mess with
  arangodb::tests::mocks::MockAqlServer server(true);
  arangodb::CreateDatabaseInfo testDBInfo(server.server(),
                                          arangodb::ExecContext::current());
  testDBInfo.load("testVocbase", 2);
  TRI_vocbase_t vocbase(std::move(testDBInfo));
  auto query = arangodb::aql::Query::create(
      arangodb::transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      arangodb::aql::QueryString(queryString), nullptr);
  query->initForTests();

  auto builder = velocypack::Parser::fromJson(data);

  // registration of AstNodes should work fine without failure points
  query->ast()->resources().registerNode(query->ast(), builder->slice());

  // set a failure point that throws in the AstNode ctor when
  // it encounters an AstNode with a string value "throw!"
  TRI_AddFailurePointDebugging("AstNode::throwOnAllocation");

  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });

  Result res = basics::catchToResult([&]() -> Result {
    // we expect this to throw a TRI_ERROR_DEBUG exception
    // because of the failure point
    query->ast()->resources().registerNode(query->ast(), builder->slice());
    return {};
  });

  EXPECT_EQ(TRI_ERROR_DEBUG, res.errorNumber());
  // we also expect implicitly that the heap was not corrupted
}
#endif
