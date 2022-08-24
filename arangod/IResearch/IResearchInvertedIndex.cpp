////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/QueryCache.h"
#include "Aql/IResearchViewNode.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/DownCast.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "IResearchDocument.h"
#include "IResearchFilterFactory.h"
#include "IResearchIdentityAnalyzer.h"
#include "Transaction/Methods.h"

#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include <index/heap_iterator.hpp>
#include "store/directory.hpp"
#include "utils/utf8_path.hpp"

namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

AnalyzerProvider makeAnalyzerProvider(IResearchInvertedIndexMeta const& meta) {
  static FieldMeta::Analyzer const defaultAnalyzer{
      IResearchAnalyzerFeature::identity()};
  return [&meta](std::string_view fieldPath) -> FieldMeta::Analyzer const& {
    for (auto const& field : meta._fields) {
      if (field.path() == fieldPath) {
        return field.analyzer();
      }
    }
    return defaultAnalyzer;
  };
}

irs::bytes_ref refFromSlice(VPackSlice slice) {
  return {slice.startAs<irs::byte_type>(), slice.byteSize()};
}

bool supportsFilterNode(
    IndexId id, std::vector<std::vector<basics::AttributeName>> const& fields,
    aql::AstNode const* node, aql::Variable const* reference,
    std::vector<InvertedIndexField> const& metaFields,
    AnalyzerProvider const* provider) {
  // We don`t want byExpression filters
  // and can`t apply index if we are not sure what attribute is
  // accessed so we provide QueryContext which is unable to
  // execute expressions and only allow to pass conditions with constant
  // attributes access/values. Otherwise if we have say d[a.smth] where 'a' is a
  // variable from the upstream loop we may get here a field we don`t have in
  // the index.
  QueryContext const queryCtx{
      .ref = reference, .isSearchQuery = false, .isOldMangling = false};

  // The analyzer is referenced in the FilterContext and used during the
  // following ::makeFilter() call, so may not be a temporary.
  FilterContext const filterCtx{.fieldAnalyzerProvider = provider,
                                .contextAnalyzer = emptyAnalyzer(),
                                .fields = metaFields};

  auto rv = FilterFactory::filter(nullptr, queryCtx, filterCtx, *node);

  LOG_TOPIC_IF("ee0f7", TRACE, arangodb::iresearch::TOPIC, rv.fail())
      << "Failed to build filter with error'" << rv.errorMessage()
      << "' Skipping index " << id.id();
  return rv.ok();
}

const irs::payload NoPayload;

inline irs::doc_iterator::ptr pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column(DocumentPrimaryKey::PK());

  return reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;
}

/// @brief  Struct represents value of a Projections[i]
///         After the document id has beend found "get" method
///         could be used to get Slice for the Projections
struct CoveringValue {
  explicit CoveringValue(irs::string_ref col) : column(col) {}
  CoveringValue(CoveringValue&& other) noexcept : column(other.column) {}

  void reset(irs::sub_reader const& rdr) {
    itr.reset();
    value = &NoPayload;
    // FIXME: this is cheap. Keep it here?
    auto extraValuesReader = column.empty() ? rdr.sort() : rdr.column(column);
    // FIXME: this is expensive - move it to get and do lazily?
    if (ADB_LIKELY(extraValuesReader)) {
      itr = extraValuesReader->iterator(irs::ColumnHint::kNormal);
      TRI_ASSERT(itr);
      if (ADB_LIKELY(itr)) {
        value = irs::get<irs::payload>(*itr);
        TRI_ASSERT(value);
        if (ADB_UNLIKELY(!value)) {
          value = &NoPayload;
        }
      }
    }
  }

  VPackSlice get(irs::doc_id_t doc, size_t index) const {
    if (itr && doc == itr->seek(doc)) {
      size_t i = 0;
      auto const totalSize = value->value.size();
      if (index == 0 && totalSize == 0) {
        // one empty field optimization
        return VPackSlice::nullSlice();
      }
      TRI_ASSERT(totalSize > 0);
      size_t size = 0;
      VPackSlice slice(value->value.c_str());
      TRI_ASSERT(slice.byteSize() <= totalSize);
      while (i < index) {
        if (ADB_LIKELY(size < totalSize)) {
          size += slice.byteSize();
          TRI_ASSERT(size <= totalSize);
          if (ADB_UNLIKELY(size > totalSize)) {
            slice = VPackSlice::noneSlice();
            break;
          }
          slice = VPackSlice{slice.end()};
          ++i;
        } else {
          slice = VPackSlice::noneSlice();
          break;
        }
      }
      return slice;
    }
    return VPackSlice::noneSlice();
  }

  irs::doc_iterator::ptr itr;
  irs::string_ref column;
  const irs::payload* value{};
};

/// @brief Represents virtual "vector" of stored values in the irsesearch index
class CoveringVector final : public IndexIteratorCoveringData {
 public:
  explicit CoveringVector(IResearchInvertedIndexMeta const& meta) {
    size_t fields{meta._sort.fields().size()};
    _coverage.reserve(meta._sort.size() + meta._storedValues.columns().size());
    if (!meta._sort.empty()) {
      _coverage.emplace_back(fields, CoveringValue(irs::string_ref::EMPTY));
    }
    for (auto const& column : meta._storedValues.columns()) {
      fields += column.fields.size();
      _coverage.emplace_back(fields, CoveringValue(column.name));
    }
    _length = fields;
  }

  CoveringVector(CoveringVector&& other) = default;
  CoveringVector(CoveringVector& other) = delete;

  CoveringVector clone() const {
    CoveringVector res;
    res._length = this->_length;
    res._coverage.reserve(this->_coverage.size());
    for (auto& c : _coverage) {
      res._coverage.emplace_back(c.first, CoveringValue(c.second.column));
    }
    return res;
  }

  void reset(irs::sub_reader const& rdr) {
    std::for_each(
        _coverage.begin(), _coverage.end(),
        [&rdr](decltype(_coverage)::value_type& v) { v.second.reset(rdr); });
  }

  void seek(irs::doc_id_t doc) { _doc = doc; }

  VPackSlice at(size_t i) const override { return get(i); }

  bool isArray() const noexcept override { return true; }

  bool empty() const { return _coverage.empty(); }

  velocypack::ValueLength length() const override { return _length; }

 private:
  // for prototype cloning
  CoveringVector() = default;

  VPackSlice get(size_t i) const {
    TRI_ASSERT(irs::doc_limits::valid(_doc));
    size_t column{0};
    // FIXME: check for the performance bottleneck!
    while (column < _coverage.size() && _coverage[column].first <= i) {
      ++column;
    }
    if (column < _coverage.size()) {
      size_t const prev = column ? _coverage[column - 1].first : 0;
      TRI_ASSERT(i >= prev);
      return _coverage[column].second.get(_doc, i - prev);
    }
    return VPackSlice::noneSlice();
  }

  std::vector<std::pair<size_t, CoveringValue>> _coverage;
  irs::doc_id_t _doc{irs::doc_limits::invalid()};
  velocypack::ValueLength _length{0};
};

class IResearchSnapshotState final : public TransactionState::Cookie {
 public:
  IResearchDataStore::Snapshot snapshot;
  std::map<aql::AstNode const*, irs::proxy_filter::cache_ptr>
      _immutablePartCache;
};

class IResearchInvertedIndexIteratorBase : public IndexIterator {
 public:
  IResearchInvertedIndexIteratorBase(LogicalCollection* collection,
                                     transaction::Methods* trx,
                                     aql::AstNode const* condition,
                                     IResearchInvertedIndex* index,
                                     aql::Variable const* variable,
                                     int mutableConditionIdx)
      : IndexIterator(collection, trx, ReadOwnWrites::no),
        _index(index),
        _variable(variable),
        _mutableConditionIdx(mutableConditionIdx) {
    resetFilter(condition);
  }

  bool canRearm() const override { return _mutableConditionIdx != -1; }

  bool rearmImpl(aql::AstNode const* node, aql::Variable const*,
                 IndexIteratorOptions const&) override {
    TRI_ASSERT(node);
    if (ADB_LIKELY(node)) {
      reset();
      resetFilter(node);
      return true;
    }
    return false;
  }

 protected:
  void resetFilter(aql::AstNode const* condition) {
    if (!_trx->state()) {
      LOG_TOPIC("a9ccd", WARN, arangodb::iresearch::TOPIC)
          << "failed to get transaction state while creating inverted index "
             "snapshot";

      return;
    }
    auto& state = *(_trx->state());

    // TODO FIXME find a better way to look up a State
    // we cannot use _index pointer as key - the same is used for storing
    // removes/inserts so we add 1 to the value (we need just something unique
    // after all)
    void const* key = reinterpret_cast<uint8_t const*>(_index) + 1;
    auto* ctx = basics::downCast<IResearchSnapshotState>(state.cookie(key));
    if (!ctx) {
      auto ptr = irs::memory::make_unique<IResearchSnapshotState>();
      ctx = ptr.get();
      state.cookie(key, std::move(ptr));

      if (!ctx) {
        LOG_TOPIC("d7061", WARN, arangodb::iresearch::TOPIC)
            << "failed to store state into a TransactionState for snapshot of "
               "inverted index ";
        return;
      }
      ctx->snapshot = _index->snapshot();
    }
    _reader = &ctx->snapshot.getDirectoryReader();
    QueryContext const queryCtx{.trx = _trx,
                                .index = _reader,
                                .ref = _variable,
                                .isSearchQuery = false,
                                .isOldMangling = false};

    AnalyzerProvider analyzerProvider = makeAnalyzerProvider(_index->meta());

    irs::Or root;
    if (condition) {
      if (_mutableConditionIdx ==
              transaction::Methods::kNoMutableConditionIdx ||
          (condition->type != aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND &&
           condition->type != aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR)) {
        // The analyzer is referenced in the FilterContext and used during the
        // following FilterFactory::::filter() call, so may not be a temporary.
        FilterContext const filterCtx{
            .fieldAnalyzerProvider = &analyzerProvider,
            .contextAnalyzer = emptyAnalyzer(),
            .fields = _index->meta()._fields};
        auto rv = FilterFactory::filter(&root, queryCtx, filterCtx, *condition);

        if (rv.fail()) {
          velocypack::Builder builder;
          condition->toVelocyPack(builder, true);
          THROW_ARANGO_EXCEPTION_MESSAGE(
              rv.errorNumber(),
              basics::StringUtils::concatT(
                  "failed to build filter while querying "
                  "inverted index, query '",
                  builder.toJson(), "': ", rv.errorMessage()));
        }
      } else {
        TRI_ASSERT(static_cast<int64_t>(condition->numMembers()) >
                   _mutableConditionIdx);
        if (ADB_UNLIKELY(static_cast<int64_t>(condition->numMembers()) <=
                         _mutableConditionIdx)) {
          velocypack::Builder builder;
          condition->toVelocyPack(builder, true);
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL_AQL,
              basics::StringUtils::concatT(
                  "Invalid condition members count while querying "
                  "inverted index, query '",
                  builder.toJson(), "'"));
        }
        irs::boolean_filter* conditionJoiner{nullptr};

        if (condition->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
          conditionJoiner = &root.add<irs::And>();
        } else {
          TRI_ASSERT((condition->type ==
                      aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR));
          conditionJoiner = &root.add<irs::Or>();
        }

        FilterContext const filterCtx{
            .fieldAnalyzerProvider = &analyzerProvider,
            .contextAnalyzer = emptyAnalyzer(),
            .fields = _index->meta()._fields};

        auto& mutable_root = conditionJoiner->add<irs::Or>();
        auto rv =
            FilterFactory::filter(&mutable_root, queryCtx, filterCtx,
                                  *condition->getMember(_mutableConditionIdx));
        if (rv.fail()) {
          velocypack::Builder builder;
          condition->toVelocyPack(builder, true);
          THROW_ARANGO_EXCEPTION_MESSAGE(
              rv.errorNumber(),
              basics::StringUtils::concatT(
                  "failed to build mutable filter part while querying "
                  "inverted index, query '",
                  builder.toJson(), "': ", rv.errorMessage()));
        }

        auto& proxy_filter = conditionJoiner->add<irs::proxy_filter>();
        auto existingCache = ctx->_immutablePartCache.find(condition);
        if (existingCache != ctx->_immutablePartCache.end()) {
          proxy_filter.set_cache(existingCache->second);
        } else {
          irs::boolean_filter* immutableRoot;
          irs::proxy_filter::cache_ptr newCache;
          if (condition->type ==
              aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
            auto res = proxy_filter.set_filter<irs::And>();
            ctx->_immutablePartCache[condition] = res.second;
            immutableRoot = &res.first;
          } else {
            TRI_ASSERT((condition->type ==
                        aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR));
            auto res = proxy_filter.set_filter<irs::Or>();
            ctx->_immutablePartCache[condition] = res.second;
            immutableRoot = &res.first;
          }

          auto const conditionSize =
              static_cast<int64_t>(condition->numMembers());

          // The analyzer is referenced in the FilterContext and used during the
          // following ::filter() call, so may not be a temporary.
          FilterContext const filterCtx{
              .fieldAnalyzerProvider = &analyzerProvider,
              .contextAnalyzer = emptyAnalyzer()};

          for (int64_t i = 0; i < conditionSize; ++i) {
            if (i != _mutableConditionIdx) {
              auto& tmp_root = immutableRoot->add<irs::Or>();
              auto rv = FilterFactory::filter(&tmp_root, queryCtx, filterCtx,
                                              *condition->getMember(i));
              if (rv.fail()) {
                velocypack::Builder builder;
                condition->toVelocyPack(builder, true);
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    rv.errorNumber(),
                    basics::StringUtils::concatT(
                        "failed to build immutable filter part while querying "
                        "inverted index, query '",
                        builder.toJson(), "': ", rv.errorMessage()));
              }
            }
          }
        }
      }
    } else {
      // sorting case
      root.add<irs::all>();
    }
    _filter = root.prepare(*_reader, _order, irs::kNoBoost, nullptr);
    TRI_ASSERT(_filter);
    if (ADB_UNLIKELY(!_filter)) {
      if (condition) {
        velocypack::Builder builder;
        condition->toVelocyPack(builder, true);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL,
            basics::StringUtils::concatT(
                "Failed to prepare the filter while querying "
                "inverted index, query '",
                builder.toJson(), "'"));
      }
    }
  }

  irs::filter::prepared::ptr _filter;
  irs::Order _order;
  irs::index_reader const* _reader{0};
  IResearchInvertedIndex* _index;
  aql::Variable const* _variable;
  int _mutableConditionIdx;
};

class IResearchInvertedIndexIterator final
    : public IResearchInvertedIndexIteratorBase {
 public:
  IResearchInvertedIndexIterator(LogicalCollection* collection,
                                 transaction::Methods* trx,
                                 aql::AstNode const* condition,
                                 IResearchInvertedIndex* index,
                                 aql::Variable const* variable,
                                 int mutableConditionIdx)
      : IResearchInvertedIndexIteratorBase(collection, trx, condition, index,
                                           variable, mutableConditionIdx),
        _projections(index->meta()) {}

  std::string_view typeName() const noexcept override {
    return "inverted-index-iterator";
  }

 protected:
  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    return nextImplInternal<LocalDocumentIdCallback, false, true>(callback,
                                                                  limit);
  }

  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    return nextImplInternal<CoveringCallback, true, true>(callback, limit);
  }

  void skipImpl(uint64_t count, uint64_t& skipped) override {
    nextImplInternal<decltype(skipped), false, false>(skipped, count);
  }

  // FIXME: Evaluate buffering iresearch reads
  template<typename Callback, bool withCovering, bool produce>
  bool nextImplInternal(Callback const& callback, uint64_t limit) {
    if (limit == 0 || !_filter) {
      TRI_ASSERT(
          limit >
          0);  // Someone called with limit == 0. Api broken
               // validate that Iterator is in a good shape and hasn't failed
      TRI_ASSERT(_filter);  // _filter is not initialized (should not happen)
      return false;
    }
    TRI_ASSERT(_reader);
    auto const count = _reader->size();
    while (limit > 0) {
      if (!_itr || !_itr->next()) {
        if (_readerOffset >= count) {
          break;
        }
        auto& segmentReader = (*_reader)[_readerOffset++];
        // always init all iterators as we do not know if it will be
        // skip-next-covering mixture of the calls
        _pkDocItr = ::pkColumn(segmentReader);
        if (_pkDocItr) {
          _pkValue = irs::get<irs::payload>(*_pkDocItr);
          if (ADB_UNLIKELY(!_pkValue)) {
            _pkValue = &NoPayload;
          }
        } else {
          // skip segment without PK column
          continue;
        }
        _projections.reset(segmentReader);
        _itr = segmentReader.mask(_filter->execute(segmentReader));
        _doc = irs::get<irs::document>(*_itr);
      } else {
        if constexpr (produce) {
          if (_doc->value == _pkDocItr->seek(_doc->value)) {
            LocalDocumentId documentId;
            bool const readSuccess =
                DocumentPrimaryKey::read(documentId, _pkValue->value);
            if (readSuccess) {
              if constexpr (withCovering) {
                _projections.seek(_doc->value);
                if (callback(documentId, _projections)) {
                  --limit;
                }
              } else {
                if (callback(documentId)) {
                  --limit;  // count only existing documents
                }
              }
            }
          }
        } else {
          --limit;
          ++callback;
        }
      }
    }
    return limit == 0;
  }

  void resetImpl() final {
    _readerOffset = 0;
    _itr.reset();
    _doc = nullptr;
  }

 private:
  // FIXME:!! check order
  irs::doc_iterator::ptr _itr;
  irs::doc_iterator::ptr _pkDocItr;
  irs::document const* _doc{};
  const irs::payload* _pkValue{};
  size_t _readerOffset{0};
  CoveringVector _projections;
};

class IResearchInvertedIndexMergeIterator final
    : public IResearchInvertedIndexIteratorBase {
 public:
  IResearchInvertedIndexMergeIterator(LogicalCollection* collection,
                                      transaction::Methods* trx,
                                      aql::AstNode const* condition,
                                      IResearchInvertedIndex* index,
                                      aql::Variable const* variable,
                                      int mutableConditionIdx)
      : IResearchInvertedIndexIteratorBase(collection, trx, condition, index,
                                           variable, mutableConditionIdx),
        _heap_it({index->meta()._sort, index->meta()._sort.size(), _segments}),
        _projectionsPrototype(index->meta()) {}

  std::string_view typeName() const noexcept final {
    return "inverted-index-merge-iterator";
  }

 protected:
  void resetImpl() final {
    _segments.clear();
    auto const size = _reader->size();
    _segments.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      auto& segment = (*_reader)[i];
      irs::doc_iterator::ptr it = segment.mask(_filter->execute(segment));
      TRI_ASSERT(!_projectionsPrototype
                      .empty());  // at least sort column should be here
      _segments.emplace_back(std::move(it), segment, _projectionsPrototype);
    }
    _heap_it.reset(_segments.size());
  }

  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    return nextImplInternal<LocalDocumentIdCallback, false, true>(callback,
                                                                  limit);
  }

  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    return nextImplInternal<CoveringCallback, true, true>(callback, limit);
  }

  void skipImpl(uint64_t count, uint64_t& skipped) override {
    nextImplInternal<decltype(skipped), false, false>(skipped, count);
  }

  template<typename Callback, bool withCovering, bool produce>
  bool nextImplInternal(Callback const& callback, uint64_t limit) {
    if (limit == 0 || !_filter) {
      TRI_ASSERT(
          limit >
          0);  // Someone called with limit == 0. Api broken
               // validate that Iterator is in a good shape and hasn't failed
      TRI_ASSERT(_filter);  // _filter is not initialized (should not happen)
      return false;
    }
    TRI_ASSERT(_reader);
    if (_segments.empty() && _reader->size()) {
      reset();
    }
    while (limit && _heap_it.next()) {
      auto& segment = _segments[_heap_it.value()];
      if constexpr (produce) {
        if (segment.doc->value == segment.pkDocItr->seek(segment.doc->value)) {
          LocalDocumentId documentId;
          bool const readSuccess =
              DocumentPrimaryKey::read(documentId, segment.pkValue->value);
          if (readSuccess) {
            if constexpr (withCovering) {
              segment.projections.seek(segment.doc->value);
              if (callback(documentId, segment.projections)) {
                --limit;
              }
            } else {
              if (callback(documentId)) {
                --limit;  // count only existing documents
              }
            }
          }
        }
      } else {
        --limit;
        ++callback;
      }
    }
    return limit == 0;
  }

 private:
  struct Segment {
    Segment(irs::doc_iterator::ptr&& docs, irs::sub_reader const& segment,
            CoveringVector const& prototype)
        : itr(std::move(docs)), projections(prototype.clone()) {
      projections.reset(segment);
      doc = irs::get<irs::document>(*itr);
      TRI_ASSERT(doc);
      pkDocItr = ::pkColumn(segment);
      TRI_ASSERT(pkDocItr);
      if (ADB_LIKELY(pkDocItr)) {
        pkValue = irs::get<irs::payload>(*pkDocItr);
        if (ADB_UNLIKELY(!pkValue)) {
          pkValue = &NoPayload;
        }
      }
    }

    Segment(Segment const&) = delete;
    Segment(Segment&&) = default;
    Segment& operator=(Segment const&) = delete;
    Segment& operator=(Segment&&) = delete;

    irs::doc_iterator::ptr itr;
    irs::doc_iterator::ptr pkDocItr;
    irs::document const* doc{};
    irs::payload const* pkValue{};
    CoveringVector projections;
    VPackSlice sortValue;
  };

  class MinHeapContext {
   public:
    MinHeapContext(IResearchInvertedIndexSort const& sort, size_t sortBuckets,
                   std::vector<Segment>& segments) noexcept
        : _less(sort, sortBuckets), _segments(&segments) {}

    // advance
    bool operator()(size_t i) const {
      assert(i < _segments->size());
      auto& segment = (*_segments)[i];
      while (segment.doc && segment.itr->next()) {
        auto const doc = segment.doc->value;
        segment.projections.seek(doc);
        segment.sortValue = segment.projections.at(0);  // Sort is always first
        if (!segment.sortValue.isNone()) {
          return true;
        }
      }
      return false;
    }

    // compare
    bool operator()(size_t lhs, size_t rhs) const {
      assert(lhs < _segments->size());
      assert(rhs < _segments->size());
      return _less(refFromSlice((*_segments)[rhs].sortValue),
                   refFromSlice((*_segments)[lhs].sortValue));
    }

    VPackComparer<IResearchInvertedIndexSort> _less;
    std::vector<Segment>* _segments;
  };

  std::vector<Segment> _segments;
  irs::ExternalHeapIterator<MinHeapContext> _heap_it;
  CoveringVector const _projectionsPrototype;
};

}  // namespace

namespace arangodb {
namespace iresearch {

IResearchInvertedIndex::IResearchInvertedIndex(IndexId iid,
                                               LogicalCollection& collection)
    : IResearchDataStore(iid, collection) {}

// Analyzer names storing
//  - forPersistence ::<analyzer> from system and <analyzer> for local and
//  definitions are stored.
//  - For user -> database-name qualified names. No definitions are stored.
void IResearchInvertedIndex::toVelocyPack(ArangodServer& server,
                                          TRI_vocbase_t const* defaultVocbase,
                                          velocypack::Builder& builder,
                                          bool forPersistence) const {
  if (!_dataStore._meta.json(builder, nullptr, nullptr)) {
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

std::vector<std::vector<basics::AttributeName>> IResearchInvertedIndex::fields(
    IResearchInvertedIndexMeta const& meta) {
  std::vector<std::vector<basics::AttributeName>> res;
  res.reserve(meta._fields.size());
  for (auto const& f : meta._fields) {
    res.push_back(f._attribute);
  }
  return res;
}

std::vector<std::vector<basics::AttributeName>>
IResearchInvertedIndex::sortedFields(IResearchInvertedIndexMeta const& meta) {
  std::vector<std::vector<basics::AttributeName>> res;
  res.reserve(meta._sort.fields().size());
  for (auto const& f : meta._sort.fields()) {
    res.push_back(f);
  }
  return res;
}

Result IResearchInvertedIndex::init(
    VPackSlice definition, bool& pathExists,
    IResearchDataStore::InitCallback const& initCallback /*= {}*/) {
  std::string errField;
  if (!_meta.init(_collection.vocbase().server(), definition, true, errField,
                  _collection.vocbase().name())) {
    LOG_TOPIC("18c17", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? (std::string(
                       "failed to initialize index fields from definition: ") +
                   definition.toString())
                : (std::string("failed to initialize index fields from "
                               "definition, error in attribute '") +
                   errField + "': " + definition.toString()));
    return {TRI_ERROR_BAD_PARAMETER, errField};
  }

  TRI_ASSERT(_meta._sort.sortCompression());
  auto r = initDataStore(pathExists, initCallback,
                         static_cast<uint32_t>(_meta._version), isSorted(),
                         _meta.hasNested(), _meta._storedValues.columns(),
                         _meta._sort.sortCompression());
  if (r.ok()) {
    _comparer.reset(_meta._sort);
  }
  properties(_meta);
  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup referenced analyzer
////////////////////////////////////////////////////////////////////////////////
AnalyzerPool::ptr IResearchInvertedIndex::findAnalyzer(
    AnalyzerPool const& analyzer) const {
  auto const it =
      _meta._analyzerDefinitions.find(irs::string_ref(analyzer.name()));

  if (it == _meta._analyzerDefinitions.end()) {
    return nullptr;
  }

  auto pool = *it;

  if (pool && analyzer == *pool) {
    return pool;
  }

  return nullptr;
}

bool IResearchInvertedIndex::covers(aql::Projections& projections) const {
  std::vector<std::vector<aql::latematerialized::ColumnVariant<true>>>
      usedColumns;
  std::vector<aql::latematerialized::AttributeAndField<
      aql::latematerialized::IndexFieldData>>
      attrs;
  if (projections.empty()) {
    return false;
  }
  for (size_t i = 0; i < projections.size(); ++i) {
    aql::latematerialized::AttributeAndField<
        aql::latematerialized::IndexFieldData>
        af;
    af.attr.reserve(projections[i].path.path.size());
    for (auto const& a : projections[i].path.path) {
      af.attr.emplace_back(a, false);  // TODO: false?
    }
    attrs.emplace_back(af);
  }
  auto const columnsCount = _meta._storedValues.columns().size() + 1;
  usedColumns.resize(columnsCount);
  if (aql::latematerialized::attributesMatch<true>(
          _meta._sort, _meta._storedValues, attrs, usedColumns, columnsCount)) {
    aql::latematerialized::setAttributesMaxMatchedColumns<true>(usedColumns,
                                                                columnsCount);
    for (size_t i = 0; i < projections.size(); ++i) {
      auto const& nodeAttr = attrs[i];
      size_t index{0};
      if (iresearch::IResearchViewNode::kSortColumnNumber ==
          nodeAttr.afData.columnNumber) {  // found in the sort column
        index = nodeAttr.afData.fieldNumber;
      } else {
        index = _meta._sort.fields().size();
        TRI_ASSERT(
            nodeAttr.afData.columnNumber <
            static_cast<ptrdiff_t>(_meta._storedValues.columns().size()));
        for (ptrdiff_t j = 0; j < nodeAttr.afData.columnNumber; j++) {
          // FIXME: we will need to decode the same back inside index iterator
          // :(
          index += _meta._storedValues.columns()[j].fields.size();
        }
      }
      TRI_ASSERT((index + nodeAttr.afData.fieldNumber) <=
                 std::numeric_limits<uint16_t>::max());
      projections[i].coveringIndexPosition =
          static_cast<uint16_t>(index + nodeAttr.afData.fieldNumber);
      TRI_ASSERT(projections[i].path.size() > nodeAttr.afData.postfix);
      projections[i].coveringIndexCutoff = static_cast<uint16_t>(
          projections[i].path.size() - nodeAttr.afData.postfix);
    }
    return true;
  }
  return false;
}

bool IResearchInvertedIndex::matchesFieldsDefinition(
    VPackSlice other, LogicalCollection const& collection) const {
  return IResearchInvertedIndexMeta::matchesFieldsDefinition(_meta, other,
                                                             collection);
}

std::unique_ptr<IndexIterator> IResearchInvertedIndex::iteratorForCondition(
    LogicalCollection* collection, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& /*opts*/, int mutableConditionIdx) {
  if (node) {
    if (_meta._sort.empty()) {
      // FIXME: we should use non-sorted iterator in case we are not "covering"
      // SORT but options flag sorted is always true
      return std::make_unique<IResearchInvertedIndexIterator>(
          collection, trx, node, this, reference, mutableConditionIdx);
    } else {
      return std::make_unique<IResearchInvertedIndexMergeIterator>(
          collection, trx, node, this, reference, mutableConditionIdx);
    }
  } else {
    // sorting  case

    // we should not be called for sort optimization if our index is not sorted
    TRI_ASSERT(!_meta._sort.empty());

    return std::make_unique<IResearchInvertedIndexMergeIterator>(
        collection, trx, node, this, reference,
        transaction::Methods::kNoMutableConditionIdx);
  }
}

Index::SortCosts IResearchInvertedIndex::supportsSortCondition(
    aql::SortCondition const* sortCondition, aql::Variable const* reference,
    size_t itemsInIndex) const {
  auto fields = sortedFields(_meta);

  // FIXME: We store null slice in case of missing attribute - so do we really
  // need onlyUsesNonNullSortAttributes ?
  // -->>!sortCondition->onlyUsesNonNullSortAttributes(fields)||
  // FIXME: We should support sort only if we support filter! Check that having
  // sparse = true is enough!
  if (!sortCondition
           ->isOnlyAttributeAccess() ||  // we don't have pre-computed fields so
                                         // only direct access
      fields.size() < sortCondition->numAttributes() ||
      sortCondition->numAttributes() >
          sortCondition->coveredAttributes(reference, fields)) {
    // no need to check has expansion as we don't support expansion for stored
    // values
    return Index::SortCosts::defaultCosts(itemsInIndex);
  }

  auto const numCovered = sortCondition->numAttributes();
  // finally check the direction
  for (size_t i = 0; i < numCovered; ++i) {
    if (std::get<2>(sortCondition->field(i)) != _meta._sort.direction(i)) {
      // index is sorted in different order than requested in SORT condition
      return Index::SortCosts::defaultCosts(itemsInIndex);
    }
  }
  return Index::SortCosts::zeroCosts(numCovered);
}

Index::FilterCosts IResearchInvertedIndex::supportsFilterCondition(
    IndexId id, std::vector<std::vector<basics::AttributeName>> const& fields,
    std::vector<std::shared_ptr<Index>> const& /*allIndexes*/,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  TRI_ASSERT(node);
  TRI_ASSERT(reference);
  auto filterCosts = Index::FilterCosts::defaultCosts(itemsInIndex);

  // non-deterministic condition will mean full-scan. So we should
  // not use index here.
  // FIXME: maybe in the future we will be able to optimize just deterministic
  // part?
  if (!node->isDeterministic()) {
    LOG_TOPIC("750e6", TRACE, iresearch::TOPIC)
        << "Found non-deterministic condition. Skipping index " << id.id();
    return filterCosts;
  }
  AnalyzerProvider analyzerProvider = makeAnalyzerProvider(_meta);

  // at first try to cover whole node
  if (supportsFilterNode(id, fields, node, reference, _meta._fields,
                         &analyzerProvider)) {
    filterCosts.supportsCondition = true;
    filterCosts.coveredAttributes = node->numMembers();
    filterCosts.estimatedCosts = static_cast<double>(itemsInIndex);
  } else if (node->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
    // for AND node we could try to support only part of the condition
    size_t const n = node->numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto part = node->getMemberUnchecked(i);
      if (supportsFilterNode(id, fields, part, reference, _meta._fields,
                             &analyzerProvider)) {
        filterCosts.supportsCondition = true;
        ++filterCosts.coveredAttributes;
        filterCosts.estimatedCosts = static_cast<double>(itemsInIndex);
      }
    }
  }
  return filterCosts;
}

void IResearchInvertedIndex::invalidateQueryCache(TRI_vocbase_t* vocbase) {
  aql::QueryCache::instance()->invalidate(vocbase, collection().guid());
}

aql::AstNode* IResearchInvertedIndex::specializeCondition(
    aql::AstNode* node, aql::Variable const* reference) const {
  auto indexedFields = fields(_meta);

  AnalyzerProvider analyzerProvider = makeAnalyzerProvider(_meta);

  if (!supportsFilterNode(id(), indexedFields, node, reference, _meta._fields,
                          &analyzerProvider)) {
    TRI_ASSERT(node->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
    std::vector<aql::AstNode const*> children;
    size_t const n = node->numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto part = node->getMemberUnchecked(i);
      if (supportsFilterNode(id(), indexedFields, part, reference,
                             _meta._fields, &analyzerProvider)) {
        children.push_back(part);
      }
    }
    // must edit in place, no access to AST; TODO change so we can replace with
    // copy
    TEMPORARILY_UNLOCK_NODE(node);
    node->clearMembers();
    for (auto& it : children) {
      node->addMember(it);
    }
  }
  return node;
}

void IResearchInvertedClusterIndex::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  auto const forPersistence =
      Index::hasFlag(flags, Index::Serialize::Internals);
  VPackObjectBuilder objectBuilder(&builder);
  IResearchInvertedIndex::toVelocyPack(
      IResearchDataStore::collection().vocbase().server(),
      &IResearchDataStore::collection().vocbase(), builder, forPersistence);
  // can't use Index::toVelocyPack as it will try to output 'fields'
  // but we have custom storage format
  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(std::to_string(_iid.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              ::velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName, velocypack::Value(name()));
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));
}

bool IResearchInvertedClusterIndex::matchesDefinition(
    velocypack::Slice const& other) const {
  TRI_ASSERT(other.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = other.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  auto typeStr = typeSlice.stringView();
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
    std::string_view idRef = value.stringView();
    return idRef == std::to_string(IResearchDataStore::id().id());
  }
  return IResearchInvertedIndex::matchesFieldsDefinition(
      other, IResearchDataStore::_collection);
}

}  // namespace iresearch
}  // namespace arangodb
