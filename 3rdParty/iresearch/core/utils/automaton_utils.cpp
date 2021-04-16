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
#include "utils/fstext/fst_table_matcher.hpp"

namespace iresearch {

void utf8_emplace_arc(
    automaton& a,
    automaton::StateId from,
    automaton::StateId rho_state,
    const bytes_ref& label,
    automaton::StateId to) {
  if (fst::kNoStateId == rho_state) {
    return utf8_emplace_arc(a, from, label, to);
  }

  if (label.empty()) {
    return;
  }

  // reserve enough arcs and states (stated ids are sequential)
  a.ReserveArcs(from, 6);
  const auto id = a.NumStates();
  a.AddStates(3 + label.size() - 1);

  const automaton::StateId rho_states[] { rho_state, id, id + 1, id + 2 };

  auto add_arcs = [&a](
      uint32_t min,
      uint32_t max,
      uint32_t label,
      automaton::StateId from,
      automaton::StateId to,
      automaton::StateId rho) mutable {
    if (label < min || label > max) {
      a.EmplaceArc(from, range_label{min, max}, rho);
      return;
    }

    if (min < label) {
      a.EmplaceArc(from, range_label{min, label - 1}, rho);
    }
    a.EmplaceArc(from, range_label{label, label}, to);
    if (++label < max) {
      a.EmplaceArc(from, range_label{label, max}, rho);
    }
  };

  const auto s0 = id + 3;
  const uint32_t lead = label.front();
  add_arcs(0, 127, lead, from, to, rho_states[0]);
  add_arcs(192, 223, lead, from, s0, rho_states[1]);
  add_arcs(224, 239, lead, from, s0, rho_states[2]);
  add_arcs(240, 255, lead, from, s0, rho_states[3]);

  switch (label.size()) {
    case 1: {
      break;
    }
    case 2: {
      add_arcs(128, 191, label[1], s0, to, rho_states[0]);
      break;
    }
    case 3: {
      const auto s1 = id + 4;
      add_arcs(128, 191, label[1], s0, s1, rho_states[1]);
      add_arcs(128, 191, label[2], s1, to, rho_states[0]);
      break;
    }
    case 4: {
      const auto s1 = id + 4;
      const auto s2 = id + 5;
      add_arcs(128, 191, label[1], s0, s1, rho_states[2]);
      add_arcs(128, 191, label[2], s1, s2, rho_states[1]);
      add_arcs(128, 191, label[3], s2, to, rho_states[0]);
      break;
    }
  }

  // connect intermediate states of default multi-byte UTF8 sequence
  constexpr range_label RHO_LABEL{128, 191};
  a.EmplaceArc(rho_states[1], RHO_LABEL, rho_states[0]);
  a.EmplaceArc(rho_states[2], RHO_LABEL, rho_states[1]);
  a.EmplaceArc(rho_states[3], RHO_LABEL, rho_states[2]);
}

void utf8_emplace_arc(
    automaton& a,
    automaton::StateId from,
    const bytes_ref& label,
    automaton::StateId to) {
  switch (label.size()) {
    case 1: {
      a.EmplaceArc(from, range_label::fromRange(label[0]), to);
      return;
    }
    case 2: {
      const auto s0 = a.AddState();
      a.EmplaceArc(from, range_label::fromRange(label[0]), s0);
      a.EmplaceArc(s0, range_label::fromRange(label[1]), to);
      return;
    }
    case 3: {
      const auto s0 = a.AddState();
      const auto s1 = a.AddState();
      a.EmplaceArc(from, range_label::fromRange(label[0]), s0);
      a.EmplaceArc(s0, range_label::fromRange(label[1]), s1);
      a.EmplaceArc(s1, range_label::fromRange(label[2]), to);
      return;
    }
    case 4: {
      const auto s0 = a.NumStates();
      const auto s1 = s0 + 1;
      const auto s2 = s0 + 2;
      a.AddStates(3);
      a.EmplaceArc(from, range_label::fromRange(label[0]), s0);
      a.EmplaceArc(s0, range_label::fromRange(label[1]), s1);
      a.EmplaceArc(s1, range_label::fromRange(label[2]), s2);
      a.EmplaceArc(s2, range_label::fromRange(label[3]), to);
      return;
    }
  }
}

void utf8_emplace_rho_arc(
    automaton& a,
    automaton::StateId from,
    automaton::StateId to) {
  const auto id = a.NumStates(); // stated ids are sequential
  a.AddStates(3);

  // add rho transitions
  a.ReserveArcs(from, 4);
  a.EmplaceArc(from, range_label{0, 127}, to);
  a.EmplaceArc(from, range_label{192, 223}, id);
  a.EmplaceArc(from, range_label{224, 239}, id + 1);
  a.EmplaceArc(from, range_label{240, 255}, id + 2);

  // connect intermediate states of default multi-byte UTF8 sequence
  constexpr auto label = range_label{128, 191};
  a.EmplaceArc(id, label, to);
  a.EmplaceArc(id + 1, label, id);
  a.EmplaceArc(id + 2, label, id + 1);
}

/*
automaton::StateId utf8_expand_labels(automaton& a) {
#ifdef IRESEARCH_DEBUG
  // ensure resulting automaton is sorted and deterministic
  static constexpr auto EXPECTED_PROPERTIES =
    fst::kIDeterministic | fst::kILabelSorted;

  assert(EXPECTED_PROPERTIES == a.Properties(EXPECTED_PROPERTIES, true));
  UNUSED(EXPECTED_PROPERTIES);
#endif

  using Label = automaton::Arc::Label;
  static_assert(sizeof(Label) == sizeof(utf8_utils::MIN_2BYTES_CODE_POINT));

  class utf8_char {
   public:
    explicit utf8_char(uint32_t utf32value) noexcept
      : size_(utf8_utils::utf32_to_utf8(utf32value, data())) {
    }

    uint32_t size() const noexcept { return size_; }
    const byte_type* c_str() const noexcept {
      return const_cast<utf8_char*>(this)->data();
    }
    operator bytes_ref() const noexcept {
      return { c_str(), size() };
    }

   private:
    byte_type* data() noexcept { return reinterpret_cast<byte_type*>(&data_); }

    uint32_t data_;
    uint32_t size_;
  };

  std::vector<std::pair<utf8_char, automaton::StateId>> utf8_arcs;

  utf8_transitions_builder builder;
  fst::ArcIteratorData<automaton::Arc> arcs;
  for (auto s = 0, nstates = a.NumStates(); s < nstates ; ++s) {
    a.InitArcIterator(s, &arcs);

    if (arcs.narcs) {
      const auto* arc = arcs.arcs + arcs.narcs - 1;

      automaton::StateId rho_state = fst::kNoLabel;
      if (arc->ilabel < static_cast<Label>(utf8_utils::MIN_2BYTES_CODE_POINT)) {
        // no rho transition, all label are single-byte UTF8 characters
        continue;
      } else if (arc->ilabel == fst::fsa::kRho) {
        rho_state = arc->nextstate;
      } else {
        ++arc;
      }

      utf8_arcs.clear();
      auto begin = arcs.arcs;
      for (; begin != arc; ++begin) {
        if (IRS_UNLIKELY(begin->ilabel > static_cast<Label>(utf8_utils::MAX_CODE_POINT))) {
          // invalid code point, give up
          return s;
        }

        utf8_arcs.emplace_back(begin->ilabel, begin->nextstate);
      }

      a.DeleteArcs(s);

      switch (utf8_arcs.size()) {
        case 0: {
          if (rho_state != fst::kNoStateId) {
            utf8_emplace_rho_arc(a, s, rho_state);
          }
        } break;
        case 1: {
          auto& utf8_arc = utf8_arcs.front();
          utf8_emplace_arc(a, s, rho_state, bytes_ref(utf8_arc.first), utf8_arc.second);
        } break;
        default: {
          //FIXME
          //builder.insert(a, s, rho_state, utf8_arcs.begin(), utf8_arcs.end());
        } break;
      }
    }
  }

#ifdef IRESEARCH_DEBUG
  // Ensure automaton is defined over the alphabet of ranges
  // [0..127], [128..191], [192..223], [224..239], [240..255]
  for (auto s = 0, nstates = a.NumStates(); s < nstates ; ++s) {
    a.InitArcIterator(s, &arcs);
    auto* begin = arcs.arcs;
    auto* end = begin + arcs.narcs;
    for (; begin != end; ++begin) {
      assert((begin->ilabel >= range_label(std::numeric_limits<byte_type>::min()) &&
              begin->ilabel <= range_label(std::numeric_limits<byte_type>::max())));
    }
  }
#endif

  return fst::kNoStateId;
}
*/

void utf8_transitions_builder::minimize(automaton& a, size_t prefix) {
  assert(prefix > 0);

  for (size_t i = last_.size(); i >= prefix; --i) {
    state& s = states_[i];
    state& p = states_[i - 1];
    assert(!p.arcs.empty());

    if (s.id == fst::kNoStateId) {
      // here we deal with rho transition only for
      // intermediate states, i.e. char range is [128;191]
      const size_t rho_idx = last_.size() - i - 1;
      assert(rho_idx >= 0 && rho_idx < IRESEARCH_COUNTOF(rho_states_));
      s.add_rho_arc(128, 192, rho_states_[rho_idx]);
    }

    p.arcs.back().id = states_map_.insert(s, a); // finalize state

    s.clear();
  }
}

void utf8_transitions_builder::insert(
    automaton& a,
    const byte_type* label,
    const size_t size,
    const automaton::StateId to) {
  assert(label && size < 5);

  const size_t prefix = 1 + common_prefix_length(last_.c_str(), last_.size(), label, size);
  minimize(a, prefix); // minimize suffix

  // add current word suffix
  for (size_t i = prefix; i <= size; ++i) {
    const auto ch = label[i - 1];
    auto& p = states_[i - 1];
    assert(i == 1 || p.id == fst::kNoStateId); // root state is already a part of automaton

    if (p.id == fst::kNoStateId) {
      // here we deal with rho transition only for
      // intermediate states, i.e. char range is [128;191]
      p.add_rho_arc(128, ch, rho_states_[size - i]);
    }

    p.arcs.emplace_back(range_label::fromRange(ch), &states_[i]);
  }

  const bool is_final = last_.size() != size || prefix != (size + 1);

  if (is_final) {
    states_[size].id = to;
  }
}

void utf8_transitions_builder::finish(automaton& a, automaton::StateId from) {
#ifdef IRESEARCH_DEBUG
  auto ensure_empty = make_finally([this]() {
    // ensure everything is cleaned up
    assert(std::all_of(
      std::begin(states_), std::end(states_), [](const state& s) noexcept {
        return s.arcs.empty() && s.id == fst::kNoStateId;
    }));
  });
#endif

  auto& root = states_[0];
  minimize(a, 1);

  if (fst::kNoStateId == rho_states_[0]) {
    // no default state: just add transitions from the
    // root node to its successors
    for (const auto& arc : root.arcs) {
      a.EmplaceArc(from, arc.ilabel, arc.id);
    }

    root.clear();

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
    assert(min < max);

    for (; arc != end && arc->max <= max; ++arc) {
      // ensure arcs are sorted
      assert(min <= arc->min);
      // ensure every arc denotes a single char, otherwise
      // we have to use "arc->min" below which is a bit more
      // expensive to access
      assert(arc->min == arc->max);

      if (min < arc->max) {
        a.EmplaceArc(from, range_label{min, arc->max - 1}, rho_state);
      }

      a.EmplaceArc(from, arc->ilabel, arc->id);
      min = arc->max + 1;
    }

    if (min < max) {
      a.EmplaceArc(from, range_label{min, max}, rho_state);
    }
  };

  add_arcs(0, 127, rho_states_[0]);
  add_arcs(192, 223, rho_states_[1]);
  add_arcs(224, 239, rho_states_[2]);
  add_arcs(240, 255, rho_states_[3]);

  root.clear();

  // connect intermediate states of default multi-byte UTF8 sequence
  constexpr auto label = range_label{128, 191};
  a.EmplaceArc(rho_states_[1], label, rho_states_[0]);
  a.EmplaceArc(rho_states_[2], label, rho_states_[1]);
  a.EmplaceArc(rho_states_[3], label, rho_states_[2]);
}

filter::prepared::ptr prepare_automaton_filter(
    const string_ref& field,
    const automaton& acceptor,
    size_t scored_terms_limit,
    const index_reader& index,
    const order::prepared& order,
    boost_t boost) {
  auto matcher = make_automaton_matcher(acceptor);

  if (fst::kError == matcher.Properties(0)) {
    IR_FRMT_ERROR("Expected deterministic, epsilon-free acceptor, "
                  "got the following properties " IR_UINT64_T_SPECIFIER "",
                  matcher.GetFst().Properties(automaton_table_matcher::FST_PROPERTIES, false));

    return filter::prepared::empty();
  }

  limited_sample_collector<term_frequency> collector(order.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  multiterm_query::states_t states(index);
  multiterm_visitor<multiterm_query::states_t> mtv(collector, states);

  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    visit(segment, *reader, matcher, mtv);
  }

  std::vector<bstring> stats;
  collector.score(index, order, stats);

  return memory::make_managed<multiterm_query>(
    std::move(states), std::move(stats),
    boost, sort::MergeType::AGGREGATE);
}

}
