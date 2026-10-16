////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <variant>

#include "analysis/token_attributes.hpp"
#include "index/index_tests.hpp"
#include "search/cost.hpp"
#include "search/filter.hpp"
#include "search/filter_visitor.hpp"
#include "search/score.hpp"
#include "search/tfidf.hpp"
#include "tests_shared.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"

namespace tests {
namespace sort {

////////////////////////////////////////////////////////////////////////////////
/// @class boost
/// @brief boost scorer assign boost value to the particular document score
////////////////////////////////////////////////////////////////////////////////
struct boost : public irs::ScorerBase<boost, void> {
  struct score_ctx final : public irs::score_ctx {
   public:
    explicit score_ctx(irs::score_t boost) noexcept : boost_(boost) {}

    irs::score_t boost_;
  };

  irs::IndexFeatures index_features() const noexcept final {
    return irs::IndexFeatures::NONE;
  }

  irs::ScoreFunction prepare_scorer(
    const irs::ColumnProvider&, const irs::feature_map_t& /*features*/,
    const irs::byte_type* /*query_attrs*/,
    const irs::attribute_provider& /*doc_attrs*/,
    irs::score_t boost) const final {
    return irs::ScoreFunction::Make<boost::score_ctx>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        const auto& state = *reinterpret_cast<score_ctx*>(ctx);

        *res = state.boost_;
      },
      irs::ScoreFunction::DefaultMin, boost);
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @brief expose sort functionality through overidable lambdas
//////////////////////////////////////////////////////////////////////////////
struct custom_sort : public irs::ScorerBase<custom_sort, void> {
  class field_collector : public irs::FieldCollector {
   public:
    field_collector(const custom_sort& sort) : sort_(sort) {}

    void collect(const irs::SubReader& segment,
                 const irs::term_reader& field) final {
      if (sort_.collector_collect_field) {
        sort_.collector_collect_field(segment, field);
      }
    }

    void reset() final {
      if (sort_.field_reset_) {
        sort_.field_reset_();
      }
    }

    void collect(irs::bytes_view /*in*/) final {
      // NOOP
    }

    void write(irs::data_output& /*out*/) const final {
      // NOOP
    }

   private:
    const custom_sort& sort_;
  };

  class term_collector : public irs::TermCollector {
   public:
    term_collector(const custom_sort& sort) : sort_(sort) {}

    void collect(const irs::SubReader& segment, const irs::term_reader& field,
                 const irs::attribute_provider& term_attrs) final {
      if (sort_.collector_collect_term) {
        sort_.collector_collect_term(segment, field, term_attrs);
      }
    }

    void reset() final {
      if (sort_.term_reset_) {
        sort_.term_reset_();
      }
    }

    void collect(irs::bytes_view /*in*/) final {
      // NOOP
    }

    void write(irs::data_output& /*out*/) const final {
      // NOOP
    }

   private:
    const custom_sort& sort_;
  };

  struct scorer final : public irs::score_ctx {
    scorer(const custom_sort& sort, const irs::ColumnProvider& segment_reader,
           const irs::feature_map_t& term_reader,
           const irs::byte_type* filter_node_attrs,
           const irs::attribute_provider& document_attrs)
      : document_attrs_(document_attrs),
        filter_node_attrs_(filter_node_attrs),
        segment_reader_(segment_reader),
        sort_(sort),
        term_reader_(term_reader) {}

    const irs::attribute_provider& document_attrs_;
    const irs::byte_type* filter_node_attrs_;
    const irs::ColumnProvider& segment_reader_;
    const custom_sort& sort_;
    const irs::feature_map_t& term_reader_;
  };

  void collect(irs::byte_type* filter_attrs, const irs::FieldCollector* field,
               const irs::TermCollector* term) const final {
    if (collectors_collect_) {
      collectors_collect_(filter_attrs, field, term);
    }
  }

  irs::IndexFeatures index_features() const override {
    return irs::IndexFeatures::NONE;
  }

  irs::FieldCollector::ptr prepare_field_collector() const final {
    if (prepare_field_collector_) {
      return prepare_field_collector_();
    }

    return std::make_unique<field_collector>(*this);
  }

  irs::ScoreFunction prepare_scorer(
    const irs::ColumnProvider& segment_reader,
    const irs::feature_map_t& term_reader,
    const irs::byte_type* filter_node_attrs,
    const irs::attribute_provider& document_attrs,
    irs::score_t boost) const final {
    if (prepare_scorer_) {
      return prepare_scorer_(segment_reader, term_reader, filter_node_attrs,
                             document_attrs, boost);
    }

    return irs::ScoreFunction::Make<custom_sort::scorer>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        const auto& state = *reinterpret_cast<scorer*>(ctx);

        if (state.sort_.scorer_score) {
          state.sort_.scorer_score(
            irs::get<irs::document>(state.document_attrs_)->value, res);
        }
      },
      irs::ScoreFunction::DefaultMin, *this, segment_reader, term_reader,
      filter_node_attrs, document_attrs);
  }

  irs::TermCollector::ptr prepare_term_collector() const final {
    if (prepare_term_collector_) {
      return prepare_term_collector_();
    }

    return std::make_unique<term_collector>(*this);
  }

  std::function<void(const irs::SubReader&, const irs::term_reader&)>
    collector_collect_field;
  std::function<void(const irs::SubReader&, const irs::term_reader&,
                     const irs::attribute_provider&)>
    collector_collect_term;
  std::function<void(irs::byte_type*, const irs::FieldCollector*,
                     const irs::TermCollector*)>
    collectors_collect_;
  std::function<irs::FieldCollector::ptr()> prepare_field_collector_;
  std::function<irs::ScoreFunction(
    const irs::ColumnProvider&, const irs::feature_map_t&,
    const irs::byte_type*, const irs::attribute_provider&, irs::score_t)>
    prepare_scorer_;
  std::function<irs::TermCollector::ptr()> prepare_term_collector_;
  std::function<void(irs::doc_id_t, irs::score_t*)> scorer_score;
  std::function<void()> term_reset_;
  std::function<void()> field_reset_;
};

struct stats_t {
  irs::doc_id_t count;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief order by frequency, then if equal order by doc_id_t
//////////////////////////////////////////////////////////////////////////////
struct frequency_sort : public irs::ScorerBase<frequency_sort, stats_t> {
  struct term_collector : public irs::TermCollector {
    size_t docs_count{};
    const irs::term_meta* meta_attr;

    void collect(const irs::SubReader& /*segment*/,
                 const irs::term_reader& /*field*/,
                 const irs::attribute_provider& term_attrs) final {
      meta_attr = irs::get<irs::term_meta>(term_attrs);
      ASSERT_NE(nullptr, meta_attr);
      docs_count += meta_attr->docs_count;
    }

    void reset() noexcept final { docs_count = 0; }

    void collect(irs::bytes_view /*in*/) final {
      // NOOP
    }

    void write(irs::data_output& /*out*/) const final {
      // NOOP
    }
  };

  struct scorer final : public irs::score_ctx {
    scorer(const irs::doc_id_t* docs_count, const irs::document* doc)
      : doc(doc), docs_count(docs_count) {}

    const irs::document* doc;
    const irs::doc_id_t* docs_count;
  };

  void collect(irs::byte_type* stats_buf, const irs::FieldCollector* /*field*/,
               const irs::TermCollector* term) const final {
    auto* term_ptr = dynamic_cast<const term_collector*>(term);
    if (term_ptr) {  // may be null e.g. 'all' filter
      stats_cast(stats_buf)->count =
        static_cast<irs::doc_id_t>(term_ptr->docs_count);
      const_cast<term_collector*>(term_ptr)->docs_count = 0;
    }
  }

  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::NONE;
  }

  irs::FieldCollector::ptr prepare_field_collector() const final {
    return nullptr;  // do not need to collect stats
  }

  irs::ScoreFunction prepare_scorer(const irs::ColumnProvider&,
                                    const irs::feature_map_t&,
                                    const irs::byte_type* stats_buf,
                                    const irs::attribute_provider& doc_attrs,
                                    irs::score_t /*boost*/) const final {
    auto* doc = irs::get<irs::document>(doc_attrs);
    auto* stats = stats_cast(stats_buf);
    const irs::doc_id_t* docs_count = &stats->count;
    return irs::ScoreFunction::Make<frequency_sort::scorer>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        const auto& state = *reinterpret_cast<scorer*>(ctx);

        // docs_count may be nullptr if no collector called,
        // e.g. by range_query for bitset_doc_iterator
        if (state.docs_count) {
          *res = 1.f / (*state.docs_count);
        } else {
          *res = std::numeric_limits<irs::score_t>::infinity();
        }
      },
      irs::ScoreFunction::DefaultMin, docs_count, doc);
  }

  irs::TermCollector::ptr prepare_term_collector() const final {
    return std::make_unique<term_collector>();
  }
};

}  // namespace sort

class FilterTestCaseBase : public index_test_base {
 protected:
  using Docs = std::vector<irs::doc_id_t>;
  using ScoredDocs =
    std::vector<std::pair<irs::doc_id_t, std::vector<irs::score_t>>>;
  using Costs = std::vector<irs::cost::cost_t>;

  struct Seek {
    irs::doc_id_t target;
  };

  struct Skip {
    irs::doc_id_t count;
  };

  struct Next {};

  using Action = std::variant<Seek, Skip, Next>;

  struct Test {
    Action action;
    irs::doc_id_t expected;
    std::vector<irs::score_t> score{};
  };

  using Tests = std::vector<Test>;

  // Validate matched documents and query cost
  static void CheckQuery(const irs::filter& filter, const Docs& expected,
                         const Costs& expected_costs,
                         const irs::IndexReader& index,
                         std::string_view source_location = {});

  // Validate matched documents
  static void CheckQuery(const irs::filter& filter, const Docs& expected,
                         const irs::IndexReader& index,
                         std::string_view source_location = {});

  // Validate documents and its scores
  static void CheckQuery(const irs::filter& filter,
                         std::span<const irs::Scorer::ptr> order,
                         const ScoredDocs& expected,
                         const irs::IndexReader& index,
                         std::string_view source_location = {});

  // Validate documents and its scores with test cases
  static void CheckQuery(const irs::filter& filter,
                         std::span<const irs::Scorer::ptr> order,
                         const std::vector<Tests>& tests,
                         const irs::IndexReader& index,
                         std::string_view source_location = {});

  static void MakeResult(const irs::filter& filter,
                         std::span<const irs::Scorer::ptr> order,
                         const irs::IndexReader& rdr,
                         std::vector<irs::doc_id_t>& result,
                         bool score_must_be_present = true,
                         bool reverse = false);

  // Validate document order
  static void CheckQuery(const irs::filter& filter,
                         std::span<const irs::Scorer::ptr> order,
                         const std::vector<irs::doc_id_t>& expected,
                         const irs::IndexReader& index,
                         bool score_must_be_present = true,
                         bool reverse = false);

 private:
  static void GetQueryResult(const irs::filter::prepared::ptr& q,
                             const irs::IndexReader& index, Docs& result,
                             Costs& result_costs,
                             std::string_view source_location);

  static void GetQueryResult(const irs::filter::prepared::ptr& q,
                             const irs::IndexReader& index,
                             const irs::Scorers& ord, ScoredDocs& result,
                             Costs& result_costs,
                             std::string_view source_location);
};

struct empty_term_reader : irs::singleton<empty_term_reader>, irs::term_reader {
  virtual irs::seek_term_iterator::ptr iterator() const {
    return irs::seek_term_iterator::empty();
  }

  virtual irs::seek_term_iterator::ptr iterator(
    irs::automaton_table_matcher&) const {
    return irs::seek_term_iterator::empty();
  }

  virtual const irs::field_meta& meta() const {
    static irs::field_meta EMPTY;
    return EMPTY;
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id) noexcept {
    return nullptr;
  }

  // total number of terms
  virtual size_t size() const { return 0; }

  // total number of documents
  virtual uint64_t docs_count() const { return 0; }

  // less significant term
  virtual irs::bytes_view(min)() const { return {}; }

  // most significant term
  virtual irs::bytes_view(max)() const { return {}; }
};

class empty_filter_visitor : public irs::filter_visitor {
 public:
  void prepare(const irs::SubReader& /*segment*/,
               const irs::term_reader& /*field*/,
               const irs::seek_term_iterator& terms) noexcept final {
    it_ = &terms;
    ++prepare_calls_counter_;
  }

  void visit(irs::score_t boost) noexcept final {
    ASSERT_NE(nullptr, it_);
    terms_.emplace_back(it_->value(), boost);
    ++visit_calls_counter_;
  }

  void reset() noexcept {
    prepare_calls_counter_ = 0;
    visit_calls_counter_ = 0;
    terms_.clear();
    it_ = nullptr;
  }

  size_t prepare_calls_counter() const noexcept {
    return prepare_calls_counter_;
  }

  size_t visit_calls_counter() const noexcept { return visit_calls_counter_; }

  const std::vector<std::pair<irs::bstring, irs::score_t>>& terms()
    const noexcept {
    return terms_;
  }

  template<typename Char>
  std::vector<std::pair<irs::basic_string_view<Char>, irs::score_t>> term_refs()
    const {
    std::vector<std::pair<irs::basic_string_view<Char>, irs::score_t>> refs(
      terms_.size());
    auto begin = refs.begin();
    for (auto& term : terms_) {
      begin->first = irs::ViewCast<Char>(irs::bytes_view{term.first});
      begin->second = term.second;
      ++begin;
    }
    return refs;
  }

  virtual void assert_boost(irs::score_t boost) {
    ASSERT_EQ(irs::kNoBoost, boost);
  }

 private:
  const irs::seek_term_iterator* it_{};
  std::vector<std::pair<irs::bstring, irs::score_t>> terms_;
  size_t prepare_calls_counter_ = 0;
  size_t visit_calls_counter_ = 0;
};

}  // namespace tests
