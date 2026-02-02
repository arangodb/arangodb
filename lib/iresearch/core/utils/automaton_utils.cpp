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

#include "utils/automaton_utils.hpp"

#include "index/index_reader.hpp"
#include "search/limited_sample_collector.hpp"

namespace irs {

void Utf8EmplaceArc(automaton& a, automaton::StateId from, bytes_view label,
                    automaton::StateId to) {
  switch (label.size()) {
    case 1: {
      a.EmplaceArc(from, RangeLabel::From(label[0]), to);
      return;
    }
    case 2: {
      const auto s0 = a.AddState();
      a.EmplaceArc(from, RangeLabel::From(label[0]), s0);
      a.EmplaceArc(s0, RangeLabel::From(label[1]), to);
      return;
    }
    case 3: {
      const auto s0 = a.AddState();
      const auto s1 = a.AddState();
      a.EmplaceArc(from, RangeLabel::From(label[0]), s0);
      a.EmplaceArc(s0, RangeLabel::From(label[1]), s1);
      a.EmplaceArc(s1, RangeLabel::From(label[2]), to);
      return;
    }
    case 4: {
      const auto s0 = a.NumStates();
      const auto s1 = s0 + 1;
      const auto s2 = s0 + 2;
      a.AddStates(3);
      a.EmplaceArc(from, RangeLabel::From(label[0]), s0);
      a.EmplaceArc(s0, RangeLabel::From(label[1]), s1);
      a.EmplaceArc(s1, RangeLabel::From(label[2]), s2);
      a.EmplaceArc(s2, RangeLabel::From(label[3]), to);
      return;
    }
  }
}

void Utf8EmplaceRhoArc(automaton& a, automaton::StateId from,
                       automaton::StateId to) {
  const auto id = a.NumStates();  // stated ids are sequential
  a.AddStates(3);

  // add rho transitions
  a.ReserveArcs(from, 4);
  a.EmplaceArc(from, kUTF8Byte1, to);
  a.EmplaceArc(from, kUTF8Byte2, id);
  a.EmplaceArc(from, kUTF8Byte3, id + 1);
  a.EmplaceArc(from, kUTF8Byte4, id + 2);

  // connect intermediate states of default multi-byte UTF8 sequence
  a.EmplaceArc(id, kRhoLabel, to);
  a.EmplaceArc(id + 1, kRhoLabel, id);
  a.EmplaceArc(id + 2, kRhoLabel, id + 1);
}

void Utf8TransitionsBuilder::Minimize(automaton& a, size_t prefix) {
  IRS_ASSERT(prefix > 0);

  for (size_t i = last_.size(); i >= prefix; --i) {
    auto& s = states_[i];
    auto& p = states_[i - 1];
    IRS_ASSERT(!p.arcs.empty());

    if (s.id == fst::kNoStateId) {
      // here we deal with rho transition only for
      // intermediate states, i.e. char range is [128;191]
      const size_t rho_idx = last_.size() - i - 1;
      IRS_ASSERT(rho_idx < std::size(rho_states_));
      s.AddRhoArc(kRhoLabel.min, kRhoLabel.max + 1, rho_states_[rho_idx]);
    }

    p.arcs.back().id = states_map_.insert(s, a);  // finalize state

    s.Clear();
  }
}

void Utf8TransitionsBuilder::Insert(automaton& a, const byte_type* label,
                                    size_t size, automaton::StateId to) {
  IRS_ASSERT(label);
  IRS_ASSERT(size < 5);

  const size_t prefix =
    1 + CommonPrefixLength(last_.data(), last_.size(), label, size);
  Minimize(a, prefix);  // minimize suffix

  // add current word suffix
  for (size_t i = prefix; i <= size; ++i) {
    const auto ch = label[i - 1];
    auto& p = states_[i - 1];
    // root state is already a part of automaton
    IRS_ASSERT(i == 1 || p.id == fst::kNoStateId);

    if (p.id == fst::kNoStateId) {
      // here we deal with rho transition only for
      // intermediate states, i.e. char range is [128;191]
      p.AddRhoArc(128, ch, rho_states_[size - i]);
    }

    p.arcs.emplace_back(RangeLabel::From(ch), &states_[i]);
  }

  const bool is_final = last_.size() != size || prefix != (size + 1);

  if (is_final) {
    states_[size].id = to;
  }
}

void Utf8TransitionsBuilder::Finish(automaton& a, automaton::StateId from) {
#ifdef IRESEARCH_DEBUG
  Finally ensure_empty = [&]() noexcept {
    // ensure everything is cleaned up
    IRS_ASSERT(std::all_of(std::begin(states_), std::end(states_),
                           [](const State& s) noexcept {
                             return s.arcs.empty() && s.id == fst::kNoStateId;
                           }));
  };
#endif

  auto& root = states_[0];
  Minimize(a, 1);

  if (fst::kNoStateId == rho_states_[0]) {
    // no default state: just add transitions from the
    // root node to its successors
    for (const auto& arc : root.arcs) {
      a.EmplaceArc(from, arc.ilabel, arc.id);
    }

    root.Clear();

    return;
  }

  // in presence of default state we have to add some extra
  // transitions from root to properly handle multi-byte sequences
  // and preserve correctness of arcs order

  // reserve some memory to store all outbound transitions
  a.ReserveArcs(from, root.arcs.size());

  auto add_arcs = [&a, from, arc = root.arcs.begin(), end = root.arcs.end()](
                    uint32_t min, uint32_t max,
                    automaton::StateId rho_state) mutable {
    IRS_ASSERT(min < max);

    for (; arc != end && arc->max <= max; ++arc) {
      // ensure arcs are sorted
      IRS_ASSERT(min <= arc->min);
      // ensure every arc denotes a single char, otherwise
      // we have to use "arc->min" below which is a bit more
      // expensive to access
      IRS_ASSERT(arc->min == arc->max);

      if (min < arc->max) {
        a.EmplaceArc(from, RangeLabel::From(min, arc->max - 1), rho_state);
      }

      a.EmplaceArc(from, arc->ilabel, arc->id);
      min = arc->max + 1;
    }

    if (min < max) {
      a.EmplaceArc(from, RangeLabel::From(min, max), rho_state);
    }
  };

  add_arcs(kUTF8Byte1.min, kUTF8Byte1.max, rho_states_[0]);
  add_arcs(kUTF8Byte2.min, kUTF8Byte2.max, rho_states_[1]);
  add_arcs(kUTF8Byte3.min, kUTF8Byte3.max, rho_states_[2]);
  add_arcs(kUTF8Byte4.min, kUTF8Byte4.max, rho_states_[3]);

  root.Clear();

  // connect intermediate states of default multi-byte UTF8 sequence
  a.EmplaceArc(rho_states_[1], kRhoLabel, rho_states_[0]);
  a.EmplaceArc(rho_states_[2], kRhoLabel, rho_states_[1]);
  a.EmplaceArc(rho_states_[3], kRhoLabel, rho_states_[2]);
}

filter::prepared::ptr PrepareAutomatonFilter(const PrepareContext& ctx,
                                             std::string_view field,
                                             const automaton& acceptor,
                                             size_t scored_terms_limit) {
  auto matcher = MakeAutomatonMatcher(acceptor);

  if (fst::kError == matcher.Properties(0)) {
    IRS_LOG_ERROR(
      absl::StrCat("Expected deterministic, epsilon-free acceptor, got the "
                   "following properties ",
                   matcher.GetFst().Properties(
                     automaton_table_matcher::FST_PROPERTIES, false)));

    return filter::prepared::empty();
  }

  // object for collecting order stats
  limited_sample_collector<term_frequency> collector(
    ctx.scorers.empty() ? 0 : scored_terms_limit);
  MultiTermQuery::States states{ctx.memory, ctx.index.size()};
  multiterm_visitor mtv{collector, states};

  for (const auto& segment : ctx.index) {
    if (const auto* reader = segment.field(field); reader) {
      Visit(segment, *reader, matcher, mtv);
    }
  }

  MultiTermQuery::Stats stats{{ctx.memory}};
  collector.score(ctx.index, ctx.scorers, stats);

  return memory::make_tracked<MultiTermQuery>(ctx.memory, std::move(states),
                                              std::move(stats), ctx.boost,
                                              ScoreMergeType::kSum, size_t{1});
}

}  // namespace irs
