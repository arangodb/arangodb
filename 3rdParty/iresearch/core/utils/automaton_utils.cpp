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

namespace {

using namespace irs;

// table contains indexes of states in
// utf8_transitions_builder::rho_states_ table
const automaton::Arc::Label UTF8_RHO_STATE_TABLE[] {
  // 1 byte sequence (0-127)
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  // invalid sequence (128-191)
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  // 2 bytes sequence (192-223)
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  // 3 bytes sequence (224-239)
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  // 4 bytes sequence (240-255)
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

// 0 is reserved for Epsilon transition
constexpr automaton::Arc::Label MIN = 1;

}

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
  a.ReserveArcs(from, 256);
  const auto id = a.NumStates();
  a.AddStates(3 + label.size() - 1);

  const automaton::StateId rho_states[] { rho_state, id, id + 1, id + 2 };

  const automaton::Arc::Label lead = label.front();
  automaton::Arc::Label min = MIN;

  for (; min < lead; ++min) {
    a.EmplaceArc(from, min, rho_states[UTF8_RHO_STATE_TABLE[min]]);
  }

  switch (label.size()) {
    case 1: {
      a.EmplaceArc(from, lead, to);
      break;
    }
    case 2: {
      const auto s0 = id + 3;
      a.EmplaceArc(from, lead, s0);
      a.EmplaceArc(s0, label[1], to);
      a.EmplaceArc(s0, fst::fsa::kRho, rho_states[0]);
      break;
    }
    case 3: {
      const auto s0 = id + 3;
      const auto s1 = id + 4;
      a.EmplaceArc(from, lead, s0);
      a.EmplaceArc(s0, label[1], s1);
      a.EmplaceArc(s1, label[2], to);
      a.EmplaceArc(s0, fst::fsa::kRho, rho_states[1]);
      a.EmplaceArc(s1, fst::fsa::kRho, rho_states[0]);
      break;
    }
    case 4: {
      const auto s0 = id + 3;
      const auto s1 = id + 4;
      const auto s2 = id + 5;
      a.EmplaceArc(from, lead, s0);
      a.EmplaceArc(s0, label[1], s1);
      a.EmplaceArc(s1, label[2], s2);
      a.EmplaceArc(s2, label[3], to);
      a.EmplaceArc(s0, fst::fsa::kRho, rho_states[2]);
      a.EmplaceArc(s1, fst::fsa::kRho, rho_states[1]);
      a.EmplaceArc(s2, fst::fsa::kRho, rho_states[0]);
      break;
    }
  }

  for (++min; min < 256; ++min) {
    a.EmplaceArc(from, min, rho_states[UTF8_RHO_STATE_TABLE[min]]);
  }

  // connect intermediate states of default multi-byte UTF8 sequence

  a.EmplaceArc(rho_states[1], fst::fsa::kRho, rho_states[0]);
  a.EmplaceArc(rho_states[2], fst::fsa::kRho, rho_states[1]);
  a.EmplaceArc(rho_states[3], fst::fsa::kRho, rho_states[2]);
}

void utf8_emplace_arc(
    automaton& a,
    automaton::StateId from,
    const bytes_ref& label,
    automaton::StateId to) {
  switch (label.size()) {
    case 1: {
      a.EmplaceArc(from, label[0], to);
      return;
    }
    case 2: {
      const auto s0 = a.AddState();
      a.EmplaceArc(from, label[0], s0);
      a.EmplaceArc(s0, label[1], to);
      return;
    }
    case 3: {
      const auto s0 = a.AddState();
      const auto s1 = a.AddState();
      a.EmplaceArc(from, label[0], s0);
      a.EmplaceArc(s0, label[1], s1);
      a.EmplaceArc(s1, label[2], to);
      return;
    }
    case 4: {
      const auto s0 = a.NumStates();
      const auto s1 = s0 + 1;
      const auto s2 = s0 + 2;
      a.AddStates(3);
      a.EmplaceArc(from, label[0], s0);
      a.EmplaceArc(s0, label[1], s1);
      a.EmplaceArc(s1, label[2], s2);
      a.EmplaceArc(s2, label[3], to);
      return;
    }
  }
}

void utf8_emplace_arc_range(
    automaton& a,
    automaton::StateId from,
    const bytes_ref& label,
    automaton::StateId to) {
  switch (label.size()) {
    case 1: {
      a.EmplaceArc(from, fst::fsa::EncodeRange(label[0]), to);
      return;
    }
    case 2: {
      const auto s0 = a.AddState();
      a.EmplaceArc(from, fst::fsa::EncodeRange(label[0]), s0);
      a.EmplaceArc(s0, fst::fsa::EncodeRange(label[1]), to);
      return;
    }
    case 3: {
      const auto s0 = a.AddState();
      const auto s1 = a.AddState();
      a.EmplaceArc(from, fst::fsa::EncodeRange(label[0]), s0);
      a.EmplaceArc(s0, fst::fsa::EncodeRange(label[1]), s1);
      a.EmplaceArc(s1, fst::fsa::EncodeRange(label[2]), to);
      return;
    }
    case 4: {
      const auto s0 = a.NumStates();
      const auto s1 = s0 + 1;
      const auto s2 = s0 + 2;
      a.AddStates(3);
      a.EmplaceArc(from, fst::fsa::EncodeRange(label[0]), s0);
      a.EmplaceArc(s0, fst::fsa::EncodeRange(label[1]), s1);
      a.EmplaceArc(s1, fst::fsa::EncodeRange(label[2]), s2);
      a.EmplaceArc(s2, fst::fsa::EncodeRange(label[3]), to);
      return;
    }
  }
}

void utf8_emplace_rho_arc_range(
    automaton& a,
    automaton::StateId from,
    automaton::StateId to) {
  const auto id = a.NumStates(); // stated ids are sequential
  a.AddStates(3);

  // add rho transitions
  a.EmplaceArc(from, fst::fsa::EncodeRange(0, 127), to);
  a.EmplaceArc(from, fst::fsa::EncodeRange(192, 223), id);
  a.EmplaceArc(from, fst::fsa::EncodeRange(224, 239), id + 1);
  a.EmplaceArc(from, fst::fsa::EncodeRange(240, 255), id + 2);

  // connect intermediate states of default multi-byte UTF8 sequence
  const auto label = fst::fsa::EncodeRange(128, 191);
  a.EmplaceArc(id, label, to);
  a.EmplaceArc(id + 1, label, id);
  a.EmplaceArc(id + 2, label, id + 1);
}

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
          builder.insert(a, s, rho_state, utf8_arcs.begin(), utf8_arcs.end());
        } break;
      }
    }
  }

#ifdef IRESEARCH_DEBUG
  // Ensure automaton is defined over the alphabet of
  // { [0..255], fst::fsa::kRho }
  for (auto s = 0, nstates = a.NumStates(); s < nstates ; ++s) {
    a.InitArcIterator(s, &arcs);
    auto* begin = arcs.arcs;
    auto* end = begin + arcs.narcs;
    for (; begin != end; ++begin) {
      assert(begin->ilabel == fst::fsa::kRho ||
               (begin->ilabel >= std::numeric_limits<byte_type>::min() &&
                begin->ilabel <= std::numeric_limits<byte_type>::max()));
    }
  }
#endif

  return fst::kNoStateId;
}

void utf8_emplace_rho_arc(
    automaton& a,
    automaton::StateId from,
    automaton::StateId to) {
  const auto id = a.NumStates(); // stated ids are sequential
  a.AddStates(3);
  const automaton::StateId rho_states[] { to, id, id + 1, id + 2 };

  // reserve enough space for arcs
  a.ReserveArcs(from, 255);

  // add rho transitions

  for (automaton::Arc::Label label = MIN; label < 256; ++label) {
    a.EmplaceArc(from, label, rho_states[UTF8_RHO_STATE_TABLE[label]]);
  }

  // connect intermediate states of default multi-byte UTF8 sequence
  a.EmplaceArc(rho_states[1], fst::fsa::kRho, rho_states[0]);
  a.EmplaceArc(rho_states[2], fst::fsa::kRho, rho_states[1]);
  a.EmplaceArc(rho_states[3], fst::fsa::kRho, rho_states[2]);
}

void utf8_emplace_rho_arc_expand(
    automaton& a,
    automaton::StateId from,
    automaton::StateId to) {
  const auto id = a.NumStates(); // stated ids are sequential
  a.AddStates(3);
  const automaton::StateId rho_states[] { to, id, id + 1, id + 2 };

  // reserve enough space for arcs
  a.ReserveArcs(from, 255);
  a.ReserveArcs(rho_states[1], 64);
  a.ReserveArcs(rho_states[2], 64);
  a.ReserveArcs(rho_states[3], 64);

  // add rho transitions

  for (automaton::Arc::Label label = MIN; label < 256; ++label) {
    a.EmplaceArc(from, label, rho_states[UTF8_RHO_STATE_TABLE[label]]);
  }

  // connect intermediate states of default multi-byte UTF8 sequence

  for (automaton::Arc::Label label = 128; label < 192; ++label) {
    a.EmplaceArc(rho_states[1], label, rho_states[0]);
    a.EmplaceArc(rho_states[2], label, rho_states[1]);
    a.EmplaceArc(rho_states[3], label, rho_states[2]);
  }
}

void utf8_transitions_builder::insert(
    automaton& a,
    const byte_type* label,
    const size_t size,
    const automaton::StateId to) {
  assert(label && size < 5);

  add_states(size); // ensure we have enough states
  const size_t prefix = 1 + common_prefix_length(last_.c_str(), last_.size(), label, size);
  minimize(a, prefix); // minimize suffix

  // add current word suffix
  for (size_t i = prefix; i <= size; ++i) {
    auto& p = states_[i - 1];
    p.arcs.emplace_back(label[i - 1], &states_[i]);
    p.rho_id = rho_states_[size - i];
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
      states_.begin(), states_.end(), [](const state& s) noexcept {
        return s.arcs.empty() &&
          s.id == fst::kNoStateId &&
          s.rho_id == fst::kNoStateId;
    }));
  });
#endif

  auto& root = states_.front();
  minimize(a, 1);

  if (fst::kNoStateId == rho_states_[0]) {
    // no default state: just add transitions from the
    // root node to its successors
    for (const auto& arc : root.arcs) {
      a.EmplaceArc(from, arc.label, arc.id);
    }

    root.clear();

    return;
  }

  // in presence of default state we have to add some extra
  // transitions from root to properly handle multi-byte sequences
  // and preserve correctness of arcs order

  auto add_rho_arc = [&a, from, this](automaton::Arc::Label label) {
    const auto rho_state_idx = UTF8_RHO_STATE_TABLE[label];
    a.EmplaceArc(from, label, rho_states_[rho_state_idx]);
  };

  // reserve enough memory to store all outbound transitions

  a.ReserveArcs(from, 255);

  automaton::Arc::Label min = MIN;

  for (const auto& arc : root.arcs) {
    assert(arc.label < 256);
    assert(min <= arc.label); // ensure arcs are sorted

    for (; min < arc.label; ++min) {
      add_rho_arc(min);
    }

    assert(min == arc.label);
    a.EmplaceArc(from, min++, arc.id);
  }

  root.clear();

  // add remaining rho transitions

  for (; min < 256; ++min) {
    add_rho_arc(min);
  }

  // connect intermediate states of default multi-byte UTF8 sequence

  a.EmplaceArc(rho_states_[1], fst::fsa::kRho, rho_states_[0]);
  a.EmplaceArc(rho_states_[2], fst::fsa::kRho, rho_states_[1]);
  a.EmplaceArc(rho_states_[3], fst::fsa::kRho, rho_states_[2]);
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
  multiterm_query::states_t states(index.size());
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
