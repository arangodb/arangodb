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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "BlockFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/Query.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/TraversalExecutor.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"
#include "VocBase/ManagedDocumentResult.h"
#include "tests/Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;

namespace arangodb {
namespace tests {

namespace aql {

class TraverserHelper : public Traverser {

  public:
  TraverserHelper(TraverserOptions* opts, transaction::Methods* trx, ManagedDocumentResult* mdr) : Traverser(opts, trx, mdr) {
  }

  void setStartVertex(std::string const& value) override {
    _usedVertexAt.push_back(value);
  }

  bool getVertex(VPackSlice edge, std::vector<arangodb::StringRef>& result) override {
    // Implement
    return false;
  }

  bool getSingleVertex(VPackSlice edge, arangodb::StringRef const sourceVertex,
      uint64_t depth, arangodb::StringRef& targetVertex) override {
    // Implement
    return false;
  }

  AqlValue fetchVertexData(StringRef vid) override {
    // IMPLEMENT
    return AqlValue(AqlValueHintNull());
  }

  void addVertexToVelocyPack(StringRef vid, VPackBuilder& builder) override {
    // IMPLEMENT
    return;
  }

  std::string const& startVertexUsedAt(uint64_t index) {
    REQUIRE(index < _usedVertexAt.size());
    return _usedVertexAt[index];
  }

  std::string const& currentStartVertex() {
  return _usedVertexAt.back();
  }

 private:
  std::vector<std::string> _usedVertexAt;
};

SCENARIO("TraversalExecutor", "[AQL][EXECUTOR][TRAVEXE]") {
  ExecutionState state;
  mocks::MockAqlServer server{};

  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();

  ResourceMonitor monitor;
  auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);

  TraverserOptions traversalOptions(fakedQuery.get());
  arangodb::transaction::Methods* trx = fakedQuery->trx();
  arangodb::ManagedDocumentResult mdr;

  auto traverserPtr = std::make_unique<TraverserHelper>(&traversalOptions, trx, &mdr);

  auto traverser = traverserPtr.get();
  TraversalExecutorInfos infos({0}, {1}, 1, 2, {}, std::move(traverserPtr));

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      TraversalExecutor testee(fetcher, infos);
      TraversalStats stats{};

      THEN("the executor should return DONE and no result") {
        OutputAqlItemRow result(std::move(block), infos);
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      TraversalExecutor testee(fetcher, infos);
      TraversalStats stats{};

      THEN("the executor should first return WAIT and no result") {
        OutputAqlItemRow result(std::move(block), infos);
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 0);

        AND_THEN("the executor should return DONE and no result") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(stats.getFiltered() == 0);
        }
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input =
        VPackParser::fromJson("[ [\"v/1\"], [\"v/2\"], [\"v/3\"] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input->steal(), false);
      TraversalExecutor testee(fetcher, infos);
      TraversalStats stats{};

      WHEN("no edges are connected to vertices") {

        THEN("the executor should fetch all rows, but not return") {
          OutputAqlItemRow row(std::move(block), infos);

          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFiltered() == 0);
          REQUIRE(!row.produced());
          REQUIRE(fetcher.isDone());
          REQUIRE(fetcher.nrCalled() == 3);

          REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
          REQUIRE(traverser->startVertexUsedAt(1) == "v/2");
          REQUIRE(traverser->startVertexUsedAt(2) == "v/3");

          AND_THEN("The output should stay stable") {
            std::tie(state, stats) = testee.produceRow(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(stats.getFiltered() == 0);
            REQUIRE(!row.produced());
            REQUIRE(fetcher.isDone());
            REQUIRE(fetcher.nrCalled() == 3);
          }
        }
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input->steal(), true);
      TraversalExecutor testee(fetcher, infos);
      TraversalStats stats{};

      WHEN("no edges are connected to vertices") {

        THEN("the executor should fetch all rows, but not return") {
          OutputAqlItemRow row(std::move(block), infos);

          for (size_t i = 0; i < 3; ++i) {
            // We expect to wait 3 times
            std::tie(state, stats) = testee.produceRow(row);
            REQUIRE(state == ExecutionState::WAITING);
          }
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFiltered() == 0);
          REQUIRE(!row.produced());
          REQUIRE(fetcher.isDone());
          REQUIRE(fetcher.nrCalled() == 3);

          REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
          REQUIRE(traverser->startVertexUsedAt(1) == "v/2");
          REQUIRE(traverser->startVertexUsedAt(2) == "v/3");

          AND_THEN("The output should stay stable") {
            std::tie(state, stats) = testee.produceRow(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(stats.getFiltered() == 0);
            REQUIRE(!row.produced());
            REQUIRE(fetcher.isDone());
            // WAITING is not part of called counts
            REQUIRE(fetcher.nrCalled() == 3);
          }
        }
      }

      WHEN("edges are connected to vertices") {
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
