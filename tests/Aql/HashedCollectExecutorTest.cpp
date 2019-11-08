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

#include "gtest/gtest.h"

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class HashedCollectExecutorTestNoRows : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;

  mocks::MockAqlServer server;
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  arangodb::transaction::Methods* trx;

  std::unordered_set<RegisterId> const regToClear;
  std::unordered_set<RegisterId> const regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  std::vector<std::string> aggregateTypes;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;

  // if count = true, then we need to set a countRegister
  RegisterId collectRegister;
  bool count;

  std::unordered_set<RegisterId> readableInputRegisters;
  std::unordered_set<RegisterId> writeableOutputRegisters;

  HashedCollectExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  HashedCollectExecutorTestNoRows()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 2)},
        collectRegister(0),
        count(false),
        infos(2 /*nrIn*/, 2 /*nrOut*/, regToClear, regToKeep,
              std::move(readableInputRegisters),
              std::move(writeableOutputRegisters), std::move(groupRegisters), 0,
              std::move(aggregateTypes), std::move(aggregateRegisters), trx, false),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)) {}
};

TEST_F(HashedCollectExecutorTestNoRows, the_producer_doesnt_wait) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(HashedCollectExecutorTestNoRows, the_producer_waits) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), true);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

class HashedCollectExecutorTestRowsNoCount : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;

  mocks::MockAqlServer server;
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  arangodb::transaction::Methods* trx;

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  std::unordered_set<RegisterId> readableInputRegisters;

  std::unordered_set<RegisterId> writeableOutputRegisters;

  RegisterId nrOutputRegister;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  std::vector<std::string> aggregateTypes;

  // if count = true, then we need to set a valid countRegister
  RegisterId collectRegister;
  bool count;

  HashedCollectExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  NoStats stats;

  HashedCollectExecutorTestRowsNoCount()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        readableInputRegisters{0},
        writeableOutputRegisters{1},
        nrOutputRegister(2),
        collectRegister(0),
        count(false),
        infos(1, nrOutputRegister, regToClear, regToKeep, std::move(readableInputRegisters),
              std::move(writeableOutputRegisters), std::move(groupRegisters), 0,
              std::move(aggregateTypes), std::move(aggregateRegisters), trx, false),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}
};

TEST_F(HashedCollectExecutorTestRowsNoCount, the_producer_doesnt_wait_1) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, the_producer_doesnt_wait_2) {
  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue y = block->getValue(1, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  AqlValue z = block->getValue(2, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myNumbers.at(2), 3);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, the_producer_doesnt_wait_3) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue y = block->getValue(1, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  AqlValue z = block->getValue(2, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myNumbers.at(2), 3);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, the_producer_doesnt_wait_4) {
  auto input = VPackParser::fromJson("[ [1], [2], [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue y = block->getValue(1, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, the_producer_waits) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), true);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
}

TEST(HashedCollectExecutorTestRowsCount, the_producer_doesnt_wait) {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("SUM");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = 2;
  writeableOutputRegisters.insert(2);

  HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   std::move(aggregateTypes),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  NoStats stats{};

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  std::vector<double> myCountNumbers;
  auto newBlock = result.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  // Check the count register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  myCountNumbers.emplace_back(xx.slice().getDouble());

  AqlValue z = newBlock->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // Check the count register
  AqlValue zz = newBlock->getValue(1, 2);
  ASSERT_TRUE(zz.isNumber());
  myCountNumbers.emplace_back(zz.slice().getDouble());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());

  std::sort(myCountNumbers.begin(), myCountNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myCountNumbers.at(0), 1);
  ASSERT_EQ(myCountNumbers.at(1), 2);
}

TEST(HashedCollectExecutorTestRowsCountNumbers, the_producer_doesnt_wait) {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = 2;
  writeableOutputRegisters.insert(2);

  HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   std::move(aggregateTypes),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  NoStats stats{};

  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<int64_t> myNumbers;
  std::vector<int64_t> myCountNumbers;
  auto newBlock = result.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  // Check the count register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  myCountNumbers.emplace_back(xx.slice().getInt());

  AqlValue z = newBlock->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // Check the count register
  AqlValue zz = newBlock->getValue(1, 2);
  ASSERT_TRUE(zz.isNumber());
  myCountNumbers.emplace_back(zz.slice().getInt());

  AqlValue y = newBlock->getValue(2, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  // Check the count register
  AqlValue yy = newBlock->getValue(2, 2);
  ASSERT_TRUE(yy.isNumber());
  myCountNumbers.emplace_back(yy.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());

  std::sort(myCountNumbers.begin(), myCountNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myNumbers.at(2), 3);
  ASSERT_EQ(myCountNumbers.at(0), 1);
  ASSERT_EQ(myCountNumbers.at(1), 1);
  ASSERT_EQ(myCountNumbers.at(2), 1);
}

TEST(HashedCollectExecutorTestRowsCountStrings, the_producer_doesnt_wait) {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = 2;
  writeableOutputRegisters.insert(2);

  HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   std::move(aggregateTypes),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  NoStats stats{};

  auto input = VPackParser::fromJson("[ [\"a\"], [\"aa\"], [\"aaa\"] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  HashedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::vector<std::string> myStrings;
  std::vector<int64_t> myCountNumbers;
  auto newBlock = result.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isString());
  myStrings.emplace_back(x.slice().copyString());

  // Check the count register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  myCountNumbers.emplace_back(xx.slice().getInt());

  AqlValue z = newBlock->getValue(1, 1);
  ASSERT_TRUE(z.isString());
  myStrings.emplace_back(z.slice().copyString());

  // Check the count register
  AqlValue zz = newBlock->getValue(1, 2);
  ASSERT_TRUE(zz.isNumber());
  myCountNumbers.emplace_back(zz.slice().getInt());

  AqlValue y = newBlock->getValue(2, 1);
  ASSERT_TRUE(y.isString());
  myStrings.emplace_back(y.slice().copyString());

  // Check the count register
  AqlValue yy = newBlock->getValue(2, 2);
  ASSERT_TRUE(yy.isNumber());
  myCountNumbers.emplace_back(yy.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myStrings.begin(), myStrings.end());

  std::sort(myCountNumbers.begin(), myCountNumbers.end());
  ASSERT_EQ(myStrings.at(0), "a");
  ASSERT_EQ(myStrings.at(1), "aa");
  ASSERT_EQ(myStrings.at(2), "aaa");
  ASSERT_EQ(myCountNumbers.at(0), 1);
  ASSERT_EQ(myCountNumbers.at(1), 1);
  ASSERT_EQ(myCountNumbers.at(2), 1);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, test_produce_datarange_1) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{R"(1)"}, {R"(2)"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  EXPECT_EQ(output.numRowsWritten(), 0);

  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 2);

  std::vector<int64_t> myNumbers;
  auto block = output.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, test_produce_datarange_2) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"(1)"}, {R"(2)"}, {R"(3)"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  EXPECT_EQ(output.numRowsWritten(), 0);

  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 3);

  std::vector<int64_t> myNumbers;
  auto block = output.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue y = block->getValue(1, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  AqlValue z = block->getValue(2, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myNumbers.at(2), 3);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, test_produce_datarange_3) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"(1)"}, {R"(2)"}, {R"(3)"}, {R"(1)"}, {R"(2)"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  EXPECT_EQ(output.numRowsWritten(), 0);

  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 3);

  std::vector<int64_t> myNumbers;
  auto block = output.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue y = block->getValue(1, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  AqlValue z = block->getValue(2, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myNumbers.at(2), 3);
}

TEST_F(HashedCollectExecutorTestRowsNoCount, test_produce_datarange_4) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"(1)"}, {R"(2)"}, {R"(1)"}, {R"(2)"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  EXPECT_EQ(output.numRowsWritten(), 0);

  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 2);

  std::vector<int64_t> myNumbers;
  auto block = output.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  AqlValue y = block->getValue(1, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
}

TEST(HashedCollectExecutorTestRowsCount, test_produce_datarange_sum) {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("SUM");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = 2;
  writeableOutputRegisters.insert(2);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   std::move(aggregateTypes),
                                   std::move(aggregateRegisters), trx, count);

  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{R"(1)"}, {R"(2)"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  EXPECT_EQ(output.numRowsWritten(), 0);
  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 2);

  std::vector<int64_t> myNumbers;
  std::vector<double> myCountNumbers;
  auto newBlock = output.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  // Check the count register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  myCountNumbers.emplace_back(xx.slice().getDouble());

  AqlValue z = newBlock->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // Check the count register
  AqlValue zz = newBlock->getValue(1, 2);
  ASSERT_TRUE(zz.isNumber());
  myCountNumbers.emplace_back(zz.slice().getDouble());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());

  std::sort(myCountNumbers.begin(), myCountNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myCountNumbers.at(0), 1);
  ASSERT_EQ(myCountNumbers.at(1), 2);
}

TEST(HashedCollectExecutorTestRowsCountNumbers, test_produce_datarange_sum_length) {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = 2;
  writeableOutputRegisters.insert(2);

  HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   std::move(aggregateTypes),
                                   std::move(aggregateRegisters), trx, count);
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};

  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"(1)"}, {R"(2)"}, {R"(3)"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  EXPECT_EQ(output.numRowsWritten(), 0);
  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 3);

  std::vector<int64_t> myNumbers;
  std::vector<int64_t> myCountNumbers;
  auto newBlock = output.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  myNumbers.emplace_back(x.slice().getInt());

  // Check the count register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  myCountNumbers.emplace_back(xx.slice().getInt());

  AqlValue z = newBlock->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  myNumbers.emplace_back(z.slice().getInt());

  // Check the count register
  AqlValue zz = newBlock->getValue(1, 2);
  ASSERT_TRUE(zz.isNumber());
  myCountNumbers.emplace_back(zz.slice().getInt());

  AqlValue y = newBlock->getValue(2, 1);
  ASSERT_TRUE(y.isNumber());
  myNumbers.emplace_back(y.slice().getInt());

  // Check the count register
  AqlValue yy = newBlock->getValue(2, 2);
  ASSERT_TRUE(yy.isNumber());
  myCountNumbers.emplace_back(yy.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myNumbers.begin(), myNumbers.end());

  std::sort(myCountNumbers.begin(), myCountNumbers.end());
  ASSERT_EQ(myNumbers.at(0), 1);
  ASSERT_EQ(myNumbers.at(1), 2);
  ASSERT_EQ(myNumbers.at(2), 3);
  ASSERT_EQ(myCountNumbers.at(0), 1);
  ASSERT_EQ(myCountNumbers.at(1), 1);
  ASSERT_EQ(myCountNumbers.at(2), 1);
}

TEST(HashedCollectExecutorTestRowsCountStrings, test_produce_datarange_sum_length_2) {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = 2;
  writeableOutputRegisters.insert(2);

  HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   std::move(aggregateTypes),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};

  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  HashedCollectExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"("a")"}, {R"("aa")"}, {R"("aaa")"}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  EXPECT_EQ(output.numRowsWritten(), 0);
  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 3);

  std::vector<std::string> myStrings;
  std::vector<int64_t> myCountNumbers;
  auto newBlock = output.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isString());
  myStrings.emplace_back(x.slice().copyString());

  // Check the count register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  myCountNumbers.emplace_back(xx.slice().getInt());

  AqlValue z = newBlock->getValue(1, 1);
  ASSERT_TRUE(z.isString());
  myStrings.emplace_back(z.slice().copyString());

  // Check the count register
  AqlValue zz = newBlock->getValue(1, 2);
  ASSERT_TRUE(zz.isNumber());
  myCountNumbers.emplace_back(zz.slice().getInt());

  AqlValue y = newBlock->getValue(2, 1);
  ASSERT_TRUE(y.isString());
  myStrings.emplace_back(y.slice().copyString());

  // Check the count register
  AqlValue yy = newBlock->getValue(2, 2);
  ASSERT_TRUE(yy.isNumber());
  myCountNumbers.emplace_back(yy.slice().getInt());

  // now sort vector and check for appearances
  std::sort(myStrings.begin(), myStrings.end());

  std::sort(myCountNumbers.begin(), myCountNumbers.end());
  ASSERT_EQ(myStrings.at(0), "a");
  ASSERT_EQ(myStrings.at(1), "aa");
  ASSERT_EQ(myStrings.at(2), "aaa");
  ASSERT_EQ(myCountNumbers.at(0), 1);
  ASSERT_EQ(myCountNumbers.at(1), 1);
  ASSERT_EQ(myCountNumbers.at(2), 1);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
