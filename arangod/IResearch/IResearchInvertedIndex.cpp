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

class IResearchInvertedIndexIterator final : public IndexIterator  {
 public:
  IResearchInvertedIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                                 aql::AstNode const& condition, IResearchInvertedIndex* index,
                                 aql::Variable const* variable, int64_t mutableConditionIdx,
                                 bool withExtra)
    : IndexIterator(collection, trx), _index(index), _condition(condition),
      _variable(variable), _mutableConditionIdx(mutableConditionIdx), _withExtra(withExtra) {}
  char const* typeName() const override { return "inverted-index-iterator"; }

  /// @brief The default index has no extra information
  bool hasExtra() const override { return _withExtra; }

 protected:
  bool nextExtraImpl(ExtraCallback const& callback, size_t limit) override {
    if (limit == 0) {
      TRI_ASSERT(false);  // Someone called with limit == 0. Api broken
                          // validate that Iterator is in a good shape and hasn't failed
      return false;
    }
    return nextImpl([&callback](LocalDocumentId const& token) {
      return false;
    },limit);
  }

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

  bool canRearm() const override { 
    return _mutableConditionIdx != -1;
  }

  bool rearmImpl(arangodb::aql::AstNode const*,
    arangodb::aql::Variable const*,
    IndexIteratorOptions const&) override {
    reset();
    return true;
  }

  // FIXME: track if the filter condition is volatile! 
  // FIXME: support sorting!
  // FIXME: consider rearm implementation!
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
          (_condition.type != aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND && 
           _condition.type != aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR)) {
      auto rv = FilterFactory::filter(&root, queryCtx, _condition, false);
      if (rv.fail()) {
        arangodb::velocypack::Builder builder;
        _condition.toVelocyPack(builder, true);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            rv.errorNumber(),
            basics::StringUtils::concatT("failed to build filter while querying "
                                 "inverted index, query '",
                                 builder.toJson(), "': ", rv.errorMessage()));
      }
    } else {
      TRI_ASSERT(_condition.numMembers() > _mutableConditionIdx);
      irs::boolean_filter* conditionJoiner{nullptr};
      std::unique_ptr<irs::boolean_filter> immutableRoot;
      if (_condition.type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
        conditionJoiner = &root.add<irs::And>();
        immutableRoot = std::make_unique<irs::And>();
        
      } else {
        TRI_ASSERT((_condition.type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR));
        conditionJoiner = &root.add<irs::Or>();
        immutableRoot = std::make_unique<irs::Or>();
      }
      auto& mutable_root = conditionJoiner->add<irs::Or>();
      auto rv = FilterFactory::filter(&mutable_root, queryCtx,
                                      *_condition.getMember(_mutableConditionIdx), false);
      if (rv.fail()) {
        arangodb::velocypack::Builder builder;
        _condition.toVelocyPack(builder, true);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            rv.errorNumber(),
            basics::StringUtils::concatT(
                "failed to build mutable filter part while querying "
                "inverted index, query '",
                builder.toJson(), "': ", rv.errorMessage()));
      }
      auto const conditionSize = static_cast<int64_t>(_condition.numMembers());
      for (int64_t i = 0; i < conditionSize; ++i) {
        if (i != _mutableConditionIdx) {
          auto& tmp_root = immutableRoot->add<irs::Or>();
          auto rv = FilterFactory::filter(&tmp_root, queryCtx,
                                          *_condition.getMember(i), false);
          if (rv.fail()) {
            arangodb::velocypack::Builder builder;
            _condition.toVelocyPack(builder, true);
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
                       .set_cache(&(ctx->_immutablePartCache[&_condition]));
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
  int64_t _mutableConditionIdx;
  bool _withExtra;
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
  auto const res = initDataStore(initCallback, isSorted(), storedValuesColumns, primarySortCompression);

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

bool IResearchInvertedIndex::matchesFieldsDefinition(VPackSlice other) const {
  auto value = other.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  auto const count =_meta._fields.size();
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
    IndexIteratorOptions const& opts) {
  if (node) {
    return std::make_unique<IResearchInvertedIndexIterator>(collection, trx, *node, this, reference, opts.mutableConditionIdx);
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
    filterCosts.estimatedCosts = 1;
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
