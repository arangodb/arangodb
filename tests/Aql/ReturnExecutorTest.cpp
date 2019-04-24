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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "BlockFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("ReturnExecutor", "[AQL][EXECUTOR][RETURN]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto registersToKeep = make_shared_unordered_set();

  RegisterId inputRegister(0);
/*
 * A hopefully temporary hack. Use TEMPLATE_TEST_CASE instead when it's
 * available.
 *
 * ATTENTION: The following tests are duplicated this way!
 */
#ifdef _WIN32
#define BLOCK(...) __VA_ARGS__
#else
#define BLOCK
#endif
// clang-format off
#define _FOR_BLOCK(name, v, block) \
  { constexpr bool name = v; block; }
#define FOR_BOOLS(name, block) \
  _FOR_BLOCK(name, true, block) \
  _FOR_BLOCK(name, false, block)
  // clang-format on

  FOR_BOOLS(
      passBlocksThrough, BLOCK({
        ReturnExecutorInfos infos(inputRegister, 1 /*nr in*/, 1 /*nr out*/, true /*do count*/,
                                  passBlocksThrough /*return inherit*/);

        auto& outputRegisters = infos.getOutputRegisters();

        GIVEN(std::string{"there are no rows upstream, passBlocksThrough="} +
              std::string{passBlocksThrough}) {
          VPackBuilder input;

          WHEN("the producer does not wait") {
            SingleRowFetcherHelper<true> fetcher(input.steal(), false);
            ReturnExecutor testee(fetcher, infos);
            CountStats stats{};

            THEN("the executor should return DONE with nullptr") {
              OutputAqlItemRow result(std::move(block), outputRegisters,
                                      registersToKeep, infos.registersToClear());
              std::tie(state, stats) = testee.produceRow(result);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!result.produced());
            }
          }

          WHEN("the producer waits") {
            SingleRowFetcherHelper<true> fetcher(input.steal(), true);
            ReturnExecutor testee(fetcher, infos);
            CountStats stats{};

            THEN("the executor should first return WAIT with nullptr") {
              OutputAqlItemRow result(std::move(block), outputRegisters,
                                      registersToKeep, infos.registersToClear());
              std::tie(state, stats) = testee.produceRow(result);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!result.produced());

              AND_THEN("the executor should return DONE with nullptr") {
                std::tie(state, stats) = testee.produceRow(result);
                REQUIRE(state == ExecutionState::DONE);
                REQUIRE(!result.produced());
              }
            }
          }
        }

        GIVEN(
            std::string{"there are rows in the upstream, passBlocksThrough="} +
            std::string{passBlocksThrough}) {
          auto input = VPackParser::fromJson("[ [true], [false], [true] ]");

          WHEN("the producer does not wait") {
            SingleRowFetcherHelper<true> fetcher(input->buffer(), false);
            ReturnExecutor testee(fetcher, infos);
            CountStats stats{};

            THEN("the executor should return the rows") {
              if (passBlocksThrough) {
                block = fetcher.getItemBlock();
              }
              OutputAqlItemRow row(std::move(block), outputRegisters,
                                   registersToKeep, infos.registersToClear());

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row.produced());
              row.advanceRow();

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row.produced());
              row.advanceRow();

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(row.produced());
              row.advanceRow();

              AND_THEN("The output should stay stable") {
                std::tie(state, stats) = testee.produceRow(row);
                REQUIRE(state == ExecutionState::DONE);
                REQUIRE(!row.produced());
              }

              // verify result
              AqlValue value;
              auto block = row.stealBlock();
              for (std::size_t index = 0; index < 3; index++) {
                value = block->getValue(index, 0);
                REQUIRE(value.isBoolean());
                REQUIRE(value.toBoolean() == input->slice().at(index).at(0).getBool());
              }

            }  // WHEN
          }    // WHEN

          WHEN("the producer waits") {
            SingleRowFetcherHelper<true> fetcher(input->steal(), true);
            ReturnExecutor testee(fetcher, infos);
            CountStats stats{};

            THEN("the executor should return the rows") {
              if (passBlocksThrough) {
                block = fetcher.getItemBlock();
              }
              OutputAqlItemRow row{std::move(block), outputRegisters,
                                   registersToKeep, infos.registersToClear()};

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row.produced());

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row.produced());
              row.advanceRow();

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row.produced());

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row.produced());
              row.advanceRow();

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row.produced());

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(row.produced());
              row.advanceRow();

              std::tie(state, stats) = testee.produceRow(row);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!row.produced());
            }
          }
        }  // GIVEN
      }))

}  // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
