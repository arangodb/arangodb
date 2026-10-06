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
#include "IResearch/IResearchCommon.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

IndexTypeCatalog::IndexTypeCatalog(
    application_features::ApplicationServer& server,
    IVectorIndexProvider const& vectorIndexProvider)
    : ApplicationFeature{server, *this},
      _hash(IndexType::Hash),
      _persistent(IndexType::Persistent),
      _skiplist(IndexType::Skiplist),
      _ttl(IndexType::TTL),
      _zkd(IndexType::Zkd),
      _mdi(IndexType::MDI),
      _vector(IndexType::Vector, vectorIndexProvider),
      _inverted(server) {
  setOptional(false);

  auto reg = [&](std::string_view name, IndexDefinition const& definition) {
    _byName.try_emplace(std::string{name}, &definition);
  };

  reg("edge", _edge);
  reg("fulltext", _fulltext);
  reg("geo", _geo);
  reg("geo1", _geo1);
  reg("geo2", _geo2);
  reg("hash", _hash);
  reg("persistent", _persistent);
  // legacy alias
  reg("rocksdb", _persistent);
  reg("skiplist", _skiplist);
  reg("primary", _primary);
  reg("ttl", _ttl);
  reg("zkd", _zkd);
  reg("mdi", _mdi);
  reg("mdi-prefixed", _mdiPrefixed);
  reg("vector", _vector);
  reg(iresearch::IRESEARCH_INVERTED_INDEX_TYPE, _inverted);
}

IndexType IndexTypeCatalog::resolve(std::string_view name) const noexcept {
  auto it = _byName.find(std::string{name});
  return it == _byName.end() ? IndexType::Unknown : it->second->_type;
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
