////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/TraversalExecutor.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"

#include "Aql/AqlItemBlockHelper.h"
#include "Aql/RowFetcherHelper.h"
#include "Mocks/Servers.h"

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
  TestGraph(std::string vertexCollection, std::string edgeCollection)
      : _vertexCollection(std::move(vertexCollection)),
        _edgeCollection(std::move(edgeCollection)) {}

  void addVertex(std::string const& key) {
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
    _outEdges[arangodb::velocypack::StringRef(eslice.get(StaticStrings::FromString))]
        .emplace_back(eslice);
    _inEdges[arangodb::velocypack::StringRef(eslice.get(StaticStrings::ToString))]
        .emplace_back(eslice);
    _dataLake.emplace_back(edge.steal());
  }

  VPackSlice getVertexData(arangodb::velocypack::StringRef id) const {
    auto const& it = _vertices.find(id);
    TRI_ASSERT(it != _vertices.end());
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
// A graph enumerator fakes a PathEnumerator that is indirectly used by the
// TraversalExecutor.
// It mostly does a breath first enumeration on the given TestGraph
// instance originating from the given startVertex.
class GraphEnumerator : public PathEnumerator {
 public:
  GraphEnumerator(Traverser* traverser, TraverserOptions* opts, TestGraph const& g)
      : PathEnumerator(traverser, opts),
        _graph(g),
        _idx(0),
        _depth(0) {}

  ~GraphEnumerator() = default;

  void setStartVertex(arangodb::velocypack::StringRef startVertex) override {
    PathEnumerator::setStartVertex(startVertex);

    _idx = 0;
    _depth = 0;
    _currentDepth.clear();
    _nextDepth.clear();
    _nextDepth.emplace_back(startVertex);
  }

  bool next() override {
    ++_idx;
    while (true) {
      if (_idx < _edges.size()) {
        _nextDepth.emplace_back(arangodb::velocypack::StringRef(
            _edges.at(_idx).get(StaticStrings::ToString)));
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
  TraverserHelper(TraverserOptions* opts, TestGraph const& g)
      : Traverser(opts), _graph(g) {
    _enumerator.reset(new GraphEnumerator(this, _opts, _graph));
  }

  void setStartVertex(std::string const& value) override {
    _usedVertexAt.push_back(value);
    _enumerator->setStartVertex(arangodb::velocypack::StringRef(_usedVertexAt.back()));
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

  bool getVertex(arangodb::velocypack::StringRef vertex, size_t depth) override {
    // Implement
    return false;
  }

  AqlValue fetchVertexData(arangodb::velocypack::StringRef vid) override {
    VPackSlice v = _graph.getVertexData(vid);
    return AqlValue(v);
  }

  void addVertexToVelocyPack(arangodb::velocypack::StringRef vid, VPackBuilder& builder) override {
    TRI_ASSERT(builder.isOpenArray());
    VPackSlice v = _graph.getVertexData(vid);
    builder.add(v);
    return;
  }

  void destroyEngines() override {}
  void clear() override {}

  std::string const& startVertexUsedAt(uint64_t index) {
    TRI_ASSERT(index < _usedVertexAt.size());
    return _usedVertexAt[index];
  }

  std::string const& currentStartVertex() {
    TRI_ASSERT(!_usedVertexAt.empty());
    return _usedVertexAt.back();
  }

 private:
  std::vector<std::string> _usedVertexAt;

  TestGraph const& _graph;
};

static TraverserOptions generateOptions(arangodb::aql::Query* query, size_t min, size_t max) {
  TraverserOptions options{*query};
  options.minDepth = min;
  options.maxDepth = max;
  return options;
}

class TraversalExecutorTestInputStartVertex : public ::testing::Test {
 protected:
  ExecutorState state;
  mocks::MockAqlServer server;

  std::unique_ptr<arangodb::aql::Query> fakedQuery;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  TraverserOptions traversalOptions;
  std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables;

  TestGraph myGraph;
  std::unique_ptr<TraverserHelper> traverserPtr;

  RegisterId inReg;
  RegisterId outReg;
  TraverserHelper* traverser;
  std::unordered_map<TraversalExecutorInfos::OutputName, RegisterId, TraversalExecutorInfos::OutputNameHash> registerMapping;

  std::string const noFixed;
  RegisterInfos registerInfos;
  TraversalExecutorInfos executorInfos;

  TraversalExecutorTestInputStartVertex()
      : fakedQuery(server.createFakeQuery()),
        itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)),
        traversalOptions(generateOptions(fakedQuery.get(), 1, 1)),
        myGraph("v", "e"),
        traverserPtr(std::make_unique<TraverserHelper>(&traversalOptions, myGraph)),
        inReg(0),
        outReg(1),
        traverser(traverserPtr.get()),
        registerMapping{{TraversalExecutorInfos::OutputName::VERTEX, outReg}},
        noFixed(""),
        registerInfos(RegIdSet{inReg}, RegIdSet{outReg}, 1, 2, {}, {RegIdSet{0}}),
        executorInfos(std::move(traverserPtr), registerMapping, noFixed, inReg, filterConditionVariables)

  {}
};

TEST_F(TraversalExecutorTestInputStartVertex, there_are_no_rows_upstream_producer_doesnt_produce) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false);

  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock = buildBlock<1>(itemBlockManager, MatrixBuilder<1>{{{}}});
  auto input = AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  OutputAqlItemRow result(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear());
  AqlCall call;

  std::tie(state, stats, call) = testee.produceRows(input, result);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(TraversalExecutorTestInputStartVertex, there_are_rows_upstream_producer_produced) {
  myGraph.addVertex("1");
  myGraph.addVertex("2");
  myGraph.addVertex("3");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{R"("v/1")"}}}, {{{R"("v/2")"}}}, {{{R"("v/3")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  OutputAqlItemRow row(std::move(block), registerInfos.getOutputRegisters(),
                       registerInfos.registersToKeep(), registerInfos.registersToClear());
  auto call = AqlCall{};

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());

  ASSERT_EQ(traverser->startVertexUsedAt(0), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(1), "v/2");
  ASSERT_EQ(traverser->startVertexUsedAt(2), "v/3");

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());
}

TEST_F(TraversalExecutorTestInputStartVertex, there_are_rows_no_edges_are_connected) {
  myGraph.addVertex("1");
  myGraph.addVertex("2");
  myGraph.addVertex("3");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), true);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{R"("v/1")"}}}, {{{R"("v/2")"}}}, {{{R"("v/3")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  OutputAqlItemRow row(std::move(block), registerInfos.getOutputRegisters(),
                       registerInfos.registersToKeep(), registerInfos.registersToClear());
  auto call = AqlCall{};

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());

  ASSERT_EQ(traverser->startVertexUsedAt(0), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(1), "v/2");
  ASSERT_EQ(traverser->startVertexUsedAt(2), "v/3");

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());
  // WAITING is not part of called counts
}

TEST_F(TraversalExecutorTestInputStartVertex, there_are_rows_upstream_edges_are_connected) {
  myGraph.addVertex("1");
  myGraph.addVertex("2");
  myGraph.addVertex("3");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), true);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{R"("v/1")"}}}, {{{R"("v/2")"}}}, {{{R"("v/3")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  myGraph.addEdge("1", "2", "1->2");
  myGraph.addEdge("2", "3", "2->3");
  myGraph.addEdge("3", "1", "3->1");

  ExecutionStats total;
  OutputAqlItemRow row(std::move(block), registerInfos.getOutputRegisters(),
                       registerInfos.registersToKeep(), registerInfos.registersToClear());
  auto call = AqlCall{};

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(row.produced());

  ASSERT_EQ(traverser->startVertexUsedAt(0), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(1), "v/2");
  ASSERT_EQ(traverser->startVertexUsedAt(2), "v/3");

  std::vector<std::string> expectedResult{"v/2", "v/3", "v/1"};
  auto block = row.stealBlock();
  for (std::size_t index = 0; index < 3; index++) {
    AqlValue value = block->getValue(index, outReg);
    ASSERT_TRUE(value.isObject());
    ASSERT_TRUE(arangodb::basics::VelocyPackHelper::compare(
                    value.slice(),
                    myGraph.getVertexData(
                        arangodb::velocypack::StringRef(expectedResult.at(index))),
                    false) == 0);
  }
}

class TraversalExecutorTestConstantStartVertex : public ::testing::Test {
 protected:
  ExecutorState state;
  mocks::MockAqlServer server;

  std::unique_ptr<arangodb::aql::Query> fakedQuery;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  TraverserOptions traversalOptions;
  std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables;

  TestGraph myGraph;
  std::unique_ptr<TraverserHelper> traverserPtr;

  RegisterId outReg;
  TraverserHelper* traverser;
  std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::unordered_map<TraversalExecutorInfos::OutputName, RegisterId, TraversalExecutorInfos::OutputNameHash> registerMapping;

  std::string const fixed;
  RegisterInfos registerInfos;
  TraversalExecutorInfos executorInfos;

  TraversalExecutorTestConstantStartVertex()
      : fakedQuery(server.createFakeQuery()),
        itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)),
        traversalOptions(generateOptions(fakedQuery.get(), 1, 1)),
        myGraph("v", "e"),
        traverserPtr(std::make_unique<TraverserHelper>(&traversalOptions, myGraph)),
        outReg(1),
        traverser(traverserPtr.get()),
        registerMapping{{TraversalExecutorInfos::OutputName::VERTEX, outReg}},
        fixed("v/1"),
        registerInfos({}, RegIdSet{1}, 1, 2, {}, {RegIdSet{0}}),
        executorInfos(std::move(traverserPtr), registerMapping, fixed,
                      RegisterPlan::MaxRegisterId, filterConditionVariables) {}
};

TEST_F(TraversalExecutorTestConstantStartVertex, no_rows_upstream_producer_doesnt_produce) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock = buildBlock<1>(itemBlockManager, MatrixBuilder<1>{{{{}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  OutputAqlItemRow result(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear());
  AqlCall call;

  std::tie(state, stats, call) = testee.produceRows(input, result);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(TraversalExecutorTestConstantStartVertex, no_rows_upstream) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), true);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock = buildBlock<1>(itemBlockManager, MatrixBuilder<1>{{{{}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  OutputAqlItemRow result(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(), registerInfos.registersToClear());
  AqlCall call;

  std::tie(state, stats, call) = testee.produceRows(input, result);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);
}

TEST_F(TraversalExecutorTestConstantStartVertex, rows_upstream_producer_doesnt_wait) {
  myGraph.addVertex("1");
  myGraph.addVertex("2");
  myGraph.addVertex("3");

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{R"("v/1")"}}}, {{{R"("v/2")"}}}, {{{R"("v/3")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  OutputAqlItemRow row(std::move(block), registerInfos.getOutputRegisters(),
                       registerInfos.registersToKeep(), registerInfos.registersToClear());
  AqlCall call;

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());

  ASSERT_EQ(traverser->startVertexUsedAt(0), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(1), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(2), "v/1");

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());
}

TEST_F(TraversalExecutorTestConstantStartVertex, rows_upstream_producer_waits_no_edges_connected) {
  myGraph.addVertex("1");
  myGraph.addVertex("2");
  myGraph.addVertex("3");

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), true);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};
  OutputAqlItemRow row(std::move(block), registerInfos.getOutputRegisters(),
                       registerInfos.registersToKeep(), registerInfos.registersToClear());

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{R"("v/1")"}}}, {{{R"("v/2")"}}}, {{{R"("v/3")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  AqlCall call;

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());

  ASSERT_EQ(traverser->startVertexUsedAt(0), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(1), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(2), "v/1");

  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());
  // WAITING is not part of called counts
}

TEST_F(TraversalExecutorTestConstantStartVertex, rows_edges_connected) {
  myGraph.addVertex("1");
  myGraph.addVertex("2");
  myGraph.addVertex("3");

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, VPackParser::fromJson("[]")->steal(), true);
  TraversalExecutor testee(fetcher, executorInfos);
  TraversalStats stats{};
  myGraph.addEdge("1", "2", "1->2");
  myGraph.addEdge("2", "3", "2->3");
  myGraph.addEdge("3", "1", "3->1");
  ExecutionStats total;
  OutputAqlItemRow row(std::move(block), registerInfos.getOutputRegisters(),
                       registerInfos.registersToKeep(), registerInfos.registersToClear());

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{R"("v/1")"}}}, {{{R"("v/2")"}}}, {{{R"("v/3")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, 0, inputBlock, 0};

  AqlCall call;

  // The traverser will lie
  std::tie(state, stats, call) = testee.produceRows(input, row);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(row.produced());

  ASSERT_EQ(traverser->startVertexUsedAt(0), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(1), "v/1");
  ASSERT_EQ(traverser->startVertexUsedAt(2), "v/1");

  std::vector<std::string> expectedResult{"v/2", "v/2", "v/2"};
  auto block = row.stealBlock();
  for (std::size_t index = 0; index < 3; index++) {
    AqlValue value = block->getValue(index, outReg);
    ASSERT_TRUE(value.isObject());
    ASSERT_TRUE(arangodb::basics::VelocyPackHelper::compare(
                    value.slice(),
                    myGraph.getVertexData(
                        arangodb::velocypack::StringRef(expectedResult.at(index))),
                    false) == 0);
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
