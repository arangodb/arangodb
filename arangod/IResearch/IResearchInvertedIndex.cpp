////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "Basics/AttributeNameParser.h"
#include "Cluster/ServerState.h"


namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

struct CheckFieldsAccess {
  CheckFieldsAccess(QueryContext const& ctx,
                    aql::Variable const& ref,
                    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields)
    : _ctx(ctx), _ref(ref) {
    _fields.insert(std::begin(fields), std::end(fields));
  }

  bool operator()(std::string_view name) const {
    try {
      _parsed.clear();
      TRI_ParseAttributeString(name, _parsed, false);
      if (_fields.find(_parsed) == _fields.end()) {
        LOG_TOPIC("bf92f", TRACE, arangodb::iresearch::TOPIC)
            << "Attribute '" << name << "' is not covered by index";
        return false;
      }
    } catch (arangodb::basics::Exception const& ex) {
      // we can`t handle expansion in ArangoSearch index
      LOG_TOPIC("2ec9a", TRACE, arangodb::iresearch::TOPIC)
          << "Failed to parse attribute access: " << ex.message();
      return false;
    }
    return true;
  }

  mutable std::vector<arangodb::basics::AttributeName> _parsed;
  QueryContext const& _ctx;
  aql::Variable const& _ref;
  using atr_ref = std::reference_wrapper<std::vector<arangodb::basics::AttributeName> const>;
  std::unordered_set<atr_ref,
                     std::hash<std::vector<arangodb::basics::AttributeName>>,
                     std::equal_to<std::vector<arangodb::basics::AttributeName>>> _fields;
};
} // namespace

namespace arangodb {
namespace iresearch {

arangodb::iresearch::IResearchInvertedIndex::IResearchInvertedIndex(
    IndexId id, LogicalCollection& collection, IResearchInvertedIndex::IResearchInvertedIndexMeta&& meta)
  : Index(id, collection, meta._name, meta.fields(), false, true), _meta(meta) {
}

void arangodb::iresearch::IResearchInvertedIndex::toVelocyPack(
    VPackBuilder& builder, std::underlying_type<Index::Serialize>::type flags) const {
}

std::unique_ptr<IndexIterator> arangodb::iresearch::IResearchInvertedIndex::iteratorForCondition(
    transaction::Methods* trx, aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  return std::make_unique<EmptyIndexIterator>(&collection(), trx);
}

Index::SortCosts IResearchInvertedIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  return SortCosts();
}

Index::FilterCosts IResearchInvertedIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  TRI_ASSERT(node);
  TRI_ASSERT(reference);
  auto filterCosts = FilterCosts::defaultCosts(itemsInIndex);

  // non-deterministic condition will mean full-scan. So we should
  // not use index here.
  // FIXME: maybe in the future we will be able to optimize just deterministic part?
  if (!node->isDeterministic()) {
    LOG_TOPIC("750e6", TRACE, iresearch::TOPIC)
             << "Found non-deterministic condition. Skipping index " << id().id();
    return  filterCosts;
  }

  // We don`t want byExpression filters
  // and can`t apply index if we are not sure what attribute is
  // accessed so we provide QueryContext which is unable to
  // execute expressions and only allow to pass conditions with constant
  // attributes access/values. Otherwise if we have say d[a.smth] where 'a' is a variable from
  // the upstream loop we may get here a field we don`t have in the index.
  QueryContext const queryCtx = {nullptr, nullptr, nullptr,
                                 nullptr, nullptr, reference};


  // check that only covered attributes are referenced
  if (!visitAllAttributeAccess(node, *reference, queryCtx, CheckFieldsAccess(queryCtx, *reference, fields()))) {
    LOG_TOPIC("d2beb", TRACE, iresearch::TOPIC)
             << "Found unknown attribute access. Skipping index " << id().id();
    return  filterCosts;
  }

  auto rv = FilterFactory::filter(nullptr, queryCtx, *node, false);
  LOG_TOPIC_IF("ee0f7", TRACE, iresearch::TOPIC, rv.fail())
             << "Failed to build filter with error'" << rv.errorMessage() <<"' Skipping index " << id().id();

  if (rv.ok()) {
    filterCosts.supportsCondition = true;
    filterCosts.coveredAttributes = 0; // FIXME: we may use stored values!
  }
  return filterCosts;
}

aql::AstNode* IResearchInvertedIndex::specializeCondition(aql::AstNode* node,
                                                          aql::Variable const* reference) const {
  return node;
}

IResearchInvertedIndexFactory::IResearchInvertedIndexFactory(application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {
}

bool IResearchInvertedIndexFactory::equal(velocypack::Slice const& lhs,
                                          velocypack::Slice const& rhs,
                                          std::string const& dbname) const {
  return false;
}

std::shared_ptr<Index> IResearchInvertedIndexFactory::instantiate(LogicalCollection& collection,
                                                                  velocypack::Slice const& definition,
                                                                  IndexId id,
                                                                  bool isClusterConstructor) const {
  IResearchInvertedIndex::IResearchInvertedIndexMeta meta;
  // FIXME: for cluster  - where to get actual collection name? Pre-store in definition I guess!
  auto res = meta.init(_server, nullptr, definition, isClusterConstructor);
  if (res.fail()) {
        LOG_TOPIC("18c17", ERR, iresearch::TOPIC)
             << "Filed to create index '" << id.id()
             << "' error:" << res.errorMessage();
    return nullptr;
  }
  return std::make_shared<IResearchInvertedIndex>(id, collection, std::move(meta));
}

Result IResearchInvertedIndexFactory::normalize(velocypack::Builder& normalized, velocypack::Slice definition,
                                                bool isCreation, TRI_vocbase_t const& vocbase) const {
  TRI_ASSERT(normalized.isOpenObject());
  Result res = IResearchInvertedIndex::IResearchInvertedIndexMeta::normalize(_server, &vocbase, normalized, definition);
  if (res.ok()) {
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                   arangodb::Index::TRI_IDX_TYPE_SEARCH_INDEX)));

    if (isCreation && !arangodb::ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
        normalized.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
    }
    normalized.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(true));
    normalized.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(false));

    // FIXME: make indexing true background?
    bool bck = basics::VelocyPackHelper::getBooleanValue(
        definition, arangodb::StaticStrings::IndexInBackground, false);
    normalized.add(arangodb::StaticStrings::IndexInBackground, VPackValue(bck));
  }
  return res;
}

Result IResearchInvertedIndex::IResearchInvertedIndexMeta::init(
    application_features::ApplicationServer& server,
    TRI_vocbase_t const* defaultVocbase,
    velocypack::Slice const info, bool isClusterConstructor) {
  std::string errField;
  if (!_indexMeta.init(info, errField)) {
     return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
        ? (std::string("failed to initialize index from definition: ") + info.toString())
        : (std::string("failed to initialize index from definition, error in attribute '")
          + errField + "': " + info.toString()));
  }
  if (!_fieldsMeta.init(server, info, false, errField,
                        defaultVocbase ? defaultVocbase->name() : irs::string_ref::NIL,
                        IResearchLinkMeta::DEFAULT(), nullptr, true)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
        ? (std::string("failed to initialize index from definition: ") + info.toString())
        : (std::string("failed to initialize index from definition, error in attribute '")
          + errField + "': " + info.toString()));
  }
  return {};
}

Result IResearchInvertedIndex::IResearchInvertedIndexMeta::normalize(
    application_features::ApplicationServer& server,
    TRI_vocbase_t const* defaultVocbase,
    velocypack::Builder& normalized, velocypack::Slice definition) {
  IResearchViewMeta tmpMeta;
  std::string errField;
  if (!tmpMeta.init(definition, errField)) {
     return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
        ? (std::string("failed to initialize index from definition: ") + definition.toString())
        : (std::string("failed to initialize index from definition, error in attribute '")
          + errField + "': " + definition.toString()));
  }
  if (!tmpMeta.json(normalized)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to initialize index from definition: ") + definition.toString());
  }
  IResearchLinkMeta tmpLinkMeta;
  if (!tmpLinkMeta.init(server, definition, false, errField,
                        defaultVocbase ? defaultVocbase->name() : irs::string_ref::NIL,
                        IResearchLinkMeta::DEFAULT(), nullptr, true)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
        ? (std::string("failed to initialize index from definition: ") + definition.toString())
        : (std::string("failed to initialize index from definition, error in attribute '")
          + errField + "': " + definition.toString()));
  }
  if (!tmpLinkMeta.json(server, normalized, false, nullptr, defaultVocbase, nullptr, true)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to initialize index from definition: ") + definition.toString());
  }
  return {};
}

namespace {
void traverseFields(
    std::vector<std::vector<arangodb::basics::AttributeName>>& total,
    FieldMeta const& meta, std::vector<arangodb::basics::AttributeName> current) {
  if (meta._fields.empty()) {
    // reached the bottom
    total.push_back(current);
    return;
  }
  for (auto const& f : meta._fields) {
    current.push_back(arangodb::basics::AttributeName(
                      arangodb::velocypack::StringRef(f.key().c_str(), f.key().size())));
    traverseFields(total, *f.value().get(), current);
  }
}
} // namespace

std::vector<std::vector<arangodb::basics::AttributeName>> IResearchInvertedIndex::IResearchInvertedIndexMeta::fields() const {
  std::vector<std::vector<arangodb::basics::AttributeName>> res;
  std::vector<arangodb::basics::AttributeName> current;
  traverseFields(res, _fieldsMeta, current);
  return res;
}

} // namespace iresearch
} // namespace arangodb
