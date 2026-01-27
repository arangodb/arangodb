////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "multiterm_query.hpp"

#include "search/bitset_doc_iterator.hpp"
#include "search/disjunction.hpp"
#include "search/min_match_disjunction.hpp"
#include "search/prepared_state_visitor.hpp"
#include "shared.hpp"
#include "utils/bitset.hpp"

namespace {

using namespace irs;

class lazy_bitset_iterator : public bitset_doc_iterator {
 public:
  lazy_bitset_iterator(
    const SubReader& segment, const term_reader& field,
    std::span<const MultiTermState::UnscoredTermState> states,
    cost::cost_t estimation) noexcept
    : bitset_doc_iterator(estimation),
      field_(&field),
      segment_(&segment),
      states_(states) {
    IRS_ASSERT(!states_.empty());
  }

  attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::type<score>::id() == id ? &score_
                                        : bitset_doc_iterator::get_mutable(id);
  }

 protected:
  bool refill(const word_t** begin, const word_t** end) final;

 private:
  score score_;
  std::unique_ptr<word_t[]> set_;
  const term_reader* field_;
  const SubReader* segment_;
  std::span<const MultiTermState::UnscoredTermState> states_;
};

bool lazy_bitset_iterator::refill(const word_t** begin, const word_t** end) {
  if (!field_) {
    return false;
  }

  const size_t bits = segment_->docs_count() + irs::doc_limits::min();
  const size_t words = bitset::bits_to_words(bits);
  set_ = std::make_unique<word_t[]>(words);
  std::memset(set_.get(), 0, sizeof(word_t) * words);

  auto provider = [begin = states_.begin(),
                   end =
                     states_.end()]() mutable noexcept -> const seek_cookie* {
    if (begin != end) {
      auto* cookie = begin->get();
      // cppcheck-suppress unreadVariable
      ++begin;
      return cookie;
    }
    return nullptr;
  };

  const size_t count = field_->bit_union(provider, set_.get());
  field_ = nullptr;

  if (count) {
    // we don't want to emit doc_limits::invalid()
    // ensure first bit isn't set,
    IRS_ASSERT(!irs::check_bit(set_[0], 0));

    *begin = set_.get();
    *end = set_.get() + words;
    return true;
  }

  return false;
}

}  // namespace

namespace irs {

void MultiTermQuery::visit(const SubReader& segment,
                           PreparedStateVisitor& visitor, score_t boost) const {
  if (auto state = states_.find(segment); state) {
    visitor.Visit(*this, *state, boost * boost_);
  }
}

doc_iterator::ptr MultiTermQuery::execute(const ExecutionContext& ctx) const {
  auto& segment = ctx.segment;
  auto& ord = ctx.scorers;

  // get term state for the specified reader
  auto state = states_.find(segment);

  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }

  auto* reader = state->reader;
  IRS_ASSERT(reader);

  // Get required features
  const IndexFeatures features = ord.features();
  const std::span stats{stats_};

  const bool has_unscored_terms = !state->unscored_terms.empty();

  ScoreAdapters itrs(state->scored_states.size() + size_t(has_unscored_terms));
  auto it = std::begin(itrs);

  // add an iterator for each of the scored states
  const bool no_score = ord.empty();
  for (auto& entry : state->scored_states) {
    IRS_ASSERT(entry.cookie);
    auto docs = reader->postings(*entry.cookie, features);

    if (IRS_UNLIKELY(!docs)) {
      continue;
    }

    if (!no_score) {
      auto* score = irs::get_mutable<irs::score>(docs.get());
      IRS_ASSERT(score);
      IRS_ASSERT(entry.stat_offset < stats.size());
      auto* stat = stats[entry.stat_offset].c_str();
      CompileScore(*score, ord.buckets(), segment, *state->reader, stat, *docs,
                   entry.boost * boost_);
    }

    IRS_ASSERT(it != std::end(itrs));
    *it = std::move(docs);
    ++it;
  }

  if (has_unscored_terms) {
    IRS_ASSERT(it != std::end(itrs));
    *it = {memory::make_managed<::lazy_bitset_iterator>(
      segment, *state->reader, state->unscored_terms,
      state->unscored_states_estimation)};
    ++it;
  }

  itrs.erase(it, std::end(itrs));

  return ResoveMergeType(
    merge_type_, ord.buckets().size(),
    [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
      using disjunction_t = min_match_iterator<doc_iterator::ptr, A>;

      return MakeWeakDisjunction<disjunction_t>({}, std::move(itrs), min_match_,
                                                std::move(aggregator),
                                                state->estimation());
    });
}

}  // namespace irs
