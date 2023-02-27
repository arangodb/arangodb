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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gtest/gtest.h"

#include "../3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/norm.hpp"
#include <filesystem>

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Aql/Function.h"
#include "Basics/DownCast.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/Search.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

#include "VocBase/Methods/Collections.h"
#include "VocBase/vocbase.h"

#include <absl/strings/substitute.h>

inline auto GetLinkVersions() noexcept {
  return testing::Values(arangodb::iresearch::LinkVersion::MIN,
                         arangodb::iresearch::LinkVersion::MAX);
}

inline auto GetIndexVersions() noexcept {
  return testing::Values(arangodb::iresearch::LinkVersion::MAX);
}

class IResearchQueryTest
    : public ::testing::TestWithParam<arangodb::iresearch::LinkVersion>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 private:
  TRI_vocbase_t* _vocbase{nullptr};

 protected:
  arangodb::tests::mocks::MockAqlServer server;

  virtual arangodb::ViewType type() const {
    return arangodb::ViewType::kArangoSearch;
  }

  IResearchQueryTest();

  TRI_vocbase_t& vocbase() {
    TRI_ASSERT(_vocbase != nullptr);
    return *_vocbase;
  }

  arangodb::iresearch::LinkVersion linkVersion() const noexcept {
    return GetParam();
  }

  arangodb::iresearch::LinkVersion version() const noexcept {
    return GetParam();
  }
};
namespace arangodb::tests {
class QueryTest : public IResearchQueryTest {
 protected:
  void createCollections();

  void checkView(LogicalView const& view, size_t expected = 2,
                 std::string_view viewName = "testView");

  void createView(std::string_view definition1, std::string_view definition2);

  void createIndexes(std::string_view definition1,
                     std::string_view definition2);

  void createSearch();

  bool runQuery(std::string_view query);

  bool runQuery(std::string_view query, std::span<VPackSlice const> expected);

  bool runQuery(std::string_view query, VPackValue v);

  template<typename It>
  bool runQuery(std::string_view query, It expected, size_t expectedCount) {
    // TODO remove string
    auto r = executeQuery(_vocbase, std::string{query});
    EXPECT_TRUE(r.result.ok()) << r.result.errorMessage();
    if (!r.data) {
      return false;
    }
    auto slice = r.data->slice();
    EXPECT_TRUE(slice.isArray()) << slice.toString();
    if (!slice.isArray()) {
      return false;
    }

    size_t errorCount = 0;
    VPackArrayIterator it{slice};
    EXPECT_EQ(it.size(), expectedCount);
    for (size_t i = 0; it.valid() && i < expectedCount; ++it, ++i, ++expected) {
      auto const resolved = it.value().resolveExternals();
      if constexpr (std::is_same_v<
                        typename std::iterator_traits<It>::value_type,
                        VPackSlice>) {
        errorCount += !checkSlices(resolved, *expected);
      } else {
        errorCount += !checkSlices(resolved, expected->slice());
      }
    }
    EXPECT_EQ(errorCount, 0U);
    return it.size() == expectedCount && errorCount == 0;
  }

  TRI_vocbase_t _vocbase{testDBInfo(server.server())};
  std::vector<velocypack::Builder> _insertedDocs;

 private:
  static bool checkSlices(VPackSlice actual, VPackSlice expected);
};

}  // namespace arangodb::tests
