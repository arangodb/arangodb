////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "column_existence_filter.hpp"

#include "formats/empty_term_reader.hpp"
#include "search/disjunction.hpp"

namespace irs {
namespace {

class column_existence_query : public filter::prepared {
 public:
  column_existence_query(std::string_view field, bstring&& stats, score_t boost)
    : field_{field}, stats_{std::move(stats)}, boost_{boost} {}

  doc_iterator::ptr execute(const ExecutionContext& ctx) const override {
    const auto& segment = ctx.segment;
    const auto* column = segment.column(field_);

    if (!column) {
      return doc_iterator::empty();
    }

    return iterator(segment, *column, ctx.scorers);
  }

  void visit(const SubReader&, PreparedStateVisitor&, score_t) const final {
    // No terms to visit
  }

  score_t boost() const noexcept final { return boost_; }

 protected:
  doc_iterator::ptr iterator(const SubReader& segment,
                             const column_reader& column,
                             const Scorers& ord) const {
    auto it = column.iterator(ColumnHint::kMask);

    if (IRS_UNLIKELY(!it)) {
      return doc_iterator::empty();
    }

    if (!ord.empty()) {
      if (auto* score = irs::get_mutable<irs::score>(it.get()); score) {
        CompileScore(*score, ord.buckets(), segment,
                     empty_term_reader(column.size()), stats_.c_str(), *it,
                     boost_);
      }
    }

    return it;
  }

  std::string field_;
  bstring stats_;
  score_t boost_;
};

class column_prefix_existence_query : public column_existence_query {
 public:
  column_prefix_existence_query(std::string_view prefix, bstring&& stats,
                                const ColumnAcceptor& acceptor, score_t boost)
    : column_existence_query{prefix, std::move(stats), boost},
      acceptor_{acceptor} {
    IRS_ASSERT(acceptor_);
  }

  irs::doc_iterator::ptr execute(const ExecutionContext& ctx) const final {
    using adapter_t = irs::ScoreAdapter<>;

    IRS_ASSERT(acceptor_);

    auto& segment = ctx.segment;
    auto& ord = ctx.scorers;
    const std::string_view prefix = field_;

    auto it = segment.columns();

    if (!it->seek(prefix)) {
      // reached the end
      return irs::doc_iterator::empty();
    }

    const auto* column = &it->value();

    std::vector<adapter_t> itrs;
    for (; column->name().starts_with(prefix); column = &it->value()) {
      if (acceptor_(column->name(), prefix)) {
        itrs.emplace_back(iterator(segment, *column, ord));
      }

      if (!it->next()) {
        break;
      }
    }

    return ResoveMergeType(
      ScoreMergeType::kSum, ord.buckets().size(),
      [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
        using disjunction_t =
          irs::disjunction_iterator<irs::doc_iterator::ptr, A>;
        return irs::MakeDisjunction<disjunction_t>(ctx.wand, std::move(itrs),
                                                   std::move(aggregator));
      });
  }

 private:
  ColumnAcceptor acceptor_;
};

}  // namespace

filter::prepared::ptr by_column_existence::prepare(
  const PrepareContext& ctx) const {
  // skip field-level/term-level statistics because there are no explicit
  // fields/terms, but still collect index-level statistics
  // i.e. all fields and terms implicitly match
  bstring stats(ctx.scorers.stats_size(), 0);
  auto* stats_buf = stats.data();

  PrepareCollectors(ctx.scorers.buckets(), stats_buf);

  const auto filter_boost = ctx.boost * boost();

  auto& acceptor = options().acceptor;

  return acceptor
           ? memory::make_tracked<column_prefix_existence_query>(
               ctx.memory, field(), std::move(stats), acceptor, filter_boost)
           : memory::make_tracked<column_existence_query>(
               ctx.memory, field(), std::move(stats), filter_boost);
}

}  // namespace irs
