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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_COMMON_H
#define ARANGOD_IRESEARCH__IRESEARCH_COMMON_H 1

#include "Basics/system-compiler.h"
#include "Logger/LogTopic.h"
#include "VocBase/LogicalDataSource.h"

namespace arangodb {
namespace iresearch {

arangodb::LogicalDataSource::Type const& dataSourceType();
arangodb::LogTopic& logTopic();

ADB_IGNORE_UNUSED static auto& DATA_SOURCE_TYPE = dataSourceType();
ADB_IGNORE_UNUSED extern arangodb::LogTopic TOPIC;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current implementation version of the iresearch interface
///        e.g. which how data is stored in iresearch
////////////////////////////////////////////////////////////////////////////////
size_t const LATEST_VERSION = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief the storage format used with IResearch index
////////////////////////////////////////////////////////////////////////////////
constexpr std::string_view LATEST_FORMAT = "1_3simd";

struct StaticStrings {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        corresponding link definitions
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const LinksField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch View definition denoting the
  ///        corresponding link definitions
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const VersionField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the id of the field in the IResearch Link definition denoting the
  ///        corresponding IResearch View
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const ViewIdField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        referenced analyzer definitions
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const AnalyzerDefinitionsField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer name
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const AnalyzerNameField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer type
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const AnalyzerTypeField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer properties
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const AnalyzerPropertiesField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the analyzer definition denoting the
  ///        corresponding analyzer features
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const AnalyzerFeaturesField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        primary sort
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const PrimarySortField;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the name of the field in the IResearch Link definition denoting the
  ///        stored values
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const StoredValuesField;
};

}  // namespace iresearch
}  // namespace arangodb

#endif
