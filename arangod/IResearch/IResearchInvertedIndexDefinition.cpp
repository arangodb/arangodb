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

#include "IResearch/IResearchInvertedIndexDefinition.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchInvertedIndexMeta.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace iresearch {

IResearchInvertedIndexDefinition::IResearchInvertedIndexDefinition(
    application_features::ApplicationServer& server)
    : IndexDefinition(IndexType::Inverted), _server(server) {}

bool IResearchInvertedIndexDefinition::equal(velocypack::Slice lhs,
                                             velocypack::Slice rhs,
                                             std::string const& dbname) const {
  IResearchInvertedIndexMeta lhsFieldsMeta;
  std::string errField;
  if (!lhsFieldsMeta.init(_server, lhs, true, errField, dbname)) {
    LOG_TOPIC("79384", ERR, iresearch::TOPIC)
        << "failed to initialize index fields from definition"
        << (errField.empty() ? absl::StrCat(": ", lhs.toString())
                             : absl::StrCat(", error in attribute '", errField,
                                            "': ", lhs.toString()));
    return false;
  }

  IResearchInvertedIndexMeta rhsFieldsMeta;
  if (!rhsFieldsMeta.init(_server, rhs, true, errField, dbname)) {
    LOG_TOPIC("31eaf", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? absl::StrCat(
                      "failed to initialize index fields from definition: ",
                      rhs.toString())
                : absl::StrCat("failed to initialize index fields from "
                               "definition, error in attribute '",
                               errField, "': ", rhs.toString()));
    return false;
  }

  return lhsFieldsMeta == rhsFieldsMeta;
}

Result IResearchInvertedIndexDefinition::normalize(
    velocypack::Builder& normalized, velocypack::Slice definition,
    bool isCreation, TRI_vocbase_t const& vocbase) const {
  TRI_ASSERT(normalized.isOpenObject());

  auto res = IndexFactory::validateFieldsDefinition(
      definition, arangodb::StaticStrings::IndexFields, 0, SIZE_MAX,
      /*allowSubAttributes*/ true, /*allowIdAttribute*/ false);
  if (res.fail()) {
    return res;
  }

  std::string errField;
  IResearchInvertedIndexMeta tmpLinkMeta;
  if (!tmpLinkMeta.init(_server, definition,
                        ServerState::instance()->isDBServer(), errField,
                        vocbase.name())) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
            ? absl::StrCat("failed to normalize index fields from definition: ",
                           definition.toString())
            : absl::StrCat("failed to normalize index fields from definition, "
                           "error in attribute '",
                           errField, "': ", definition.toString())};
  }
  if (!tmpLinkMeta.json(_server, normalized, isCreation, &vocbase)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat(
                "failed to write normalized index fields from definition: ",
                definition.toString())};
  }
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  if (nameSlice.isString() && nameSlice.getStringLength() > 0) {
    normalized.add(arangodb::StaticStrings::IndexName, nameSlice);
  } else if (!nameSlice.isNone()) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat(
            "failed to normalize index from definition, error in attribute '",
            arangodb::StaticStrings::IndexName, "': ", definition.toString())};
  }

  normalized.add(arangodb::StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Inverted)));

  // objectId is added by RocksDBIndexFactory::finalizeDefinition instead
  normalized.add(arangodb::StaticStrings::IndexSparse, velocypack::Value(true));
  normalized.add(arangodb::StaticStrings::IndexUnique,
                 velocypack::Value(false));

  IndexFactory::processIndexInBackground(definition, normalized);
  IndexFactory::processIndexParallelism(definition, normalized);

  return res;
}

}  // namespace iresearch
}  // namespace arangodb
