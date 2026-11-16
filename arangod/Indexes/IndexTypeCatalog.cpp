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

#include "IndexTypeCatalog.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Indexes/DeprecatedSecondaryIndexDefinition.h"
#include "Indexes/EdgeIndexDefinition.h"
#include "Indexes/FulltextIndexDefinition.h"
#include "Indexes/Geo1IndexDefinition.h"
#include "Indexes/Geo2IndexDefinition.h"
#include "Indexes/GeoIndexDefinition.h"
#include "Indexes/Index.h"
#include "Indexes/MdiIndexDefinition.h"
#include "Indexes/MdiPrefixedIndexDefinition.h"
#include "Indexes/PrimaryIndexDefinition.h"
#include "Indexes/SecondaryIndexDefinition.h"
#include "Indexes/TtlIndexDefinition.h"
#include "Indexes/VectorIndexDefinition.h"
#include "IResearch/IResearchCommon.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

IndexTypeCatalog::IndexTypeCatalog(
    application_features::ApplicationServer& server) {
  // a definition is registered under its persisted name, see Index::oldtypeName
  auto reg = [&](std::unique_ptr<IndexDefinition> definition) {
    auto* ptr = definition.get();
    std::string name{Index::oldtypeName(ptr->type())};
    _definitions.try_emplace(name, std::move(definition));
    _byName.try_emplace(std::move(name), ptr);
  };

  reg(std::make_unique<EdgeIndexDefinition>());
  reg(std::make_unique<FulltextIndexDefinition>());
  reg(std::make_unique<GeoIndexDefinition>());
  reg(std::make_unique<Geo1IndexDefinition>());
  reg(std::make_unique<Geo2IndexDefinition>());
  reg(std::make_unique<DeprecatedSecondaryIndexDefinition>(IndexType::Hash,
                                                           "hash"));
  reg(std::make_unique<SecondaryIndexDefinition>(IndexType::Persistent));
  reg(std::make_unique<DeprecatedSecondaryIndexDefinition>(IndexType::Skiplist,
                                                           "skiplist"));
  reg(std::make_unique<PrimaryIndexDefinition>());
  reg(std::make_unique<TtlIndexDefinition>());
  reg(std::make_unique<MdiIndexDefinition>(IndexType::Zkd));
  reg(std::make_unique<MdiIndexDefinition>(IndexType::MDI));
  reg(std::make_unique<MdiPrefixedIndexDefinition>());
  reg(std::make_unique<VectorIndexDefinition>());

  // legacy alias of persistent
  _byName.try_emplace("rocksdb", _byName.at("persistent"));
}

IndexType IndexTypeCatalog::resolve(std::string_view name) const noexcept {
  auto const* def = definitionFor(name);
  return def ? def->type() : IndexType::Unknown;
}

IndexDefinition const* IndexTypeCatalog::definitionFor(
    std::string_view name) const noexcept {
  auto it = _byName.find(name);
  return it == _byName.end() ? nullptr : it->second;
}

Result IndexTypeCatalog::normalizeType(std::string_view name,
                                       velocypack::Builder& normalized,
                                       velocypack::Slice definition,
                                       bool isCreation,
                                       Database const& vocbase) const {
  auto const* def = definitionFor(name);
  if (!def) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat("invalid index type '", name, "'"));
  }
  return def->normalize(normalized, definition, isCreation, vocbase);
}

bool IndexTypeCatalog::equal(velocypack::Slice lhs, velocypack::Slice rhs,
                             std::string const& dbname) const {
  auto lhsTypeSlice = lhs.get(StaticStrings::IndexType);
  auto rhsTypeSlice = rhs.get(StaticStrings::IndexType);
  if (!lhsTypeSlice.isString() || !rhsTypeSlice.isString()) {
    return false;
  }

  // zkd is the old name of mdi
  auto typeName = [](velocypack::Slice s) -> std::string_view {
    if (s.stringView() == "zkd") {
      return "mdi";
    }
    return s.stringView();
  };

  auto lhsType = typeName(lhsTypeSlice);
  auto rhsType = typeName(rhsTypeSlice);
  if (lhsType != rhsType) {
    return false;
  }

  auto const* def = definitionFor(lhsType);
  return def != nullptr && def->equal(lhs, rhs, dbname);
}

std::vector<std::pair<std::string_view, std::string_view>>
IndexTypeCatalog::aliases() const {
  return {{"zkd", "mdi"}};
}

Result IndexTypeCatalog::add(std::string_view name,
                             std::unique_ptr<IndexDefinition> definition) {
  // _byName also covers aliases, so it's the real uniqueness check
  if (_byName.contains(name)) {
    return Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                  absl::StrCat("index type '", name,
                               "' already registered in the index catalog"));
  }
  auto* ptr = definition.get();
  _definitions.try_emplace(std::string{name}, std::move(definition));
  _byName.try_emplace(std::string{name}, ptr);
  return Result();
}
