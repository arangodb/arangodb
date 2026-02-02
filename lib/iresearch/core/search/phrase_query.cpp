////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include "phrase_query.hpp"

#include "index/field_meta.hpp"
#include "search/phrase_filter.hpp"
#include "search/phrase_iterator.hpp"

namespace irs {
namespace {

// Get index features required for offsets
constexpr IndexFeatures kRequireOffs =
  FixedPhraseQuery::kRequiredFeatures | IndexFeatures::OFFS;

template<bool OneShot, bool HasFreq>
using FixedPhraseIterator =
  PhraseIterator<Conjunction<ScoreAdapter<>, NoopAggregator>,
                 FixedPhraseFrequency<OneShot, HasFreq>>;

// FIXME add proper handling of overlapped case
template<typename Adapter, bool VolatileBoost, bool OneShot, bool HasFreq>
using VariadicPhraseIterator = PhraseIterator<
  Conjunction<ScoreAdapter<>, NoopAggregator>,
  VariadicPhraseFrequency<Adapter, VolatileBoost, OneShot, HasFreq>>;

}  // namespace

doc_iterator::ptr FixedPhraseQuery::execute(const ExecutionContext& ctx) const {
  auto& rdr = ctx.segment;
  auto& ord = ctx.scorers;

  // get phrase state for the specified reader
  auto phrase_state = states_.find(rdr);

  if (!phrase_state) {
    // invalid state
    return doc_iterator::empty();
  }

  auto* reader = phrase_state->reader;
  IRS_ASSERT(reader);

  if (kRequiredFeatures !=
      (reader->meta().index_features & kRequiredFeatures)) {
    return doc_iterator::empty();
  }

  // get index features required for query & order
  const IndexFeatures features = ord.features() | kRequiredFeatures;

  ScoreAdapters itrs;
  itrs.reserve(phrase_state->terms.size());

  std::vector<FixedTermPosition> positions;
  positions.reserve(phrase_state->terms.size());

  auto position = std::begin(positions_);

  for (const auto& term_state : phrase_state->terms) {
    IRS_ASSERT(term_state.first);

    // get postings using cached state
    auto& docs =
      itrs.emplace_back(reader->postings(*term_state.first, features));

    if (IRS_UNLIKELY(!docs)) {
      return doc_iterator::empty();
    }

    auto* pos = irs::get_mutable<irs::position>(docs.it.get());

    if (!pos) {
      // positions not found
      return doc_iterator::empty();
    }

    positions.emplace_back(std::ref(*pos), *position);

    ++position;
  }

  if (ord.empty()) {
    return memory::make_managed<FixedPhraseIterator<true, false>>(
      std::move(itrs), std::move(positions));
  }

  return memory::make_managed<FixedPhraseIterator<false, true>>(
    std::move(itrs), std::move(positions), rdr, *phrase_state->reader,
    stats_.c_str(), ord, boost_);
}

doc_iterator::ptr FixedPhraseQuery::ExecuteWithOffsets(
  const SubReader& rdr) const {
  using FixedPhraseIterator =
    PhraseIterator<Conjunction<ScoreAdapter<>, NoopAggregator>,
                   PhrasePosition<FixedPhraseFrequency<true, false>>>;

  // get phrase state for the specified reader
  auto phrase_state = states_.find(rdr);

  if (!phrase_state) {
    // invalid state
    return doc_iterator::empty();
  }

  ScoreAdapters itrs;
  itrs.reserve(phrase_state->terms.size());

  std::vector<FixedPhraseIterator::TermPosition> positions;
  positions.reserve(phrase_state->terms.size());

  auto* reader = phrase_state->reader;
  IRS_ASSERT(reader);

  if (kRequireOffs != (reader->meta().index_features & kRequireOffs)) {
    return doc_iterator::empty();
  }

  auto position = std::begin(positions_);
  IRS_ASSERT(!phrase_state->terms.empty());

  auto term_state = std::begin(phrase_state->terms);

  auto add_iterator = [&](IndexFeatures features) {
    IRS_ASSERT(term_state->first);

    // get postings using cached state
    auto& docs =
      itrs.emplace_back(reader->postings(*term_state->first, features));

    if (IRS_UNLIKELY(!docs)) {
      return false;
    }

    auto* pos = irs::get_mutable<irs::position>(docs.it.get());

    if (!pos) {
      return false;
    }

    positions.emplace_back(std::ref(*pos), *position);

    if (IndexFeatures::OFFS == (features & IndexFeatures::OFFS)) {
      if (!irs::get<irs::offset>(*pos)) {
        return false;
      }
    }

    ++position;
    ++term_state;
    return true;
  };

  if (!add_iterator(kRequireOffs)) {
    return doc_iterator::empty();
  }

  if (term_state != std::end(phrase_state->terms)) {
    auto back = std::prev(std::end(phrase_state->terms));

    while (term_state != back) {
      if (!add_iterator(kRequiredFeatures)) {
        return doc_iterator::empty();
      }
    }

    if (!add_iterator(kRequireOffs)) {
      return doc_iterator::empty();
    }
  }

  return memory::make_managed<FixedPhraseIterator>(std::move(itrs),
                                                   std::move(positions));
}

doc_iterator::ptr VariadicPhraseQuery::execute(
  const ExecutionContext& ctx) const {
  using Adapter = VariadicPhraseAdapter;
  using CompoundDocIterator = irs::compound_doc_iterator<Adapter>;
  using Disjunction = disjunction<doc_iterator::ptr, NoopAggregator, Adapter>;
  auto& rdr = ctx.segment;

  // get phrase state for the specified reader
  auto phrase_state = states_.find(rdr);

  if (!phrase_state) {
    // invalid state
    return doc_iterator::empty();
  }

  // find term using cached state
  auto* reader = phrase_state->reader;
  IRS_ASSERT(reader);

  if (kRequiredFeatures !=
      (reader->meta().index_features & kRequiredFeatures)) {
    return doc_iterator::empty();
  }

  auto& ord = ctx.scorers;

  // get features required for query & order
  const IndexFeatures features = ord.features() | kRequiredFeatures;

  ScoreAdapters conj_itrs;
  conj_itrs.reserve(phrase_state->terms.size());

  const auto phrase_size = phrase_state->num_terms.size();

  std::vector<VariadicTermPosition<Adapter>> positions;
  positions.resize(phrase_size);

  auto position = std::begin(positions_);

  auto term_state = std::begin(phrase_state->terms);
  for (size_t i = 0; i < phrase_size; ++i) {
    const auto num_terms = phrase_state->num_terms[i];
    auto& pos = positions[i];
    pos.second = *position;

    std::vector<Adapter> disj_itrs;
    disj_itrs.reserve(num_terms);
    for (const auto end = term_state + num_terms; term_state != end;
         ++term_state) {
      IRS_ASSERT(term_state->first);

      auto it = reader->postings(*term_state->first, features);

      if (IRS_UNLIKELY(!it)) {
        continue;
      }

      Adapter docs{std::move(it), term_state->second};

      if (!docs.position) {
        // positions not found
        continue;
      }

      disj_itrs.emplace_back(std::move(docs));
    }

    if (disj_itrs.empty()) {
      return doc_iterator::empty();
    }

    // TODO(MBkkt) VariadicPhrase wand support
    auto disj =
      MakeDisjunction<Disjunction>({}, std::move(disj_itrs), NoopAggregator{});
    pos.first = DownCast<CompoundDocIterator>(disj.get());
    conj_itrs.emplace_back(std::move(disj));
    ++position;
  }
  IRS_ASSERT(term_state == std::end(phrase_state->terms));

  if (ord.empty()) {
    return memory::make_managed<
      VariadicPhraseIterator<Adapter, false, true, false>>(
      std::move(conj_itrs), std::move(positions));
  }

  if (phrase_state->volatile_boost) {
    return memory::make_managed<
      VariadicPhraseIterator<Adapter, true, false, true>>(
      std::move(conj_itrs), std::move(positions), rdr, *phrase_state->reader,
      stats_.c_str(), ord, boost_);
  }

  return memory::make_managed<
    VariadicPhraseIterator<Adapter, false, false, true>>(
    std::move(conj_itrs), std::move(positions), rdr, *phrase_state->reader,
    stats_.c_str(), ord, boost_);
}

doc_iterator::ptr VariadicPhraseQuery::ExecuteWithOffsets(
  const irs::SubReader& rdr) const {
  using Adapter = VariadicPhraseOffsetAdapter;
  using FixedPhraseIterator = PhraseIterator<
    Conjunction<ScoreAdapter<>, NoopAggregator>,
    PhrasePosition<VariadicPhraseFrequency<Adapter, false, true, false>>>;
  using CompundDocIterator = irs::compound_doc_iterator<Adapter>;
  using Disjunction = disjunction<doc_iterator::ptr, NoopAggregator, Adapter>;

  // get phrase state for the specified reader
  auto phrase_state = states_.find(rdr);

  if (!phrase_state) {
    // invalid state
    return doc_iterator::empty();
  }

  ScoreAdapters conj_itrs;
  conj_itrs.reserve(phrase_state->terms.size());

  const auto phrase_size = phrase_state->num_terms.size();

  std::vector<FixedPhraseIterator::TermPosition> positions;
  positions.resize(phrase_size);

  // find term using cached state
  auto* reader = phrase_state->reader;
  IRS_ASSERT(reader);

  if (kRequireOffs != (reader->meta().index_features & kRequireOffs)) {
    return doc_iterator::empty();
  }

  auto position = std::begin(positions_);

  auto term_state = std::begin(phrase_state->terms);

  size_t i = 0;
  auto add_iterator = [&](IndexFeatures features) {
    const auto num_terms = phrase_state->num_terms[i];
    auto& pos = positions[i];
    pos.second = *position;

    std::vector<Adapter> disj_itrs;
    disj_itrs.reserve(num_terms);
    for (const auto end = term_state + num_terms; term_state != end;
         ++term_state) {
      IRS_ASSERT(term_state->first);

      auto it = reader->postings(*term_state->first, features);

      if (IRS_UNLIKELY(!it)) {
        continue;
      }

      Adapter docs{std::move(it), term_state->second};

      if (!docs.position) {
        // positions not found
        continue;
      }

      if (IndexFeatures::OFFS == (features & IndexFeatures::OFFS)) {
        if (!irs::get<irs::offset>(*docs.position)) {
          continue;
        }
      }

      disj_itrs.emplace_back(std::move(docs));
    }

    if (disj_itrs.empty()) {
      return false;
    }

    // TODO(MBkkt) VariadicPhrase wand support
    auto disj =
      MakeDisjunction<Disjunction>({}, std::move(disj_itrs), NoopAggregator{});
    pos.first = DownCast<CompundDocIterator>(disj.get());
    conj_itrs.emplace_back(std::move(disj));
    ++position;
    ++i;
    return true;
  };

  if (!add_iterator(kRequireOffs)) {
    return doc_iterator::empty();
  }

  if (i < phrase_size) {
    for (auto size = phrase_size - 1; i < size;) {
      if (!add_iterator(kRequiredFeatures)) {
        return doc_iterator::empty();
      }
    }

    if (!add_iterator(kRequireOffs)) {
      return doc_iterator::empty();
    }
  }
  IRS_ASSERT(term_state == std::end(phrase_state->terms));

  return memory::make_managed<FixedPhraseIterator>(std::move(conj_itrs),
                                                   std::move(positions));
}

}  // namespace irs
