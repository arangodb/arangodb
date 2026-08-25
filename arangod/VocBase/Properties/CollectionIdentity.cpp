////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "CollectionIdentity.h"

#include "Basics/NumberUtils.h"
#include "Basics/Result.h"
#include "Inspection/Status.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

using namespace arangodb;

inspection::Status CollectionIdentity::Transformers::IdIdentifier::toSerialized(
    DataSourceId v, std::string& result) {
  result = std::to_string(v.id());
  return {};
}

inspection::Status
CollectionIdentity::Transformers::IdIdentifier::fromSerialized(
    std::string const& v, DataSourceId& result) {
  char const* p = v.c_str();
  result = DataSourceId{NumberUtils::atoi_zero<uint64_t>(p, p + v.length())};
  return {};
}

[[nodiscard]] arangodb::Result
CollectionIdentity::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  if (id.empty()) {
    id = config.idGenerator();
  }
  return {TRI_ERROR_NO_ERROR};
}
