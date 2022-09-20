////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "CollectionInternalProperties.h"

#include "Inspection/Status.h"

using namespace arangodb;
// Stolen from LogicalCollection.cpp
// Plan ist to eventually remove it from there.
namespace {
static std::string translateStatus(TRI_vocbase_col_status_e status) {
  switch (status) {
    case TRI_VOC_COL_STATUS_LOADED:
      return "loaded";
    case TRI_VOC_COL_STATUS_DELETED:
      return "deleted";
    case TRI_VOC_COL_STATUS_CORRUPTED:
    default:
      return "unknown";
  }
}
}  // namespace

inspection::Status
CollectionInternalProperties::Transformers::StatusString::toSerialized(
    std::underlying_type_t<TRI_vocbase_col_status_e> v, std::string& result) {
  result = translateStatus(static_cast<TRI_vocbase_col_status_e>(v));
  return {};
}

inspection::Status
CollectionInternalProperties::Transformers::StatusString::fromSerialized(
    std::string const& v,
    std::underlying_type_t<TRI_vocbase_col_status_e>& result) {
  // Just ignore the serialized variant, and take whatever is the default
  // We have stored the status entry as well.
  return {};
}
