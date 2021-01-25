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

#include "IResearchCommon.h"

#include <velocypack/StringRef.h>

namespace {
static const char* TYPE = "arangosearch";
}

namespace arangodb {
namespace iresearch {

LogTopic TOPIC(::TYPE, LogLevel::INFO);

arangodb::LogicalDataSource::Type const& dataSourceType() {
  static auto& type =
      arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef(TYPE));

  return type;
}

arangodb::LogTopic& logTopic() { return TOPIC; }

// -----------------------------------------------------------------------------
// --SECTION--                                                     StaticStrings
// -----------------------------------------------------------------------------

/*static*/ std::string const StaticStrings::LinksField("links");
/*static*/ std::string const StaticStrings::VersionField("version");
/*static*/ std::string const StaticStrings::ViewIdField("view");
/*static*/ std::string const StaticStrings::AnalyzerDefinitionsField("analyzerDefinitions");
/*static*/ std::string const StaticStrings::AnalyzerFeaturesField("features");
/*static*/ std::string const StaticStrings::AnalyzerNameField("name");
/*static*/ std::string const StaticStrings::AnalyzerPropertiesField("properties");
/*static*/ std::string const StaticStrings::AnalyzerTypeField("type");
/*static*/ std::string const StaticStrings::PrimarySortField("primarySort");
/*static*/ std::string const StaticStrings::StoredValuesField("storedValues");
/*static*/ std::string const StaticStrings::CollectionNameField("collectionName");

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
