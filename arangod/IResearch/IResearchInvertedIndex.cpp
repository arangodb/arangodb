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

#include "IResearchInvertedIndex.h"
#include "Basics/AttributenameParser.h"
#include "Basics/Common.h"
#include "Basics/StringUtils.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/env_encryption.h>
#include "utils/encryption.hpp"
#include <analysis/token_attributes.hpp>
#include <search/boolean_filter.hpp>
#include <search/score.hpp>
#include <search/cost.hpp>


namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

constexpr irs::payload NoPayload;

inline irs::doc_iterator::ptr pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column_reader(DocumentPrimaryKey::PK());

  return reader ? reader->iterator() : nullptr;
}

class SearchIndexTrxState final : public TransactionState::Cookie {
 public:
   IResearchLink::Snapshot snapshot;
};


class IResearchInvertedIndexIterator final : public IndexIterator  {
 public:
  IResearchInvertedIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                                 aql::AstNode const& condition, IResearchInvertedIndex* index,
                                 aql::Variable const* variable)
    : IndexIterator(collection, trx), _index(index), _condition(condition),
      _variable(variable) {}
  char const* typeName() const override { return "search-index-iterator"; }
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

  void resetFilter()  {
    if (!_trx->state()) {
      LOG_TOPIC("!!!!", WARN, arangodb::iresearch::TOPIC)
        << "failed to get transaction state while creating search index "
           "snapshot";

      return;
    }
    auto& state = *(_trx->state());

    // TODO FIXME find a better way to look up a State
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* ctx = dynamic_cast<SearchIndexTrxState*>(state.cookie(this));
#else
    auto* ctx = static_cast<SearchIndexTrxState*>(state.cookie(this));
#endif
    if (!ctx) {
      auto ptr = irs::memory::make_unique<SearchIndexTrxState>();
      ctx = ptr.get();
      state.cookie(this, std::move(ptr));

      if (!ctx) {
        LOG_TOPIC("!!!!!", WARN, arangodb::iresearch::TOPIC)
            << "failed to store state into a TransactionState for snapshot of "
               "search index ";
        return;
      }
      ctx->snapshot = _index->snapshot();
      _reader =  &static_cast<irs::directory_reader const&>(ctx->snapshot);
    }
    QueryContext const queryCtx = { _trx, nullptr, nullptr, // FIXME: Kludge, We will fail to create byExpression filters
                                    _expressionCtx, _reader, _variable};
    irs::Or root;
    auto rv = FilterFactory::filter(&root, queryCtx, _condition);

    if (rv.fail()) {
      arangodb::velocypack::Builder builder;
      _condition.toVelocyPack(builder, true);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          rv.errorNumber(),
          basics::StringUtils::concatT("failed to build filter while querying "
                               "search index, query '",
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
  aql::ExpressionContext* _expressionCtx{nullptr}; // FIXME: this should be non needed as we are not expected to execute Aql expressions here.
};
}

namespace arangodb {
namespace iresearch {

IResearchInvertedIndexFactory::IResearchInvertedIndexFactory(application_features::ApplicationServer& server) 
  : IndexTypeFactory(server)
{
}

bool IResearchInvertedIndexFactory::equal(velocypack::Slice const& lhs,
                                          velocypack::Slice const& rhs,
                                          std::string const& dbname) const {
  return false;
}

std::shared_ptr<Index> IResearchInvertedIndexFactory::instantiate(
    LogicalCollection& collection, velocypack::Slice const& definition,
    IndexId id, bool isClusterConstructor) const {
    uint64_t objectId = basics::VelocyPackHelper::stringUInt64(definition, arangodb::StaticStrings::ObjectId);
  auto link = std::make_shared<IResearchInvertedIndex>(id, collection, objectId);

  auto const res = link->init(definition, [this](irs::directory& dir) {
    //auto& selector = _server.getFeature<EngineSelectorFeature>();
    //TRI_ASSERT(selector.isRocksDB());
    //auto& engine = selector.engine<RocksDBEngine>();
    //auto* encryption = engine.encryptionProvider();
    //if (encryption) {
    //  dir.attributes().emplace<RocksDBEncryptionProvider>(*encryption,
    //                                                      engine.rocksDBOptions());
    //}
  });

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return link;
}

Result IResearchInvertedIndexFactory::normalize(velocypack::Builder& normalized,
                                                velocypack::Slice definition,
                                                bool isCreation,
                                                TRI_vocbase_t const& vocbase) const {
  for (velocypack::ObjectIterator itr(definition); itr.valid(); ++itr) {
    normalized.add(itr.key().stringRef(), itr.value());
  }
  return Result();
}

IResearchInvertedIndex::IResearchInvertedIndex(IndexId iid, LogicalCollection& collection, uint64_t objectId)
  : IResearchRocksDBLink(iid, collection, objectId)
{
}

Result IResearchInvertedIndex::init(velocypack::Slice const& definition, InitCallback const& initCallback)
{
  std::string error;
  IResearchLinkMeta meta;
  // definition should already be normalized and analyzers created if required
  if (!meta.init(IResearchLink::_collection.vocbase().server(), definition, true, error,
                 IResearchLink::_collection.vocbase().name())) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "error parsing  search index parameters from json: " + error
    };
  }
  for (auto const& field : meta._fields) {
    std::vector<basics::AttributeName> fields;
    TRI_ParseAttributeString(field.key(), fields, false);
    _fields.push_back(std::move(fields));
  }
  
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);
  bool const sorted = !_meta._sort.empty();
  auto const& storedValuesColumns = _meta._storedValues.columns();
  TRI_ASSERT(_meta._sortCompression);
  auto const primarySortCompression = _meta._sortCompression
      ? _meta._sortCompression
      : getDefaultCompression();
  auto res = initDataStore(initCallback, sorted, storedValuesColumns, primarySortCompression);
  if (res.fail()) {
    return res;
  }
  return IResearchLink::properties(IResearchViewMeta::DEFAULT());
}

void IResearchInvertedIndex::toVelocyPack(VPackBuilder& builder, std::underlying_type<Index::Serialize>::type flags) const
{

  auto forPersistence = Index::hasFlag(flags, Index::Serialize::Internals);

  builder.openObject();
  Index::toVelocyPack(builder, flags);
  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to generate link definition for search index '") +
            std::to_string(Index::id().id()) + "'"));
  }
  builder.close();
}

Result IResearchInvertedIndex::properties(velocypack::Builder& builder, bool forPersistence) const
{
  IResearchLinkMeta::Mask mask(true);
  mask._fields = false; // we want to emit fields as array FIXME: Kludge!

  if (!builder.isOpenObject()  // not an open object
      || !_meta.json(IResearchLink::_collection.vocbase().server(), builder, forPersistence,
                    nullptr, &(IResearchLink::_collection.vocbase()), &mask)) {
    return Result(TRI_ERROR_BAD_PARAMETER);
  }
  {
    VPackArrayBuilder fieldsArray(&builder, "fields");
    for (auto const& field : _meta._fields) {
      builder.add(VPackValue(field.key()));
    }
  }
  return {};
}

std::unique_ptr<IndexIterator> IResearchInvertedIndex::iteratorForCondition(
    transaction::Methods* trx, aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts)
{
  if (node) {
    return std::make_unique<IResearchInvertedIndexIterator>(&IResearchLink::collection(), trx, *node, this, reference);
  } else {
    TRI_ASSERT(false);
    //sorting  case
    return std::make_unique<EmptyIndexIterator>(&IResearchLink::collection(), trx);
  }
}

Index::FilterCosts IResearchInvertedIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const
{
  return SortedIndexAttributeMatcher::supportsFilterCondition(allIndexes, this, node, reference, itemsInIndex);
}

aql::AstNode* iresearch::IResearchInvertedIndex::specializeCondition(aql::AstNode* node, aql::Variable const* reference) const
{
  return SortedIndexAttributeMatcher::specializeCondition(this, node, reference);
}

} // namespace iresearch
} // namespace arangodb
