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

#include "MultiDepFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutorInfos.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {
std::ostream& operator<<(std::ostream& stream, arangodb::aql::InputAqlItemRow const& row);
}
}

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

std::ostream& arangodb::aql::operator<<(std::ostream& stream, InputAqlItemRow const& row) {
  if (!row) {
    return stream << "InvalidRow{}";
  }

  auto monitor = ResourceMonitor{};
  auto manager = AqlItemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
  auto regs = arangodb::aql::make_shared_unordered_set(row.getNrRegisters());
  // copy the row into a block, just so we can read its registers.
  auto block = row.cloneToBlock(manager, *regs, row.getNrRegisters());
  EXPECT_EQ(1, block->size());
  EXPECT_EQ(row.getNrRegisters(), block->getNrRegs());

  stream << "Row{";
  if (block->getNrRegs() > 0) {
    stream << block->getValue(0, 0).slice().toJson();
  }
  for (size_t i = 1; i < block->getNrRegs(); ++i) {
    stream << ", ";
    stream << block->getValue(0, i).slice().toJson();
  }
  stream << "}";
  return stream;
}

void arangodb::tests::aql::runFetcher(arangodb::aql::MultiDependencySingleRowFetcher& testee,
                                      std::vector<FetcherIOPair> const& inputOutputPairs) {
  size_t i = 0;
  for (auto const& iop : inputOutputPairs) {
    struct Visitor : public boost::static_visitor<> {
      Visitor(MultiDependencySingleRowFetcher& testee, size_t i)
          : testee(testee), i(i) {}
      void operator()(ConcreteFetcherIOPair<PrefetchNumberOfRows> const& iop) {
        auto const& args = iop.first;
        auto const& expected = iop.second;
        auto actual = testee.preFetchNumberOfRows(args.atMost);
        EXPECT_EQ(expected, actual) << "during step " << i;
      }
      void operator()(ConcreteFetcherIOPair<FetchRowForDependency> const& iop) {
        auto const& args = iop.first;
        auto const& expected = iop.second;
        auto actual = testee.fetchRowForDependency(args.dependency, args.atMost);
        EXPECT_EQ(expected, actual) << "during step " << i;
      }
      void operator()(ConcreteFetcherIOPair<SkipRowsForDependency> const& iop) {
        auto const& args = iop.first;
        auto const& expected = iop.second;
        auto actual = testee.skipRowsForDependency(args.dependency, args.atMost);
        EXPECT_EQ(expected, actual) << "during step " << i;
      }
      void operator()(ConcreteFetcherIOPair<FetchShadowRow> const& iop) {
        auto const& args = iop.first;
        auto const& expected = iop.second;
        auto actual = testee.fetchShadowRow(args.atMost);
        EXPECT_EQ(expected, actual) << "during step " << i;
      }

     private:
      MultiDependencySingleRowFetcher& testee;
      size_t const i;
    } visitor{testee, i};
    boost::apply_visitor(visitor, iop);
    ++i;
  }
}
