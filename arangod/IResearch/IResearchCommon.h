////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/system-compiler.h"
#include "Logger/LogTopic.h"

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief the delimiter used to separate jSON nesting levels when
/// generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
inline constexpr char const NESTING_LEVEL_DELIMITER = '.';

////////////////////////////////////////////////////////////////////////////////
/// @brief the prefix used to denote start of jSON list offset when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
inline constexpr char const NESTING_LIST_OFFSET_PREFIX = '[';

////////////////////////////////////////////////////////////////////////////////
/// @brief the suffix used to denote end of jSON list offset when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
inline constexpr char const NESTING_LIST_OFFSET_SUFFIX = ']';

[[maybe_unused]] extern LogTopic TOPIC;
[[maybe_unused]] inline constexpr std::string_view
    IRESEARCH_INVERTED_INDEX_TYPE = "inverted";

////////////////////////////////////////////////////////////////////////////////
/// @brief defines the implementation version of the iresearch view interface
///        e.g. which how data is stored in iresearch
////////////////////////////////////////////////////////////////////////////////
enum class ViewVersion : uint32_t {
  MIN = 1,
  MAX = 1  // the latest
};

////////////////////////////////////////////////////////////////////////////////
/// @brief defines the implementation version of the iresearch index interface
///        e.g. which how data is stored in iresearch
////////////////////////////////////////////////////////////////////////////////
enum class LinkVersion : uint32_t {
  MIN = 0,
  MAX = 1  // the latest
};

////////////////////////////////////////////////////////////////////////////////
/// @return default index version
////////////////////////////////////////////////////////////////////////////////
constexpr LinkVersion getDefaultVersion(bool isUserRequest) noexcept {
  return isUserRequest ? LinkVersion::MAX : LinkVersion::MIN;
}

////////////////////////////////////////////////////////////////////////////////
/// @return format identifier according to a specified index version
////////////////////////////////////////////////////////////////////////////////
constexpr std::string_view getFormat(LinkVersion version) noexcept {
  constexpr std::array<std::string_view, 2> IRESEARCH_FORMATS{
      "1_3simd",  // the old storage format used with IResearch index
      "1_4simd"   // the current storage format used with IResearch index
  };

  return IRESEARCH_FORMATS[static_cast<uint32_t>(version)];
}

struct StaticStrings {
  static constexpr std::string_view ViewType = "arangosearch";
  static constexpr std::string_view SearchType = "search";

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        corresponding link definitions
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view LinksField{"links"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        corresponding link definitions
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view VersionField{"version"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the id of the field in the IResearch Link definition denoting the
  ///        corresponding IResearch View
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view ViewIdField{"view"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        referenced analyzer definitions
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view AnalyzerDefinitionsField{
      "analyzerDefinitions"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer name
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view AnalyzerNameField{"name"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer type
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view AnalyzerTypeField{"type"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer properties
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view AnalyzerPropertiesField{"properties"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer features
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view AnalyzerFeaturesField{"features"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        primary sort
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view PrimarySortField{"primarySort"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        primary sort compression
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view PrimarySortCompressionField{
      "primarySortCompression"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        stored values
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view StoredValuesField{"storedValues"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        corresponding collection name in cluster (not shard name!)
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view CollectionNameField{"collectionName"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        time in Ms between running consolidations
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view ConsolidationIntervalMsec{
      "consolidationIntervalMsec"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        time in Ms between running commits
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view CommitIntervalMsec{"commitIntervalMsec"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        number of completed consolidtions before cleanup is run
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view CleanupIntervalStep{"cleanupIntervalStep"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///         consolidation policy properties
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view ConsolidationPolicy{"consolidationPolicy"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        maximum number of concurrent active writers (segments) that perform
  ///        a transaction. Other writers (segments) wait till current active
  ///        writers (segments) finish.
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view WritebufferActive{"writebufferActive"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        maximum number of writers (segments) cached in the pool.
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view WritebufferIdle{"writebufferIdle"};

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        maximum memory byte size per writer (segment) before a writer
  ///        (segment) flush is triggered
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::string_view WritebufferSizeMax{"writebufferSizeMax"};
};

}  // namespace iresearch
}  // namespace arangodb
