////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBHashIndex.h"
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/VelocyPackHelper.h"

using namespace arangodb;

/// @brief Test if this index matches the definition
/// different to the Index::matchesDefinition because the ordering can
/// be arbitrary
bool RocksDBHashIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get("id");
  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
  }

  value = info.get("fields");
  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", false)) {
    return false;
  }

  // This check does not take ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (auto const& f : VPackArrayIterator(value)) {
    bool found = false;
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    translate.clear();
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);

    for (size_t i = 0; i < n; ++i) {
      if (arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                       false)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

