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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include "utils/string.hpp"
#include "Basics/system-compiler.h"
#include "Logger/LogTopic.h"
#include "VocBase/LogicalDataSource.h"

namespace arangodb {
namespace iresearch {

LogicalDataSource::Type const& dataSourceType();
LogTopic& logTopic();

[[maybe_unused]] static auto& DATA_SOURCE_TYPE = dataSourceType();
[[maybe_unused]] extern LogTopic TOPIC;

inline ::iresearch::string_ref const PK_COLUMN("@_PK");
////////////////////////////////////////////////////////////////////////////////
/// @brief defines the implementation version of the iresearch view interface
///        e.g. which how data is stored in iresearch
////////////////////////////////////////////////////////////////////////////////
enum class ViewVersion : uint32_t {
  MIN = 1,
  MAX = 1  // the latest
};

////////////////////////////////////////////////////////////////////////////////
/// @brief defines the implementation version of the iresearch link interface
///        e.g. which how data is stored in iresearch
////////////////////////////////////////////////////////////////////////////////
enum class LinkVersion : uint32_t {
  MIN = 0,
  MAX = 1  // the latest
};

////////////////////////////////////////////////////////////////////////////////
/// @return default link version
////////////////////////////////////////////////////////////////////////////////
constexpr LinkVersion getDefaultVersion(bool isUserRequest) noexcept {
  return isUserRequest ? LinkVersion::MAX : LinkVersion::MIN;
}

////////////////////////////////////////////////////////////////////////////////
/// @return format identifier according to a specified link version
////////////////////////////////////////////////////////////////////////////////
constexpr std::string_view getFormat(LinkVersion version) noexcept {
  constexpr std::array<std::string_view, 2> IRESEARCH_FORMATS{
      "1_3simd",  // the old storage format used with IResearch index
      "1_4simd"   // the current storage format used with IResearch index
  };

  return IRESEARCH_FORMATS[static_cast<uint32_t>(version)];
}

struct StaticStrings {
  static constexpr velocypack::StringRef DataSourceType{"arangosearch"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        corresponding link definitions
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef LinksField{"links"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        corresponding link definitions
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef VersionField{"version"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the id of the field in the IResearch Link definition denoting the
  ///        corresponding IResearch View
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef ViewIdField{"view"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief attribute name for storing link/inverted index errors
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef LinkError{"error"};
  static constexpr velocypack::StringRef LinkErrorOutOfSync{"outOfSync"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        referenced analyzer definitions
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef AnalyzerDefinitionsField{
      "analyzerDefinitions"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer name
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef AnalyzerNameField{"name"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer type
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef AnalyzerTypeField{"type"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer properties
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef AnalyzerPropertiesField{"properties"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer features
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef AnalyzerFeaturesField{"features"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        primary sort
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef PrimarySortField{"primarySort"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        primary sort compression
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef PrimarySortCompressionField{
      "primarySortCompression"};

  static constexpr velocypack::StringRef kPrimarySortCacheField{
      "primarySortCache"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        stored values
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef StoredValuesField{"storedValues"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        corresponding collection name in cluster (not shard name!)
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr velocypack::StringRef CollectionNameField{"collectionName"};

  static constexpr velocypack::StringRef kCacheField{"cache"};

  static constexpr velocypack::StringRef kCachePrimaryKeyField{"cachePrimaryKey"};
};

}  // namespace iresearch
}  // namespace arangodb
