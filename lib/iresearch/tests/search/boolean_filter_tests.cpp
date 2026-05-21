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

#include <functional>

#include "filter_test_case_base.hpp"
#include "formats/empty_term_reader.hpp"
#include "index/iterators.hpp"
#include "search/all_filter.hpp"
#include "search/all_iterator.hpp"
#include "search/bm25.hpp"
#include "search/boolean_filter.hpp"
#include "search/conjunction.hpp"
#include "search/disjunction.hpp"
#include "search/exclusion.hpp"
#include "search/min_match_disjunction.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/term_query.hpp"
#include "search/tfidf.hpp"
#include "tests_shared.hpp"

namespace {

template<typename Filter>
Filter make_filter(const std::string_view& field, const std::string_view term) {
  Filter q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ViewCast<irs::byte_type>(term);
  return q;
}

template<typename Filter>
Filter& append(irs::boolean_filter& root, const std::string_view& name,
               const std::string_view& term) {
  auto& sub = root.add<Filter>();
  *sub.mutable_field() = name;
  sub.mutable_options()->term = irs::ViewCast<irs::byte_type>(term);
  return sub;
}

}  // namespace

namespace tests {
namespace detail {

struct basic_sort : irs::ScorerBase<basic_sort, void> {
  explicit basic_sort(size_t idx) : idx(idx) {}

  struct basic_scorer final : irs::score_ctx {
    explicit basic_scorer(size_t idx) noexcept : idx(idx) {}

    size_t idx;
  };

  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::NONE;
  }

  irs::ScoreFunction prepare_scorer(const irs::ColumnProvider&,
                                    const irs::feature_map_t&,
                                    const irs::byte_type*,
                                    const irs::attribute_provider&,
                                    irs::score_t) const final {
    return irs::ScoreFunction::Make<basic_scorer>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        ASSERT_NE(nullptr, res);
        ASSERT_NE(nullptr, ctx);
        const auto& state = *static_cast<basic_scorer*>(ctx);
        *res = static_cast<uint32_t>(state.idx);
      },
      irs::ScoreFunction::DefaultMin, idx);
  }

  size_t idx;
};

class basic_doc_iterator : public irs::doc_iterator, irs::score_ctx {
 public:
  typedef std::vector<irs::doc_id_t> docids_t;

#if defined(_MSC_VER)
#pragma warning(disable : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

  basic_doc_iterator(const docids_t::const_iterator& first,
                     const docids_t::const_iterator& last,
                     const irs::byte_type* stats = nullptr,
                     const irs::Scorers& ord = irs::Scorers::kUnordered,
                     irs::score_t boost = irs::kNoBoost)
    : first_(first),
      last_(last),
      stats_(stats),
      doc_(irs::doc_limits::invalid()) {
    est_.reset(std::distance(first_, last_));
    attrs_[irs::type<irs::cost>::id()] = &est_;
    attrs_[irs::type<irs::document>::id()] = &doc_;

    if (!ord.empty()) {
      IRS_ASSERT(stats_);

      scorers_ =
        irs::PrepareScorers(ord.buckets(), irs::SubReader::empty(),
                            irs::empty_term_reader{0}, stats_, *this, boost);

      score_.Reset(*this, [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        const auto& self = *static_cast<basic_doc_iterator*>(ctx);
        for (auto& scorer : self.scorers_) {
          scorer(res++);
        }
      });

      attrs_[irs::type<irs::score>::id()] = &score_;
    }
  }

#if defined(_MSC_VER)
#pragma warning(default : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  irs::doc_id_t value() const final { return doc_.value; }

  bool next() final {
    if (first_ == last_) {
      doc_.value = irs::doc_limits::eof();
      return false;
    }

    doc_.value = *first_;
    ++first_;
    return true;
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    const auto it = attrs_.find(type);
    return it == attrs_.end() ? nullptr : it->second;
  }

  irs::doc_id_t seek(irs::doc_id_t doc) final {
    if (irs::doc_limits::eof(doc_.value) || doc <= doc_.value) {
      return doc_.value;
    }

    do {
      next();
    } while (doc_.value < doc);

    return doc_.value;
  }

 private:
  std::map<irs::type_info::type_id, irs::attribute*> attrs_;
  irs::cost est_;
  irs::ScoreFunctions scorers_;
  docids_t::const_iterator first_;
  docids_t::const_iterator last_;
  const irs::byte_type* stats_;
  irs::score score_;
  irs::document doc_;
};

std::vector<irs::doc_id_t> union_all(
  const std::vector<std::vector<irs::doc_id_t>>& docs) {
  std::vector<irs::doc_id_t> result;
  for (auto& part : docs) {
    std::copy(part.begin(), part.end(), std::back_inserter(result));
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

template<typename DocIterator>
std::vector<DocIterator> execute_all(
  std::span<const std::vector<irs::doc_id_t>> docs) {
  std::vector<DocIterator> itrs;
  itrs.reserve(docs.size());
  for (const auto& doc : docs) {
    itrs.emplace_back(irs::memory::make_managed<detail::basic_doc_iterator>(
      doc.begin(), doc.end()));
  }

  return itrs;
}

template<typename DocIterator>
std::vector<DocIterator> execute_all(
  std::span<const std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs) {
  const auto emptyBytesRef = irs::kEmptyStringView<irs::byte_type>;
  const irs::byte_type* stats = emptyBytesRef.data();
  std::vector<DocIterator> itrs;
  itrs.reserve(docs.size());
  for (const auto& [doc, ord] : docs) {
    if (ord.empty()) {
      itrs.emplace_back(irs::memory::make_managed<detail::basic_doc_iterator>(
        doc.begin(), doc.end()));
    } else {
      itrs.emplace_back(irs::memory::make_managed<detail::basic_doc_iterator>(
        doc.begin(), doc.end(), stats, ord, irs::kNoBoost));
    }
  }

  return itrs;
}

struct seek_doc {
  irs::doc_id_t target;
  irs::doc_id_t expected;
};

}  // namespace detail

namespace detail {

struct boosted : public irs::FilterWithBoost {
  struct prepared : irs::filter::prepared {
    explicit prepared(const basic_doc_iterator::docids_t& docs,
                      irs::score_t boost)
      : docs{docs}, boost_{boost} {}

    irs::doc_iterator::ptr execute(
      const irs::ExecutionContext& ctx) const final {
      boosted::execute_count++;
      return irs::memory::make_managed<basic_doc_iterator>(
        docs.begin(), docs.end(), stats.c_str(), ctx.scorers, boost());
    }

    void visit(const irs::SubReader&, irs::PreparedStateVisitor&,
               irs::score_t) const final {
      // No terms to visit
    }

    irs::score_t boost() const noexcept final { return boost_; }

    basic_doc_iterator::docids_t docs;
    irs::bstring stats;

   private:
    irs::score_t boost_;
  };

  irs::filter::prepared::ptr prepare(
    const irs::PrepareContext& ctx) const final {
    return irs::memory::make_managed<boosted::prepared>(docs,
                                                        ctx.boost * boost());
  }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<boosted>::id();
  }

  basic_doc_iterator::docids_t docs;
  static unsigned execute_count;
};

unsigned boosted::execute_count{0};

}  // namespace detail

TEST(boolean_query_boost, hierarchy) {
  // hierarchy of boosted subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    root.boost(value);
    {
      auto& sub = root.add<irs::Or>();
      sub.boost(value);
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2, 3};
        node.boost(value);
      }
    }

    {
      auto& sub = root.add<irs::Or>();
      sub.boost(value);
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2, 3};
        node.boost(value);
      }
    }

    {
      auto& sub = root.add<detail::boosted>();
      sub.docs = {1, 2};
      sub.boost(value);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    /* the first hit should be scored as 2*value^3 +2*value^3+value^2 since it
     * exists in all results */
    {
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(4 * value * value * value + value * value, doc_boost);
    }

    /* the second hit should be scored as 2*value^3+value^2 since it
     * exists in all results */
    {
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(4 * value * value * value + value * value, doc_boost);
    }

    ASSERT_FALSE(docs->next());
  }

  // hierarchy of boosted subqueries (multiple Or's)
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    root.boost(value);
    {
      auto& sub = root.add<irs::Or>();
      sub.boost(value);
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 3};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
      }
    }

    {
      auto& sub = root.add<irs::Or>();
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2, 3};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1};
        node.boost(value);
      }
    }

    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2, 3};
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    /* the first hit should be scored as 2*value^3+value^2+3*value^2+value
     * since it exists in all results */
    {
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(2 * value * value * value + 4 * value * value + value,
                doc_boost);
    }

    /* the second hit should be scored as value^3+value^2+2*value^2 since it
     * exists in all results */
    {
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(value * value * value + 3 * value * value + value, doc_boost);
    }

    /* the third hit should be scored as value^3+value^2 since it
     * exists in all results */
    {
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(value * value * value + value * value + value, doc_boost);
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // hierarchy of boosted subqueries (multiple And's)
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;
    root.boost(value);
    {
      auto& sub = root.add<irs::And>();
      sub.boost(value);
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 3};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
      }
    }

    {
      auto& sub = root.add<irs::And>();
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1, 2, 3};
        node.boost(value);
      }
      {
        auto& node = sub.add<detail::boosted>();
        node.docs = {1};
        node.boost(value);
      }
    }

    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2, 3};
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    // the first hit should be scored as value^3+2*value^2+3*value^2+value
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(value * value * value + 5 * value * value + value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    // the second hit should be scored as value
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    // the third hit should be scored as value
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }
}

TEST(boolean_query_boost, and_filter) {
  // empty boolean unboosted query
  {
    irs::And root;

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    ASSERT_EQ(irs::kNoBoost, prep->boost());
  }

  // boosted empty boolean query
  {
    const irs::score_t value = 5;

    irs::And root;
    root.boost(value);

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    ASSERT_EQ(irs::kNoBoost, prep->boost());
  }

  // single boosted subquery
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_TRUE(docs->next());
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(value, doc_boost);
    ASSERT_FALSE(docs->next());
  }

  // boosted root & single boosted subquery
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    root.boost(value);

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_TRUE(docs->next());
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(value * value, doc_boost);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // boosted root & several boosted subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }
    root.boost(value);

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    /* the first hit should be scored as value*value + value*value since it
     * exists in both results */
    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_TRUE(docs->next());
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(2 * value * value, doc_boost);
    ASSERT_EQ(docs->value(), doc->value);

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // boosted root & several boosted subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    root.boost(value);
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(3 * value * value + value, doc_boost);

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // unboosted root & several boosted subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(3 * value, doc_boost);

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // unboosted root & several unboosted subqueries
  {
    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::And root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(irs::score_t(0), doc_boost);

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }
}

TEST(boolean_query_boost, or_filter) {
  // single unboosted query
  {
    irs::Or root;

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    ASSERT_EQ(irs::kNoBoost, prep->boost());
  }

  // empty single boosted query
  {
    const irs::score_t value = 5;

    irs::Or root;
    root.boost(value);

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    ASSERT_EQ(irs::kNoBoost, prep->boost());
  }

  // boosted empty single query
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
    }
    root.boost(value);

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(docs->next());
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(value, doc_boost);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // boosted single query & subquery
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    root.boost(value);

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    irs::score_t doc_boost;
    scr->operator()(&doc_boost);
    ASSERT_EQ(value * value, doc_boost);
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // boosted single query & several subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }
    root.boost(value);

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    // the first hit should be scored as value*value + value*value since it
    // exists in both results
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(2 * value * value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    // the second hit should be scored as value*value since it
    // exists in second result only
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(value * value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // boosted root & several boosted subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;
    root.boost(value);

    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    // first hit
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(3 * value * value + value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    // second hit
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(2 * value * value + value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // unboosted root & several boosted subqueries
  {
    const irs::score_t value = 5;

    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;

    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(value);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    // first hit
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(3 * value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    // second hit
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(2 * value, doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }

  // unboosted root & several unboosted subqueries
  {
    tests::sort::boost sort;
    auto pord = irs::Scorers::Prepare(sort);

    irs::Or root;
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }
    {
      auto& node = root.add<detail::boosted>();
      node.docs = {1, 2};
      node.boost(0.f);
    }

    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    auto docs =
      prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));

    auto* scr = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!scr);

    // first hit
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(irs::score_t(0), doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    // second hit
    {
      ASSERT_TRUE(docs->next());
      irs::score_t doc_boost;
      scr->operator()(&doc_boost);
      ASSERT_EQ(irs::score_t(0), doc_boost);
      ASSERT_EQ(docs->value(), doc->value);
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                         Boolean query estimation
// ----------------------------------------------------------------------------

namespace detail {

struct unestimated : public irs::FilterWithBoost {
  struct doc_iterator : irs::doc_iterator {
    irs::doc_id_t value() const final {
      // prevent iterator to filter out
      return irs::doc_limits::invalid();
    }
    bool next() final { return false; }
    irs::doc_id_t seek(irs::doc_id_t) final {
      // prevent iterator to filter out
      return irs::doc_limits::invalid();
    }
    irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
      return type == irs::type<irs::document>::id() ? &doc : nullptr;
    }

    irs::document doc;
  };

  struct prepared : public irs::filter::prepared {
    irs::doc_iterator::ptr execute(const irs::ExecutionContext&) const final {
      return irs::memory::make_managed<unestimated::doc_iterator>();
    }
    void visit(const irs::SubReader&, irs::PreparedStateVisitor&,
               irs::score_t) const final {
      // No terms to visit
    }

    irs::score_t boost() const noexcept final { return irs::kNoBoost; }
  };

  filter::prepared::ptr prepare(
    const irs::PrepareContext& /*ctx*/) const final {
    return irs::memory::make_managed<unestimated::prepared>();
  }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<unestimated>::id();
  }
};

struct estimated : public irs::FilterWithBoost {
  struct doc_iterator : irs::doc_iterator {
    doc_iterator(irs::cost::cost_t est, bool* evaluated) {
      cost.reset([est, evaluated]() noexcept {
        *evaluated = true;
        return est;
      });
    }
    irs::doc_id_t value() const final {
      // prevent iterator to filter out
      return irs::doc_limits::invalid();
    }
    bool next() final { return false; }
    irs::doc_id_t seek(irs::doc_id_t) final {
      // prevent iterator to filter out
      return irs::doc_limits::invalid();
    }
    irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
      if (type == irs::type<irs::cost>::id()) {
        return &cost;
      }

      return type == irs::type<irs::document>::id() ? &doc : nullptr;
    }

    irs::document doc;
    irs::cost cost;
  };

  struct prepared : public irs::filter::prepared {
    explicit prepared(irs::cost::cost_t est, bool* evaluated)
      : evaluated(evaluated), est(est) {}

    irs::doc_iterator::ptr execute(const irs::ExecutionContext&) const final {
      return irs::memory::make_managed<estimated::doc_iterator>(est, evaluated);
    }

    void visit(const irs::SubReader&, irs::PreparedStateVisitor&,
               irs::score_t) const final {
      // No terms to visit
    }

    irs::score_t boost() const noexcept final { return irs::kNoBoost; }

    bool* evaluated;
    irs::cost::cost_t est;
  };

  filter::prepared::ptr prepare(
    const irs::PrepareContext& /*ctx*/) const final {
    return irs::memory::make_managed<estimated::prepared>(est, &evaluated);
  }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<estimated>::id();
  }

  mutable bool evaluated = false;
  irs::cost::cost_t est{};
};

}  // namespace detail

TEST(boolean_query_estimation, or_filter) {
  MaxMemoryCounter counter;

  // estimated subqueries
  {
    irs::Or root;
    root.add<detail::estimated>().est = 100;
    root.add<detail::estimated>().est = 320;
    root.add<detail::estimated>().est = 10;
    root.add<detail::estimated>().est = 1;
    root.add<detail::estimated>().est = 100;

    auto prep = root.prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    // check that subqueries were not estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      ASSERT_TRUE(est_query != nullptr);
      ASSERT_FALSE(est_query->evaluated);
    }

    ASSERT_EQ(531, irs::cost::extract(*docs));

    // check that subqueries were estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      ASSERT_TRUE(est_query != nullptr);
      ASSERT_TRUE(est_query->evaluated);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // unestimated subqueries
  {
    irs::Or root;
    root.add<detail::unestimated>();
    root.add<detail::unestimated>();
    root.add<detail::unestimated>();
    root.add<detail::unestimated>();

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});
    ASSERT_EQ(0, irs::cost::extract(*docs));
  }

  // estimated/unestimated subqueries
  {
    irs::Or root;
    root.add<detail::estimated>().est = 100;
    root.add<detail::estimated>().est = 320;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 10;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 1;
    root.add<detail::estimated>().est = 100;
    root.add<detail::unestimated>();

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    /* check that subqueries were not estimated */
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_FALSE(est_query->evaluated);
      }
    }

    ASSERT_EQ(531, irs::cost::extract(*docs));

    /* check that subqueries were estimated */
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_TRUE(est_query->evaluated);
      }
    }
  }

  // estimated/unestimated/negative subqueries
  {
    irs::Or root;
    root.add<detail::estimated>().est = 100;
    root.add<detail::estimated>().est = 320;
    root.add<irs::Not>().filter<detail::estimated>().est = 3;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 10;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 7;
    root.add<detail::estimated>().est = 100;
    root.add<irs::Not>().filter<detail::unestimated>();
    root.add<irs::Not>().filter<detail::estimated>().est = 0;
    root.add<detail::unestimated>();

    // we need order to suppress optimization
    // which will clean include group and leave only 'all' filter
    tests::sort::boost impl;
    const irs::Scorer* sort{&impl};

    auto pord = irs::Scorers::Prepare(std::span{&sort, 1});
    auto prep =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    // check that subqueries were not estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_FALSE(est_query->evaluated);
      }
    }

    ASSERT_EQ(537, irs::cost::extract(*docs));

    // check that subqueries were estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_TRUE(est_query->evaluated);
      }
    }
  }

  // empty case
  {
    irs::Or root;

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});
    ASSERT_EQ(0, irs::cost::extract(*docs));
  }
}

TEST(boolean_query_estimation, and_filter) {
  // estimated subqueries
  {
    irs::And root;
    root.add<detail::estimated>().est = 100;
    root.add<detail::estimated>().est = 320;
    root.add<detail::estimated>().est = 10;
    root.add<detail::estimated>().est = 1;
    root.add<detail::estimated>().est = 100;

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    // check that subqueries were estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_TRUE(est_query->evaluated);
      }
    }

    ASSERT_EQ(1, irs::cost::extract(*docs));
  }

  // unestimated subqueries
  {
    irs::And root;
    root.add<detail::unestimated>();
    root.add<detail::unestimated>();
    root.add<detail::unestimated>();
    root.add<detail::unestimated>();

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    // check that subqueries were estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_TRUE(est_query->evaluated);
      }
    }

    ASSERT_EQ(decltype(irs::cost::kMax)(irs::cost::kMax),
              irs::cost::extract(*docs));
  }

  // estimated/unestimated subqueries
  {
    irs::And root;
    root.add<detail::estimated>().est = 100;
    root.add<detail::estimated>().est = 320;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 10;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 1;
    root.add<detail::estimated>().est = 100;
    root.add<detail::unestimated>();

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    // check that subqueries were estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_TRUE(est_query->evaluated);
      }
    }

    ASSERT_EQ(1, irs::cost::extract(*docs));
  }

  // estimated/unestimated/negative subqueries
  {
    irs::And root;
    root.add<detail::estimated>().est = 100;
    root.add<detail::estimated>().est = 320;
    root.add<irs::Not>().filter<detail::estimated>().est = 3;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 10;
    root.add<detail::unestimated>();
    root.add<detail::estimated>().est = 7;
    root.add<detail::estimated>().est = 100;
    root.add<irs::Not>().filter<detail::unestimated>();
    root.add<irs::Not>().filter<detail::estimated>().est = 0;
    root.add<detail::unestimated>();

    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});

    // check that subqueries were estimated
    for (auto it = root.begin(), end = root.end(); it != end; ++it) {
      auto* est_query = dynamic_cast<detail::estimated*>(it->get());
      if (est_query) {
        ASSERT_TRUE(est_query->evaluated);
      }
    }

    ASSERT_EQ(7, irs::cost::extract(*docs));
  }

  // empty case
  {
    irs::And root;
    auto prep = root.prepare({.index = irs::SubReader::empty()});

    auto docs = prep->execute({.segment = irs::SubReader::empty()});
    ASSERT_EQ(0, irs::cost::extract(*docs));
  }
}
// ----------------------------------------------------------------------------
// --SECTION--                       basic disjunction (iterator0 OR iterator1)
// ----------------------------------------------------------------------------

TEST(basic_disjunction, next) {
  using disjunction =
    irs::basic_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  // simple case
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6, 12, 29};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;

    {
      disjunction it(disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         first.begin(), first.end())),
                     disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         last.begin(), last.end())));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // basic case : single dataset
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         first.begin(), first.end())),
                     disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         last.begin(), last.end())));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(first, result);
  }

  // basic case : single dataset
  {
    std::vector<irs::doc_id_t> first{};
    std::vector<irs::doc_id_t> last{1, 5, 6, 12, 29};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         first.begin(), first.end())),
                     disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         last.begin(), last.end())));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(last, result);
  }

  // basic case : same datasets
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         first.begin(), first.end())),
                     disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         last.begin(), last.end())));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(first, result);
  }

  // basic case : single dataset
  {
    std::vector<irs::doc_id_t> first{24};
    std::vector<irs::doc_id_t> last{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         first.begin(), first.end())),
                     disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         last.begin(), last.end())));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(first, result);
  }

  // empty
  {
    std::vector<irs::doc_id_t> first{};
    std::vector<irs::doc_id_t> last{};
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         first.begin(), first.end())),
                     disjunction::adapter(
                       irs::memory::make_managed<detail::basic_doc_iterator>(
                         last.begin(), last.end())));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }
}

TEST(basic_disjunction_test, seek) {
  using disjunction =
    irs::basic_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;

  // simple case
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6, 12, 29};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {9, 9},
      {8, 9},
      {irs::doc_limits::invalid(), 9},
      {12, 12},
      {8, 12},
      {13, 29},
      {45, 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       first.begin(), first.end())),
                   disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       last.begin(), last.end())));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(it.value(), doc->value);
    }
  }

  // empty datasets
  {
    std::vector<irs::doc_id_t> first{};
    std::vector<irs::doc_id_t> last{};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};

    disjunction it(disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       first.begin(), first.end())),
                   disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       last.begin(), last.end())));
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(it.value(), doc->value);
    }
  }

  // NO_MORE_DOCS
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6, 12, 29};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};

    disjunction it(disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       first.begin(), first.end())),
                   disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       last.begin(), last.end())));
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(it.value(), doc->value);
    }
  }

  // INVALID_DOC
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6, 12, 29};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {9, 9},
      {12, 12},
      {irs::doc_limits::invalid(), 12},
      {45, 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       first.begin(), first.end())),
                   disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       last.begin(), last.end())));
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(it.value(), doc->value);
    }
  }
}

TEST(basic_disjunction_test, seek_next) {
  using disjunction =
    irs::basic_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;

  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6};

    disjunction it(disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       first.begin(), first.end())),
                   disjunction::adapter(
                     irs::memory::make_managed<detail::basic_doc_iterator>(
                       last.begin(), last.end())));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(11, it.seek(10));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(basic_disjunction_test, scored_seek_next) {
  const auto empty_ref = irs::kEmptyStringView<irs::byte_type>;
  const irs::byte_type* empty_stats = empty_ref.data();

  // disjunction without order
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};

    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    auto prepared_first_order = irs::Scorers::Prepare(sort1);

    std::vector<irs::doc_id_t> last{1, 5, 6};
    auto prepared_last_order = irs::Scorers::Prepare(sort2);

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 0,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats, prepared_first_order)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats, prepared_last_order)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(11, it.seek(10));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, aggregate scores
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    detail::basic_sort first_order{1};
    auto prepared_first_order = irs::Scorers::Prepare(first_order);

    std::vector<irs::doc_id_t> last{1, 5, 6};
    detail::basic_sort last_order{2};
    auto prepared_last_order = irs::Scorers::Prepare(last_order);

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats, prepared_first_order)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats, prepared_last_order)),
          std::move(aggregator), 1U);
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(3, tmp);  // 1 + 2
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(3, tmp);  // 1 + 2
    ASSERT_TRUE(it.next());
    score(&tmp);
    ASSERT_EQ(2, tmp);  // 2
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, max score
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    detail::basic_sort first_order{1};
    auto prepared_first_order = irs::Scorers::Prepare(first_order);

    std::vector<irs::doc_id_t> last{1, 5, 6};
    detail::basic_sort last_order{2};
    auto prepared_last_order = irs::Scorers::Prepare(last_order);

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats, prepared_first_order)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats, prepared_last_order)),
          std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(2, tmp);  // std::max(1, 2)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // std::max(1, 2)
    ASSERT_TRUE(it.next());
    score(&tmp);
    ASSERT_EQ(2, tmp);  // std::max(2)
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, iterators without order, aggregation
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    score(&tmp);
    ASSERT_EQ(45, it.value());
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, iterators without order, max
  {
    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, first iterator with order, aggregation
  {
    detail::basic_sort sort1{1};

    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6};
    auto prepared_first_order = irs::Scorers::Prepare(sort1);

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats, prepared_first_order)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, first iterator with order, max
  {
    detail::basic_sort sort1{1};

    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    auto prepared_first_order = irs::Scorers::Prepare(sort1);

    std::vector<irs::doc_id_t> last{1, 5, 6};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats, prepared_first_order)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, last iterator with order, aggregation
  {
    detail::basic_sort sort1{1};

    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6};
    auto prepared_last_order = irs::Scorers::Prepare(sort1);

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats, prepared_last_order)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with order, last iterator with order, max
  {
    detail::basic_sort sort1{1};

    std::vector<irs::doc_id_t> first{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> last{1, 5, 6};
    auto prepared_last_order = irs::Scorers::Prepare(sort1);

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::basic_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            first.begin(), first.end(), empty_stats)),
          adapter(irs::memory::make_managed<detail::basic_doc_iterator>(
            last.begin(), last.end(), empty_stats, prepared_last_order)),
          std::move(aggregator));
      });

    using ExpectedType =
      irs::basic_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    ASSERT_NE(nullptr, irs::get<irs::score>(it));
    auto& score = irs::score::get(it);
    ASSERT_NE(&irs::score::kNoScore, &score);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // estimation
    ASSERT_EQ(first.size() + last.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(11, it.seek(10));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

// ----------------------------------------------------------------------------
// --SECTION--   small disjunction (iterator0 OR iterator1 OR iterator2 OR ...)
// ----------------------------------------------------------------------------

TEST(small_disjunction_test, next) {
  using disjunction =
    irs::small_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // basic case : single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(docs[0], result);
  }

  // basic case : same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // basic case : single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,   7,   9,   11,   12,
                                        29, 45, 79, 101, 141, 256, 1025, 1101};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1}, {2}, {3}};

    std::vector<irs::doc_id_t> expected{1, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }
}

TEST(small_disjunction_test, seek) {
  using disjunction =
    irs::small_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {9, 9},
      {8, 9},
      {irs::doc_limits::invalid(), 9},
      {12, 12},
      {8, 12},
      {13, 29},
      {45, 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // NO_MORE_DOCS
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // INVALID_DOC
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {9, 9},
      {12, 12},
      {irs::doc_limits::invalid(), 12},
      {45, 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,   7,   9,   11,   12,
                                        29, 45, 79, 101, 141, 256, 1025, 1101};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1}, {2}, {3}};

    std::vector<irs::doc_id_t> expected{1, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }
}

TEST(small_disjunction_test, seek_next) {
  using disjunction =
    irs::small_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(small_disjunction_test, scored_seek_next) {
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto itrs = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(itrs), std::move(aggregator), 1U);
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores AGGREGATED score
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(6, tmp);  // 2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // 2
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, MAX score
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1, 2, 4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1, 2, 4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(2, 4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // std::max(2)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators partially with scores, aggregation
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // 4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators partially with scores, max scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1, 4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1, 4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // default value
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators partially without scores, aggregation
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators partially without scores, max
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::small_disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::small_disjunction<irs::doc_iterator::ptr,
                             irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

// ----------------------------------------------------------------------------
// --SECTION--         block_disjunction (iterator0 OR iterator1 OR iterator2 OR
// ...)
// ----------------------------------------------------------------------------

TEST(block_disjunction_test, check_attributes) {
  // no scoring, no order
  {
    using disjunction = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());
    auto* score = irs::get<irs::score>(it);
    ASSERT_NE(nullptr, score);
    ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
  }

  // scoring, no order
  {
    using disjunction = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());
    auto* score = irs::get<irs::score>(it);
    ASSERT_NE(nullptr, score);
    ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
  }

  // no scoring, order
  {
    auto scorer = irs::BM25{};
    auto prepared = irs::Scorers::Prepare(scorer);

    using disjunction = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

    disjunction it(disjunction::doc_iterators_t{}, size_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());
    auto* score = irs::get<irs::score>(it);
    ASSERT_NE(nullptr, score);
    ASSERT_TRUE(score->Func() != &irs::ScoreFunction::DefaultScore);
  }

  // scoring, order
  {
    auto scorer = irs::BM25{};
    auto prepared = irs::Scorers::Prepare(scorer);

    using disjunction = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

    disjunction it(disjunction::doc_iterators_t{}, size_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());
    auto* score = irs::get<irs::score>(it);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);
  }
}

TEST(block_disjunction_test, next) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      while (it.next()) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
    };
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 45, 65, 78, 127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127}};
    std::vector<irs::doc_id_t> expected{1,  2,    5,      7,       9,
                                        11, 1145, 111165, 1111178, 111111127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 127}, {1, 5, 6, 12, 29, 126}};
    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,  7,  9,   11,
                                        12, 29, 45, 65, 78, 126, 127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 5, 6, 12, 29, 126}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 6, 7, 9, 11, 12, 29, 126, 1145, 111165, 1111178, 111111127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 7, 9, 11, 45, 1145, 111165, 1111178, 111111127, 1111111127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,   7,   9,   11,   12,
                                        29, 45, 79, 101, 141, 256, 1025, 1101};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1}, {2}, {3}};

    std::vector<irs::doc_id_t> expected{1, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // empty iterators
  {
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;

    disjunction::doc_iterators_t itrs;
    itrs.emplace_back(irs::doc_iterator::empty());
    itrs.emplace_back(irs::doc_iterator::empty());
    itrs.emplace_back(irs::doc_iterator::empty());
    disjunction it{std::move(itrs)};
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    for (; it.next();) {
      result.push_back(it.value());
      ASSERT_EQ(1, it.match_count());
    }
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(expected, result);
  }
}

TEST(block_disjunction_test, next_scored) {
  // single iterator case, values fit 1 block
  // disjunction without score, sub-iterators with scores
  {
    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 0}, {2, 0}, {5, 0}, {7, 0}, {9, 0}, {11, 0}, {45, 0}};

    std::vector<std::pair<irs::doc_id_t, size_t>> result;

    {
      detail::basic_sort sort{1};
      std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
      docs.emplace_back(
        std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                       irs::Scorers::Prepare(sort)));

      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, irs::Scorers::kUnordered.buckets().size(),
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 1U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::NoopAggregator,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score, no order set
      auto& score = irs::score::get(it);
      ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(1, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      while (it.next()) {
        ASSERT_EQ(doc->value, it.value());
        result.emplace_back(it.value(), 0);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block
  // disjunction score, sub-iterators with scores
  {
    detail::basic_sort sort{1};

    std::vector<std::pair<irs::doc_id_t, irs::score_t>> expected{
      {1, 1.f},  {2, 1.f},  {5, 1.f},  {7, 1.f},  {9, 1.f},
      {11, 1.f}, {45, 1.f}, {65, 1.f}, {78, 1.f}, {127, 1.f}};
    std::vector<std::pair<irs::doc_id_t, irs::score_t>> result;

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
      irs::Scorers::Prepare(sort)));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 1U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(1, irs::cost::extract(it));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block, gap between block
  // disjunction score, sub-iterators with scores
  {
    detail::basic_sort sort{2};

    std::vector<std::pair<irs::doc_id_t, irs::score_t>> expected{
      {1, 2.f},  {2.f, 2.f},  {5, 2.f},      {7, 2.f},       {9, 2.f},
      {11, 2.f}, {1145, 2.f}, {111165, 2.f}, {1111178, 2.f}, {111111127, 2.f}};
    std::vector<std::pair<irs::doc_id_t, irs::score_t>> result;

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145, 111165,
                                                1111178, 111111127},
                     irs::Scorers::Prepare(sort)));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single block
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort{2};

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 0}, {2, 0},  {5, 0},  {6, 0},  {7, 0},
      {9, 0}, {11, 0}, {12, 0}, {29, 0}, {45, 0}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});

    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 0,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::NoopAggregator,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score, no order set
      auto& score = irs::score::get(it);
      ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.emplace_back(it.value(), 0);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  // disjunction score, sub-iterators with partially with scores
  {
    detail::basic_sort sort{3};

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 3},  {2, 3},  {5, 3},  {6, 0},  {7, 3},  {9, 3},   {11, 3},
      {12, 0}, {29, 0}, {45, 3}, {65, 3}, {78, 3}, {126, 0}, {127, 3}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
      irs::Scorers::Prepare(sort)));
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 5, 6, 12, 29, 126}, irs::Scorers{}));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block, aggregation
  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort3{3};

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{{1, 5},
                                                           {2, 3},
                                                           {5, 5},
                                                           {6, 2},
                                                           {7, 3},
                                                           {9, 3},
                                                           {11, 3},
                                                           {
                                                             12,
                                                             2,
                                                           },
                                                           {29, 2},
                                                           {126, 2},
                                                           {1145, 3},
                                                           {111165, 3},
                                                           {1111178, 3},
                                                           {111111127, 3}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145, 111165,
                                                1111178, 111111127},
                     irs::Scorers::Prepare(sort3)));
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29, 126},
                     irs::Scorers::Prepare(sort2)));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block, max
  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort3{3};

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{{1, 3},
                                                           {2, 3},
                                                           {5, 3},
                                                           {6, 2},
                                                           {7, 3},
                                                           {9, 3},
                                                           {11, 3},
                                                           {
                                                             12,
                                                             2,
                                                           },
                                                           {29, 2},
                                                           {126, 2},
                                                           {1145, 3},
                                                           {111165, 3},
                                                           {1111178, 3},
                                                           {111111127, 3}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145, 111165,
                                                1111178, 111111127},
                     irs::Scorers::Prepare(sort3)));
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29, 126},
                     irs::Scorers::Prepare(sort2)));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // disjunction score, sub-iterators partially with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 4},      {2, 4},       {5, 4},         {7, 4},
      {9, 4},      {11, 4},      {45, 0},        {1145, 4},
      {111165, 4}, {1111178, 4}, {111111127, 4}, {1111111127, 1}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145,
                                                 111165, 1111178, 111111127},
                      irs::Scorers::Prepare(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1111111127},
                      irs::Scorers::Prepare(sort1));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    detail::basic_sort sort4{4};
    detail::basic_sort sort5{5};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort5));

    std::vector<irs::doc_id_t> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        irs::score_t score_value;
        score(&score_value);
        ASSERT_EQ(9.f, score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front().first, result);
  }

  // single dataset
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{24},
                      irs::Scorers::Prepare(sort4));

    std::vector<irs::doc_id_t> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        irs::score_t score_value;
        score(&score_value);
        ASSERT_EQ(4.f, score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front().first, result);
  }

  // empty
  {
    detail::basic_sort sort4{4};
    detail::basic_sort sort5{5};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{},
                      irs::Scorers::Prepare(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{},
                      irs::Scorers::Prepare(sort5));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator));  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(0, irs::cost::extract(it));
      ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
  }

  // no iterators provided
  {
    using disjunction = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

    disjunction it(disjunction::doc_iterators_t{}, size_t{});

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort1));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 4}, {2, 4},  {5, 4},  {6, 2},  {7, 4},
      {9, 4}, {11, 4}, {12, 2}, {29, 2}, {45, 4}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};
    detail::basic_sort sort16{16};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort16));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort8));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{256},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      irs::Scorers::Prepare(sort1));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 28},  {2, 16},  {5, 28},   {6, 12},  {7, 16}, {9, 16},
      {11, 17}, {12, 8},  {29, 8},   {45, 16}, {79, 1}, {101, 1},
      {141, 1}, {256, 2}, {1025, 1}, {1101, 1}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{2},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{3},
                      irs::Scorers::Prepare(sort4));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 1}, {2, 2}, {3, 4}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort4));

    std::vector<irs::doc_id_t> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        irs::score_t score_value;
        score(&score_value);
        ASSERT_EQ(4, score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front().first, result);
  }

  // empty datasets
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 3U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    // score is set
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(3, irs::cost::extract(it));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

TEST(block_disjunction_test, next_scored_two_blocks) {
  auto order = [](auto& scorer) -> irs::Scorers {
    return irs::Scorers::Prepare(scorer);
  };

  // single iterator case, values fit 1 block
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45}, order(sort1)));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 0}, {2, 0}, {5, 0}, {7, 0}, {9, 0}, {11, 0}, {45, 0}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 0,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 1U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::NoopAggregator,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score, no order set
      auto& score = irs::score::get(it);
      ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(1, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      while (it.next()) {
        ASSERT_EQ(doc->value, it.value());
        result.emplace_back(it.value(), 0);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block
  // disjunction score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
      order(sort1)));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 1},  {2, 1},  {5, 1},  {7, 1},  {9, 1},
      {11, 1}, {45, 1}, {65, 1}, {78, 1}, {127, 1}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 1U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(1, irs::cost::extract(it));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block, gap between block
  // disjunction score, sub-iterators with scores
  {
    detail::basic_sort sort2{2};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145, 1264,
                                                111165, 1111178, 111111127},
                     order(sort2)));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 2},    {2, 2},    {5, 2},      {7, 2},       {9, 2},        {11, 2},
      {1145, 2}, {1264, 2}, {111165, 2}, {1111178, 2}, {111111127, 2}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single block
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort2{2};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers());
    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 0}, {2, 0},  {5, 0},  {6, 0},  {7, 0},
      {9, 0}, {11, 0}, {12, 0}, {29, 0}, {45, 0}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 0,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::NoopAggregator,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score, no order set
      auto& score = irs::score::get(it);
      ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.emplace_back(it.value(), 0);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  // disjunction score, sub-iterators with partially with scores
  {
    detail::basic_sort sort3{3};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
      order(sort3)));
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 5, 6, 12, 29, 126}, irs::Scorers()));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 3},  {2, 3},  {5, 3},  {6, 0},  {7, 3},  {9, 3},   {11, 3},
      {12, 0}, {29, 0}, {45, 3}, {65, 3}, {78, 3}, {126, 0}, {127, 3}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block, aggregation
  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort3{3};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145, 111165,
                                                1111178, 111111127},
                     order(sort3)));
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 5, 6, 12, 29, 126}, order(sort2)));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{{1, 5},
                                                           {2, 3},
                                                           {5, 5},
                                                           {6, 2},
                                                           {7, 3},
                                                           {9, 3},
                                                           {11, 3},
                                                           {
                                                             12,
                                                             2,
                                                           },
                                                           {29, 2},
                                                           {126, 2},
                                                           {1145, 3},
                                                           {111165, 3},
                                                           {1111178, 3},
                                                           {111111127, 3}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block, max
  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort3{3};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::make_pair(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145, 111165,
                                                1111178, 111111127},
                     order(sort3)));
    docs.emplace_back(std::make_pair(
      std::vector<irs::doc_id_t>{1, 5, 6, 12, 29, 126}, order(sort2)));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{{1, 3},
                                                           {2, 3},
                                                           {5, 3},
                                                           {6, 2},
                                                           {7, 3},
                                                           {9, 3},
                                                           {11, 3},
                                                           {
                                                             12,
                                                             2,
                                                           },
                                                           {29, 2},
                                                           {126, 2},
                                                           {1145, 3},
                                                           {111165, 3},
                                                           {1111178, 3},
                                                           {111111127, 3}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // disjunction score, sub-iterators partially with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 1145,
                                                 111165, 1111178, 111111127},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{1111111127}, order(sort1));
    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 4},      {2, 4},       {5, 4},         {7, 4},
      {9, 4},      {11, 4},      {45, 0},        {1145, 4},
      {111165, 4}, {1111178, 4}, {111111127, 4}, {1111111127, 1}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    detail::basic_sort sort4{4};
    detail::basic_sort sort5{5};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort5));
    std::vector<irs::doc_id_t> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        irs::score_t score_value;
        score(&score_value);
        ASSERT_EQ(9.f, score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front().first, result);
  }

  // single dataset
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{24}, order(sort4));
    std::vector<irs::doc_id_t> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 2U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(2, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        irs::score_t score_value;
        score(&score_value);
        ASSERT_EQ(4, score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front().first, result);
  }

  // empty
  {
    detail::basic_sort sort4{4};
    detail::basic_sort sort5{5};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort5));
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(std::move(res),
                                                        std::move(aggregator));
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(0, irs::cost::extract(it));
      ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
  }

  // no iterators provided
  {
    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(std::vector<adapter>{},
                                                      std::move(aggregator));
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort1));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 4}, {2, 4},  {5, 4},  {6, 2},  {7, 4},
      {9, 4}, {11, 4}, {12, 2}, {29, 2}, {45, 4}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};
    detail::basic_sort sort16{16};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort16));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort8));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      order(sort1));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 28},  {2, 16},  {5, 28},   {6, 12},  {7, 16}, {9, 16},
      {11, 17}, {12, 8},  {29, 8},   {45, 16}, {79, 1}, {101, 1},
      {141, 1}, {256, 2}, {1025, 1}, {1101, 1}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1}, order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{2}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{3}, order(sort4));

    std::vector<std::pair<irs::doc_id_t, size_t>> expected{
      {1, 1}, {2, 2}, {3, 4}};
    std::vector<std::pair<irs::doc_id_t, size_t>> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kSum, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        irs::score_t score_value;
        score(&score_value);
        result.emplace_back(it.value(), score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));

    std::vector<irs::doc_id_t> result;
    {
      auto it_ptr = irs::ResoveMergeType(
        irs::ScoreMergeType::kMax, 1,
        [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
          using disjunction = irs::block_disjunction<
            irs::doc_iterator::ptr, A,
            irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
          using adapter = typename disjunction::adapter;

          auto res = detail::execute_all<adapter>(docs);

          return irs::memory::make_managed<disjunction>(
            std::move(res), std::move(aggregator), 3U);  // custom cost
        });

      using ExpectedType = irs::block_disjunction<
        irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
        irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
      ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
      auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

      // score is set
      auto& score = irs::score::get(it);
      ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(3, irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        irs::score_t score_value;
        score(&score_value);
        ASSERT_EQ(4, score_value);
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front().first, result);
  }

  // empty datasets
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 3U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    // score is set
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(3, irs::cost::extract(it));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

TEST(block_disjunction_test, min_match_next) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMinMatch, false, 1>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      while (it.next()) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, unreachable condition
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
    };
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 45, 65, 78, 127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127}};
    std::vector<irs::doc_id_t> expected{1,  2,    5,      7,       9,
                                        11, 1145, 111165, 1111178, 111111127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<size_t> match_counts{2, 1, 2, 1, 1, 1, 1, 1, 1, 1};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127}, {1, 5, 6, 12, 29, 126}};
    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,  7,  9,   11,
                                        12, 29, 45, 65, 78, 126, 127};
    std::vector<size_t> match_counts{2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127}, {1, 5, 6, 12, 29, 126}, {126}};
    std::vector<irs::doc_id_t> expected{1, 5, 126};
    std::vector<size_t> match_counts{2, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // early break
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127},
      {1, 5, 6, 12, 29, 126},
      {1, 129}};
    std::vector<irs::doc_id_t> expected{1};
    std::vector<size_t> match_counts{3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 3U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // early break
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127}, {1, 5, 6, 12, 29, 126}, {129}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 3U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 5, 6, 12, 29, 126, 111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 6, 7, 9, 11, 12, 29, 126, 1145, 111165, 1111178, 111111127};
    std::vector<size_t> match_counts{2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 1U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111178, 1111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 7, 9, 11, 45, 1145, 111165, 1111178, 111111127, 1111111127};
    std::vector<size_t> match_counts{2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // min_match == 0
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111178, 1111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 7, 9, 11, 45, 1145, 111165, 1111178, 111111127, 1111111127};
    std::vector<size_t> match_counts{2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 0U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111178, 1111111127}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 1111178};
    std::vector<size_t> match_counts{2, 2, 2, 2, 2, 2, 2};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(2, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}, {24}, {24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(3, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};
    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<size_t> match_counts{3, 1, 3, 2, 1, 1, 1, 1, 1, 1};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,   7,   9,   11,   12,
                                        29, 45, 79, 101, 141, 256, 1025, 1101};
    std::vector<size_t> match_counts{3, 1, 3, 2, 1, 1, 2, 1,
                                     1, 1, 1, 1, 1, 1, 1, 1};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1}, {2}, {3}};

    std::vector<irs::doc_id_t> expected{1, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(3, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

TEST(block_disjunction_test, min_match_next_two_blocks) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMinMatch, false, 2>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      while (it.next()) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, unreachable condition
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 127},
    };
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 45, 65, 78, 127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127}};
    std::vector<irs::doc_id_t> expected{1,  2,    5,      7,       9,
                                        11, 1145, 111165, 1111178, 111111127};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        ASSERT_EQ(doc->value, it.value());
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<size_t> match_counts{2, 1, 2, 1, 1, 1, 1, 1, 1, 1};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127}, {1, 5, 6, 12, 29, 126}};
    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,  7,  9,   11,
                                        12, 29, 45, 65, 78, 126, 127};
    std::vector<size_t> match_counts{2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127}, {1, 5, 6, 12, 29, 126}, {126}};
    std::vector<irs::doc_id_t> expected{1, 5, 126};
    std::vector<size_t> match_counts{2, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // early break
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127},
      {1, 5, 6, 12, 29, 126},
      {1, 129}};
    std::vector<irs::doc_id_t> expected{1};
    std::vector<size_t> match_counts{3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 3U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // early break
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 65, 78, 126, 127}, {1, 5, 6, 12, 29, 126}, {129}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 3U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 5, 6, 12, 29, 126, 111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 6, 7, 9, 11, 12, 29, 126, 1145, 111165, 1111178, 111111127};
    std::vector<size_t> match_counts{2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 1U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111178, 1111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 7, 9, 11, 45, 1145, 111165, 1111178, 111111127, 1111111127};
    std::vector<size_t> match_counts{2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // min_match == 0
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111178, 1111111127}};
    std::vector<irs::doc_id_t> expected{
      1, 2, 5, 7, 9, 11, 45, 1145, 111165, 1111178, 111111127, 1111111127};
    std::vector<size_t> match_counts{2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 0U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 1145, 111165, 1111178, 111111127},
      {1, 2, 5, 7, 9, 11, 45},
      {1111178, 1111111127}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 11, 1111178};
    std::vector<size_t> match_counts{2, 2, 2, 2, 2, 2, 2};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_EQ(match_count, match_counts.end());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(2, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}, {24}, {24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(3, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};
    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<size_t> match_counts{3, 1, 3, 2, 1, 1, 1, 1, 1, 1};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,   7,   9,   11,   12,
                                        29, 45, 79, 101, 141, 256, 1025, 1101};
    std::vector<size_t> match_counts{3, 1, 3, 2, 1, 1, 2, 1,
                                     1, 1, 1, 1, 1, 1, 1, 1};
    ASSERT_EQ(expected.size(), match_counts.size());
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      auto match_count = match_counts.begin();
      for (; it.next(); ++match_count) {
        result.push_back(it.value());
        ASSERT_EQ(*match_count, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1}, {2}, {3}};

    std::vector<irs::doc_id_t> expected{1, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(1, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(3, it.match_count());
      }
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      ASSERT_FALSE(it.next());
      ASSERT_EQ(0, it.match_count());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    ASSERT_EQ(0, it.match_count());
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

TEST(block_disjunction_test, seek_no_readahead) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  struct seek_doc {
    irs::doc_id_t target;
    irs::doc_id_t expected;
    size_t match_count;
  };

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 78, 127},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {128, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 12, 29, 45,
                                                  65, 127, 1145, 111165,
                                                  1111178, 111111127}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {111165, 111165, 1},
      {111166, 1111178, 1},
      {1111177, 1111178, 1},
      {1111178, 1111178, 1},
      {111111127, 111111127, 1},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {12, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {44, 45, 1},
      {irs::doc_limits::invalid(), 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {13, 29, 1},
      {45, 45, 1},
      {80, 101, 1},
      {513, 1025, 1},
      {2, 1025, 1},
      {irs::doc_limits::invalid(), 1025, 1},
      {2001, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {1201, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty iterators
  {
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;

    disjunction::doc_iterators_t itrs;
    itrs.emplace_back(irs::doc_iterator::empty());
    itrs.emplace_back(irs::doc_iterator::empty());
    itrs.emplace_back(irs::doc_iterator::empty());
    disjunction it{std::move(itrs)};
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(1));
    ASSERT_EQ(0, it.match_count());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(expected, result);
  }
}

TEST(block_disjunction_test, seek_scored_no_readahead) {
  auto order = [](auto& scorer) -> irs::Scorers {
    return irs::Scorers::Prepare(scorer);
  };

  struct seek_doc {
    irs::doc_id_t target;
    irs::doc_id_t expected;
    size_t match_count;
    size_t score;
  };

  // no iterators provided
  {
    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

        using adapter = typename disjunction::adapter;

        return irs::memory::make_managed<disjunction>(std::vector<adapter>{},
                                                      std::move(aggregator));
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // single iterator case, values fit 1 block
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 12, 29, 45},
                      order(sort4));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 0},
      {9, 9, 1, 0},
      {8, 9, 1, 0},
      {irs::doc_limits::invalid(), 9, 1, 0},
      {12, 12, 1, 0},
      {8, 12, 1, 0},
      {13, 29, 1, 0},
      {45, 45, 1, 0},
      {57, irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    // no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block
  // disjunction with score, sub-iterators with scores
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 78, 127},
      order(sort4));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 4},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {irs::doc_limits::invalid(), 9, 1, 4},
      {12, 12, 1, 4},
      {8, 12, 1, 4},
      {13, 29, 1, 4},
      {45, 45, 1, 4},
      {57, 65, 1, 4},
      {126, 127, 1, 4},
      {128, irs::doc_limits::eof(), 0, 4},  // stay at previous score
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 4},
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  // single iterator case, values don't fit single block, gap between block
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 127, 1145,
                                 111165, 1111178, 111111127},
      order(sort4));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 4},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {irs::doc_limits::invalid(), 9, 1, 4},
      {12, 12, 1, 4},
      {8, 12, 1, 4},
      {13, 29, 1, 4},
      {45, 45, 1, 4},
      {57, 65, 1, 4},
      {126, 127, 1, 4},
      {111165, 111165, 1, 4},
      {111166, 1111178, 1, 4},
      {1111177, 1111178, 1, 4},
      {1111178, 1111178, 1, 4},
      {111111127, 111111127, 1, 4},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0,
       4},  // stay at previous score
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 6},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {irs::doc_limits::invalid(), 9, 1, 4},
      {12, 12, 1, 2},
      {8, 12, 1, 2},
      {13, 29, 1, 2},
      {45, 45, 1, 4},
      {57, irs::doc_limits::eof(), 0, 4}  // stay at previous score
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);
        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  // empty datasets
  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort2));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {6, irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;

    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
      {9, irs::doc_limits::eof(), 0, 0},
      {12, irs::doc_limits::eof(), 0, 0},
      {13, irs::doc_limits::eof(), 0, 0},
      {45, irs::doc_limits::eof(), 0, 0},
      {57, irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {9, 9, 1, 4},
      {12, 12, 1, 2},
      {irs::doc_limits::invalid(), 12, 1, 2},
      {45, 45, 1, 4},
      {57, irs::doc_limits::eof(), 0, 4}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort1));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 7},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {12, 12, 1, 2},
      {13, 29, 1, 2},
      {45, 45, 1, 4},
      {44, 45, 1, 4},
      {irs::doc_limits::invalid(), 45, 1, 4},
      {57, irs::doc_limits::eof(), 0, 4}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      order(sort8));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 7},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {13, 29, 1, 2},
      {45, 45, 1, 4},
      {80, 101, 1, 8},
      {256, 256, 1, 0},
      {513, 1025, 1, 8},
      {2, 1025, 1, 8},
      {irs::doc_limits::invalid(), 1025, 1, 8},
      {2001, irs::doc_limits::eof(), 0, 8}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  // empty datasets
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort8));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort1));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {6, irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort8));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      order(sort1));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
      {9, irs::doc_limits::eof(), 0, 0},
      {12, irs::doc_limits::eof(), 0, 0},
      {13, irs::doc_limits::eof(), 0, 0},
      {45, irs::doc_limits::eof(), 0, 0},
      {57, irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      irs::Scorers());

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {9, 9, 1, 0},
      {12, 12, 1, 0},
      {irs::doc_limits::invalid(), 12, 1, 0},
      {45, 45, 1, 0},
      {1201, irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }
}

TEST(block_disjunction_test, seek_scored_readahead) {
  auto order = [](auto& scorer) { return irs::Scorers::Prepare(scorer); };

  struct seek_doc {
    irs::doc_id_t target;
    irs::doc_id_t expected;
    size_t match_count;
    size_t score;
  };

  // no iterators provided
  {
    using disjunction = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;

    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // single iterator case, values fit 1 block
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 12, 29, 45},
                      order(sort4));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 0},
      {9, 9, 1, 0},
      {8, 9, 1, 0},
      {irs::doc_limits::invalid(), 9, 1, 0},
      {12, 12, 1, 0},
      {8, 12, 1, 0},
      {13, 29, 1, 0},
      {45, 45, 1, 0},
      {57, irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    // no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block
  // disjunction with score, sub-iterators with scores
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 78, 127},
      order(sort4));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 4},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {irs::doc_limits::invalid(), 9, 1, 4},
      {12, 12, 1, 4},
      {8, 12, 1, 4},
      {13, 29, 1, 4},
      {45, 45, 1, 4},
      {57, 65, 1, 4},
      {126, 127, 1, 4},
      {128, irs::doc_limits::eof(), 0, 4},  // stay at previous score
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 4},
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  // single iterator case, values don't fit single block, gap between block
  {
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 127, 1145,
                                 111165, 1111178, 111111127},
      order(sort4));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 4},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {irs::doc_limits::invalid(), 9, 1, 4},
      {12, 12, 1, 4},
      {8, 12, 1, 4},
      {13, 29, 1, 4},
      {45, 45, 1, 4},
      {57, 65, 1, 4},
      {126, 127, 1, 4},
      {111165, 111165, 1, 4},
      {111166, 1111178, 1, 4},
      {1111177, 1111178, 1, 4},
      {1111178, 1111178, 1, 4},
      {111111127, 111111127, 1, 4},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0,
       4},  // stay at previous score
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 6},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {irs::doc_limits::invalid(), 9, 1, 4},
      {12, 12, 1, 2},
      {8, 12, 1, 2},
      {13, 29, 1, 2},
      {45, 45, 1, 4},
      {57, irs::doc_limits::eof(), 0, 4}  // stay at previous score
    };

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  // empty datasets
  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort2));

    std::vector<seek_doc> expected{
      {6, irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
      {9, irs::doc_limits::eof(), 0, 0},
      {12, irs::doc_limits::eof(), 0, 0},
      {13, irs::doc_limits::eof(), 0, 0},
      {45, irs::doc_limits::eof(), 0, 0},
      {57, irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));

    std::vector<seek_doc> expected{{9, 9, 1, 4},
                                   {12, 12, 1, 2},
                                   {irs::doc_limits::invalid(), 12, 1, 2},
                                   {45, 45, 1, 4},
                                   {57, irs::doc_limits::eof(), 0, 4}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort1));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 7},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {12, 12, 1, 2},
      {13, 29, 1, 2},
      {45, 45, 1, 4},
      {44, 45, 1, 4},
      {irs::doc_limits::invalid(), 45, 1, 4},
      {57, irs::doc_limits::eof(), 0, 4}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      order(sort8));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {1, 1, 1, 7},
      {9, 9, 1, 4},
      {8, 9, 1, 4},
      {13, 29, 1, 2},
      {45, 45, 1, 4},
      {80, 101, 1, 8},
      {256, 256, 1, 0},
      {513, 1025, 1, 8},
      {2, 1025, 1, 8},
      {irs::doc_limits::invalid(), 1025, 1, 8},
      {2001, irs::doc_limits::eof(), 0, 8}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  // empty datasets
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort8));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{}, order(sort1));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {6, irs::doc_limits::eof(), 0, 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort8{8};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      order(sort8));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      order(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, order(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, order(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      order(sort1));

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0, 0},
      {9, irs::doc_limits::eof(), 0, 0},
      {12, irs::doc_limits::eof(), 0, 0},
      {13, irs::doc_limits::eof(), 0, 0},
      {45, irs::doc_limits::eof(), 0, 0},
      {57, irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
      irs::score_t score_value;
      score(&score_value);
      ASSERT_EQ(target.score, score_value);
    }
  }

  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{256}, irs::Scorers());
    docs.emplace_back(std::vector<irs::doc_id_t>{11, 79, 101, 141, 1025, 1101},
                      irs::Scorers());

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1, 0},
      {9, 9, 1, 0},
      {12, 12, 1, 0},
      {irs::doc_limits::invalid(), 12, 1, 0},
      {45, 45, 1, 0},
      {1201, irs::doc_limits::eof(), 0, 0}};

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 2U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(2, irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }
}

TEST(block_disjunction_test, min_match_seek_no_readahead) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMinMatch, false, 1>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  struct seek_doc {
    irs::doc_id_t target;
    irs::doc_id_t expected;
    size_t match_count;
  };

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {1, irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 78, 127},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {128, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 12, 29, 45,
                                                  65, 127, 1145, 111165,
                                                  1111178, 111111127}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {111165, 111165, 1},
      {111166, 1111178, 1},
      {1111177, 1111178, 1},
      {1111178, 1111178, 1},
      {111111127, 111111127, 1},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 2},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 3},
      {9, 9, 1},
      {8, 9, 1},
      {12, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {44, 45, 1},
      {irs::doc_limits::invalid(), 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 3},
      {9, 9, 1},
      {8, 9, 1},
      {13, 29, 1},
      {45, 45, 1},
      {80, 101, 1},
      {513, 1025, 1},
      {2, 1025, 1},
      {irs::doc_limits::invalid(), 1025, 1},
      {2001, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, 6, 2},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {1201, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 8, 12, 29},
      {1, 5, 6},
      {8, 256},
      {8, 11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {5, 5, 3},
      {7, 8, 3},
      {9, irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 3U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }
}

TEST(block_disjunction_test, seek_readahead) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMatch, true, 1>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  struct seek_doc {
    irs::doc_id_t target;
    irs::doc_id_t expected;
    size_t match_count;
  };

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 78, 127},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {128, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 12, 29, 45,
                                                  65, 127, 1145, 111165,
                                                  1111178, 111111127}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {111165, 111165, 1},
      {111166, 1111178, 1},
      {1111177, 1111178, 1},
      {1111178, 1111178, 1},
      {111111127, 111111127, 1},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(it.match_count(), it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(it.match_count(), it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<seek_doc> expected{
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<seek_doc> expected{{1, 1, 1},
                                   {9, 9, 1},
                                   {8, 9, 1},
                                   {12, 12, 1},
                                   {13, 29, 1},
                                   {45, 45, 1},
                                   {44, 45, 1},
                                   {irs::doc_limits::invalid(), 45, 1},
                                   {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 1},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {13, 29, 1},
      {45, 45, 1},
      {80, 101, 1},
      {513, 1025, 1},
      {2, 1025, 1},
      {irs::doc_limits::invalid(), 1025, 1},
      {2001, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};
    std::vector<seek_doc> expected{
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {9, 9, 1},   {12, 12, 1},     {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1}, {1024, 1025, 1}, {1201, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{{9, 9, 1},
                                   {12, 12, 1},
                                   {irs::doc_limits::invalid(), 12, 1},
                                   {45, 45, 1},
                                   {1201, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }
}

TEST(block_disjunction_test, min_match_seek_readahead) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMinMatch, true, 1>>;

  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  struct seek_doc {
    irs::doc_id_t target;
    irs::doc_id_t expected;
    size_t match_count;
  };

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {1, irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values fit 1 block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {8, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 2U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 12, 29, 45, 65, 78, 127},
    };

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {128, irs::doc_limits::eof(), 0},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // single iterator case, values don't fit single block, gap between block
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 12, 29, 45,
                                                  65, 127, 1145, 111165,
                                                  1111178, 111111127}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 1},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, 65, 1},
      {126, 127, 1},
      {111165, 111165, 1},
      {111166, 1111178, 1},
      {1111177, 1111178, 1},
      {1111178, 1111178, 1},
      {111111127, 111111127, 1},
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 2},
      {9, 9, 1},
      {8, 9, 1},
      {irs::doc_limits::invalid(), 9, 1},
      {12, 12, 1},
      {8, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 3},
      {9, 9, 1},
      {8, 9, 1},
      {12, 12, 1},
      {13, 29, 1},
      {45, 45, 1},
      {44, 45, 1},
      {irs::doc_limits::invalid(), 45, 1},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {1, 1, 3},
      {9, 9, 1},
      {8, 9, 1},
      {13, 29, 1},
      {45, 45, 1},
      {80, 101, 1},
      {513, 1025, 1},
      {2, 1025, 1},
      {irs::doc_limits::invalid(), 1025, 1},
      {2001, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};
    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, irs::doc_limits::eof(), 0},
      {irs::doc_limits::invalid(), irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::eof(), irs::doc_limits::eof(), 0},
      {9, irs::doc_limits::eof(), 0},
      {12, irs::doc_limits::eof(), 0},
      {13, irs::doc_limits::eof(), 0},
      {45, irs::doc_limits::eof(), 0},
      {57, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {6, 6, 2},
      {9, 9, 1},
      {12, 12, 1},
      {irs::doc_limits::invalid(), 12, 1},
      {45, 45, 1},
      {1201, irs::doc_limits::eof(), 0}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 8, 12, 29},
      {1, 5, 6},
      {8, 256},
      {8, 11, 79, 101, 141, 1025, 1101}};

    std::vector<seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid(), 0},
      {5, 5, 3},
      {7, 8, 3},
      {9, irs::doc_limits::eof(), 0},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs), 3U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(doc->value, it.value());
      ASSERT_EQ(target.match_count, it.match_count());
    }
  }
}

TEST(block_disjunction_test, seek_next_no_readahead) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 256, 1145},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(45, it.seek(45));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(256, it.value());
    ASSERT_EQ(1145, it.seek(1144));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(block_disjunction_test, next_seek_no_readahead) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29, 54, 61},
                                                 {1, 5, 6, 67, 80, 84}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());

    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    ASSERT_EQ(5, it.seek(4));
    ASSERT_EQ(5, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(67, it.seek(64));
    ASSERT_EQ(67, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(80, it.value());
    ASSERT_EQ(84, it.seek(83));
    ASSERT_EQ(84, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(block_disjunction_test, seek_next_no_readahead_two_blocks) {
  using disjunction = irs::block_disjunction<
    irs::doc_iterator::ptr, irs::NoopAggregator,
    irs::block_disjunction_traits<irs::MatchType::kMatch, false, 2>>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 170, 255, 1145},
    };

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(45, it.seek(45));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(170, it.value());
    ASSERT_EQ(1145, it.seek(1144));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(block_disjunction_test, scored_seek_next_no_readahead) {
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::NoopAggregator,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, aggregate
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(6, tmp);  // 2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // 2
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // std::max(2)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores partially, aggregate
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // 4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores partially, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators without scores, aggregate
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::SumMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators without scores, max
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::block_disjunction<
          irs::doc_iterator::ptr, A,
          irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::block_disjunction<
      irs::doc_iterator::ptr, irs::Aggregator<irs::MaxMerger, 1>,
      irs::block_disjunction_traits<irs::MatchType::kMatch, false, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

// ----------------------------------------------------------------------------
// --SECTION--    disjunction (iterator0 OR iterator1 OR iterator2 OR ...)
// ----------------------------------------------------------------------------

TEST(disjunction_test, next) {
  using disjunction =
    irs::disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      size_t heap{0};
      auto visitor = [](void* ptr, disjunction::adapter& iter) {
        EXPECT_FALSE(irs::doc_limits::eof(iter.doc->value));
        auto pval = static_cast<uint32_t*>(ptr);
        *pval = *pval + 1;
        return true;
      };
      for (; it.next();) {
        result.push_back(it.value());
        it.visit(&heap, visitor);
      }
      ASSERT_GT(heap, 0);  // some iterators should be visited
      heap = 0;
      ASSERT_FALSE(it.next());
      it.visit(&heap, visitor);
      ASSERT_EQ(0, heap);  // nothing to visit
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(expected, result);
  }

  // basic case : single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }

    ASSERT_EQ(docs[0], result);
  }

  // basic case : same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // basic case : single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{24}};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_TRUE(!irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 7, 9, 11, 12, 29, 45};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1,  2,  5,  6,   7,   9,   11,   12,
                                        29, 45, 79, 101, 141, 256, 1025, 1101};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1}, {2}, {3}};

    std::vector<irs::doc_id_t> expected{1, 2, 3};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45},
                                                 {1, 2, 5, 7, 9, 11, 45}};

    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs[0], result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      disjunction it(detail::execute_all<disjunction::adapter>(docs));
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
                irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }
}

TEST(disjunction_test, seek) {
  using disjunction =
    irs::disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  // no iterators provided
  {
    disjunction it(disjunction::doc_iterators_t{});
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(0, irs::cost::extract(it));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(irs::doc_limits::eof(), it.seek(42));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {9, 9},
      {8, 9},
      {irs::doc_limits::invalid(), 9},
      {12, 12},
      {8, 12},
      {13, 29},
      {45, 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // NO_MORE_DOCS
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // INVALID_DOC
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{1, 2, 5, 7, 9, 11, 45},
                                                 {1, 5, 6, 12, 29}};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {9, 9},
      {12, 12},
      {irs::doc_limits::invalid(), 12},
      {45, 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {9, 9},
      {8, 9},
      {12, 12},
      {13, 29},
      {45, 45},
      {44, 45},
      {irs::doc_limits::invalid(), 45},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {9, 9},
      {8, 9},
      {13, 29},
      {45, 45},
      {80, 101},
      {513, 1025},
      {2, 1025},
      {irs::doc_limits::invalid(), 1025},
      {2001, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // NO_MORE_DOCS
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // INVALID_DOC
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {9, 9},
      {12, 12},
      {irs::doc_limits::invalid(), 12},
      {45, 45},
      {1201, irs::doc_limits::eof()}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }
}

TEST(disjunction_test, seek_next) {
  using disjunction =
    irs::disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
  auto sum = [](size_t sum, const std::vector<irs::doc_id_t>& docs) {
    return sum += docs.size();
  };

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6}};

    disjunction it(detail::execute_all<disjunction::adapter>(docs));
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::accumulate(docs.begin(), docs.end(), size_t(0), sum),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(disjunction_test, scored_seek_next) {
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, irs::Scorers::kUnordered.buckets().size(),
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType =
      irs::disjunction<irs::doc_iterator::ptr, irs::NoopAggregator>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, aggregate
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::disjunction<irs::doc_iterator::ptr,
                                          irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(6, tmp);  // 2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // 2
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::disjunction<irs::doc_iterator::ptr,
                                          irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(2, tmp);  // std::max(2)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores partially, aggregate
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::disjunction<irs::doc_iterator::ptr,
                                          irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // 4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores partially, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::disjunction<irs::doc_iterator::ptr,
                                          irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // std::max(1)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators without scores, aggregate
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::disjunction<irs::doc_iterator::ptr,
                                          irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  //
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators without scores, max
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6}, irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) mutable -> irs::doc_iterator::ptr {
        using disjunction = irs::disjunction<irs::doc_iterator::ptr, A>;
        using adapter = typename disjunction::adapter;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(
          std::move(res), std::move(aggregator), 1U);  // custom cost
      });

    using ExpectedType = irs::disjunction<irs::doc_iterator::ptr,
                                          irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(1, irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(7, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(45, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

// ----------------------------------------------------------------------------
// --SECTION--  Minimum match count: iterator0 OR iterator1 OR iterator2 OR ...
// ----------------------------------------------------------------------------

TEST(min_match_disjunction_test, next) {
  using disjunction = irs::MinMatchDisjunction<irs::NoopAggregator>;
  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
    };

    {
      const size_t min_match_count = 0;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    {
      const size_t min_match_count = 1;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    {
      const size_t min_match_count = 2;
      std::vector<irs::doc_id_t> expected{};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    {
      const size_t min_match_count = 6;
      std::vector<irs::doc_id_t> expected{};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it{detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count};
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    {
      const size_t min_match_count = std::numeric_limits<size_t>::max();
      std::vector<irs::doc_id_t> expected{};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it{detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count};
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));

        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {7, 15, 26, 212, 239},
      {1001, 4001, 5001},
      {10, 101, 490, 713, 1201, 2801},
    };

    {
      const size_t min_match_count = 0;
      std::vector<irs::doc_id_t> expected = detail::union_all(docs);
      std::vector<irs::doc_id_t> result;
      {
        disjunction it{detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count};
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    {
      const size_t min_match_count = 1;
      std::vector<irs::doc_id_t> expected = detail::union_all(docs);
      std::vector<irs::doc_id_t> result;
      {
        disjunction it{detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count};
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    {
      const size_t min_match_count = 2;
      std::vector<irs::doc_id_t> expected{7};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it{detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count};
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    {
      const size_t min_match_count = 3;
      std::vector<irs::doc_id_t> expected;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = 4;
      std::vector<irs::doc_id_t> expected;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = 5;
      std::vector<irs::doc_id_t> expected{};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = std::numeric_limits<size_t>::max();
      std::vector<irs::doc_id_t> expected{};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {1, 2, 5, 8, 13, 29},
    };

    {
      const size_t min_match_count = 0;
      std::vector<irs::doc_id_t> expected{1, 2,  5,  6,  7,  8,
                                          9, 11, 12, 13, 29, 45};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    {
      const size_t min_match_count = 1;
      std::vector<irs::doc_id_t> expected{1, 2,  5,  6,  7,  8,
                                          9, 11, 12, 13, 29, 45};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    {
      const size_t min_match_count = 2;
      std::vector<irs::doc_id_t> expected{1, 2, 5, 6, 29};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    {
      const size_t min_match_count = 3;
      std::vector<irs::doc_id_t> expected{1, 5};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = 4;
      std::vector<irs::doc_id_t> expected{1, 5};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = 5;
      std::vector<irs::doc_id_t> expected{1, 5};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = std::numeric_limits<size_t>::max();
      std::vector<irs::doc_id_t> expected{1, 5};
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(expected, result);
    }
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 2, 5, 7, 9, 11, 45},
      {1, 2, 5, 7, 9, 11, 45},
      {1, 2, 5, 7, 9, 11, 45},
    };

    // equals to disjunction
    {
      const size_t min_match_count = 0;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    // equals to disjunction
    {
      const size_t min_match_count = 1;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    {
      const size_t min_match_count = 2;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    {
      const size_t min_match_count = 3;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = 4;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = 5;
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }

    // equals to conjunction
    {
      const size_t min_match_count = std::numeric_limits<size_t>::max();
      std::vector<irs::doc_id_t> result;
      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       min_match_count);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(irs::doc_limits::invalid(), it.value());
        for (; it.next();) {
          result.push_back(it.value());
        }
        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
      ASSERT_EQ(docs.front(), result);
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}};

    std::vector<irs::doc_id_t> expected{};
    {
      std::vector<irs::doc_id_t> expected{};
      {
        std::vector<irs::doc_id_t> result;
        {
          disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 0U);
          auto* doc = irs::get<irs::document>(it);
          ASSERT_TRUE(bool(doc));
          ASSERT_EQ(irs::doc_limits::invalid(), it.value());
          for (; it.next();) {
            result.push_back(it.value());
          }
          ASSERT_FALSE(it.next());
          ASSERT_TRUE(irs::doc_limits::eof(it.value()));
        }
        ASSERT_EQ(docs.front(), result);
      }

      {
        std::vector<irs::doc_id_t> result;
        {
          disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 1U);
          auto* doc = irs::get<irs::document>(it);
          ASSERT_TRUE(bool(doc));
          ASSERT_EQ(irs::doc_limits::invalid(), it.value());
          for (; it.next();) {
            result.push_back(it.value());
          }
          ASSERT_FALSE(it.next());
          ASSERT_TRUE(irs::doc_limits::eof(it.value()));
        }
        ASSERT_EQ(docs.front(), result);
      }

      {
        std::vector<irs::doc_id_t> result;
        {
          disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                         std::numeric_limits<size_t>::max());
          auto* doc = irs::get<irs::document>(it);
          ASSERT_TRUE(bool(doc));
          ASSERT_EQ(irs::doc_limits::invalid(), it.value());
          for (; it.next();) {
            result.push_back(it.value());
          }
          ASSERT_FALSE(it.next());
          ASSERT_TRUE(irs::doc_limits::eof(it.value()));
        }
        ASSERT_EQ(docs.front(), result);
      }
    }
  }
}

TEST(min_match_disjunction_test, seek) {
  using disjunction = irs::MinMatchDisjunction<irs::NoopAggregator>;

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 29, 45}, {1, 5, 6, 12, 29}, {1, 5, 6, 12}};

    // equals to disjunction
    {
      const size_t min_match_count = 0;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {9, 9},
        {irs::doc_limits::invalid(), 9},
        {12, 12},
        {11, 12},
        {13, 29},
        {45, 45},
        {57, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    // equals to disjunction
    {
      const size_t min_match_count = 1;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {9, 9},
        {8, 9},
        {12, 12},
        {13, 29},
        {irs::doc_limits::invalid(), 29},
        {45, 45},
        {57, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      const size_t min_match_count = 2;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {6, 6},
        {4, 6},
        {7, 12},
        {irs::doc_limits::invalid(), 12},
        {29, 29},
        {45, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    // equals to conjunction
    {
      const size_t min_match_count = 3;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {6, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    // equals to conjunction
    {
      const size_t min_match_count = std::numeric_limits<size_t>::max();
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {6, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45, 79, 101},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    // equals to disjunction
    {
      const size_t min_match_count = 0;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {9, 9},
        {8, 9},
        {13, 29},
        {45, 45},
        {irs::doc_limits::invalid(), 45},
        {80, 101},
        {513, 1025},
        {2001, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    // equals to disjunction
    {
      const size_t min_match_count = 1;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {9, 9},
        {8, 9},
        {13, 29},
        {45, 45},
        {irs::doc_limits::invalid(), 45},
        {80, 101},
        {513, 1025},
        {2001, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      const size_t min_match_count = 2;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {6, 6},
        {2, 6},
        {13, 79},
        {irs::doc_limits::invalid(), 79},
        {101, 101},
        {513, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
        ASSERT_EQ(it.value(), doc->value);
      }
    }

    {
      const size_t min_match_count = 3;
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, 1},
        {6, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
        ASSERT_EQ(it.value(), doc->value);
      }
    }

    // equals to conjunction
    {
      const size_t min_match_count = std::numeric_limits<size_t>::max();
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {1, irs::doc_limits::eof()},
        {6, irs::doc_limits::eof()}};

      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     min_match_count);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
        ASSERT_EQ(it.value(), doc->value);
      }
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 0U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 1U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     std::numeric_limits<size_t>::max());
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }
  }

  // NO_MORE_DOCS
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 0U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 1U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 2U);
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }

    {
      disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                     std::numeric_limits<size_t>::max());
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      for (const auto& target : expected) {
        ASSERT_EQ(target.expected, it.seek(target.target));
      }
    }
  }

  // INVALID_DOC
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 6},
      {256},
      {11, 79, 101, 141, 1025, 1101}};

    // equals to disjunction
    {
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {9, 9},
        {12, 12},
        {irs::doc_limits::invalid(), 12},
        {45, 45},
        {44, 45},
        {1201, irs::doc_limits::eof()}};

      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 0U);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        for (const auto& target : expected) {
          ASSERT_EQ(target.expected, it.seek(target.target));
        }
      }

      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 1U);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        for (const auto& target : expected) {
          ASSERT_EQ(target.expected, it.seek(target.target));
        }
      }
    }

    {
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {6, 6},
        {irs::doc_limits::invalid(), 6},
        {12, irs::doc_limits::eof()}};

      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 2U);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        for (const auto& target : expected) {
          ASSERT_EQ(target.expected, it.seek(target.target));
        }
      }
    }

    {
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {6, irs::doc_limits::eof()},
        {irs::doc_limits::invalid(), irs::doc_limits::eof()},
      };

      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 3U);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        for (const auto& target : expected) {
          ASSERT_EQ(target.expected, it.seek(target.target));
        }
      }
    }

    // equals to conjuction
    {
      std::vector<detail::seek_doc> expected{
        {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
        {6, irs::doc_limits::eof()},
        {irs::doc_limits::invalid(), irs::doc_limits::eof()},
      };

      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 5U);
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        for (const auto& target : expected) {
          ASSERT_EQ(target.expected, it.seek(target.target));
        }
      }

      {
        disjunction it(detail::execute_all<irs::CostAdapter<>>(docs),
                       std::numeric_limits<size_t>::max());
        auto* doc = irs::get<irs::document>(it);
        ASSERT_TRUE(bool(doc));
        for (const auto& target : expected) {
          ASSERT_EQ(target.expected, it.seek(target.target));
        }
      }
    }
  }
}

TEST(min_match_disjunction_test, seek_next) {
  using disjunction = irs::MinMatchDisjunction<irs::NoopAggregator>;

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 5, 7, 9, 11, 45}, {1, 5, 6, 12, 29}, {1, 5, 6, 9, 29}};

    disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 2U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(irs::doc_limits::invalid(), it.value());

    ASSERT_EQ(5, it.seek(5));
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_EQ(6, it.value());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_EQ(9, it.value());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_EQ(29, it.seek(27));
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_EQ(it.value(), doc->value);
  }
}

TEST(min_match_disjunction_test, match_count) {
  using disjunction = irs::MinMatchDisjunction<irs::NoopAggregator>;

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 3}, {1, 2, 3, 4}, {1, 3, 4}, {1, 3, 4}};

    disjunction it(detail::execute_all<irs::CostAdapter<>>(docs), 1U);
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(irs::doc_limits::invalid(), it.value());

    ASSERT_TRUE(it.next());  // |1,1,1,1
    ASSERT_EQ(1, it.value());
    ASSERT_TRUE(it.next());  // 3,3,3|2
    ASSERT_EQ(2, it.value());
    ASSERT_EQ(4, it.seek(4));        // 3,3,3|4
    ASSERT_EQ(3, it.match_count());  // 3,3,3|4
    ASSERT_FALSE(it.next());
  }
}

TEST(min_match_disjunction_test, scored_seek_next) {
  // disjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 0,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType = irs::MinMatchDisjunction<irs::NoopAggregator>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(5, it.seek(5));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    ASSERT_EQ(29, it.seek(27));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, aggregate
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType =
      irs::MinMatchDisjunction<irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(6, tmp);  // 2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(6, tmp);  // 2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType =
      irs::MinMatchDisjunction<irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,4)
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(2,4)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores partially, aggregate
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType =
      irs::MinMatchDisjunction<irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // 2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+4
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // 2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators with scores partially, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers::Prepare(sort4));

    auto prepared_order = irs::Scorers::Prepare(
      detail::basic_sort{std::numeric_limits<size_t>::max()});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType =
      irs::MinMatchDisjunction<irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(4, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(4, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators without scores, aggregate
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType =
      irs::MinMatchDisjunction<irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+4
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // disjunction with score, sub-iterators without scores, max
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 2, 5, 7, 9, 11, 45},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 12, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 5, 6, 9, 29},
                      irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction = irs::MinMatchDisjunction<A>;
        using adapter = typename irs::CostAdapter<>;

        auto res = detail::execute_all<adapter>(docs);

        return irs::memory::make_managed<disjunction>(std::move(res), 2U,
                                                      std::move(aggregator));
      });

    using ExpectedType =
      irs::MinMatchDisjunction<irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(
      docs[0].first.size() + docs[1].first.size() + docs[2].first.size(),
      irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(5, it.seek(5));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(6, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(9, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(29, it.seek(27));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                    iterator0 AND iterator1 AND iterator2 AND ...
// ----------------------------------------------------------------------------
using DocIterator = irs::ScoreAdapter<>;

TEST(conjunction_test, next) {
  auto shortest = [](const std::vector<irs::doc_id_t>& lhs,
                     const std::vector<irs::doc_id_t>& rhs) {
    return lhs.size() < rhs.size();
  };

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 6},
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29},
      {1, 5, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{1, 5};
    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
        ASSERT_EQ(it.value(), doc->value);
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // not optimal case, first is the longest
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      {1, 5, 11, 21, 27, 31}};

    std::vector<irs::doc_id_t> expected{1, 5, 11, 21, 27, 31};
    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // simple case
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 11, 21, 27, 31},
      {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
    };

    std::vector<irs::doc_id_t> expected{1, 5, 11, 21, 27, 31};
    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // not optimal case, first is the longest
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 79, 101, 141, 1025, 1101},
      {1, 5, 6},
      {1, 2, 5, 7, 9, 11, 45},
      {1, 5, 6, 12, 29}};

    std::vector<irs::doc_id_t> expected{1, 5};
    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // same datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 79, 101, 141, 1025, 1101},
      {1, 5, 79, 101, 141, 1025, 1101},
      {1, 5, 79, 101, 141, 1025, 1101},
      {1, 5, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // single dataset
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(docs.front(), result);
  }

  // empty intersection
  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 6},
      {1, 2, 3, 7, 9, 11, 45},
      {3, 5, 6, 12, 29},
      {1, 5, 79, 101, 141, 1025, 1101}};

    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};

    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                        detail::execute_all<DocIterator>(docs));
      auto& it = *itPtr;
      auto* doc = irs::get<irs::document>(it);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
                irs::cost::extract(it));
      ASSERT_EQ(irs::doc_limits::invalid(), it.value());
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }
}

TEST(conjunction_test, seek) {
  auto shortest = [](const std::vector<irs::doc_id_t>& lhs,
                     const std::vector<irs::doc_id_t>& rhs) {
    return lhs.size() < rhs.size();
  };

  // simple case
  {
    // 1 6 28 45 99 256
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 5, 6, 45, 77, 99, 256, 988},
      {1, 2, 5, 6, 7, 9, 11, 28, 45, 99, 256},
      {1, 5, 6, 12, 28, 45, 99, 124, 256, 553},
      {1, 6, 11, 29, 45, 99, 141, 256, 1025, 1101}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {6, 6},
      {irs::doc_limits::invalid(), 6},
      {29, 45},
      {46, 99},
      {68, 99},
      {256, 256},
      {257, irs::doc_limits::eof()}};

    auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                      detail::execute_all<DocIterator>(docs));
    auto& it = *itPtr;
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
      ASSERT_EQ(it.value(), doc->value);
    }
  }

  // not optimal, first is the longest
  {
    // 1 6 28 45 99 256
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 6, 11, 29, 45, 99, 141, 256, 1025, 1101},
      {1, 2, 5, 6, 7, 9, 11, 28, 45, 99, 256},
      {1, 5, 6, 12, 29, 45, 99, 124, 256, 553},
      {1, 5, 6, 45, 77, 99, 256, 988}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 1},
      {6, 6},
      {29, 45},
      {44, 45},
      {46, 99},
      {irs::doc_limits::invalid(), 99},
      {256, 256},
      {257, irs::doc_limits::eof()}};

    auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                      detail::execute_all<DocIterator>(docs));
    auto& it = *itPtr;
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // empty datasets
  {
    std::vector<std::vector<irs::doc_id_t>> docs{{}, {}, {}, {}};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};

    auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                      detail::execute_all<DocIterator>(docs));
    auto& it = *itPtr;
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // NO_MORE_DOCS
  {
    // 1 6 28 45 99 256
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 6, 11, 29, 45, 99, 141, 256, 1025, 1101},
      {1, 2, 5, 6, 7, 9, 11, 28, 45, 99, 256},
      {1, 5, 6, 12, 29, 45, 99, 124, 256, 553},
      {1, 5, 6, 45, 77, 99, 256, 988}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};

    auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                      detail::execute_all<DocIterator>(docs));
    auto& it = *itPtr;
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // INVALID_DOC
  {
    // 1 6 28 45 99 256
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 6, 11, 29, 45, 99, 141, 256, 1025, 1101},
      {1, 2, 5, 6, 7, 9, 11, 28, 45, 99, 256},
      {1, 5, 6, 12, 29, 45, 99, 124, 256, 553},
      {1, 5, 6, 45, 77, 99, 256, 988}};

    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, 6},
      {45, 45},
      {irs::doc_limits::invalid(), 45},
      {99, 99},
      {257, irs::doc_limits::eof()}};

    auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                      detail::execute_all<DocIterator>(docs));
    auto& it = *itPtr;
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
              irs::cost::extract(it));
    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }
}

TEST(conjunction_test, seek_next) {
  auto shortest = [](const std::vector<irs::doc_id_t>& lhs,
                     const std::vector<irs::doc_id_t>& rhs) {
    return lhs.size() < rhs.size();
  };

  {
    std::vector<std::vector<irs::doc_id_t>> docs{
      {1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      {1, 4, 5, 6, 8, 12, 14, 29},
      {1, 4, 5, 8, 14}};

    auto itPtr = irs::MakeConjunction({}, irs::NoopAggregator{},
                                      detail::execute_all<DocIterator>(docs));
    auto& it = *itPtr;
    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(std::min_element(docs.begin(), docs.end(), shortest)->size(),
              irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(4, it.seek(3));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    ASSERT_EQ(14, it.seek(14));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

TEST(conjunction_test, scored_seek_next) {
  // conjunction with score, sub-iterators with scores, aggregation
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction without score, sub-iterators with scores
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 0,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::NoopAggregator>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score, no order set
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_EQ(4, it.seek(3));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    ASSERT_EQ(14, it.seek(14));
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with 4 sub-iterators with score, sub-iterators with scores,
  // aggregation
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};
    detail::basic_sort sort5{5};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort5));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(12, tmp);  // 1+2+4+5
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(12, tmp);  // 1+2+4+5
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(12, tmp);  // 1+2+4+5
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(12, tmp);  // 1+2+4+5
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(12, tmp);  // 1+2+4+5
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, sub-iterators with scores, aggregation
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(7, tmp);  // 1+2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort2{2};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers::Prepare(sort2));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, 1 sub-iterator with scores, aggregation
  {
    detail::basic_sort sort1{1};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(1, tmp);  // 1
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, 1 sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(1, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, 2 sub-iterators with scores, aggregation
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(5, tmp);  // 1+2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, 2 sub-iterators with scores, max
  {
    detail::basic_sort sort1{1};
    detail::basic_sort sort4{4};

    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers::Prepare(sort1));
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers::Prepare(sort4));

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_FALSE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(4, tmp);  // std::max(1,2,4)
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, sub-iterators without scores, aggregation
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kSum, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::SumMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(0, tmp);  // 1+2+4
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }

  // conjunction with score, sub-iterators without scores, max
  {
    std::vector<std::pair<std::vector<irs::doc_id_t>, irs::Scorers>> docs;
    docs.emplace_back(
      std::vector<irs::doc_id_t>{1, 2, 4, 5, 7, 8, 9, 11, 14, 45},
      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 6, 8, 12, 14, 29},
                      irs::Scorers{});
    docs.emplace_back(std::vector<irs::doc_id_t>{1, 4, 5, 8, 14},
                      irs::Scorers{});

    auto it_ptr = irs::ResoveMergeType(
      irs::ScoreMergeType::kMax, 1,
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        auto res = detail::execute_all<DocIterator>(docs);
        return irs::MakeConjunction({}, std::move(aggregator), std::move(res));
      });

    using ExpectedType =
      irs::Conjunction<irs::ScoreAdapter<>, irs::Aggregator<irs::MaxMerger, 1>>;
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(it_ptr.get()));
    auto& it = dynamic_cast<ExpectedType&>(*it_ptr);

    auto* doc = irs::get<irs::document>(it);
    ASSERT_TRUE(bool(doc));

    // score
    auto& score = irs::score::get(it);
    ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
    ASSERT_EQ(&score, irs::get_mutable<irs::score>(&it));

    // cost
    ASSERT_EQ(docs[2].first.size(), irs::cost::extract(it));

    ASSERT_EQ(irs::doc_limits::invalid(), it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(1, it.value());
    irs::score_t tmp;
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(4, it.seek(3));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(5, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_TRUE(it.next());
    ASSERT_EQ(8, it.value());
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_EQ(14, it.seek(14));
    score(&tmp);
    ASSERT_EQ(0, tmp);
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::doc_limits::eof(), it.value());
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                      iterator0 AND NOT iterator1
// ----------------------------------------------------------------------------

TEST(exclusion_test, next) {
  // simple case
  {
    std::vector<irs::doc_id_t> included{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> excluded{1, 5, 6, 12, 29};
    std::vector<irs::doc_id_t> expected{2, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> result;
    {
      irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                          included.begin(), included.end()),
                        irs::memory::make_managed<detail::basic_doc_iterator>(
                          excluded.begin(), excluded.end()));

      // score, no order set
      auto& score = irs::score::get(it);
      ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_FALSE(irs::get_mutable<irs::score>(&it));
      ASSERT_EQ(&score, &irs::score::kNoScore);

      // cost
      ASSERT_EQ(included.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // basic case: single dataset
  {
    std::vector<irs::doc_id_t> included{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> excluded{};
    std::vector<irs::doc_id_t> result;
    {
      irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                          included.begin(), included.end()),
                        irs::memory::make_managed<detail::basic_doc_iterator>(
                          excluded.begin(), excluded.end()));
      ASSERT_EQ(included.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(included, result);
  }

  // basic case: single dataset
  {
    std::vector<irs::doc_id_t> included{};
    std::vector<irs::doc_id_t> excluded{1, 5, 6, 12, 29};
    std::vector<irs::doc_id_t> result;
    {
      irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                          included.begin(), included.end()),
                        irs::memory::make_managed<detail::basic_doc_iterator>(
                          excluded.begin(), excluded.end()));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(included, result);
  }

  // basic case: same datasets
  {
    std::vector<irs::doc_id_t> included{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> excluded{1, 2, 5, 7, 9, 11, 45};
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                          included.begin(), included.end()),
                        irs::memory::make_managed<detail::basic_doc_iterator>(
                          excluded.begin(), excluded.end()));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }

  // basic case: single dataset
  {
    std::vector<irs::doc_id_t> included{24};
    std::vector<irs::doc_id_t> excluded{};
    std::vector<irs::doc_id_t> result;
    {
      irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                          included.begin(), included.end()),
                        irs::memory::make_managed<detail::basic_doc_iterator>(
                          excluded.begin(), excluded.end()));
      ASSERT_EQ(included.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(included, result);
  }

  // empty
  {
    std::vector<irs::doc_id_t> included{};
    std::vector<irs::doc_id_t> excluded{};
    std::vector<irs::doc_id_t> expected{};
    std::vector<irs::doc_id_t> result;
    {
      irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                          included.begin(), included.end()),
                        irs::memory::make_managed<detail::basic_doc_iterator>(
                          excluded.begin(), excluded.end()));
      ASSERT_EQ(included.size(), irs::cost::extract(it));
      ASSERT_FALSE(irs::doc_limits::valid(it.value()));
      for (; it.next();) {
        result.push_back(it.value());
      }
      ASSERT_FALSE(it.next());
      ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    }
    ASSERT_EQ(expected, result);
  }
}

TEST(exclusion_test, seek) {
  // simple case
  {
    // 2, 7, 9, 11, 45
    std::vector<irs::doc_id_t> included{1, 2, 5, 7, 9, 11, 29, 45};
    std::vector<irs::doc_id_t> excluded{1, 5, 6, 12, 29};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {1, 2},
      {5, 7},
      {irs::doc_limits::invalid(), 7},
      {9, 9},
      {45, 45},
      {43, 45},
      {57, irs::doc_limits::eof()}};
    irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                        included.begin(), included.end()),
                      irs::memory::make_managed<detail::basic_doc_iterator>(
                        excluded.begin(), excluded.end()));
    ASSERT_EQ(included.size(), irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // empty datasets
  {
    std::vector<irs::doc_id_t> included{};
    std::vector<irs::doc_id_t> excluded{};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {6, irs::doc_limits::eof()},
      {irs::doc_limits::invalid(), irs::doc_limits::eof()}};
    irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                        included.begin(), included.end()),
                      irs::memory::make_managed<detail::basic_doc_iterator>(
                        excluded.begin(), excluded.end()));
    ASSERT_EQ(included.size(), irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // NO_MORE_DOCS
  {
    // 2, 7, 9, 11, 45
    std::vector<irs::doc_id_t> included{1, 2, 5, 7, 9, 11, 29, 45};
    std::vector<irs::doc_id_t> excluded{1, 5, 6, 12, 29};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {irs::doc_limits::eof(), irs::doc_limits::eof()},
      {9, irs::doc_limits::eof()},
      {12, irs::doc_limits::eof()},
      {13, irs::doc_limits::eof()},
      {45, irs::doc_limits::eof()},
      {57, irs::doc_limits::eof()}};
    irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                        included.begin(), included.end()),
                      irs::memory::make_managed<detail::basic_doc_iterator>(
                        excluded.begin(), excluded.end()));
    ASSERT_EQ(included.size(), irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }

  // INVALID_DOC
  {
    // 2, 7, 9, 11, 45
    std::vector<irs::doc_id_t> included{1, 2, 5, 7, 9, 11, 29, 45};
    std::vector<irs::doc_id_t> excluded{1, 5, 6, 12, 29};
    std::vector<detail::seek_doc> expected{
      {irs::doc_limits::invalid(), irs::doc_limits::invalid()},
      {7, 7},
      {11, 11},
      {irs::doc_limits::invalid(), 11},
      {45, 45},
      {57, irs::doc_limits::eof()}};
    irs::exclusion it(irs::memory::make_managed<detail::basic_doc_iterator>(
                        included.begin(), included.end()),
                      irs::memory::make_managed<detail::basic_doc_iterator>(
                        excluded.begin(), excluded.end()));
    ASSERT_EQ(included.size(), irs::cost::extract(it));

    for (const auto& target : expected) {
      ASSERT_EQ(target.expected, it.seek(target.target));
    }
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                Boolean test case
// ----------------------------------------------------------------------------

class boolean_filter_test_case : public FilterTestCaseBase {};

TEST_P(boolean_filter_test_case, or_sequential_multiple_segments) {
  // populate index
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);

    const tests::document* doc1 = gen.next();
    const tests::document* doc2 = gen.next();
    const tests::document* doc3 = gen.next();
    const tests::document* doc4 = gen.next();
    const tests::document* doc5 = gen.next();
    const tests::document* doc6 = gen.next();
    const tests::document* doc7 = gen.next();
    const tests::document* doc8 = gen.next();
    const tests::document* doc9 = gen.next();

    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));  // A
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));  // B
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));  // C
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end(),
                       doc4->stored.begin(), doc4->stored.end()));  // D
    writer->Commit();
    AssertSnapshotEquality(*writer);
    ASSERT_TRUE(insert(*writer, doc5->indexed.begin(), doc5->indexed.end(),
                       doc5->stored.begin(), doc5->stored.end()));  // E
    ASSERT_TRUE(insert(*writer, doc6->indexed.begin(), doc6->indexed.end(),
                       doc6->stored.begin(), doc6->stored.end()));  // F
    ASSERT_TRUE(insert(*writer, doc7->indexed.begin(), doc7->indexed.end(),
                       doc7->stored.begin(), doc7->stored.end()));  // G
    writer->Commit();
    AssertSnapshotEquality(*writer);
    ASSERT_TRUE(insert(*writer, doc8->indexed.begin(), doc8->indexed.end(),
                       doc8->stored.begin(), doc8->stored.end()));  // H
    ASSERT_TRUE(insert(*writer, doc9->indexed.begin(), doc9->indexed.end(),
                       doc9->stored.begin(), doc9->stored.end()));  // I
    writer->Commit();
    AssertSnapshotEquality(*writer);
  }

  auto rdr = open_reader();
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "B");
    append<irs::by_term>(root, "name", "F");
    append<irs::by_term>(root, "name", "I");

    auto prep = root.prepare({.index = rdr});
    auto segment = rdr.begin();
    {
      auto docs = prep->execute({.segment = *segment});
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(2, docs->value());
      ASSERT_FALSE(docs->next());
    }

    ++segment;
    {
      auto docs = prep->execute({.segment = *segment});
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(2, docs->value());
      ASSERT_FALSE(docs->next());
    }

    ++segment;
    {
      auto docs = prep->execute({.segment = *segment});
      ASSERT_TRUE(docs->next());
      ASSERT_EQ(2, docs->value());
      ASSERT_FALSE(docs->next());
    }
  }
}

TEST_P(boolean_filter_test_case, or_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  { CheckQuery(irs::Or(), Docs{}, rdr); }

  {
    irs::Or root;
    append<irs::by_term>(root, "name", "V");  // 22

    CheckQuery(root, Docs{22}, rdr);
  }

  // name=W OR name=Z
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "W");  // 23
    append<irs::by_term>(root, "name", "C");  // 3

    CheckQuery(root, Docs{3, 23}, rdr);
  }

  // name=A OR name=Q OR name=Z
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");  // 1
    append<irs::by_term>(root, "name", "Q");  // 17
    append<irs::by_term>(root, "name", "Z");  // 26

    CheckQuery(root, Docs{1, 17, 26}, rdr);
  }

  // name=A OR name=Q OR same!=xyz
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");  // 1
    append<irs::by_term>(root, "name", "Q");  // 17
    root.add<irs::Or>().add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("same",
                                "xyz");  // none (not within an OR must be
                                         // wrapped inside a single-branch OR)

    CheckQuery(root, Docs{1, 17}, rdr);
  }

  // (name=A OR name=Q) OR same!=xyz
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");  // 1
    append<irs::by_term>(root, "name", "Q");  // 17
    root.add<irs::Or>().add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("same",
                                "xyz");  // none (not within an OR must be
                                         // wrapped inside a single-branch OR)

    CheckQuery(root, Docs{1, 17}, rdr);
  }

  // name=A OR name=Q OR name=Z OR same=invalid_term OR invalid_field=V
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");  // 1
    append<irs::by_term>(root, "name", "Q");  // 17
    append<irs::by_term>(root, "name", "Z");  // 26
    append<irs::by_term>(root, "same", "invalid_term");
    append<irs::by_term>(root, "invalid_field", "V");

    CheckQuery(root, Docs{1, 17, 26}, rdr);
  }

  // search : all terms
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");    // 1
    append<irs::by_term>(root, "name", "Q");    // 17
    append<irs::by_term>(root, "name", "Z");    // 26
    append<irs::by_term>(root, "same", "xyz");  // 1..32
    append<irs::by_term>(root, "same", "invalid_term");

    CheckQuery(root, Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                          12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                          23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
               rdr);
  }

  // min match count == 0
  {
    irs::Or root;
    root.min_match_count(0);
    append<irs::by_term>(root, "name", "V");  // 22

    CheckQuery(root, Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                          12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                          23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
               rdr);
  }

  // min match count == 0
  {
    irs::Or root;
    root.min_match_count(0);

    CheckQuery(root, Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                          12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                          23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
               rdr);
  }

  // min match count is geater than a number of conditions
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");    // 1
    append<irs::by_term>(root, "name", "Q");    // 17
    append<irs::by_term>(root, "name", "Z");    // 26
    append<irs::by_term>(root, "same", "xyz");  // 1..32
    append<irs::by_term>(root, "same", "invalid_term");
    root.min_match_count(root.size() + 1);

    CheckQuery(root, Docs{}, rdr);
  }

  // name=A OR false
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");  // 1
    root.add<irs::Empty>();

    CheckQuery(root, Docs{1}, rdr);
  }

  // name!=A OR false
  {
    irs::Or root;
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "A");  // 1
    root.add<irs::Empty>();

    CheckQuery(
      root, Docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      rdr);
  }

  // Not with impossible name!=A OR same="NOT POSSIBLE"
  {
    irs::Or root;
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "A");  // 1
    append<irs::by_term>(root, "same", "NOT POSSIBLE");
    CheckQuery(
      root, Docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      rdr);
  }

  // optimization should adjust min_match
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    append<irs::by_term>(root, "duplicated", "abcd");
    root.min_match_count(5);
    CheckQuery(root, Docs{1}, rdr);
  }

  // optimization should adjust min_match same but with score to check scored
  // optimization
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    append<irs::by_term>(root, "duplicated", "abcd");
    root.min_match_count(5);
    irs::Scorer::ptr sort{std::make_unique<sort::custom_sort>()};
    CheckQuery(root, std::span{&sort, 1}, Docs{1}, rdr);
  }

  // optimization should adjust min_match
  // case where it should be dropped to 1
  // as optimized more filters than min_match
  // unscored
  {
    irs::Or root;
    append<irs::by_term>(root, "name", "A");
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    append<irs::by_term>(root, "duplicated", "abcd");
    root.min_match_count(3);
    CheckQuery(root, Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                          12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                          23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
               rdr);
  }

  // scored
  {
    irs::Or root;
    root.merge_type(irs::ScoreMergeType::kMax);
    append<irs::by_term>(root, "name", "A");
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    root.add<irs::all>();
    append<irs::by_term>(root, "duplicated", "abcd");
    root.min_match_count(3);
    irs::Scorer::ptr sort{std::make_unique<sort::custom_sort>()};
    auto& impl = static_cast<sort::custom_sort&>(*sort);
    impl.scorer_score = [](auto doc, auto* score) { *score = doc; };

    CheckQuery(
      root, std::span{&sort, 1},
      Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      rdr);
  }
}

TEST_P(boolean_filter_test_case, and_schemas) {
  // write segments
  {
    auto writer = open_writer(irs::OM_CREATE);

    std::vector<doc_generator_base::ptr> gens;

    gens.emplace_back(new tests::json_doc_generator(
      resource("AdventureWorks2014.json"), &tests::generic_json_field_factory));
    gens.emplace_back(
      new tests::json_doc_generator(resource("AdventureWorks2014Edges.json"),
                                    &tests::generic_json_field_factory));
    gens.emplace_back(new tests::json_doc_generator(
      resource("Northwnd.json"), &tests::generic_json_field_factory));
    gens.emplace_back(new tests::json_doc_generator(
      resource("NorthwndEdges.json"), &tests::generic_json_field_factory));

    add_segments(*writer, gens);
  }

  auto rdr = open_reader();

  // Name = Product AND source=AdventureWor3ks2014
  {
    irs::And root;
    append<irs::by_term>(root, "Name", "Product");
    append<irs::by_term>(root, "source", "AdventureWor3ks2014");
    CheckQuery(root, Docs{}, rdr);
  }
}

TEST_P(boolean_filter_test_case, and_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  { CheckQuery(irs::And(), Docs{}, rdr); }

  // name=V
  {
    irs::And root;
    append<irs::by_term>(root, "name", "V");  // 22

    CheckQuery(root, Docs{22}, rdr);
  }

  // duplicated=abcd AND same=xyz
  {
    irs::And root;
    append<irs::by_term>(root, "duplicated", "abcd");  // 1,5,11,21,27,31
    append<irs::by_term>(root, "same", "xyz");         // 1..32
    CheckQuery(root, Docs{1, 5, 11, 21, 27, 31}, rdr);
  }

  // duplicated=abcd AND same=xyz AND name=A
  {
    irs::And root;
    append<irs::by_term>(root, "duplicated", "abcd");  // 1,5,11,21,27,31
    append<irs::by_term>(root, "same", "xyz");         // 1..32
    append<irs::by_term>(root, "name", "A");           // 1
    CheckQuery(root, Docs{1}, rdr);
  }

  // duplicated=abcd AND same=xyz AND name=B
  {
    irs::And root;
    append<irs::by_term>(root, "duplicated", "abcd");  // 1,5,11,21,27,31
    append<irs::by_term>(root, "same", "xyz");         // 1..32
    append<irs::by_term>(root, "name", "B");           // 2
    CheckQuery(root, Docs{}, rdr);
  }
}

TEST_P(boolean_filter_test_case, not_standalone_sequential_ordered) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // reverse order
  {
    const std::string column_name = "duplicated";

    std::vector<irs::doc_id_t> expected = {32, 30, 29, 28, 26, 25, 24, 23, 22,
                                           20, 19, 18, 17, 16, 15, 14, 13, 12,
                                           10, 9,  8,  7,  6,  4,  3,  2};

    irs::Not not_node;
    not_node.filter<irs::by_term>() =
      make_filter<irs::by_term>(column_name, "abcd");

    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;

    sort::custom_sort sort;

    sort.collector_collect_field = [&collector_collect_field_count](
                                     const irs::SubReader&,
                                     const irs::term_reader&) -> void {
      ++collector_collect_field_count;
    };
    sort.collector_collect_term = [&collector_collect_term_count](
                                    const irs::SubReader&,
                                    const irs::term_reader&,
                                    const irs::attribute_provider&) -> void {
      ++collector_collect_term_count;
    };
    sort.collectors_collect_ = [&collector_finish_count](
                                 irs::byte_type*, const irs::FieldCollector*,
                                 const irs::TermCollector*) -> void {
      ++collector_finish_count;
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t doc,
                                              irs::score_t* score) {
      ++scorer_score_count;
      *score = doc;
    };

    auto prepared_order = irs::Scorers::Prepare(sort);
    auto prepared_filter =
      not_node.prepare({.index = *rdr, .scorers = prepared_order});
    std::multimap<irs::score_t, irs::doc_id_t, std::greater<>> scored_result;

    ASSERT_EQ(1, rdr->size());
    auto& segment = (*rdr)[0];

    auto filter_itr =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    ASSERT_EQ(32, irs::cost::extract(*filter_itr));

    size_t docs_count = 0;
    auto* score = irs::get<irs::score>(*filter_itr);

    while (filter_itr->next()) {
      ASSERT_FALSE(!score);
      irs::score_t score_value;
      score->operator()(&score_value);
      scored_result.emplace(score_value, filter_itr->value());
      ++docs_count;
    }

    ASSERT_EQ(expected.size(), docs_count);

    ASSERT_EQ(
      0, collector_collect_field_count);  // should not be executed (a negated
                                          // possibly complex filter)
    ASSERT_EQ(0, collector_collect_term_count);  // should not be executed
    ASSERT_EQ(1, collector_finish_count);        // from "all" query
    ASSERT_EQ(expected.size(), scorer_score_count);

    std::vector<irs::doc_id_t> actual;

    for (auto& entry : scored_result) {
      actual.emplace_back(entry.second);
    }

    ASSERT_EQ(expected, actual);
  }
}

TEST_P(boolean_filter_test_case, not_sequential_ordered) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // reverse order
  {
    const std::string column_name = "duplicated";

    std::vector<irs::doc_id_t> expected = {32, 30, 29, 28, 26, 25, 24, 23, 22,
                                           20, 19, 18, 17, 16, 15, 14, 13, 12,
                                           10, 9,  8,  7,  6,  4,  3,  2};

    irs::And root;
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>(column_name, "abcd");

    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;

    sort::custom_sort sort;

    sort.collector_collect_field = [&collector_collect_field_count](
                                     const irs::SubReader&,
                                     const irs::term_reader&) -> void {
      ++collector_collect_field_count;
    };
    sort.collector_collect_term = [&collector_collect_term_count](
                                    const irs::SubReader&,
                                    const irs::term_reader&,
                                    const irs::attribute_provider&) -> void {
      ++collector_collect_term_count;
    };
    sort.collectors_collect_ = [&collector_finish_count](
                                 irs::byte_type*, const irs::FieldCollector*,
                                 const irs::TermCollector*) -> void {
      ++collector_finish_count;
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t doc,
                                              irs::score_t* score) -> void {
      ++scorer_score_count;
      *score = doc;
    };

    auto prepared_order = irs::Scorers::Prepare(sort);
    auto prepared_filter =
      root.prepare({.index = *rdr, .scorers = prepared_order});
    std::multimap<irs::score_t, irs::doc_id_t, std::greater<>> scored_result;

    ASSERT_EQ(1, rdr->size());
    auto& segment = (*rdr)[0];

    auto filter_itr =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    ASSERT_EQ(32, irs::cost::extract(*filter_itr));

    size_t docs_count = 0;
    auto* score = irs::get<irs::score>(*filter_itr);

    while (filter_itr->next()) {
      ASSERT_FALSE(!score);
      irs::score_t score_value;
      score->operator()(&score_value);
      scored_result.emplace(score_value, filter_itr->value());
      ++docs_count;
    }

    ASSERT_EQ(expected.size(), docs_count);

    ASSERT_EQ(
      0, collector_collect_field_count);  // should not be executed (a negated
                                          // possibly complex filter)
    ASSERT_EQ(0, collector_collect_term_count);  // should not be executed
    ASSERT_EQ(1, collector_finish_count);        // from "all" query
    ASSERT_EQ(expected.size(), scorer_score_count);

    std::vector<irs::doc_id_t> actual;

    for (auto& entry : scored_result) {
      actual.emplace_back(entry.second);
    }

    ASSERT_EQ(expected, actual);
  }
}

TEST_P(boolean_filter_test_case, not_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  { CheckQuery(irs::Not(), Docs{}, rdr); }

  // single not statement - empty result
  {
    irs::Not root;
    root.filter<irs::by_term>() = make_filter<irs::by_term>("same", "xyz");

    CheckQuery(root, Docs{}, rdr);
  }

  // duplicated=abcd AND (NOT ( NOT name=A ))
  {
    irs::And root;
    root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
    root.add<irs::Not>().filter<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "A");
    CheckQuery(root, Docs{1}, rdr);
  }

  // duplicated=abcd AND (NOT ( NOT (NOT (NOT ( NOT name=A )))))
  {
    irs::And root;
    root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
    root.add<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::by_term>() = make_filter<irs::by_term>("name", "A");
    CheckQuery(root, Docs{5, 11, 21, 27, 31}, rdr);
  }

  // * AND NOT *
  {{irs::And root;
  root.add<irs::all>();
  root.add<irs::Not>().filter<irs::all>();
  CheckQuery(root, Docs{}, rdr);
}

{
  irs::Or root;
  root.add<irs::all>();
  root.add<irs::Not>().filter<irs::all>();
  CheckQuery(root, Docs{}, rdr);
}
}  // namespace tests

// duplicated=abcd AND NOT name=A
{{irs::And root;
root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
root.add<irs::Not>().filter<irs::by_term>() = make_filter<irs::by_term>("name",
                                                                        "A");
CheckQuery(root, Docs{5, 11, 21, 27, 31}, rdr);
}

{
  irs::Or root;
  root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
  root.add<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("name", "A");
  CheckQuery(
    root, Docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
               18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
    rdr);
}
// check 'all' filter added for Not nodes does not affects score
{
  irs::Or root;
  auto& left_branch = root.add<irs::And>();
  // this three filters fire at same doc so it will get score = 3
  append<irs::by_term>(left_branch, "name", "A");
  append<irs::by_term>(left_branch, "duplicated", "abcd");
  append<irs::by_term>(left_branch, "same", "xyz");

  auto& right_branch = root.add<irs::And>();
  append<irs::by_term>(right_branch, "name", "B");  // +1 score
  auto& sub = right_branch.add<irs::Or>();          // this OR we actually test
  append<irs::by_term>(sub, "name", "B");           // +1 score
  // will exclude some docs (but A will stay) and produce 'all'
  sub.add<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("prefix", "abcde");
  // will exclude some docs (but A will stay) and produce another 'all'
  sub.add<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("duplicated", "abcd");
  // if 'all' will add at least 1 to score totals score will be 3 and expected
  // order will break
  irs::Scorer::ptr sort{std::make_unique<tests::sort::boost>()};
  CheckQuery(root, std::span{&sort, 1}, Docs{2, 1}, rdr);
}
}

// duplicated=abcd AND NOT name=A AND NOT name=A
{{irs::And root;
root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
root.add<irs::Not>().filter<irs::by_term>() = make_filter<irs::by_term>("name",
                                                                        "A");
root.add<irs::Not>().filter<irs::by_term>() = make_filter<irs::by_term>("name",
                                                                        "A");
CheckQuery(root, Docs{5, 11, 21, 27, 31}, rdr);
}

{
  irs::Or root;
  root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
  root.add<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("name", "A");
  root.add<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("name", "A");
  CheckQuery(
    root, Docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
               18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
    rdr);
}
}

// duplicated=abcd AND NOT name=A AND NOT name=E
{
  {
    irs::And root;
    root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "A");
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "E");
    CheckQuery(root, Docs{11, 21, 27, 31}, rdr);
  }

  {
    irs::Or root;
    root.add<irs::by_term>() = make_filter<irs::by_term>("duplicated", "abcd");
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "A");
    root.add<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("prefix", "abcd");
    CheckQuery(
      root, Docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      rdr);
  }
}
}

TEST_P(boolean_filter_test_case, not_standalone_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  { CheckQuery(irs::Not(), Docs{}, rdr); }

  // single not statement - empty result
  {
    irs::Not not_node;
    not_node.filter<irs::by_term>() = make_filter<irs::by_term>("same", "xyz"),

    CheckQuery(not_node, Docs{}, rdr);
  }

  // single not statement - all docs
  {
    irs::Not not_node;
    not_node.filter<irs::by_term>() =
      make_filter<irs::by_term>("same", "invalid_term"),

    CheckQuery(not_node, Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                              12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                              23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
               rdr);
  }

  // (NOT (NOT name=A))
  {
    irs::Not not_node;
    not_node.filter<irs::Not>().filter<irs::by_term>() =
      make_filter<irs::by_term>("name", "A");
    CheckQuery(not_node, Docs{1}, rdr);
  }

  // (NOT (NOT (NOT (NOT (NOT name=A)))))
  {
    irs::Not not_node;
    not_node.filter<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::Not>()
      .filter<irs::by_term>() = make_filter<irs::by_term>("name", "A");

    CheckQuery(not_node, Docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                              13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                              24, 25, 26, 27, 28, 29, 30, 31, 32},
               rdr);
  }
}

TEST_P(boolean_filter_test_case, mixed) {
  {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // (same=xyz AND duplicated=abcd) OR (same=xyz AND duplicated=vczc)
    {
      irs::Or root;

      // same=xyz AND duplicated=abcd
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");
        append<irs::by_term>(child, "duplicated", "abcd");
      }

      // same=xyz AND duplicated=vczc
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");
        append<irs::by_term>(child, "duplicated", "vczc");
      }

      CheckQuery(root, Docs{1, 2, 3, 5, 8, 11, 14, 17, 19, 21, 24, 27, 31},
                 rdr);
    }

    // ((same=xyz AND duplicated=abcd) OR (same=xyz AND duplicated=vczc)) AND
    // name=X
    {
      irs::And root;
      append<irs::by_term>(root, "name", "X");

      // ( same = xyz AND duplicated = abcd ) OR( same = xyz AND duplicated =
      // vczc )
      {
        irs::Or& child = root.add<irs::Or>();

        // same=xyz AND duplicated=abcd
        {
          irs::And& subchild = child.add<irs::And>();
          append<irs::by_term>(subchild, "same", "xyz");
          append<irs::by_term>(subchild, "duplicated", "abcd");
        }

        // same=xyz AND duplicated=vczc
        {
          irs::And& subchild = child.add<irs::And>();
          append<irs::by_term>(subchild, "same", "xyz");
          append<irs::by_term>(subchild, "duplicated", "vczc");
        }
      }

      CheckQuery(root, Docs{24}, rdr);
    }

    // ((same=xyz AND duplicated=abcd) OR (name=A or name=C or NAME=P or name=U
    // or name=X)) OR (same=xyz AND (duplicated=vczc OR (name=A OR name=C OR
    // NAME=P OR name=U OR name=X)) ) 1, 2, 3, 4, 5, 8, 11, 14, 16, 17, 19, 21,
    // 24, 27, 31
    {
      irs::Or root;

      // (same=xyz AND duplicated=abcd) OR (name=A or name=C or NAME=P or name=U
      // or name=X) 1, 3, 5,11, 16, 21, 24, 27, 31
      {
        irs::Or& child = root.add<irs::Or>();

        // ( same = xyz AND duplicated = abcd )
        {
          irs::And& subchild = root.add<irs::And>();
          append<irs::by_term>(subchild, "same", "xyz");
          append<irs::by_term>(subchild, "duplicated", "abcd");
        }

        append<irs::by_term>(child, "name", "A");
        append<irs::by_term>(child, "name", "C");
        append<irs::by_term>(child, "name", "P");
        append<irs::by_term>(child, "name", "X");
      }

      // (same=xyz AND (duplicated=vczc OR (name=A OR name=C OR NAME=P OR name=U
      // OR name=X)) 1, 2, 3, 8, 14, 16, 17, 19, 21, 24
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");

        // (duplicated=vczc OR (name=A OR name=C OR NAME=P OR name=U OR name=X)
        {
          irs::Or& subchild = child.add<irs::Or>();
          append<irs::by_term>(subchild, "duplicated", "vczc");

          // name=A OR name=C OR NAME=P OR name=U OR name=X
          {
            irs::Or& subsubchild = subchild.add<irs::Or>();
            append<irs::by_term>(subsubchild, "name", "A");
            append<irs::by_term>(subsubchild, "name", "C");
            append<irs::by_term>(subsubchild, "name", "P");
            append<irs::by_term>(subsubchild, "name", "X");
          }
        }
      }

      CheckQuery(root, Docs{1, 2, 3, 5, 8, 11, 14, 16, 17, 19, 21, 24, 27, 31},
                 rdr);
    }

    // (same=xyz AND duplicated=abcd) OR (same=xyz AND duplicated=vczc) AND *
    {
      irs::Or root;

      // *
      root.add<irs::all>();

      // same=xyz AND duplicated=abcd
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");
        append<irs::by_term>(child, "duplicated", "abcd");
      }

      // same=xyz AND duplicated=vczc
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");
        append<irs::by_term>(child, "duplicated", "vczc");
      }

      CheckQuery(root, Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                            12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                            23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
                 rdr);
    }

    // (same=xyz AND duplicated=abcd) OR (same=xyz AND duplicated=vczc) OR NOT *
    {
      irs::Or root;

      // NOT *
      root.add<irs::Not>().filter<irs::all>();

      // same=xyz AND duplicated=abcd
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");
        append<irs::by_term>(child, "duplicated", "abcd");
      }

      // same=xyz AND duplicated=vczc
      {
        irs::And& child = root.add<irs::And>();
        append<irs::by_term>(child, "same", "xyz");
        append<irs::by_term>(child, "duplicated", "vczc");
      }

      CheckQuery(root, Docs{}, rdr);
    }
  }
}

TEST_P(boolean_filter_test_case, mixed_ordered) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_TRUE(bool(rdr));

  {
    irs::Or root;
    auto& sub = root.add<irs::And>();
    {
      auto& filter = sub.add<irs::by_range>();
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("!"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = sub.add<irs::by_range>();
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("~"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    std::array<irs::Scorer::ptr, 2> ord{std::make_unique<irs::TFIDF>(),
                                        std::make_unique<irs::BM25>()};

    auto prepared_ord = irs::Scorers::Prepare(ord);
    ASSERT_FALSE(prepared_ord.empty());
    ASSERT_EQ(2, prepared_ord.buckets().size());

    auto prepared = root.prepare({.index = *rdr, .scorers = prepared_ord});
    ASSERT_NE(nullptr, prepared);

    std::vector<irs::doc_id_t> expected_docs{
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 29, 30, 31, 32};

    auto expected_doc = expected_docs.begin();
    for (const auto& sub : rdr) {
      auto docs = prepared->execute({.segment = sub, .scorers = prepared_ord});

      auto* doc = irs::get<irs::document>(*docs);
      ASSERT_TRUE(
        bool(doc));  // ensure all iterators contain "document" attribute

      const auto* score = irs::get<irs::score>(*docs);
      ASSERT_NE(nullptr, score);

      std::vector<irs::bstring> scores;
      while (docs->next()) {
        EXPECT_EQ(*expected_doc, doc->value);
        ++expected_doc;

        irs::bstring score_value(prepared_ord.score_size(), 0);
        score->operator()(reinterpret_cast<irs::score_t*>(score_value.data()));
        scores.emplace_back(std::move(score_value));
      }

      ASSERT_EQ(expected_docs.end(), expected_doc);
      ASSERT_TRUE(irs::irstd::all_equal(scores.begin(), scores.end()));
    }
  }
}

TEST(Not_test, ctor) {
  irs::Not q;
  ASSERT_EQ(irs::type<irs::Not>::id(), q.type());
  ASSERT_EQ(nullptr, q.filter());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(Not_test, equal) {
  {
    irs::Not lhs, rhs;
    ASSERT_EQ(lhs, rhs);
  }

  {
    irs::Not lhs;
    lhs.filter<irs::by_term>() = make_filter<irs::by_term>("abc", "def");

    irs::Not rhs;
    rhs.filter<irs::by_term>() = make_filter<irs::by_term>("abc", "def");
    ASSERT_EQ(lhs, rhs);
  }

  {
    irs::Not lhs;
    lhs.filter<irs::by_term>() = make_filter<irs::by_term>("abc", "def");

    irs::Not rhs;
    rhs.filter<irs::by_term>() = make_filter<irs::by_term>("abcd", "def");
    ASSERT_NE(lhs, rhs);
  }
}

TEST(And_test, ctor) {
  irs::And q;
  ASSERT_EQ(irs::type<irs::And>::id(), q.type());
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(And_test, add_clear) {
  irs::And q;
  q.add<irs::by_term>();
  q.add<irs::by_term>();
  ASSERT_FALSE(q.empty());
  ASSERT_EQ(2, q.size());
  q.clear();
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
}

TEST(And_test, equal) {
  irs::And lhs;
  append<irs::by_term>(lhs, "field", "term");
  append<irs::by_term>(lhs, "field1", "term1");
  {
    irs::And& subq = lhs.add<irs::And>();
    append<irs::by_term>(subq, "field123", "dfterm");
    append<irs::by_term>(subq, "fieasfdld1", "term1");
  }

  {
    irs::And rhs;
    append<irs::by_term>(rhs, "field", "term");
    append<irs::by_term>(rhs, "field1", "term1");
    {
      irs::And& subq = rhs.add<irs::And>();
      append<irs::by_term>(subq, "field123", "dfterm");
      append<irs::by_term>(subq, "fieasfdld1", "term1");
    }

    ASSERT_EQ(lhs, rhs);
  }

  {
    irs::And rhs;
    append<irs::by_term>(rhs, "field", "term");
    append<irs::by_term>(rhs, "field1", "term1");
    {
      irs::And& subq = rhs.add<irs::And>();
      append<irs::by_term>(subq, "field123", "dfterm");
      append<irs::by_term>(subq, "fieasfdld1", "term1");
      append<irs::by_term>(subq, "fieasfdld1", "term1");
    }

    ASSERT_NE(lhs, rhs);
  }
}

TEST(And_test, optimize_double_negation) {
  irs::And root;
  root.add<irs::Not>().filter<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("test_field", "test_term");

  auto prepared = root.prepare({.index = irs::SubReader::empty()});
  ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
}

TEST(And_test, prepare_empty_filter) {
  irs::And root;
  auto prepared = root.prepare({.index = irs::SubReader::empty()});
  ASSERT_NE(nullptr, prepared);
  ASSERT_EQ(typeid(irs::filter::prepared::empty().get()),
            typeid(prepared.get()));
}

TEST(And_test, optimize_single_node) {
  // simple hierarchy
  {
    irs::And root;
    append<irs::by_term>(root, "test_field", "test_term");

    auto prepared = root.prepare({.index = irs::SubReader::empty()});
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
  }

  // complex hierarchy
  {
    irs::And root;
    root.add<irs::And>().add<irs::And>().add<irs::by_term>() =
      make_filter<irs::by_term>("test_field", "test_term");

    auto prepared = root.prepare({.index = irs::SubReader::empty()});
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
  }
}

TEST(And_test, optimize_all_filters) {
  // single `all` filter
  {
    irs::And root;
    root.add<irs::all>().boost(5.f);

    auto prepared = root.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(
      typeid(irs::all().prepare({.index = irs::SubReader::empty()}).get()),
      typeid(prepared.get()));
    ASSERT_EQ(5.f, prepared->boost());
  }

  // multiple `all` filters
  {
    irs::And root;
    root.add<irs::all>().boost(5.f);
    root.add<irs::all>().boost(2.f);
    root.add<irs::all>().boost(3.f);

    auto prepared = root.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(
      typeid(irs::all().prepare({.index = irs::SubReader::empty()}).get()),
      typeid(prepared.get()));
    ASSERT_EQ(10.f, prepared->boost());
  }

  // multiple `all` filters + term filter
  {
    irs::And root;
    root.add<irs::all>().boost(5.f);
    root.add<irs::all>().boost(2.f);
    append<irs::by_term>(root, "test_field", "test_term");

    tests::sort::boost sort{};
    auto pord = irs::Scorers::Prepare(sort);
    auto prepared =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
    ASSERT_EQ(8.f, prepared->boost());
  }

  // `all` filter + term filter
  {
    tests::sort::boost sort{};
    irs::And root;
    append<irs::by_term>(root, "test_field", "test_term");
    root.add<irs::all>().boost(5.f);
    auto pord = irs::Scorers::Prepare(sort);
    auto prepared =
      root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
    ASSERT_EQ(6.f, prepared->boost());
  }
}

TEST(And_test, not_boosted) {
  tests::sort::boost sort{};
  auto pord = irs::Scorers::Prepare(sort);
  irs::And root;
  {
    auto& neg = root.add<irs::Not>();
    auto& node = neg.filter<detail::boosted>();
    node.docs = {5, 6};
    node.boost(4);
  }
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {1};
    node.boost(5);
  }
  auto prep = root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
  auto docs =
    prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
  auto* scr = irs::get<irs::score>(*docs);
  ASSERT_FALSE(!scr);
  auto* doc = irs::get<irs::document>(*docs);

  ASSERT_TRUE(docs->next());
  irs::score_t doc_boost;
  scr->operator()(&doc_boost);
  ASSERT_EQ(5., doc_boost);  // FIXME: should be 9 if we will boost negation
  ASSERT_EQ(1, doc->value);

  ASSERT_FALSE(docs->next());
}

TEST(Or_test, ctor) {
  irs::Or q;
  ASSERT_EQ(irs::type<irs::Or>::id(), q.type());
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
  ASSERT_EQ(1, q.min_match_count());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(Or_test, add_clear) {
  irs::Or q;
  q.add<irs::by_term>();
  q.add<irs::by_term>();
  ASSERT_FALSE(q.empty());
  ASSERT_EQ(2, q.size());
  q.clear();
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
}

TEST(Or_test, equal) {
  irs::Or lhs;
  append<irs::by_term>(lhs, "field", "term");
  append<irs::by_term>(lhs, "field1", "term1");
  {
    irs::And& subq = lhs.add<irs::And>();
    append<irs::by_term>(subq, "field123", "dfterm");
    append<irs::by_term>(subq, "fieasfdld1", "term1");
  }

  {
    irs::Or rhs;
    append<irs::by_term>(rhs, "field", "term");
    append<irs::by_term>(rhs, "field1", "term1");
    {
      irs::And& subq = rhs.add<irs::And>();
      append<irs::by_term>(subq, "field123", "dfterm");
      append<irs::by_term>(subq, "fieasfdld1", "term1");
    }

    ASSERT_EQ(lhs, rhs);
  }

  {
    irs::Or rhs;
    append<irs::by_term>(rhs, "field", "term");
    append<irs::by_term>(rhs, "field1", "term1");
    {
      irs::And& subq = rhs.add<irs::And>();
      append<irs::by_term>(subq, "field123", "dfterm");
      append<irs::by_term>(subq, "fieasfdld1", "term1");
      append<irs::by_term>(subq, "fieasfdld1", "term1");
    }

    ASSERT_NE(lhs, rhs);
  }
}

TEST(Or_test, optimize_double_negation) {
  irs::Or root;
  root.add<irs::Not>().filter<irs::Not>().filter<irs::by_term>() =
    make_filter<irs::by_term>("test_field", "test_term");

  auto prepared = root.prepare({.index = irs::SubReader::empty()});
  ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
}

TEST(Or_test, optimize_single_node) {
  // simple hierarchy
  {
    irs::Or root;
    append<irs::by_term>(root, "test_field", "test_term");

    auto prepared = root.prepare({.index = irs::SubReader::empty()});
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
  }

  // complex hierarchy
  {
    irs::Or root;
    root.add<irs::Or>().add<irs::Or>().add<irs::by_term>() =
      make_filter<irs::by_term>("test_field", "test_term");

    auto prepared = root.prepare({.index = irs::SubReader::empty()});
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
  }
}

TEST(Or_test, optimize_all_unscored) {
  irs::Or root;
  detail::boosted::execute_count = 0;
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {1};
  }
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {2};
  }
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {3};
  }
  root.add<irs::all>();
  root.add<irs::Empty>();
  root.add<irs::all>();
  root.add<irs::Empty>();

  auto prep = root.prepare({.index = irs::SubReader::empty()});

  prep->execute({.segment = irs::SubReader::empty()});
  ASSERT_EQ(
    0, detail::boosted::execute_count);  // specific filters should be opt out
}

TEST(Or_test, optimize_all_scored) {
  irs::Or root;
  detail::boosted::execute_count = 0;
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {1};
  }
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {2};
  }
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {3};
  }
  root.add<irs::all>();
  root.add<irs::Empty>();
  root.add<irs::all>();
  root.add<irs::Empty>();
  tests::sort::boost sort{};
  auto pord = irs::Scorers::Prepare(sort);
  auto prep = root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

  prep->execute({.segment = irs::SubReader::empty()});
  ASSERT_EQ(3, detail::boosted::execute_count);  // specific filters should
                                                 // executed as score needs them
}

TEST(Or_test, optimize_only_all_boosted) {
  tests::sort::boost sort{};
  auto pord = irs::Scorers::Prepare(sort);
  irs::Or root;
  root.boost(2);
  root.add<irs::all>().boost(3);
  root.add<irs::all>().boost(5);

  auto prep = root.prepare({.index = irs::SubReader::empty(), .scorers = pord});

  prep->execute({.segment = irs::SubReader::empty()});
  ASSERT_EQ(16, prep->boost());
}

TEST(Or_test, boosted_not) {
  tests::sort::boost sort{};
  auto pord = irs::Scorers::Prepare(sort);
  irs::Or root;
  {
    auto& neg = root.add<irs::Not>();
    auto& node = neg.filter<detail::boosted>();
    node.docs = {5, 6};
    node.boost(4);
  }
  {
    auto& node = root.add<detail::boosted>();
    node.docs = {1};
    node.boost(5);
  }
  auto prep = root.prepare({.index = irs::SubReader::empty(), .scorers = pord});
  auto docs =
    prep->execute({.segment = irs::SubReader::empty(), .scorers = pord});
  auto* scr = irs::get<irs::score>(*docs);
  ASSERT_FALSE(!scr);
  auto* doc = irs::get<irs::document>(*docs);

  ASSERT_TRUE(docs->next());
  irs::score_t doc_boost;
  scr->operator()(&doc_boost);
  ASSERT_EQ(5., doc_boost);  // FIXME: should be 9 if we will boost negation
  ASSERT_EQ(1, doc->value);
  ASSERT_FALSE(docs->next());
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(boolean_filter_test, boolean_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         boolean_filter_test_case::to_string);

}  // tests
