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

#include "CollectionStorageProperties.h"
#include "Basics/NumberUtils.h"

namespace arangodb {

inspection::Status
CollectionStorageProperties::Transformers::ObjectIdAsString::toSerialized(
    uint64_t v, std::string& result) {
  result = std::to_string(v);
  return {};
}

inspection::Status
CollectionStorageProperties::Transformers::ObjectIdAsString::fromSerialized(
    std::string const& v, uint64_t& result) {
  char const* p = v.c_str();
  result = NumberUtils::atoi_zero<uint64_t>(p, p + v.length());
  return {};
}

inspection::Status
CollectionStorageProperties::Transformers::VersionAsNumber::toSerialized(
    CollectionVersion v, std::underlying_type_t<CollectionVersion>& result) {
  result = static_cast<std::underlying_type_t<CollectionVersion>>(v);
  return {};
}

inspection::Status
CollectionStorageProperties::Transformers::VersionAsNumber::fromSerialized(
    std::underlying_type_t<CollectionVersion> const& v,
    CollectionVersion& result) {
  // Any number is accepted here; LogicalCollection rejects versions below
  // minimumCollectionVersion() with a "run --database.auto-upgrade" error.
  result = static_cast<CollectionVersion>(v);
  return {};
}

}  // namespace arangodb