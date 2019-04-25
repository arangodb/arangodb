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

#include "RowFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/TraversalExecutor.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"
#include "tests/Mocks/Servers.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;

namespace arangodb {
namespace tests {

namespace aql {

class TestGraph {
 public:
  TestGraph(std::string const& vertexCollection, std::string const& edgeCollection)
      : _vertexCollection(vertexCollection), _edgeCollection(edgeCollection) {}

  void addVertex(std::string key) {
    VPackBuilder vertex;
    vertex.openObject();
    vertex.add(StaticStrings::IdString, VPackValue(_vertexCollection + "/" + key));
    vertex.add(StaticStrings::KeyString, VPackValue(key));
    vertex.add(StaticStrings::RevString, VPackValue("123"));  // just to have it there
    vertex.close();
    auto vslice = vertex.slice();
    arangodb::velocypack::StringRef id(vslice.get(StaticStrings::IdString));
    _dataLake.emplace_back(vertex.steal());
    _vertices.emplace(id, vslice);
  }

  void addEdge(std::string const& from, std::string const& to, std::string const& key) {
    VPackBuilder edge;
    std::string fromVal = _vertexCollection + "/" + from;
    std::string toVal = _vertexCollection + "/" + to;
    edge.openObject();
    edge.add(StaticStrings::IdString, VPackValue(_edgeCollection + "/" + key));
    edge.add(StaticStrings::KeyString, VPackValue(key));
    edge.add(StaticStrings::RevString, VPackValue("123"));  // just to have it there
    edge.add(StaticStrings::FromString, VPackValue(fromVal));
    edge.add(StaticStrings::ToString, VPackValue(toVal));
    edge.close();
    auto eslice = edge.slice();
    _outEdges[arangodb::velocypack::StringRef(eslice.get(StaticStrings::FromString))].emplace_back(eslice);
    _inEdges[arangodb::velocypack::StringRef(eslice.get(StaticStrings::ToString))].emplace_back(eslice);
    _dataLake.emplace_back(edge.steal());
  }

  VPackSlice getVertexData(arangodb::velocypack::StringRef id) const {
    auto const& it = _vertices.find(id);
    REQUIRE(it != _vertices.end());
    return it->second;
  }

  std::vector<VPackSlice> const& getInEdges(arangodb::velocypack::StringRef id) const {
    auto it = _inEdges.find(id);
    if (it == _inEdges.end()) {
      return _noEdges;
    }
    return it->second;
  }

  std::vector<VPackSlice> const& getOutEdges(arangodb::velocypack::StringRef id) const {
    auto it = _outEdges.find(id);
    if (it == _outEdges.end()) {
      return _noEdges;
    }
    return it->second;
  }

 private:
  std::string const _vertexCollection;
  std::string const _edgeCollection;

  std::vector<VPackSlice> _noEdges;
  std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> _dataLake;
  std::unordered_map<arangodb::velocypack::StringRef, VPackSlice> _vertices;
  std::unordered_map<arangodb::velocypack::StringRef, std::vector<VPackSlice>> _outEdges;
  std::unordered_map<arangodb::velocypack::StringRef, std::vector<VPackSlice>> _inEdges;
};

// @brief
// A graph enumerator fakes a PathEnumerator that is inderectly used by the
// TraversalExecutor.
// It mostly does a breath first enumeration on the given TestGraph
// instance originating from the given startVertex.
class GraphEnumerator : public PathEnumerator {
 public:
  GraphEnumerator(Traverser* traverser, std::string const& startVertex,
                  TraverserOptions* opts, TestGraph const& g)
      : PathEnumerator(traverser, startVertex, opts),
        _graph(g),
        _idx(0),
        _depth(0),
        _currentDepth{},
        _nextDepth{arangodb::velocypack::StringRef(startVertex)} {}

  ~GraphEnumerator() {}

  bool next() override {
    ++_idx;
    while (true) {
      if (_idx < _edges.size()) {
        _nextDepth.emplace_back(arangodb::velocypack::StringRef(_edges.at(_idx).get(StaticStrings::ToString)));
        return true;
      } else {
        if (_currentDepth.empty()) {
          if (_nextDepth.empty() || _depth >= _opts->maxDepth) {
            return false;
          }
          _depth++;
          _currentDepth.swap(_nextDepth);
        }
        _edges = _graph.getOutEdges(_currentDepth.back());
        _idx = 0;
        _currentDepth.pop_back();
      }
    }
  }

  arangodb::aql::AqlValue lastVertexToAqlValue() override {
    return AqlValue(_graph.getVertexData(_nextDepth.back()));
  }

  arangodb::aql::AqlValue lastEdgeToAqlValue() override {
    return AqlValue(_edges.at(_idx));
  }

  arangodb::aql::AqlValue pathToAqlValue(VPackBuilder& builder) override {
    return AqlValue(AqlValueHintNull());
  }

 private:
  TestGraph const& _graph;
  size_t _idx;
  size_t _depth;
  std::vector<VPackSlice> _edges;
  std::vector<arangodb::velocypack::StringRef> _currentDepth;
  std::vector<arangodb::velocypack::StringRef> _nextDepth;
};

class TraverserHelper : public Traverser {
 public:
  TraverserHelper(TraverserOptions* opts, transaction::Methods* trx, TestGraph const& g)
      : Traverser(opts, trx), _graph(g) {}

  void setStartVertex(std::string const& value) override {
    _usedVertexAt.push_back(value);
    _enumerator.reset(new GraphEnumerator(this, _usedVertexAt.back(), _opts, _graph));
    _done = false;
  }

  bool getVertex(VPackSlice edge, std::vector<arangodb::velocypack::StringRef>& result) override {
    // Implement
    return false;
  }

  bool getSingleVertex(VPackSlice edge, arangodb::velocypack::StringRef const sourceVertex,
                       uint64_t depth, arangodb::velocypack::StringRef& targetVertex) override {
    // Implement
    return false;
  }

  AqlValue fetchVertexData(arangodb::velocypack::StringRef vid) override {
    VPackSlice v = _graph.getVertexData(vid);
    return AqlValue(v);
  }

  void addVertexToVelocyPack(arangodb::velocypack::StringRef vid, VPackBuilder& builder) override {
    REQUIRE(builder.isOpenArray());
    VPackSlice v = _graph.getVertexData(vid);
    builder.add(v);
    return;
  }

  void destroyEngines() override {}

  std::string const& startVertexUsedAt(uint64_t index) {
    REQUIRE(index < _usedVertexAt.size());
    return _usedVertexAt[index];
  }

  std::string const& currentStartVertex() {
    REQUIRE(!_usedVertexAt.empty());
    return _usedVertexAt.back();
  }

 private:
  std::vector<std::string> _usedVertexAt;

  TestGraph const& _graph;
};

SCENARIO("TraversalExecutor", "[AQL][EXECUTOR][TRAVEXE]") {
  ExecutionState state;
  mocks::MockAqlServer server{};

  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};

  TraverserOptions traversalOptions(fakedQuery.get());
  traversalOptions.minDepth = 1;
  traversalOptions.maxDepth = 1;
  arangodb::transaction::Methods* trx = fakedQuery->trx();
  std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables{};

  TestGraph myGraph("v", "e");
  auto traverserPtr = std::make_unique<TraverserHelper>(&traversalOptions, trx, myGraph);
  GIVEN("using input as start vertex") {
    RegisterId inReg = 0;
    RegisterId outReg = 1;
    auto traverser = traverserPtr.get();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{inReg});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{outReg});
    std::unordered_map<TraversalExecutorInfos::OutputName, RegisterId, TraversalExecutorInfos::OutputNameHash> registerMapping{
        {TraversalExecutorInfos::OutputName::VERTEX, outReg}};

    std::string const noFixed = "";
    TraversalExecutorInfos infos(inputRegisters, outputRegisters, 1, 2, {}, {0},
                                 std::move(traverserPtr), registerMapping,
                                 noFixed, inReg, filterConditionVariables);

    GIVEN("there are no rows upstream") {
      VPackBuilder input;

      WHEN("the producer does not wait") {
        SingleRowFetcherHelper<false> fetcher(input.steal(), false);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        THEN("the executor should return DONE and no result") {
          OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                  infos.registersToKeep(), infos.registersToClear());
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }

      WHEN("the producer waits") {
        SingleRowFetcherHelper<false> fetcher(input.steal(), true);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        THEN("the executor should first return WAIT and no result") {
          OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                  infos.registersToKeep(), infos.registersToClear());
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!result.produced());
          REQUIRE(stats.getFiltered() == 0);

          AND_THEN("the executor should return DONE and no result") {
            std::tie(state, stats) = testee.produceRows(result);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!result.produced());
            REQUIRE(stats.getFiltered() == 0);
          }
        }
      }
    }

    GIVEN("there are rows in the upstream") {
      myGraph.addVertex("1");
      myGraph.addVertex("2");
      myGraph.addVertex("3");
      auto input = VPackParser::fromJson(R"([["v/1"], ["v/2"], ["v/3"]])");

      WHEN("the producer does not wait") {
        SingleRowFetcherHelper<false> fetcher(input->steal(), false);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        WHEN("no edges are connected to vertices") {
          THEN("the executor should fetch all rows, but not return") {
            OutputAqlItemRow row(std::move(block), infos.getOutputRegisters(),
                                 infos.registersToKeep(), infos.registersToClear());

            std::tie(state, stats) = testee.produceRows(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(stats.getFiltered() == 0);
            REQUIRE(!row.produced());
            REQUIRE(fetcher.isDone());
            REQUIRE(fetcher.nrCalled() == 3);

            REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(1) == "v/2");
            REQUIRE(traverser->startVertexUsedAt(2) == "v/3");

            AND_THEN("The output should stay stable") {
              std::tie(state, stats) = testee.produceRows(row);
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
        SingleRowFetcherHelper<false> fetcher(input->steal(), true);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        WHEN("no edges are connected to vertices") {
          THEN("the executor should fetch all rows, but not return") {
            OutputAqlItemRow row(std::move(block), infos.getOutputRegisters(),
                                 infos.registersToKeep(), infos.registersToClear());

            for (size_t i = 0; i < 3; ++i) {
              // We expect to wait 3 times
              std::tie(state, stats) = testee.produceRows(row);
              REQUIRE(state == ExecutionState::WAITING);
            }
            std::tie(state, stats) = testee.produceRows(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(stats.getFiltered() == 0);
            REQUIRE(!row.produced());
            REQUIRE(fetcher.isDone());
            REQUIRE(fetcher.nrCalled() == 3);

            REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(1) == "v/2");
            REQUIRE(traverser->startVertexUsedAt(2) == "v/3");

            AND_THEN("The output should stay stable") {
              std::tie(state, stats) = testee.produceRows(row);
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
          myGraph.addEdge("1", "2", "1->2");
          myGraph.addEdge("2", "3", "2->3");
          myGraph.addEdge("3", "1", "3->1");
          ExecutionStats total;
          THEN("the executor should fetch all rows") {
            OutputAqlItemRow row(std::move(block), infos.getOutputRegisters(),
                                 infos.registersToKeep(), infos.registersToClear());

            for (int64_t i = 0; i < 3; ++i) {
              // We expect to wait 3 times
              std::tie(state, stats) = testee.produceRows(row);
              total += stats;
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row.produced());
              std::tie(state, stats) = testee.produceRows(row);
              REQUIRE(row.produced());
              REQUIRE(state == ExecutionState::HASMORE);
              row.advanceRow();
              total += stats;
              REQUIRE(total.filtered == 0);
              /* We cannot ASSERT this because of internally to complex
              mechanism */
              // REQUIRE(total.scannedIndex == i + 1);
              REQUIRE(fetcher.nrCalled() == (uint64_t)(i + 1));
            }
            REQUIRE(fetcher.isDone());
            // The traverser will lie
            std::tie(state, stats) = testee.produceRows(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row.produced());

            REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(1) == "v/2");
            REQUIRE(traverser->startVertexUsedAt(2) == "v/3");

            std::vector<std::string> expectedResult{"v/2", "v/3", "v/1"};
            auto block = row.stealBlock();
            for (std::size_t index = 0; index < 3; index++) {
              AqlValue value = block->getValue(index, outReg);
              REQUIRE(value.isObject());
              REQUIRE(arangodb::basics::VelocyPackHelper::compare(
                          value.slice(),
                          myGraph.getVertexData(arangodb::velocypack::StringRef(expectedResult.at(index))),
                          false) == 0);
            }
          }
        }
      }
    }
  }

  GIVEN("using constant as start vertex") {
    RegisterId outReg = 1;
    auto traverser = traverserPtr.get();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{1});
    std::unordered_map<TraversalExecutorInfos::OutputName, RegisterId, TraversalExecutorInfos::OutputNameHash> registerMapping{
        {TraversalExecutorInfos::OutputName::VERTEX, outReg}};

    std::string const fixed = "v/1";
    TraversalExecutorInfos infos(inputRegisters, outputRegisters, 1, 2, {}, {0},
                                 std::move(traverserPtr), registerMapping, fixed,
                                 ExecutionNode::MaxRegisterId, filterConditionVariables);

    GIVEN("there are no rows upstream") {
      VPackBuilder input;

      WHEN("the producer does not wait") {
        SingleRowFetcherHelper<false> fetcher(input.steal(), false);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        THEN("the executor should return DONE and no result") {
          OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                  infos.registersToKeep(), infos.registersToClear());
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }

      WHEN("the producer waits") {
        SingleRowFetcherHelper<false> fetcher(input.steal(), true);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        THEN("the executor should first return WAIT and no result") {
          OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                  infos.registersToKeep(), infos.registersToClear());
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!result.produced());
          REQUIRE(stats.getFiltered() == 0);

          AND_THEN("the executor should return DONE and no result") {
            std::tie(state, stats) = testee.produceRows(result);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!result.produced());
            REQUIRE(stats.getFiltered() == 0);
          }
        }
      }
    }

    GIVEN("there are rows in the upstream") {
      myGraph.addVertex("1");
      myGraph.addVertex("2");
      myGraph.addVertex("3");
      auto input = VPackParser::fromJson(R"([ ["v/1"], ["v/2"], ["v/3"] ])");

      WHEN("the producer does not wait") {
        SingleRowFetcherHelper<false> fetcher(input->steal(), false);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        WHEN("no edges are connected to vertices") {
          THEN("the executor should fetch all rows, but not return") {
            OutputAqlItemRow row(std::move(block), infos.getOutputRegisters(),
                                 infos.registersToKeep(), infos.registersToClear());

            std::tie(state, stats) = testee.produceRows(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(stats.getFiltered() == 0);
            REQUIRE(!row.produced());
            REQUIRE(fetcher.isDone());
            REQUIRE(fetcher.nrCalled() == 3);

            REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(1) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(2) == "v/1");

            AND_THEN("The output should stay stable") {
              std::tie(state, stats) = testee.produceRows(row);
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
        SingleRowFetcherHelper<false> fetcher(input->steal(), true);
        TraversalExecutor testee(fetcher, infos);
        TraversalStats stats{};

        WHEN("no edges are connected to vertices") {
          THEN("the executor should fetch all rows, but not return") {
            OutputAqlItemRow row(std::move(block), infos.getOutputRegisters(),
                                 infos.registersToKeep(), infos.registersToClear());

            for (size_t i = 0; i < 3; ++i) {
              // We expect to wait 3 times
              std::tie(state, stats) = testee.produceRows(row);
              REQUIRE(state == ExecutionState::WAITING);
            }
            std::tie(state, stats) = testee.produceRows(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(stats.getFiltered() == 0);
            REQUIRE(!row.produced());
            REQUIRE(fetcher.isDone());
            REQUIRE(fetcher.nrCalled() == 3);

            REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(1) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(2) == "v/1");

            AND_THEN("The output should stay stable") {
              std::tie(state, stats) = testee.produceRows(row);
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
          myGraph.addEdge("1", "2", "1->2");
          myGraph.addEdge("2", "3", "2->3");
          myGraph.addEdge("3", "1", "3->1");
          ExecutionStats total;
          THEN("the executor should fetch all rows, but not return") {
            OutputAqlItemRow row(std::move(block), infos.getOutputRegisters(),
                                 infos.registersToKeep(), infos.registersToClear());

            for (int64_t i = 0; i < 3; ++i) {
              // We expect to wait 3 times
              std::tie(state, stats) = testee.produceRows(row);
              total += stats;
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row.produced());
              std::tie(state, stats) = testee.produceRows(row);
              REQUIRE(row.produced());
              REQUIRE(state == ExecutionState::HASMORE);
              row.advanceRow();
              total += stats;
              REQUIRE(total.filtered == 0);
              /* We cannot ASSERT this because of internally to complex
              mechanism */
              // REQUIRE(total.scannedIndex == i + 1);
              REQUIRE(fetcher.nrCalled() == (uint64_t)(i + 1));
            }
            REQUIRE(fetcher.isDone());
            // The traverser will lie
            std::tie(state, stats) = testee.produceRows(row);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row.produced());

            REQUIRE(traverser->startVertexUsedAt(0) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(1) == "v/1");
            REQUIRE(traverser->startVertexUsedAt(2) == "v/1");

            std::vector<std::string> expectedResult{"v/2", "v/2", "v/2"};
            auto block = row.stealBlock();
            for (std::size_t index = 0; index < 3; index++) {
              AqlValue value = block->getValue(index, outReg);
              REQUIRE(value.isObject());
              REQUIRE(arangodb::basics::VelocyPackHelper::compare(
                          value.slice(),
                          myGraph.getVertexData(arangodb::velocypack::StringRef(expectedResult.at(index))),
                          false) == 0);
            }
          }
        }
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
