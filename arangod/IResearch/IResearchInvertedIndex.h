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

#pragma once

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexFactory.h"
#include "IResearchCommon.h"
#include "IResearchDataStore.h"
#include "IResearchViewMeta.h"
#include "IResearchLinkMeta.h"

#include "search/boolean_filter.hpp"
#include "search/bitset_doc_iterator.hpp"
#include "search/score.hpp"
#include "search/conjunction.hpp"
#include "search/cost.hpp"

namespace arangodb {
namespace iresearch {


class lazy_filter_bitset_iterator final : public irs::bitset_doc_iterator {
 public:
  lazy_filter_bitset_iterator(
      const irs::sub_reader& segment,
      const irs::filter::prepared& filter,
      irs::cost::cost_t estimation) noexcept
    : bitset_doc_iterator(estimation),
      filter_(filter),
      segment_(&segment) {
  }

 protected:
  bool refill(const word_t** begin, const word_t** end) {
    if (refilled_) {
     return false;
    }
    refilled_ = true;
    const size_t bits = segment_->docs_count() + irs::doc_limits::min();
    words_ = irs::bitset::bits_to_words(bits);
    set_ = irs::memory::make_unique<word_t[]>(words_);
    std::memset(set_.get(), 0, sizeof(word_t)*words_);
    auto executed = segment_->mask(filter_.execute(*segment_));
    auto doc = irs::get<irs::document>(*executed);
    constexpr auto BITS{irs::bits_required<word_t>()};
    if (executed->next()) {
      auto doc_id = doc->value;
      irs::set_bit(set_[doc_id / BITS], doc_id % BITS);
      while (executed->next()) {
        doc_id = doc->value;
        irs::set_bit(set_[doc_id / BITS], doc_id % BITS);
      }
      *begin = set_.get();
      *end = set_.get() + words_;
      return true;
    }
    return false;
  }

 private:
  std::unique_ptr<word_t[]> set_;
  const irs::filter::prepared& filter_;
  const irs::sub_reader* segment_;
  size_t words_{0};
  bool refilled_{false};
};

class proxy_query final : public irs::filter::prepared {
 public:

  struct proxy_cache {
    std::vector<const irs::sub_reader*> readers_;
    std::vector<lazy_filter_bitset_iterator> doc_;
    irs::filter::prepared::ptr prepared_real_filter_;
  };

  proxy_query(proxy_cache& cache, irs::filter::ptr&& filter,
              const irs::index_reader& index, const irs::order::prepared& order)
    :cache_(cache), real_filter_(std::move(filter)), index_(index), order_(order) {}
   
  irs::doc_iterator::ptr execute(
     const irs::sub_reader& rdr,
     const irs::order::prepared&,
     const irs::attribute_provider* /*ctx*/) const override {

    // first try to find segment in cache.
    auto cached = std::find(cache_.readers_.begin(), cache_.readers_.end(), &rdr);
    if (cached != cache_.readers_.end()) {
      TRI_ASSERT(static_cast<ptrdiff_t>(cache_.doc_.size()) > std::distance(cache_.readers_.begin(), cached));
      auto& cached_doc  = cache_.doc_[std::distance(cache_.readers_.begin(), cached)];
      cached_doc.reset();
      return irs::memory::to_managed<irs::doc_iterator, false>(&cached_doc);
    } else {
      cache_.readers_.push_back(&rdr);
      if (!cache_.prepared_real_filter_) {
        cache_.prepared_real_filter_ = real_filter_->prepare(index_, order_);
      }
      cache_.doc_.emplace_back(rdr, *(cache_.prepared_real_filter_.get()), rdr.docs_count());
      return irs::memory::to_managed<irs::doc_iterator, false>(&cache_.doc_.back());
    }
  }

 private:
  proxy_cache& cache_;
  irs::filter::ptr real_filter_;
  const irs::index_reader& index_;
  const irs::order::prepared& order_;
};

class proxy_filter final : public irs::filter {
 public:

  DECLARE_FACTORY();

  proxy_filter() noexcept
    : filter(irs::type<proxy_filter>::get()) {}

  irs::filter::prepared::ptr prepare(
      const irs::index_reader& rdr,
      const irs::order::prepared& ord,
      irs::boost_t boost,
      const irs::attribute_provider* ctx) const override {
    if (!real_filter_ || !cache_) {
      TRI_ASSERT(false);
      return irs::filter::prepared::empty();
    }
    return irs::memory::make_managed<proxy_query>(*cache_, std::move(real_filter_), rdr, ord);
  }

  proxy_filter& add(irs::filter::ptr&& real_filter) {
    real_filter_ = std::move(real_filter);
    return *this;
  }

  proxy_filter& set_cache(proxy_query::proxy_cache* cache) {
    cache_ = cache;
    return *this;
  }

 private:
   mutable irs::filter::ptr real_filter_{nullptr};
   mutable proxy_query::proxy_cache* cache_{nullptr};
};

class IResearchInvertedIndex  : public IResearchDataStore {
 public:
  explicit IResearchInvertedIndex(IndexId iid, LogicalCollection& collection, InvertedIndexFieldMeta&& meta);

  void toVelocyPack(application_features::ApplicationServer& server,
                    TRI_vocbase_t const* defaultVocbase,
                    velocypack::Builder& builder, bool forPersistence) const;

  bool isSorted() const {
    return !_meta._sort.empty();
  }
  
  Result init(InitCallback const& initCallback = {});

  static std::vector<std::vector<arangodb::basics::AttributeName>> fields(InvertedIndexFieldMeta const& meta);

  bool matchesFieldsDefinition(VPackSlice other) const;

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  bool inProgress() const {
    return false;
  }

  std::unique_ptr<IndexIterator> iteratorForCondition(LogicalCollection* collection,
                                                      transaction::Methods* trx,
                                                      aql::AstNode const* node,
                                                      aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts);

  Index::SortCosts supportsSortCondition(aql::SortCondition const* sortCondition,
                                         aql::Variable const* reference,
                                         size_t itemsInIndex) const;

  Index::FilterCosts supportsFilterCondition(IndexId id,
                                             std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
                                             std::vector<std::shared_ptr<Index>> const& allIndexes,
                                             aql::AstNode const* node,
                                             aql::Variable const* reference,
                                             size_t itemsInIndex) const;

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const;

  InvertedIndexFieldMeta _meta;
};
} // namespace iresearch
} // namespace arangodb
