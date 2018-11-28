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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchCommon.h"
#include "velocypack/StringRef.h"

namespace {

static const char* TYPE = "arangosearch";

}

namespace arangodb {
namespace iresearch {

arangodb::LogicalDataSource::Type const& dataSourceType() {
  static auto& type = arangodb::LogicalDataSource::Type::emplace(
    arangodb::velocypack::StringRef(TYPE)
  );

  return type;
}

arangodb::LogTopic& logTopic() {
  static arangodb::LogTopic topic(TYPE, LogLevel::INFO);

  return topic;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     StaticStrings
// -----------------------------------------------------------------------------

std::string const StaticStrings::LinksField("links");
std::string const StaticStrings::VersionField("version");
std::string const StaticStrings::ViewIdField("view");

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------