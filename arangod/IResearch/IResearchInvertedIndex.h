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

class lazy_bitset : private irs::util::noncopyable {
 public:
  using word_t = size_t;

  lazy_bitset(
      const irs::sub_reader& segment,
      const irs::filter::prepared& filter) noexcept
    : filter_(filter),
      segment_(&segment) {
  }

  bool get(size_t word_id, word_t* data);

 private:
  std::unique_ptr<word_t[]> set_;
  const word_t* begin_{nullptr};
  const word_t* end_{nullptr};
  const irs::filter::prepared& filter_;
  const irs::sub_reader* segment_;
  irs::doc_iterator::ptr real_doc_itr_;
  const irs::document* real_doc_{nullptr};
  size_t words_{0};
};

class lazy_filter_bitset_iterator final : public irs::doc_iterator,
                                          private irs::util::noncopyable {
 public:

  lazy_filter_bitset_iterator(
      lazy_bitset& bitset,
      irs::cost::cost_t estimation) noexcept
    : cost_(estimation),
      bitset_(bitset) {
    reset();
  }

  bool next() override final;
  irs::doc_id_t seek(irs::doc_id_t target) override final;
  irs::doc_id_t value() const noexcept override final { return doc_.value; }
  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept override;
  void reset() noexcept;

 private:
  lazy_bitset& bitset_;
  irs::cost cost_;
  irs::document doc_;
  irs::doc_id_t word_idx_{0};
  lazy_bitset::word_t word_{0};
  irs::doc_id_t base_{irs::doc_limits::invalid()};
};

class proxy_query final : public irs::filter::prepared {
 public:

  struct proxy_cache {
    std::map<const irs::sub_reader*, std::unique_ptr<lazy_bitset>> readers_;
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
    auto cached = cache_.readers_.find(&rdr);
    if (cached != cache_.readers_.end()) {
      auto cached_bitset  = cached->second.get();
      TRI_ASSERT(cached_bitset);
      return irs::memory::make_managed<lazy_filter_bitset_iterator>(*cached_bitset, rdr.docs_count());
    } else {
      if (!cache_.prepared_real_filter_) {
        cache_.prepared_real_filter_ = real_filter_->prepare(index_, order_);
      }
      auto inserted = cache_.readers_.insert({&rdr, std::make_unique<lazy_bitset>(rdr, *(cache_.prepared_real_filter_.get()))});
      return irs::memory::make_managed<lazy_filter_bitset_iterator>(*inserted.first->second.get(), rdr.docs_count());
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

  static ptr make();

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
