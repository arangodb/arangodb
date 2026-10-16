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

#include "filter_test_case_base.hpp"

#include <compare>

namespace tests {

void FilterTestCaseBase::GetQueryResult(const irs::filter::prepared::ptr& q,
                                        const irs::IndexReader& rdr,
                                        Docs& result, Costs& result_costs,
                                        std::string_view source_location) {
  SCOPED_TRACE(source_location);
  result_costs.reserve(rdr.size());

  for (const auto& sub : rdr) {
    auto random_docs = q->execute({.segment = sub});
    ASSERT_NE(nullptr, random_docs);
    auto sequential_docs = q->execute({.segment = sub});
    ASSERT_NE(nullptr, sequential_docs);

    auto* doc = irs::get<irs::document>(*sequential_docs);
    ASSERT_NE(nullptr, doc);

    result_costs.emplace_back(irs::cost::extract(*sequential_docs));

    while (sequential_docs->next()) {
      auto stateless_random_docs = q->execute({.segment = sub});
      ASSERT_NE(nullptr, stateless_random_docs);
      ASSERT_EQ(sequential_docs->value(), doc->value);
      ASSERT_EQ(doc->value, random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, random_docs->value());
      ASSERT_EQ(doc->value, random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, random_docs->value());
      ASSERT_EQ(doc->value, stateless_random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, stateless_random_docs->value());
      ASSERT_EQ(doc->value, stateless_random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, stateless_random_docs->value());

      result.push_back(sequential_docs->value());
    }
    ASSERT_FALSE(sequential_docs->next());
    ASSERT_FALSE(random_docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(sequential_docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));

    // seek to eof
    ASSERT_TRUE(irs::doc_limits::eof(
      q->execute({.segment = sub})->seek(irs::doc_limits::eof())));
  }
}

void FilterTestCaseBase::GetQueryResult(const irs::filter::prepared::ptr& q,
                                        const irs::IndexReader& rdr,
                                        const irs::Scorers& ord,
                                        ScoredDocs& result, Costs& result_costs,
                                        std::string_view source_location) {
  SCOPED_TRACE(source_location);
  result_costs.reserve(rdr.size());

  for (const auto& sub : rdr) {
    auto random_docs = q->execute({.segment = sub, .scorers = ord});
    ASSERT_NE(nullptr, random_docs);
    auto sequential_docs = q->execute({.segment = sub, .scorers = ord});
    ASSERT_NE(nullptr, sequential_docs);

    auto* doc = irs::get<irs::document>(*sequential_docs);
    ASSERT_NE(nullptr, doc);

    auto* score = irs::get<irs::score>(*sequential_docs);

    result_costs.emplace_back(irs::cost::extract(*sequential_docs));

    auto assert_equal_scores = [&ord](const std::vector<irs::score_t>& lhs,
                                      irs::doc_iterator& rhs) {
      auto* score = irs::get<irs::score>(rhs);
      std::vector<irs::score_t> tmp(ord.buckets().size());
      if (score) {
        (*score)(tmp.data());
      }
      ASSERT_EQ(lhs, tmp);
    };

    while (sequential_docs->next()) {
      auto stateless_random_docs = q->execute({.segment = sub, .scorers = ord});
      ASSERT_NE(nullptr, stateless_random_docs);
      ASSERT_EQ(sequential_docs->value(), doc->value);
      ASSERT_EQ(doc->value, random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, random_docs->value());
      ASSERT_EQ(doc->value, stateless_random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, stateless_random_docs->seek(doc->value));
      ASSERT_EQ(doc->value, stateless_random_docs->value());

      std::vector<irs::score_t> score_value(ord.buckets().size());
      if (score) {
        (*score)(score_value.data());
      }

      assert_equal_scores(score_value, *stateless_random_docs);
      assert_equal_scores(score_value, *random_docs);

      result.emplace_back(sequential_docs->value(), std::move(score_value));
    }
    ASSERT_FALSE(sequential_docs->next());
    ASSERT_FALSE(random_docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(sequential_docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));

    // seek to eof
    ASSERT_TRUE(irs::doc_limits::eof(
      q->execute({.segment = sub})->seek(irs::doc_limits::eof())));
  }
}

void FilterTestCaseBase::CheckQuery(const irs::filter& filter,
                                    const Docs& expected,
                                    const Costs& expected_costs,
                                    const irs::IndexReader& index,
                                    std::string_view source_location) {
  SCOPED_TRACE(source_location);
  Docs result;
  Costs result_costs;
  GetQueryResult(filter.prepare({.index = index}), index, result, result_costs,
                 source_location);
  ASSERT_EQ(expected, result);
  ASSERT_EQ(expected_costs, result_costs);
}

void FilterTestCaseBase::CheckQuery(const irs::filter& filter,
                                    std::span<const irs::Scorer::ptr> order,
                                    const std::vector<Tests>& tests,
                                    const irs::IndexReader& rdr,
                                    std::string_view source_location) {
  SCOPED_TRACE(source_location);
  auto ord = irs::Scorers::Prepare(order);
  auto q = filter.prepare({.index = rdr, .scorers = ord});
  ASSERT_NE(nullptr, q);

  auto assert_equal_scores = [&](const std::vector<irs::score_t>& expected,
                                 irs::doc_iterator& rhs) {
    auto* score = irs::get<irs::score>(rhs);

    if (expected.empty()) {
      ASSERT_TRUE(nullptr == score || score->Ctx() == nullptr);
    } else {
      ASSERT_NE(nullptr, score);
      ASSERT_FALSE(score->Ctx() == nullptr);

      std::vector<irs::score_t> actual(ord.buckets().size());
      if (score) {
        (*score)(actual.data());
      }
      ASSERT_EQ(expected, actual);
    }
  };

  auto assert_iterator = [&](auto& test, irs::doc_iterator& it) {
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    std::visit(
      [&it, expected = test.expected]<typename A>(A action) {
        if constexpr (std::is_same_v<A, Seek>) {
          ASSERT_EQ(expected, it.seek(action.target));
        } else if constexpr (std::is_same_v<A, Next>) {
          ASSERT_EQ(!irs::doc_limits::eof(expected), it.next());
        } else if constexpr (std::is_same_v<A, Skip>) {
          for (auto count = action.count; count; --count) {
            it.next();
          }
        }
      },
      test.action);
    ASSERT_EQ(test.expected, it.value());
    ASSERT_EQ(test.expected, doc->value);
    if (!irs::doc_limits::eof(test.expected)) {
      assert_equal_scores(test.score, it);
    }
  };

  auto test = std::begin(tests);
  for (const auto& sub : rdr) {
    ASSERT_NE(test, std::end(tests));
    auto random_docs = q->execute({.segment = sub, .scorers = ord});
    ASSERT_NE(nullptr, random_docs);

    for (auto& test : *test) {
      assert_iterator(test, *random_docs);
    }

    ++test;
  }
}

void FilterTestCaseBase::CheckQuery(const irs::filter& filter,
                                    std::span<const irs::Scorer::ptr> order,
                                    const ScoredDocs& expected,
                                    const irs::IndexReader& index,
                                    std::string_view source_location) {
  SCOPED_TRACE(source_location);
  ScoredDocs result;
  Costs result_costs;
  auto prepared = irs::Scorers::Prepare(order);
  GetQueryResult(filter.prepare({.index = index, .scorers = prepared}), index,
                 prepared, result, result_costs, source_location);
  ASSERT_EQ(expected, result);
}

void FilterTestCaseBase::CheckQuery(const irs::filter& filter,
                                    const Docs& expected,
                                    const irs::IndexReader& index,
                                    std::string_view source_location) {
  SCOPED_TRACE(source_location);
  Docs result;
  Costs result_costs;
  GetQueryResult(filter.prepare({.index = index}), index, result, result_costs,
                 source_location);
  ASSERT_EQ(expected, result);
}

void FilterTestCaseBase::MakeResult(const irs::filter& filter,
                                    std::span<const irs::Scorer::ptr> order,
                                    const irs::IndexReader& rdr,
                                    std::vector<irs::doc_id_t>& result,
                                    bool score_must_be_present, bool reverse) {
  auto prepared_order = irs::Scorers::Prepare(order);
  auto prepared_filter =
    filter.prepare({.index = rdr, .scorers = prepared_order});
  auto score_less =
    [reverse, size = prepared_order.buckets().size()](
      const std::pair<irs::bstring, irs::doc_id_t>& lhs,
      const std::pair<irs::bstring, irs::doc_id_t>& rhs) -> bool {
    const auto& [lhs_buf, lhs_doc] = lhs;
    const auto& [rhs_buf, rhs_doc] = rhs;

    const auto* lhs_score = reinterpret_cast<const float*>(lhs_buf.c_str());
    const auto* rhs_score = reinterpret_cast<const float*>(rhs_buf.c_str());

    for (size_t i = 0; i < size; ++i) {
      const auto r = (lhs_score[i] <=> rhs_score[i]);

      if (r < 0) {
        return !reverse;
      }

      if (r > 0) {
        return reverse;
      }
    }

    return lhs_doc < rhs_doc;
  };

  std::multiset<std::pair<irs::bstring, irs::doc_id_t>, decltype(score_less)>
    scored_result{score_less};

  for (const auto& sub : rdr) {
    auto docs =
      prepared_filter->execute({.segment = sub, .scorers = prepared_order});

    auto* doc = irs::get<irs::document>(*docs);
    // ensure all iterators contain "document" attribute
    ASSERT_TRUE(bool(doc));

    const auto* score = irs::get<irs::score>(*docs);

    if (!score) {
      ASSERT_FALSE(score_must_be_present);
    }

    irs::bstring score_value(prepared_order.score_size(), 0);

    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);

      if (score && score->Func() != &irs::ScoreFunction::DefaultScore) {
        (*score)(reinterpret_cast<irs::score_t*>(score_value.data()));

        scored_result.emplace(score_value, docs->value());
      } else {
        scored_result.emplace(irs::bstring(prepared_order.score_size(), 0),
                              docs->value());
      }
    }
    ASSERT_FALSE(docs->next());
  }

  result.clear();
  for (auto& entry : scored_result) {
    result.emplace_back(entry.second);
  }
}

void FilterTestCaseBase::CheckQuery(const irs::filter& filter,
                                    std::span<const irs::Scorer::ptr> order,
                                    const std::vector<irs::doc_id_t>& expected,
                                    const irs::IndexReader& rdr,
                                    bool score_must_be_present, bool reverse) {
  std::vector<irs::doc_id_t> result;
  MakeResult(filter, order, rdr, result, score_must_be_present, reverse);
  ASSERT_EQ(expected, result);
}

}  // namespace tests
