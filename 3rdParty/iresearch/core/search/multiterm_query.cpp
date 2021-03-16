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

#include "shared.hpp"
#include "search/bitset_doc_iterator.hpp"
#include "search/disjunction.hpp"
#include "utils/bitset.hpp"
#include "utils/range.hpp"

namespace {

using namespace irs;

class lazy_bitset_iterator final : public bitset_doc_iterator {
 public:
  lazy_bitset_iterator(
      const sub_reader& segment,
      const term_reader& field,
      const order::prepared& ord,
      const std::vector<multiterm_state::unscored_term_state>& states,
      cost::cost_t estimation) noexcept
    : bitset_doc_iterator(estimation),
      score_(ord),
      field_(&field),
      segment_(&segment),
      states_(states.data(), states.size()) {
    assert(!states_.empty());
  }

  virtual attribute* get_mutable(irs::type_info::type_id id) noexcept override {
    return irs::type<score>::id() == id
      ? &score_
      : bitset_doc_iterator::get_mutable(id);
  }

 protected:
  virtual bool refill(const word_t** begin, const word_t** end) override;

 private:
  score score_;
  std::unique_ptr<word_t[]> set_;
  const term_reader* field_;
  const sub_reader* segment_;
  range<const multiterm_state::unscored_term_state> states_;
}; // lazy_bitset_iterator

bool lazy_bitset_iterator::refill(
    const word_t** begin,
    const word_t** end) {
  if (!field_) {
    return false;
  }

  const size_t bits = segment_->docs_count() + irs::doc_limits::min();
  const size_t words = bitset::bits_to_words(bits);
  set_ = memory::make_unique<word_t[]>(words);
  std::memset(set_.get(), 0, sizeof(word_t)*words);

  auto provider = [begin = states_.begin(), end = states_.end()]() mutable noexcept
      -> const seek_term_iterator::seek_cookie* {
    if (begin != end) {
      auto* cookie = begin->get();
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
    assert(!irs::check_bit(set_[0], 0));

    *begin = set_.get();
    *end = set_.get() + words;
    return true;
  }

  return false;
}

}

namespace iresearch {

doc_iterator::ptr multiterm_query::execute(
    const sub_reader& segment,
    const order::prepared& ord,
    const attribute_provider* /*ctx*/) const {
  using scored_disjunction_t = scored_disjunction_iterator<doc_iterator::ptr>;
  using disjunction_t = disjunction_iterator<doc_iterator::ptr>;

  // get term state for the specified reader
  auto state = states_.find(segment);

  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }

  // get terms iterator
  auto terms = state->reader->iterator();

  if (IRS_UNLIKELY(!terms)) {
    return doc_iterator::empty();
  }

  // get required features for order
  auto& features = ord.features();
  auto& stats = this->stats();

  const bool has_unscored_terms = !state->unscored_terms.empty();

  disjunction_t::doc_iterators_t itrs(
    state->scored_states.size() + size_t(has_unscored_terms));
  auto it = itrs.begin();

  // add an iterator for each of the scored states
  const bool no_score = ord.empty();
  for (auto& entry : state->scored_states) {
    assert(entry.cookie);
    if (!terms->seek(bytes_ref::NIL, *entry.cookie)) {
      return doc_iterator::empty(); // internal error
    }

    auto docs = terms->postings(features);

    if (IRS_UNLIKELY(!docs)) {
      continue;
    }

    if (!no_score) {
      auto* score = irs::get_mutable<irs::score>(docs.get());

      if (score) {
        assert(entry.stat_offset < stats.size());
        auto* stat = stats[entry.stat_offset].c_str();

        order::prepared::scorers scorers(
          ord, segment, *state->reader, stat,
          score->realloc(ord), *docs, entry.boost*boost());

        irs::reset(*score, std::move(scorers));
      }
    }

    *it = std::move(docs);
    ++it;
  }

  if (has_unscored_terms) {
    *it = {
      memory::make_managed<::lazy_bitset_iterator>(
        segment, *state->reader, ord,
        state->unscored_terms,
        state->unscored_states_estimation)
    };
    ++it;
  }

  if (IRS_UNLIKELY(it != itrs.end())) {
    itrs.erase(it, itrs.end());
  }

  if (ord.empty()) {
    return make_disjunction<disjunction_t>(
      std::move(itrs), ord, merge_type_,
      state->estimation());
  }

  return make_disjunction<scored_disjunction_t>(
    std::move(itrs), ord, merge_type_,
    state->estimation());
}

} // ROOT
