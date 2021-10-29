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

#include "AqlHelper.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"
#include "Aql/Projections.h"
#include "Aql/IResearchViewNode.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "IResearchDocument.h"
#include "IResearchFilterFactory.h"
#include "IResearchIdentityAnalyzer.h"
#include "Transaction/Methods.h"

#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
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

constexpr irs::payload NoPayload;

inline irs::doc_iterator::ptr pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column_reader(DocumentPrimaryKey::PK());

  return reader ? reader->iterator() : nullptr;
}

class IResearchSnapshotState final : public TransactionState::Cookie {
 public:
  IResearchDataStore::Snapshot snapshot;
  std::map<aql::AstNode const*, proxy_query::proxy_cache> _immutablePartCache;
};

// FIXME: this is the duplicate of what is used for StoredValues in ArangoSearch View
struct ColumnIterator {
  irs::doc_iterator::ptr itr;
  const irs::payload* value{};
};

class IResearchInvertedIndexIterator final : public IndexIterator  {
 public:
  IResearchInvertedIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                                 aql::AstNode const& condition, IResearchInvertedIndex* index,
                                 aql::Variable const* variable, int64_t mutableConditionIdx,
                                 std::string_view extraFieldName, aql::Projections const* projections)
    : IndexIterator(collection, trx, ReadOwnWrites::no), _index(index),
      _variable(variable), _mutableConditionIdx(mutableConditionIdx), _extraName(extraFieldName) {
    resetFilter(condition);
    if (!extraFieldName.empty()) {
      TRI_ASSERT(projections == nullptr || projections->empty());
      TRI_ASSERT(extraFieldName == "_from" || extraFieldName == "_to");
      auto columnSize = index->_meta._sort.fields().size();
      // extra is expected to be the _from/_to attribute. So don't bother with the full
      // path inspection
      for (size_t i = 0; i < columnSize; ++i) {
        if (index->_meta._sort.fields()[i].size() == 1 &&
            index->_meta._sort.fields()[i].front().name == extraFieldName) {
          _projections.emplace_back(irs::string_ref::EMPTY, i);
          break;
        }
      }
      if (_projections.empty()) { // try to find in other stored
        for (size_t columnIndex = 0; _projections.empty() &&
                                     columnIndex < index->_meta._storedValues.columns().size();
             ++columnIndex) {
          auto const& column = index->_meta._storedValues.columns()[columnIndex];
          columnSize = column.fields.size();
          for (size_t i = 0; i < columnSize; ++i) {
            if (column.fields[i].second.size() == 1 &&
                column.fields[i].second.front().name == extraFieldName) {
              _projections.emplace_back(column.name, i);
              break;
            }
          }
        }
      }
    }
    auto projectionsCount = projections ? projections->size() : 0;
    for (size_t i = 0; i < projectionsCount; ++i) {
      TRI_ASSERT(extraFieldName.empty());
      size_t columnIndex{0};
      size_t fieldIndex{0};
      auto columnSize = index->_meta._sort.fields().size();
      auto const& projection = (*projections)[i];
      if (projection.coveringIndexPosition < columnSize) { // cover from sort
        _projections.emplace_back(irs::string_ref::EMPTY, projection.coveringIndexPosition);
      } else {
        fieldIndex = columnSize;
        while (fieldIndex < projection.coveringIndexPosition &&
               columnIndex < index->_meta._storedValues.columns().size()) {
          columnSize = index->_meta._storedValues.columns()[columnIndex].fields.size();
          if (fieldIndex + columnSize >= projection.coveringIndexPosition) {
            // we found a column
            _projections.emplace_back(index->_meta._storedValues.columns()[columnIndex].name,
                                      projection.coveringIndexPosition - fieldIndex);
            break;
          } else {
            ++columnIndex;
            fieldIndex += columnSize;
            TRI_ASSERT(columnIndex < index->_meta._storedValues.columns().size());
            if (ADB_UNLIKELY(columnIndex >= index->_meta._storedValues.columns().size())) {
              LOG_TOPIC("04235", WARN, arangodb::iresearch::TOPIC)
                << "Failed to find stored column for desired projection position "
                << projection.coveringIndexPosition << " while creating iterator for index "
                << index->id().id();
              i = projectionsCount + 1; // terminate the cycle. Fallback to non-covering state
              _projections.clear();
              break;
            }
          }
        }
      }
    }
  }

  char const* typeName() const override { return "inverted-index-iterator"; }

  /// @brief The default index has no extra information
  bool hasExtra() const override { return !_extraName.empty() &&
                                          _projections.size() == 1; }

 protected:
  bool nextExtraImpl(ExtraCallback const& callback, size_t limit) override {
    if (limit == 0) {
      TRI_ASSERT(false);  // Someone called with limit == 0. Api broken
                          // validate that Iterator is in a good shape and hasn't failed
      return false;
    }
    TRI_ASSERT(hasExtra());
    return nextImplInternal<ExtraCallback, true, false>(callback, limit);
  }

  bool nextImpl(LocalDocumentIdCallback const& callback, size_t limit) override {
    return nextImplInternal<LocalDocumentIdCallback, false, false>(callback, limit);
  }

  bool nextCoveringImpl(DocumentCallback const& callback, size_t limit) override {
    return nextImplInternal<DocumentCallback, false, true>(callback, limit);
  }

  // FIXME: Evaluate buffering iresearch reads
  template<typename Callback, bool withExtra, bool withCovering>
  bool nextImplInternal(Callback const& callback, size_t limit)  {
    if (limit == 0 || !_filter) {
      TRI_ASSERT(limit > 0); // Someone called with limit == 0. Api broken
                             // validate that Iterator is in a good shape and hasn't failed
      TRI_ASSERT(_filter); // _filter is not initialized (should not happen)
      return false;
    }
    TRI_ASSERT(_reader);
    auto const count  = _reader->size();
    while (limit > 0) {
      if (!_itr || !_itr->next()) {
        if (_readerOffset >= count) {
          break;
        }
        auto& segmentReader = (*_reader)[_readerOffset++];
        _pkDocItr = ::pkColumn(segmentReader);
        _pkValue = irs::get<irs::payload>(*_pkDocItr);
        if constexpr (withExtra || withCovering) {
          for (auto& projection : _projections) {
            projection.reset(segmentReader);
          }
        }
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
            if constexpr (withExtra) {
              TRI_ASSERT(!_projections.empty());
              auto extraSlice = _projections[0].get(_doc->value);
              if (!extraSlice.isNone()) {
                --limit;
                callback(documentId, extraSlice);
              } 
            } else if constexpr (withCovering) {
              // FIXME: it is faster to have "tuple of slices" implementation for callback
              _coveringBuffer.reset(); // reassemble VPackArray as required by current API
              VPackBuilder builder(_coveringBuffer);
              {
                VPackArrayBuilder array(&builder);
                for (auto& projection : _projections) {
                  builder.add(projection.get(_doc->value));
                }
              }
              callback(documentId, builder.slice());
            } else {
              --limit;
              callback(documentId);
            }
          }
        }
      }
    }
    return limit == 0;
  }

  bool canRearm() const override {
    return _mutableConditionIdx != -1;
  }

  bool rearmImpl(arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const*,
    IndexIteratorOptions const&) override {
    reset();
    TRI_ASSERT(node);
    resetFilter(*node);
    return true;
  }

  // FIXME: support sorting!
  void resetFilter(arangodb::aql::AstNode const& condition) {
    if (!_trx->state()) {
      LOG_TOPIC("a9ccd", WARN, arangodb::iresearch::TOPIC)
        << "failed to get transaction state while creating inverted index "
           "snapshot";

      return;
    }
    auto& state = *(_trx->state());

    // TODO FIXME find a better way to look up a State
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* ctx = dynamic_cast<IResearchSnapshotState*>(state.cookie(_index));
#else
    auto* ctx = static_cast<IResearchSnapshotState*>(state.cookie(_index));
#endif
    if (!ctx) {
      auto ptr = irs::memory::make_unique<IResearchSnapshotState>();
      ctx = ptr.get();
      state.cookie(_index, std::move(ptr));

      if (!ctx) {
        LOG_TOPIC("d7061", WARN, arangodb::iresearch::TOPIC)
            << "failed to store state into a TransactionState for snapshot of "
               "inverted index ";
        return;
      }
      ctx->snapshot = _index->snapshot();
    }
    _reader =  &static_cast<irs::directory_reader const&>(ctx->snapshot);
    QueryContext const queryCtx = { _trx, nullptr, nullptr,
                                    nullptr, _reader, _variable};

    irs::Or root;
    if (_mutableConditionIdx == -1 ||
          (condition.type != aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND &&
           condition.type != aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR)) {
      auto rv = FilterFactory::filter(&root, queryCtx, condition, false);
      if (rv.fail()) {
        arangodb::velocypack::Builder builder;
        condition.toVelocyPack(builder, true);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            rv.errorNumber(),
            basics::StringUtils::concatT("failed to build filter while querying "
                                 "inverted index, query '",
                                 builder.toJson(), "': ", rv.errorMessage()));
      }
    } else {
      TRI_ASSERT(static_cast<int64_t>(condition.numMembers()) > _mutableConditionIdx);
      if (ADB_UNLIKELY(static_cast<int64_t>(condition.numMembers()) <= _mutableConditionIdx)) {
        arangodb::velocypack::Builder builder;
        condition.toVelocyPack(builder, true);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL, basics::StringUtils::concatT(
                                  "Invalid condition members count while querying "
                                  "inverted index, query '",
                                  builder.toJson(), "'"));
      }
      irs::boolean_filter* conditionJoiner{nullptr};
      std::unique_ptr<irs::boolean_filter> immutableRoot;
      if (condition.type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
        conditionJoiner = &root.add<irs::And>();
        immutableRoot = std::make_unique<irs::And>();
      } else {
        TRI_ASSERT((condition.type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR));
        conditionJoiner = &root.add<irs::Or>();
        immutableRoot = std::make_unique<irs::Or>();
      }
      auto& mutable_root = conditionJoiner->add<irs::Or>();
      auto rv = FilterFactory::filter(&mutable_root, queryCtx,
                                      *condition.getMember(_mutableConditionIdx), false);
      if (rv.fail()) {
        arangodb::velocypack::Builder builder;
        condition.toVelocyPack(builder, true);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            rv.errorNumber(),
            basics::StringUtils::concatT(
                "failed to build mutable filter part while querying "
                "inverted index, query '",
                builder.toJson(), "': ", rv.errorMessage()));
      }
      auto const conditionSize = static_cast<int64_t>(condition.numMembers());
      for (int64_t i = 0; i < conditionSize; ++i) {
        if (i != _mutableConditionIdx) {
          auto& tmp_root = immutableRoot->add<irs::Or>();
          auto rv = FilterFactory::filter(&tmp_root, queryCtx,
                                          *condition.getMember(i), false);
          if (rv.fail()) {
            arangodb::velocypack::Builder builder;
            condition.toVelocyPack(builder, true);
            THROW_ARANGO_EXCEPTION_MESSAGE(
                rv.errorNumber(),
                basics::StringUtils::concatT(
                    "failed to build immutable filter part while querying "
                    "inverted index, query '",
                    builder.toJson(), "': ", rv.errorMessage()));
          }
        }
      }
      conditionJoiner->add<proxy_filter>().add(std::move(immutableRoot))
                       .set_cache(&(ctx->_immutablePartCache[&condition]));
    }
    _filter = root.prepare(*_reader, _order, irs::no_boost(), nullptr);
    TRI_ASSERT(_filter);
    if (ADB_UNLIKELY(!_filter)) {
      arangodb::velocypack::Builder builder;
      condition.toVelocyPack(builder, true);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL_AQL,
          basics::StringUtils::concatT(
              "Failed to prepare the filter while querying "
              "inverted index, query '",
              builder.toJson(), "'"));
    }
  }

  void resetImpl() override {
    _readerOffset = 0;
    _itr.reset();
    _doc = nullptr;
  }

 private:

  /// @brief  Struct represents value of a Projections[i]
  ///         After the document id has beend found "get" method
  ///         could be used to get Slice for the Projections
  struct CoveringValue : ColumnIterator {
    CoveringValue(irs::string_ref col, size_t idx)
      : _column(col), _index(idx) {}

    void reset(irs::sub_reader const& rdr) {
      itr.reset();
      value = &NoPayload;
      auto extraValuesReader = _column.empty() ? rdr.sort() : rdr.column_reader(_column);
      if (ADB_LIKELY(extraValuesReader)) {
        itr = extraValuesReader->iterator();
        TRI_ASSERT(itr);
        if (ADB_LIKELY(itr)) {
          value = irs::get<irs::payload>(*itr);
          if (ADB_UNLIKELY(!value)) {
            value = &NoPayload;
          }
        } 
      } 
    }

    VPackSlice get(irs::doc_id_t doc) {
      if (itr && doc == itr->seek(doc)) {
        size_t i = 0;
        auto const totalSize = value->value.size();
        if (ADB_LIKELY(totalSize > 0)) {
          size_t size = 0;
          VPackSlice slice(value->value.c_str());
          while (i < _index) {
            size += slice.byteSize();
            TRI_ASSERT(size <= totalSize);
            if (ADB_UNLIKELY(size > totalSize)) {
              slice = VPackSlice::noneSlice();
              break;
            }
            slice = VPackSlice{slice.end()};
            ++i;
          }
          TRI_ASSERT(!slice.isNone());
          return slice;
        }
      }
      return VPackSlice::noneSlice();
    }

    irs::string_ref _column;
    size_t _index;
  };

  VPackBuffer<uint8_t> _coveringBuffer;
  irs::doc_iterator::ptr _itr;
  irs::doc_iterator::ptr _pkDocItr;
  irs::filter::prepared::ptr _filter;
  irs::document const* _doc{};
  const irs::payload* _pkValue{};
  irs::index_reader const* _reader{0};
  IResearchInvertedIndex* _index;
  aql::Variable const* _variable;
  size_t _readerOffset{0};
  int64_t _mutableConditionIdx;
  irs::order::prepared _order;

  std::vector<CoveringValue> _projections;
  irs::string_ref _extraName;
};
} // namespace



namespace arangodb {
namespace iresearch {

DEFINE_FACTORY_DEFAULT(proxy_filter);

arangodb::iresearch::IResearchInvertedIndex::IResearchInvertedIndex(
    IndexId iid, LogicalCollection& collection, InvertedIndexFieldMeta&& meta)
  : IResearchDataStore(iid, collection), _meta(std::move(meta)) {}

// Analyzer names storing
//  - forPersistence ::<analyzer> from system and <analyzer> for local and definitions are stored.
//  - For user -> database-name qualified names. No definitions are stored.
void IResearchInvertedIndex::toVelocyPack(
    application_features::ApplicationServer& server,
    TRI_vocbase_t const* defaultVocbase,
    velocypack::Builder& builder,
    bool forPersistence) const {
  if (_dataStore._meta.json(builder, nullptr, nullptr)) {
    //_meta.json(server, builder, forPersistence, nullptr, defaultVocbase, nullptr, true)) {
  } else {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("Failed to generate inverted index store definition")));
  }
  if (!_meta.json(server, builder, forPersistence, defaultVocbase)) {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("Failed to generate inverted index field definition")));
  }
}

std::vector<std::vector<arangodb::basics::AttributeName>> IResearchInvertedIndex::fields(InvertedIndexFieldMeta const& meta) {
  std::vector<std::vector<arangodb::basics::AttributeName>> res;
  for (auto const& f : meta._fields) {
    res.push_back(f.first);
  }
  return res;
}

Result IResearchInvertedIndex::init(IResearchDataStore::InitCallback const& initCallback /*= {}*/) {
  auto const& storedValuesColumns = _meta._storedValues.columns();
  TRI_ASSERT(_meta._sortCompression);
  auto const primarySortCompression = _meta._sortCompression
      ? _meta._sortCompression
      : getDefaultCompression();
  auto const res = initDataStore(initCallback, _meta._version, isSorted(), storedValuesColumns, primarySortCompression);

  if (!res.ok()) {
    return res;
  }
  return {};
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

bool IResearchInvertedIndex::covers(arangodb::aql::Projections& projections) const {
  std::vector<std::vector<aql::latematerialized::ColumnVariant<true>>> usedColumns;
  std::vector<aql::latematerialized::AttributeAndField<aql::latematerialized::IndexFieldData>> attrs;
  for (size_t i = 0; i < projections.size(); ++i) {
    aql::latematerialized::AttributeAndField<aql::latematerialized::IndexFieldData> af;
    for (auto const& a : projections[i].path.path) {
      af.attr.emplace_back(a, false); // TODO: false? 
    }
    attrs.emplace_back(af);
  }
  auto const columnsCount  = _meta._storedValues.columns().size() + 1;
  usedColumns.resize(columnsCount);
  if (aql::latematerialized::attributesMatch<true>(_meta._sort,
                                                  _meta._storedValues,
                                                   attrs, usedColumns, columnsCount)) {
    aql::latematerialized::setAttributesMaxMatchedColumns<true>(usedColumns, columnsCount);
    for (size_t i = 0; i < projections.size(); ++i) {
      auto const& nodeAttr = attrs[i];
      size_t index{0};
      if (iresearch::IResearchViewNode::SortColumnNumber == nodeAttr.afData.columnNumber) { // found in the sort column
        index = nodeAttr.afData.fieldNumber;
      }
      else {
        index = _meta._sort.fields().size();
        TRI_ASSERT(nodeAttr.afData.columnNumber < static_cast<ptrdiff_t>(_meta._storedValues.columns().size()));
        for (ptrdiff_t j = 0; j < nodeAttr.afData.columnNumber; j++) {
          // FIXME: we will need to decode the same back inside index iterator :(
          index += _meta._storedValues.columns()[j].fields.size();
        }
      }
      TRI_ASSERT((index + nodeAttr.afData.fieldNumber) <=
                 std::numeric_limits<uint16_t>::max());
      projections[i].coveringIndexPosition = static_cast<uint16_t>(index + nodeAttr.afData.fieldNumber);
      TRI_ASSERT(projections[i].path.size() > nodeAttr.afData.postfix);
      projections[i].coveringIndexCutoff = static_cast<uint16_t>(projections[i].path.size() - nodeAttr.afData.postfix);
    }
    return true;
  }
  return false;
}

bool IResearchInvertedIndex::matchesFieldsDefinition(VPackSlice other) const {
  auto value = other.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  auto const count = _meta._fields.size();
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
    TRI_ParseAttributeString(in, translate, true);
    for (auto const& f : _meta._fields) {
      if (f.second._shortName == analyzerName) { // FIXME check case custom1 <> _system::custom1
        if (arangodb::basics::AttributeName::isIdentical(f.first, translate, false)) {
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
    IndexIteratorOptions const& opts, int mutableConditionIdx, aql::Projections const* projections) {
  if (node) {
    std::string_view extraFieldName(nullptr, 0);
    if (mutableConditionIdx >= 0 && (!projections || projections->empty())) {
      TRI_ASSERT(mutableConditionIdx < static_cast<int64_t>(node->numMembers()));
      // we are in traversal. So try to find extra. If we are searching for '_to' then
      // "next" step (and our extra) is '_from' and vice versa
      auto mutableCondition = node->getMember(mutableConditionIdx);
      if (mutableCondition->type == aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
        TRI_ASSERT(mutableCondition->numMembers() == 2);
        auto attributeAccess = mutableCondition->getMember(0)->type == aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS ?
                               mutableCondition->getMember(0) : mutableCondition->getMember(1);
        if (attributeAccess->type == aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS &&
            attributeAccess->value.type == aql::AstNodeValueType::VALUE_TYPE_STRING) {
          auto fieldName = attributeAccess->getStringRef();
          if (fieldName == "_from") {
            extraFieldName = "_to";
          } else if (fieldName == "_to") {
            extraFieldName = "_from";
          }
        }
      }
    }
    return std::make_unique<IResearchInvertedIndexIterator>(collection, trx, *node, this, reference, mutableConditionIdx,
                                                            extraFieldName, projections);
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
    // for filter we could safely assume 0 as we don't store exact token values in the index and can't return it.
    filterCosts.coveredAttributes = 0;
    filterCosts.estimatedCosts = 1;// FIXME: use itemsInIndex;
  }
  return filterCosts;
}

aql::AstNode* IResearchInvertedIndex::specializeCondition(aql::AstNode* node,
                                                          aql::Variable const* reference) const {
  return node;
}

bool lazy_filter_bitset_iterator::next() {
  while (!word_) {
    if (bitset_.get(word_idx_, &word_)) {
      word_idx_++; // move only if ok. Or we could be overflowed!
      base_ += irs::bits_required<lazy_bitset::word_t>();
      doc_.value = base_ - 1;
      continue;
    }
    doc_.value = irs::doc_limits::eof();
    word_ = 0;
    return false;
  }
  const irs::doc_id_t delta = irs::doc_id_t(irs::math::math_traits<lazy_bitset::word_t>::ctz(word_));
  assert(delta < irs::bits_required<lazy_bitset::word_t>());
  word_ = (word_ >> delta) >> 1;
  doc_.value += 1 + delta;
  return true;
}

irs::doc_id_t lazy_filter_bitset_iterator::seek(irs::doc_id_t target) {
  word_idx_ = target / irs::bits_required<lazy_bitset::word_t>();
  if (bitset_.get(word_idx_, &word_)) {
    const irs::doc_id_t bit_idx = target % irs::bits_required<lazy_bitset::word_t>();
    base_ = word_idx_ * irs::bits_required<lazy_bitset::word_t>();
    word_ = word_ >> bit_idx;
    doc_.value = base_ - 1 + bit_idx;
    word_idx_++; // mark this word as consumed
    // FIXME consider inlining to speedup
    next();
    return doc_.value;
  } else {
    doc_.value = irs::doc_limits::eof();
    word_ = 0;
    return doc_.value;
  }
}

irs::attribute* lazy_filter_bitset_iterator::get_mutable(irs::type_info::type_id id) noexcept {
  if (irs::type<irs::document>::id() == id) {
    return &doc_;
  }
  return irs::type<irs::cost>::id() == id
    ? &cost_ : nullptr;
}

void lazy_filter_bitset_iterator::reset() noexcept {
  word_idx_ = 0;
  word_ = 0;
  base_ = irs::doc_limits::invalid() - irs::bits_required<lazy_bitset::word_t>(); // before the first word
  doc_.value = irs::doc_limits::invalid();
}

bool lazy_bitset::get(size_t word_idx, word_t* data) {
  constexpr auto BITS{ irs::bits_required<word_t>() };
  if (!set_) {
    const size_t bits = segment_->docs_count() + irs::doc_limits::min();
    words_ = irs::bitset::bits_to_words(bits);
    set_ = irs::memory::make_unique<word_t[]>(words_);
    std::memset(set_.get(), 0, sizeof(word_t) * words_);
    real_doc_itr_ = segment_->mask(filter_.execute(*segment_));
    real_doc_ = irs::get<irs::document>(*real_doc_itr_);
    begin_ = set_.get();
    end_ = begin_;
  }
  if (word_idx >= words_) {
    return false;
  }
  word_t* requested = set_.get() + word_idx;
  if (requested >= end_) {
    auto block_limit = ((word_idx + 1) * BITS) - 1;
    bool target_passed{false};
    auto doc_id{irs::doc_limits::invalid()};
    while (real_doc_itr_->next()) {
      doc_id = real_doc_->value;
      irs::set_bit(set_[doc_id / BITS], doc_id % BITS);
      if (doc_id >= block_limit) {
        break;  // we've filled requested word
      }
    }
    end_ = requested + 1;
  }
  *data = *requested;
  return true;
}

} // namespace iresearch
} // namespace arangodb
