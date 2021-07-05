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

#include "Basics/AttributeNameParser.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchIdentityAnalyzer.h"
#include "IResearch/IResearchFilterFactory.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"


//#include <rocksdb/env_encryption.h>
//#include "utils/encryption.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
#include "search/boolean_filter.hpp"
#include "search/score.hpp"
#include "search/cost.hpp"
#include "utils/utf8_path.hpp"

namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

struct CheckFieldsAccess {
  CheckFieldsAccess(QueryContext const& ctx,
                    aql::Variable const& ref,
                    std::vector<std::vector<basics::AttributeName>> const& fields)
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
    } catch (basics::Exception const& ex) {
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
  using atr_ref = std::reference_wrapper<std::vector<basics::AttributeName> const>;
  std::unordered_set<atr_ref,
                     std::hash<std::vector<basics::AttributeName>>,
                     std::equal_to<std::vector<basics::AttributeName>>> _fields;
};

class FieldsBuilder {
 public:
  explicit FieldsBuilder(std::vector<std::vector<basics::AttributeName>>& fields) : _fields(fields) {}

  bool operator()(irs::hashed_string_ref const name, FieldMeta const& meta) {
    if (!name.empty()) {
      if (meta._fields.empty()) {
        // found last field in branch
        _fields.push_back(_stack);
        _fields.back().emplace_back(velocypack::StringRef(name.c_str(), name.size()), false);
        processLeaf(meta);
      } else {
        _stack.emplace_back(velocypack::StringRef(name.c_str(), name.size()), false);
      }
    }
    return true;
  }

  void pop() {
    if (!_stack.empty()) {
      _stack.pop_back();
    }
  }

  virtual void processLeaf(FieldMeta const& meta) {}

 private:
  std::vector<basics::AttributeName> _stack;
  std::vector<std::vector<basics::AttributeName>>& _fields;
};

class FieldsBuilderWithAnalyzer : public FieldsBuilder {
 public:
  explicit FieldsBuilderWithAnalyzer(std::vector<std::vector<basics::AttributeName>>& fields)
    : FieldsBuilder(fields) {}

  void processLeaf(FieldMeta const& meta) override {
     if (!meta._analyzers.empty()) {
        TRI_ASSERT(meta._analyzers.size() == 1);
        _analyzerNames.emplace_back(meta._analyzers.front()._pool->name());
      }
  }

  std::vector<irs::string_ref> _analyzerNames;
};

template<typename Visitor>
bool visitFields(
    irs::hashed_string_ref const myName,
    FieldMeta const& meta, Visitor& visitor) {
  if (visitor(myName, meta)) {
    for (auto const& f : meta._fields) {
      visitFields<Visitor>(f.key(),  *f.value().get(), visitor);
    }
    visitor.pop();
  } else {
    return false;
  }
  return true;
}

constexpr irs::payload NoPayload;

inline irs::doc_iterator::ptr pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column_reader(DocumentPrimaryKey::PK());

  return reader ? reader->iterator() : nullptr;
}

class IResearchSnapshotState final : public TransactionState::Cookie {
 public:
   IResearchDataStore::Snapshot snapshot;
};

class IResearchInvertedIndexIterator final : public IndexIterator  {
 public:
  IResearchInvertedIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                                 aql::AstNode const& condition, IResearchInvertedIndex* index,
                                 aql::Variable const* variable)
    : IndexIterator(collection, trx), _index(index), _condition(condition),
      _variable(variable) {}
  char const* typeName() const override { return "inverted-index-iterator"; }

 protected:
  bool nextImpl(LocalDocumentIdCallback const& callback, size_t limit) override {
    if (limit == 0) {
      TRI_ASSERT(false);  // Someone called with limit == 0. Api broken
                          // validate that Iterator is in a good shape and hasn't failed
      return false;
    }
    if (!_filter) {
      resetFilter();
      if (!_filter) {
        return false;
      }
    }
    TRI_ASSERT(_reader);
    auto const count  = _reader->size();
    while (limit > 0) {
      if(!_itr || !_itr->next()) {
        if (_readerOffset >= count) {
          break;
        }
        auto& segmentReader = (*_reader)[_readerOffset++];
        _pkDocItr = ::pkColumn(segmentReader);
        _pkValue = irs::get<irs::payload>(*_pkDocItr);
        if (ADB_UNLIKELY(!_pkValue)) {
          _pkValue = &NoPayload;
        }
        _itr = segmentReader.mask(_filter->execute(segmentReader));
        _doc = irs::get<irs::document>(*_itr);
      } else {
        if (_doc->value == _pkDocItr->seek(_doc->value)) {
          LocalDocumentId documentId;
          bool const readSuccess = DocumentPrimaryKey::read(documentId,  _pkValue->value);
          if (readSuccess) {
            --limit;
            callback(documentId);
          }
        }
      }
    }
    return limit == 0;
  }

  // FIXME: track if the filter condition is volatile!
  // FIXME: support sorting!
  void resetFilter()  {
    if (!_trx->state()) {
      LOG_TOPIC("a9ccd", WARN, arangodb::iresearch::TOPIC)
        << "failed to get transaction state while creating inverted index "
           "snapshot";

      return;
    }
    auto& state = *(_trx->state());

    // TODO FIXME find a better way to look up a State
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* ctx = dynamic_cast<IResearchSnapshotState*>(state.cookie(this));
#else
    auto* ctx = static_cast<IResearchSnapshotState*>(state.cookie(this));
#endif
    if (!ctx) {
      auto ptr = irs::memory::make_unique<IResearchSnapshotState>();
      ctx = ptr.get();
      state.cookie(this, std::move(ptr));

      if (!ctx) {
        LOG_TOPIC("d7061", WARN, arangodb::iresearch::TOPIC)
            << "failed to store state into a TransactionState for snapshot of "
               "inverted index ";
        return;
      }
      ctx->snapshot = _index->snapshot();
      _reader =  &static_cast<irs::directory_reader const&>(ctx->snapshot);
    }
    QueryContext const queryCtx = { _trx, nullptr, nullptr,
                                    nullptr, _reader, _variable};
    irs::Or root;
    auto rv = FilterFactory::filter(&root, queryCtx, _condition);

    if (rv.fail()) {
      arangodb::velocypack::Builder builder;
      _condition.toVelocyPack(builder, true);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          rv.errorNumber(),
          basics::StringUtils::concatT("failed to build filter while querying "
                               "inverted index, query '",
                               builder.toJson(), "': ", rv.errorMessage()));
    }
    _filter = root.prepare(*_reader, _order, irs::no_boost(), nullptr);
  }

  void resetImpl() override {
    _filter.reset();
    _readerOffset = 0;
    _itr.reset();
    _doc = nullptr;
    _reader = nullptr;
  }

 private:
  const irs::payload* _pkValue{};
  irs::index_reader const* _reader{0};
  irs::doc_iterator::ptr _itr;
  irs::doc_iterator::ptr _pkDocItr;
  irs::document const* _doc{};
  size_t _readerOffset{0};
  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  IResearchInvertedIndex* _index;
  aql::AstNode const& _condition;
  aql::Variable const* _variable;
};
} // namespace

namespace arangodb {
namespace iresearch {

arangodb::iresearch::IResearchInvertedIndex::IResearchInvertedIndex(
    IndexId iid, LogicalCollection& collection, IResearchLinkMeta&& meta)
  : IResearchDataStore(iid, collection, std::move(meta)) {}

// Analyzer names storing
//  - forPersistence ::<analyzer> from system and <analyzer> for local and definitions are stored.
//  - For user -> database-name qualified names. No definitions are stored.
void IResearchInvertedIndex::toVelocyPack(
    application_features::ApplicationServer& server,
    TRI_vocbase_t const* defaultVocbase,
    velocypack::Builder& builder,
    bool forPersistence) const {
  if (_dataStore._meta.json(builder, nullptr, nullptr) &&
    _meta.json(server, builder, forPersistence, nullptr, defaultVocbase, nullptr, true)) {
  } else {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("Failed to generate inverted index definition")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup referenced analyzer
////////////////////////////////////////////////////////////////////////////////
AnalyzerPool::ptr IResearchInvertedIndex::findAnalyzer(AnalyzerPool const& analyzer) const {
  auto const it = _meta._analyzerDefinitions.find(irs::string_ref(analyzer.name()));

  if (it == _meta._analyzerDefinitions.end()) {
    return nullptr;
  }

  auto const pool = *it;

  if (pool && analyzer == *pool) {
    return pool;
  }

  return nullptr;
}

bool IResearchInvertedIndex::matchesFieldsDefinition(VPackSlice other) const {
  std::vector<std::vector<basics::AttributeName>> myFields;
  FieldsBuilderWithAnalyzer fb(myFields);
  visitFields({0, irs::hashed_string_ref::NIL}, _meta, fb);
  TRI_ASSERT(fb._analyzerNames.size() == myFields.size());
  auto value = other.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  auto const count = myFields.size();
  if (n != count) {
    return false;
  }

  // Order of fields does not matter
  std::vector<arangodb::basics::AttributeName> translate;
  size_t matched{0};
  for (auto fieldSlice : VPackArrayIterator(value)) {
    TRI_ASSERT(fieldSlice.isObject()); // We expect only normalized definitions here.
                              // Otherwise we will need vocbase to properly match analyzers.
    if (ADB_UNLIKELY(!fieldSlice.isObject())) {
      return false;
    }

    auto name = fieldSlice.get("name");
    auto analyzer = fieldSlice.get("analyzer");
    TRI_ASSERT(name.isString() &&  // We expect only normalized definitions here.
               analyzer.isString()); // Otherwise we will need vocbase to properly match analyzers.
    if (ADB_UNLIKELY(!name.isString() || !analyzer.isString())) {
      return false;
    }

    auto in = name.stringRef();
    irs::string_ref analyzerName = analyzer.stringView();
    TRI_ParseAttributeString(in, translate, false);
    size_t fieldIdx{0};
    for (auto const& f : myFields) {
      if (fb._analyzerNames[fieldIdx++] == analyzerName) {
        if (arangodb::basics::AttributeName::isIdentical(f, translate, false)) {
          matched++;
          break;
        }
      }
    }
    translate.clear();
  }
  return matched == count;
}

std::unique_ptr<IndexIterator> arangodb::iresearch::IResearchInvertedIndex::iteratorForCondition(
    LogicalCollection* collection, transaction::Methods* trx, aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  if (node) {
    return std::make_unique<IResearchInvertedIndexIterator>(collection, trx, *node, this, reference);
  } else {
    TRI_ASSERT(false);
    //sorting  case
    return std::make_unique<EmptyIndexIterator>(collection, trx);
  }
}

Index::SortCosts IResearchInvertedIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  return Index::SortCosts();
}

Index::FilterCosts IResearchInvertedIndex::supportsFilterCondition(
    IndexId id,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  TRI_ASSERT(node);
  TRI_ASSERT(reference);
  auto filterCosts = Index::FilterCosts::defaultCosts(itemsInIndex);

  // non-deterministic condition will mean full-scan. So we should
  // not use index here.
  // FIXME: maybe in the future we will be able to optimize just deterministic part?
  if (!node->isDeterministic()) {
    LOG_TOPIC("750e6", TRACE, iresearch::TOPIC)
             << "Found non-deterministic condition. Skipping index " << id.id();
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
  if (!visitAllAttributeAccess(node, *reference, queryCtx, CheckFieldsAccess(queryCtx, *reference, fields))) {
    LOG_TOPIC("d2beb", TRACE, iresearch::TOPIC)
             << "Found unknown attribute access. Skipping index " << id.id();
    return  filterCosts;
  }

  auto rv = FilterFactory::filter(nullptr, queryCtx, *node, false);
  LOG_TOPIC_IF("ee0f7", TRACE, iresearch::TOPIC, rv.fail())
             << "Failed to build filter with error'" << rv.errorMessage() <<"' Skipping index " << id.id();

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

IResearchRocksDBInvertedIndexFactory::IResearchRocksDBInvertedIndexFactory(application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {
}

bool IResearchRocksDBInvertedIndexFactory::equal(velocypack::Slice const& lhs,
                                          velocypack::Slice const& rhs,
                                          std::string const& dbname) const {
  return false;
}

std::shared_ptr<Index> IResearchRocksDBInvertedIndexFactory::instantiate(LogicalCollection& collection,
                                                                  velocypack::Slice const& definition,
                                                                  IndexId id,
                                                                  bool isClusterConstructor) const {
  IResearchViewMeta indexMeta;
  std::string errField;
  if (!indexMeta.init(definition, errField)) {
    LOG_TOPIC("a9ccd", ERR, iresearch::TOPIC)
      << (errField.empty() ?
           (std::string("failed to initialize index from definition: ") + definition.toString())
           : (std::string("failed to initialize index from definition, error in attribute '")
               + errField + "': " + definition.toString()));
     return nullptr;
  }
  IResearchLinkMeta fieldsMeta;
  if (!fieldsMeta.init(_server, definition, false, errField,
                        collection.vocbase().name(), IResearchLinkMeta::DEFAULT(), nullptr, true)) {
    LOG_TOPIC("18c17", ERR, iresearch::TOPIC)
      << (errField.empty() ?
           (std::string("failed to initialize index from definition: ") + definition.toString())
           : (std::string("failed to initialize index from definition, error in attribute '")
               + errField + "': " + definition.toString()));
    return nullptr;
  }
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  std::string indexName;
  if (!nameSlice.isNone()) {
    if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
      LOG_TOPIC("91ebd", ERR, iresearch::TOPIC) <<
        std::string("failed to initialize index from definition, error in attribute '")
        + arangodb::StaticStrings::IndexName + "': "
        + definition.toString();
      return nullptr;
    }
  }

  auto objectIdSlice = definition.get(arangodb::StaticStrings::ObjectId);
  uint64_t objectId{0};
  if (!objectIdSlice.isNone()) {
    if (!objectIdSlice.isNumber<uint64_t>() || (objectId = objectIdSlice.getNumber<uint64_t>()) == 0) {
      LOG_TOPIC("b97dc", ERR, iresearch::TOPIC) <<
        std::string("failed to initialize index from definition, error in attribute '")
        + arangodb::StaticStrings::ObjectId + "': "
        + definition.toString();
      return nullptr;
    }
  }
  auto index = std::make_shared<IResearchRocksDBInvertedIndex>(id, collection, objectId, indexName, std::move(fieldsMeta));
  // separate call as here object should be fully constructed and could be safely used by async tasks
  auto initRes = index->properties(indexMeta);
  if (initRes.fail()) {
    LOG_TOPIC("9c949", ERR, iresearch::TOPIC) <<
      std::string("failed to initialize index from definition, error setting index properties ")
      << initRes.errorMessage();
    return nullptr;
  }
  return index;
}

Result IResearchRocksDBInvertedIndexFactory::normalize(velocypack::Builder& normalized, velocypack::Slice definition,
                                                bool isCreation, TRI_vocbase_t const& vocbase) const {
  TRI_ASSERT(normalized.isOpenObject());

  auto res  =  IndexFactory::validateFieldsDefinition(definition, 1, SIZE_MAX, true);
  if (res.fail()) {
    return res;
  }

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
  if (!tmpLinkMeta.init(_server, definition, false, errField,
                        vocbase.name(), IResearchLinkMeta::DEFAULT(), nullptr, true)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
        ? (std::string("failed to initialize index from definition: ") + definition.toString())
        : (std::string("failed to initialize index from definition, error in attribute '")
          + errField + "': " + definition.toString()));
  }
  if (!tmpLinkMeta.json(_server, normalized, false, nullptr, &vocbase, nullptr, true)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to initialize index from definition: ") + definition.toString());
  }
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  if (nameSlice.isString() && nameSlice.getStringLength() > 0) {
    normalized.add(arangodb::StaticStrings::IndexName, nameSlice);
  } else if (!nameSlice.isNone()) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to initialize index from definition, error in attribute '")
        + arangodb::StaticStrings::IndexName + "': "
        + definition.toString());
  }
  auto objectIdSlice = definition.get(arangodb::StaticStrings::ObjectId);
  if (!objectIdSlice.isNone()) {
    if (!objectIdSlice.isNumber<uint64_t>() || objectIdSlice.getNumber<uint64_t>() == 0) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to initialize index from definition, error in attribute '")
        + arangodb::StaticStrings::ObjectId + "': "
        + definition.toString());
    }
    normalized.add(arangodb::StaticStrings::ObjectId, objectIdSlice);
  }

  normalized.add(arangodb::StaticStrings::IndexType,
                 arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                     arangodb::Index::TRI_IDX_TYPE_INVERTED_INDEX)));

  if (isCreation && !arangodb::ServerState::instance()->isCoordinator() &&
      !definition.hasKey(arangodb::StaticStrings::ObjectId)) {
    normalized.add(arangodb::StaticStrings::ObjectId, VPackValue(std::to_string(TRI_NewTickServer())));
  }
  normalized.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(true));
  normalized.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(false));

  // FIXME: make indexing true background?
  bool bck = basics::VelocyPackHelper::getBooleanValue(definition,
                                                       arangodb::StaticStrings::IndexInBackground,
                                                       false);
  normalized.add(arangodb::StaticStrings::IndexInBackground, VPackValue(bck));

  return res;
}

std::vector<std::vector<arangodb::basics::AttributeName>> IResearchInvertedIndex::fields(IResearchLinkMeta const& meta) {
  std::vector<std::vector<arangodb::basics::AttributeName>> res;
  FieldsBuilder fb(res);
  visitFields({0, irs::hashed_string_ref::NIL}, meta, fb);
  return res;
}

IResearchRocksDBInvertedIndex::IResearchRocksDBInvertedIndex(
    IndexId id, LogicalCollection& collection, uint64_t objectId, std::string const& name, IResearchLinkMeta&& meta)
    : IResearchInvertedIndex(id, collection, std::move(meta)),
      RocksDBIndex(id, collection, name, IResearchInvertedIndex::fields(_meta), false, false,
                   RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Invalid), objectId, false) {}

void IResearchRocksDBInvertedIndex::toVelocyPack(VPackBuilder & builder,
                                                 std::underlying_type<Index::Serialize>::type flags) const {
  auto const forPersistence = Index::hasFlag(flags, Index::Serialize::Internals);
  VPackObjectBuilder objectBuilder(&builder);
  IResearchInvertedIndex::toVelocyPack(IResearchDataStore::collection().vocbase().server(),
                                       &IResearchDataStore::collection().vocbase(), builder, forPersistence);
  if (forPersistence) {
    TRI_ASSERT(objectId() != 0);  // If we store it, it cannot be 0
    builder.add(arangodb::StaticStrings::ObjectId, VPackValue(std::to_string(objectId())));
  }
  // can't use Index::toVelocyPack as it will try to output 'fields'
  // but we have custom storage format
  builder.add(arangodb::StaticStrings::IndexId,
              arangodb::velocypack::Value(std::to_string(_iid.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              arangodb::velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName, arangodb::velocypack::Value(name()));
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));
}

bool IResearchRocksDBInvertedIndex::matchesDefinition(arangodb::velocypack::Slice const& other) const {
  TRI_ASSERT(other.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = other.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  arangodb::velocypack::StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = other.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    arangodb::velocypack::StringRef idRef(value);
    return idRef == std::to_string(IResearchDataStore::id().id());
  }
  return IResearchInvertedIndex::matchesFieldsDefinition(other);
}

} // namespace iresearch
} // namespace arangodb
