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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

using namespace arangodb;

inspection::Status CollectionIdentity::Transformers::IdIdentifier::toSerialized(
    DataSourceId v, velocypack::Builder& result) {
  result.add(velocypack::Value(std::to_string(v.id())));
  return {};
}

inspection::Status
CollectionIdentity::Transformers::IdIdentifier::fromSerialized(
    velocypack::Builder const& b, DataSourceId& result) {
  auto v = b.slice();
  if (v.isString()) {
    velocypack::ValueLength length;
    char const* p = v.getStringUnchecked(length);
    result = DataSourceId{NumberUtils::atoi_zero<uint64_t>(p, p + length)};
    return {};
  }
  if (v.isNumber()) {
    try {
      result = DataSourceId{v.getNumber<uint64_t>()};
      return {};
    } catch (...) {
      // disallowed number type, e.g. negative
    }
  }
  return {"Only a string or an unsigned integer number is allowed"};
}

[[nodiscard]] arangodb::Result
CollectionIdentity::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  if (id.empty()) {
    id = config.idGenerator();
  }
  return {TRI_ERROR_NO_ERROR};
}
