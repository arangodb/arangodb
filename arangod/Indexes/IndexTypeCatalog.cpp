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
#include "Indexes/Index.h"
#include "IResearch/IResearchCommon.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

IndexTypeCatalog::IndexTypeCatalog(
    application_features::ApplicationServer& server,
    IVectorIndexProvider const& vectorIndexProvider)
    : _hash(IndexType::Hash),
      _persistent(IndexType::Persistent),
      _skiplist(IndexType::Skiplist),
      _ttl(IndexType::TTL),
      _zkd(IndexType::Zkd),
      _mdi(IndexType::MDI),
      _vector(IndexType::Vector, vectorIndexProvider),
      _inverted(server) {
  // a definition is registered under its persisted name, see Index::oldtypeName
  auto reg = [&](IndexDefinition const& definition) {
    _byName.try_emplace(std::string{Index::oldtypeName(definition._type)},
                        &definition);
  };

  reg(_edge);
  reg(_fulltext);
  reg(_geo);
  reg(_geo1);
  reg(_geo2);
  reg(_hash);
  reg(_persistent);
  reg(_skiplist);
  reg(_primary);
  reg(_ttl);
  reg(_zkd);
  reg(_mdi);
  reg(_mdiPrefixed);
  reg(_vector);
  reg(_inverted);
  // legacy alias of persistent
  _byName.try_emplace("rocksdb", &_persistent);
}

IndexType IndexTypeCatalog::resolve(std::string_view name) const noexcept {
  auto it = _byName.find(std::string{name});
  return it == _byName.end() ? IndexType::Unknown : it->second->_type;
}

IndexDefinition const* IndexTypeCatalog::definitionFor(
    std::string_view name) const noexcept {
  auto it = _byName.find(std::string{name});
  return it == _byName.end() ? nullptr : it->second;
}

Result IndexTypeCatalog::normalizeType(std::string_view name,
                                       velocypack::Builder& normalized,
                                       velocypack::Slice definition,
                                       bool isCreation,
                                       Database const& vocbase) const {
  auto it = _byName.find(std::string{name});
  if (it == _byName.end()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat("invalid index type '", name, "'"));
  }
  return it->second->normalize(normalized, definition, isCreation, vocbase);
}

bool IndexTypeCatalog::equal(velocypack::Slice lhs, velocypack::Slice rhs,
                             std::string const& dbname) const {
  // zkd is the old name of mdi
  auto typeName = [](velocypack::Slice s) -> std::string_view {
    TRI_ASSERT(s.isString());
    if (s.stringView() == "zkd") {
      return "mdi";
    }
    return s.stringView();
  };

  auto lhsType = typeName(lhs.get(StaticStrings::IndexType));
  auto rhsType = typeName(rhs.get(StaticStrings::IndexType));
  if (lhsType != rhsType) {
    return false;
  }

  auto it = _byName.find(std::string{lhsType});
  return it != _byName.end() && it->second->equal(lhs, rhs, dbname);
}

std::vector<std::pair<std::string_view, std::string_view>>
IndexTypeCatalog::aliases(uint32_t apiVersion) const {
  if (apiVersion == 0) {
    return {
        {"hash", "persistent"},
        {"skiplist", "persistent"},
        {"zkd", "mdi"},
    };
  }
  return {{"zkd", "mdi"}};
}

Result IndexTypeCatalog::add(std::string_view name,
                             IndexDefinition const& definition) {
  if (!_byName.try_emplace(std::string{name}, &definition).second) {
    return Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                  absl::StrCat("index type '", name,
                               "' already registered in the index catalog"));
  }
  return Result();
}
