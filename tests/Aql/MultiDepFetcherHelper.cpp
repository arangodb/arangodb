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
#include "Aql/AqlItemRowPrinter.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ShadowAqlItemRow.h"

#include <velocypack/Slice.h>


using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

constexpr auto options = &velocypack::Options::Defaults;

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
        auto const& expectedState = expected.first;
        auto const& expectedRow = expected.second;
        auto actual = testee.fetchRowForDependency(args.dependency, args.atMost);
        auto const& actualState = actual.first;
        auto const& actualRow = actual.second;
        EXPECT_EQ(expectedState, actualState) << "during step " << i;
        EXPECT_TRUE(expectedRow.equates(actualRow, options))
            << "  expected: " << expectedRow << "\n  actual: " << actualRow
            << "\n  during step " << i;
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
        auto const& expectedState = expected.first;
        auto const& expectedRow = expected.second;
        auto actual = testee.fetchShadowRow(args.atMost);
        auto const& actualState = actual.first;
        auto const& actualRow = actual.second;
        EXPECT_EQ(expectedState, actualState) << "during step " << i;
        EXPECT_TRUE(expectedRow.equates(actualRow, options))
            << "  expected: " << expectedRow << "\n  actual: " << actualRow
            << "\n  during step " << i;
      }

     private:
      MultiDependencySingleRowFetcher& testee;
      size_t const i;
    } visitor{testee, i};
    boost::apply_visitor(visitor, iop);
    ++i;
  }
}
