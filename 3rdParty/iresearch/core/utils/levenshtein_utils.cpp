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

#include "levenshtein_utils.hpp"

#include <unordered_map>
#include <queue>
#include <cmath>

#include "shared.hpp"
#include "store/store_utils.hpp"
#include "automaton_utils.hpp"
#include "arena_allocator.hpp"
#include "bit_utils.hpp"
#include "bitset.hpp"
#include "map_utils.hpp"
#include "hash_utils.hpp"
#include "utf8_utils.hpp"
#include "misc.hpp"

namespace {

using namespace irs;

// -----------------------------------------------------------------------------
// --SECTION--                    Helpers for parametric description computation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid parametric state
////////////////////////////////////////////////////////////////////////////////
constexpr uint32_t INVALID_STATE = 0;

////////////////////////////////////////////////////////////////////////////////
/// @struct position
/// @brief describes parametric transition related to a certain
///        parametric state
////////////////////////////////////////////////////////////////////////////////
struct position {
  explicit position(
      uint32_t offset = 0,
      byte_type distance = 0,
      bool transpose = false) noexcept
    : offset(offset),
      distance(distance),
      transpose(transpose) {
  }

  bool operator<(const position& rhs) const noexcept {
    if (offset == rhs.offset) {
      if (distance == rhs.distance) {
        return transpose < rhs.transpose;
      }

      return distance < rhs.distance;
    }

    return offset < rhs.offset;
  }

  bool operator==(const position& rhs) const noexcept {
    return offset == rhs.offset &&
        distance == rhs.distance &&
        transpose == rhs.transpose;
  }

  bool operator!=(const position& rhs) const noexcept {
    return !(*this == rhs);
  }

  uint32_t offset{};     // parametric position offset
  byte_type distance{};  // parametric position distance
  bool transpose{false}; // position is introduced by transposition
}; // position

FORCE_INLINE uint32_t abs_diff(uint32_t lhs, uint32_t rhs) noexcept {
  return lhs < rhs ? rhs - lhs : lhs - rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns true if position 'lhs' subsumes 'rhs',
///          i.e. |rhs.offset-lhs.offset| < rhs.distance - lhs.distance
////////////////////////////////////////////////////////////////////////////////
FORCE_INLINE bool subsumes(const position& lhs, const position& rhs) noexcept {
  return (lhs.transpose | !rhs.transpose)
      ? abs_diff(lhs.offset, rhs.offset) + lhs.distance <= rhs.distance
      : abs_diff(lhs.offset, rhs.offset) + lhs.distance <  rhs.distance;
}

////////////////////////////////////////////////////////////////////////////////
/// @class parametric_state
/// @brief describes parametric state of levenshtein automaton, basically a
///        set of positions.
////////////////////////////////////////////////////////////////////////////////
class parametric_state {
 public:
  bool emplace(uint32_t offset, byte_type distance, bool transpose) {
    return emplace(position(offset, distance, transpose));
  }

  bool emplace(const position& new_pos) {
    for (auto& pos : positions_) {
      if (subsumes(pos, new_pos)) {
        // nothing to do
        return false;
      }
    }

    if (!positions_.empty()) {
      for (auto begin = positions_.data(), end = positions_.data() + positions_.size(); begin != end; ) {
        if (subsumes(new_pos, *begin)) {
          // removed positions subsumed by new_pos
          irstd::swap_remove(positions_, begin);
          --end;
        } else {
          ++begin;
        }
      }
    }

    positions_.emplace_back(new_pos);
    return true;
  }

  std::vector<position>::iterator begin() noexcept {
    return positions_.begin();
  }

  std::vector<position>::iterator end() noexcept {
    return positions_.end();
  }

  std::vector<position>::const_iterator begin() const noexcept {
    return positions_.begin();
  }

  std::vector<position>::const_iterator end() const noexcept {
    return positions_.end();
  }

  bool empty() const noexcept { return positions_.empty(); }

  void clear() noexcept { return positions_.clear(); }

  bool operator==(const parametric_state& rhs) const noexcept {
    return positions_ == rhs.positions_;
  }

  bool operator!=(const parametric_state& rhs) const noexcept {
    return !(*this == rhs);
  }

 private:
  std::vector<position> positions_;
}; // parametric_state

static_assert(std::is_nothrow_move_constructible_v<parametric_state>);
static_assert(std::is_nothrow_move_assignable_v<parametric_state>);

////////////////////////////////////////////////////////////////////////////////
/// @class parametric_states
/// @brief container ensures uniquiness of 'parametric_state's
////////////////////////////////////////////////////////////////////////////////
class parametric_states {
 public:
  explicit parametric_states(size_t capacity = 0) {
    if (capacity) {
      states_.reserve(capacity);
      states_by_id_.reserve(capacity);
    }
  }

  uint32_t emplace(parametric_state&& state) {
    const auto res = states_.try_emplace(std::move(state),
                                         states_.size());

    if (res.second) {
      states_by_id_.emplace_back(&res.first->first);
    }

    assert(states_.size() == states_by_id_.size());

    return res.first->second;
  }

  const parametric_state& operator[](size_t i) const noexcept {
    assert(i < states_by_id_.size());
    return *states_by_id_[i];
  }

  size_t size() const noexcept {
    return states_.size();
  }

 private:
  struct parametric_state_hash {
    bool operator()(const parametric_state& state) const noexcept {
      size_t seed = 0;
      for (auto& pos: state) {
        seed = irs::hash_combine(seed, pos.offset);
        seed = irs::hash_combine(seed, pos.distance);
        seed = irs::hash_combine(seed, pos.transpose);
      }
      return seed;
    }
  };

  std::unordered_map<parametric_state, uint32_t, parametric_state_hash> states_;
  std::vector<const parametric_state*> states_by_id_;
}; // parametric_states

////////////////////////////////////////////////////////////////////////////////
/// @brief adds elementary transition denoted by 'pos' to parametric state
///        'state' according to a specified characteristic vector 'chi'
////////////////////////////////////////////////////////////////////////////////
void add_elementary_transitions(
    parametric_state& state,
    const position& pos,
    const uint64_t chi,
    const byte_type max_distance,
    const bool with_transpositions) {
  if (irs::check_bit<0>(chi)) {
    // Situation 1: [i+1,e] subsumes { [i,e+1], [i+1,e+1], [i+1,e] }
    state.emplace(pos.offset + 1, pos.distance, false);

    if (pos.transpose) {
      state.emplace(pos.offset + 2, pos.distance, false);
    }
  }

  if (pos.distance < max_distance) {
    // Situation 2, 3 [i,e+1] - X is inserted before X[i+1]
    state.emplace(pos.offset, pos.distance + 1, false);

    // Situation 2, 3 [i+1,e+1] - X[i+1] is substituted by X
    state.emplace(pos.offset + 1, pos.distance + 1, false);

    // Situation 2, [i+j,e+j-1] - elements X[i+1:i+j-1] are deleted
    for (size_t j = 1, max = max_distance + 1 - pos.distance; j < max; ++j) {
      if (irs::check_bit(chi, j)) {
        state.emplace(pos.offset + 1 + j, pos.distance + j, false);
      }
    }

    if (with_transpositions && irs::check_bit<1>(chi)) {
      state.emplace(pos.offset, pos.distance + 1, true);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds elementary transitions for corresponding transition from
///        parametric state denoted by 'from' to parametric state 'to'
///        according to a specified characteristic vector 'cv'
////////////////////////////////////////////////////////////////////////////////
void add_transition(
    parametric_state& to,
    const parametric_state& from,
    const uint64_t cv,
    const byte_type max_distance,
    const bool with_transpositions) {
  to.clear();
  for (const auto& pos : from) {
    assert(pos.offset < irs::bits_required<decltype(cv)>());
    const auto chi = cv >> pos.offset;
    add_elementary_transitions(to, pos, chi, max_distance, with_transpositions);
  }

  std::sort(to.begin(), to.end());
}

////////////////////////////////////////////////////////////////////////////////
/// @returns size of characteristic vector
////////////////////////////////////////////////////////////////////////////////
FORCE_INLINE uint32_t chi_size(uint32_t max_distance) noexcept {
  return 2*max_distance + 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns max value of characteristic vector
////////////////////////////////////////////////////////////////////////////////
FORCE_INLINE uint64_t chi_max(uint32_t chi_size) noexcept {
  return UINT64_C(1) << chi_size;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns number of states in parametric description according to
///          specified options
////////////////////////////////////////////////////////////////////////////////
size_t predict_num_states(byte_type max_distance, bool with_transpositions) noexcept {
  static constexpr size_t NUM_STATES[] {
    2, 2,        // distance 0
    6, 8,        // distance 1
    31, 68,      // distance 2
    197, 769,    // distance 3
    1354, 9628,  // distance 4
    9714, 0      // distance 5
  };

  const size_t idx = size_t(2)*max_distance + size_t(with_transpositions);
  return idx < IRESEARCH_COUNTOF(NUM_STATES) ? NUM_STATES[idx] : 0;
}

uint32_t normalize(parametric_state& state) noexcept {
  const auto it = std::min_element(
    state.begin(), state.end(),
    [](const position& lhs, const position& rhs) noexcept {
      return lhs.offset < rhs.offset;
  });

  const auto min_offset = (it == state.end() ? 0 : it->offset);

  for (auto& pos : state) {
    pos.offset -= min_offset;
  }

  std::sort(state.begin(), state.end());

  return min_offset;
}

uint32_t distance(
    const parametric_state& state,
    const uint32_t max_distance,
    const uint32_t offset) noexcept {
  assert(max_distance < parametric_description::MAX_DISTANCE);
  uint32_t min_dist = max_distance + 1;

  for (auto& pos : state) {
    const uint32_t dist = pos.distance + abs_diff(offset, pos.offset);

    if (dist < min_dist) {
      min_dist = dist;
    }
  }

  return min_dist;
}

// -----------------------------------------------------------------------------
// --SECTION--                                     Helpers for DFA instantiation
// -----------------------------------------------------------------------------

struct character {
  bitset chi; // characteristic vector
  byte_type utf8[utf8_utils::MAX_CODE_POINT_SIZE]{};
  size_t size{};
  uint32_t cp{}; // utf8 code point

  const byte_type* begin() const noexcept { return utf8; }
  const byte_type* end() const noexcept { return utf8 + size; }
};

//////////////////////////////////////////////////////////////////////////////
/// @return characteristic vectors for a specified word
//////////////////////////////////////////////////////////////////////////////
std::vector<character> make_alphabet(
    const bytes_ref& word,
    size_t& utf8_size) {
  memory::arena<uint32_t, 16> arena;
  memory::arena_vector<uint32_t, decltype(arena)> chars(arena);
  utf8_utils::utf8_to_utf32<false>(word, std::back_inserter(chars));
  utf8_size = chars.size();

  std::sort(chars.begin(), chars.end());
  auto cbegin = chars.begin();
  auto cend = std::unique(cbegin, chars.end()); // no need to erase here

  std::vector<character> alphabet(1 + size_t(std::distance(cbegin, cend))); // +1 for rho
  auto begin = alphabet.begin();

  // ensure we have enough capacity
  const auto capacity = utf8_size + bits_required<bitset::word_t>();

  begin->cp = fst::fsa::kRho;
  begin->chi.reset(capacity);
  ++begin;

  for (; cbegin != cend; ++cbegin, ++begin) {
    const auto c = *cbegin;

    // set code point
    begin->cp = c;

    // set utf8 representation
    begin->size = utf8_utils::utf32_to_utf8(c, begin->utf8);

    // evaluate characteristic vector
    auto& chi = begin->chi;
    chi.reset(capacity);
    auto utf8_begin = word.begin();
    for (size_t i = 0; i < utf8_size; ++i) {
      chi.reset(i, c == utf8_utils::next(utf8_begin));
    }
    IRS_ASSERT(utf8_begin == word.end());
  }

  return alphabet;
}

//////////////////////////////////////////////////////////////////////////////
/// @return characteristic vector for a given character
//////////////////////////////////////////////////////////////////////////////
template<typename Iterator>
uint64_t chi(Iterator begin, Iterator end, uint32_t c) noexcept {
  uint64_t chi = 0;
  for (size_t i = 0; begin < end; ++begin, ++i) {
    chi |= uint64_t(c == *begin) << i;
  }
  return chi;
}

//////////////////////////////////////////////////////////////////////////////
/// @return characteristic vector by a given offset and mask
//////////////////////////////////////////////////////////////////////////////
uint64_t chi(const bitset& bs, size_t offset, uint64_t mask) noexcept {
  auto word = bitset::word(offset);

  auto align = offset - bitset::bit_offset(word);
  if (!align) {
    return bs[word] & mask;
  }

  const auto lhs = bs[word] >> align;
  const auto rhs = bs[word+1] << (bits_required<bitset::word_t>() - align);
  return (lhs | rhs) & mask;
}

}

namespace iresearch {

parametric_description::parametric_description(
    std::vector<transition_t>&& transitions,
    std::vector<byte_type>&& distance,
    byte_type max_distance) noexcept
  : transitions_(std::move(transitions)),
    distance_(std::move(distance)),
    chi_size_(::chi_size(max_distance)),
    chi_max_(::chi_max(chi_size_)), // can't be 0
    num_states_(transitions_.size() / chi_max_),
    max_distance_(max_distance) {
  assert(0 == (transitions_.size() % chi_max_));
  assert(0 == (distance_.size() % chi_size_));
}

parametric_description make_parametric_description(
    byte_type max_distance,
    bool with_transpositions) {
  if (max_distance > parametric_description::MAX_DISTANCE) {
    // invalid parametric description
    return {};
  }

  // predict number of states for known cases
  const size_t num_states = predict_num_states(max_distance, with_transpositions);

  // evaluate shape of characteristic vector
  const uint32_t chi_size = ::chi_size(max_distance);
  const uint64_t chi_max = ::chi_max(chi_size);

  parametric_states states(num_states);
  std::vector<parametric_description::transition_t> transitions;
  if (num_states) {
    transitions.reserve(num_states * chi_max);
  }

  // empty state
  parametric_state to;
  size_t from_id = states.emplace(std::move(to));
  assert(to.empty());

  // initial state
  to.emplace(UINT32_C(0), UINT8_C(0), false);
  states.emplace(std::move(to));
  assert(to.empty());

  for (; from_id != states.size(); ++from_id) {
    for (uint64_t chi = 0; chi < chi_max; ++chi) {
      add_transition(to, states[from_id], chi, max_distance, with_transpositions);

      const auto min_offset = normalize(to);
      const auto to_id = states.emplace(std::move(to));

      transitions.emplace_back(to_id, min_offset);
    }

    // optimization for known cases
    if (num_states && transitions.size() == transitions.capacity()) {
      break;
    }
  }

  std::vector<byte_type> distance(states.size() * chi_size);
  auto begin = distance.begin();
  for (size_t i = 0, size = states.size(); i < size; ++i) {
    auto& state = states[i];
    for (uint32_t offset = 0; offset < chi_size; ++offset, ++begin) {
      *begin = byte_type(::distance(state, max_distance, offset));
    }
  }

  return { std::move(transitions), std::move(distance), max_distance };
}

void write(const parametric_description& description, data_output& out) {
  uint32_t last_state = 0;
  uint32_t last_offset = 0;

  out.write_byte(description.max_distance());

  const auto transitions = description.transitions();
  out.write_vlong(transitions.size());
  for (auto& transition : transitions) {
    write_zvint(out, transition.first - last_state);
    write_zvint(out, transition.second - last_offset);
    last_state = transition.first;
    last_offset = transition.second;
  }

  const auto distances = description.distances();
  out.write_vlong(distances.size());
  out.write_bytes(distances.begin(), distances.size());
}

parametric_description read(data_input& in) {
  const byte_type max_distance = in.read_byte();

  const size_t tcount = in.read_vlong();
  std::vector<parametric_description::transition_t> transitions(tcount);

  uint32_t last_state = 0;
  uint32_t last_offset = 0;
  for (auto& transition : transitions) {
    transition.first = last_state + read_zvint(in);
    transition.second = last_offset + read_zvint(in);
    last_state = transition.first;
    last_offset = transition.second;
  }

  const size_t dcount = in.read_vlong();
  std::vector<byte_type> distances(dcount);
  in.read_bytes(distances.data(), distances.size());

  return { std::move(transitions), std::move(distances), max_distance };
}

automaton make_levenshtein_automaton(
    const parametric_description& description,
    const bytes_ref& target) {
  assert(description);

  struct state {
    state(size_t offset, uint32_t state_id, automaton::StateId from) noexcept
      : offset(offset), state_id(state_id), from(from) {
    }

    size_t offset;           // state offset
    uint32_t state_id;       // corresponding parametric state
    automaton::StateId from; // automaton state
  };

  size_t utf8_size;
  const auto alphabet = make_alphabet(target, utf8_size);
  const auto num_offsets = 1 + utf8_size;
  const uint64_t mask = (UINT64_C(1) << description.chi_size()) - 1;

  // transitions table of resulting automaton
  std::vector<automaton::StateId> transitions(description.size()*num_offsets,
                                              fst::kNoStateId);

  automaton a;
  a.ReserveStates(transitions.size());

  // terminal state without outbound transitions
  const auto invalid_state = a.AddState();
  assert(INVALID_STATE == invalid_state);
  UNUSED(invalid_state);

  // initial state
  a.SetStart(a.AddState());

  // check if start state is final
  const auto distance = description.distance(1, utf8_size);

  if (distance <= description.max_distance()) {
    a.SetFinal(a.Start(), {true, distance});
  }

  // state stack
  std::vector<state> stack;
  stack.emplace_back(0, 1, a.Start());  // 0 offset, 1st parametric state, initial automaton state

  std::vector<std::pair<bytes_ref, automaton::StateId>> arcs;
  arcs.resize(utf8_size); // allocate space for max possible number of arcs

  for (utf8_transitions_builder builder; !stack.empty(); ) {
    const auto state = stack.back();
    stack.pop_back();
    arcs.clear();

    automaton::StateId default_state = fst::kNoStateId; // destination of rho transition if exist
    bool ascii = true; // ascii only input

    for (auto& entry : alphabet) {
      const auto chi = ::chi(entry.chi, state.offset, mask);
      auto& transition = description.transition(state.state_id, chi);

      const size_t offset = transition.first ? transition.second + state.offset : 0;
      assert(transition.first*num_offsets + offset < transitions.size());
      auto& to = transitions[transition.first*num_offsets + offset];

      if (INVALID_STATE == transition.first) {
        to = INVALID_STATE;
      } else if (fst::kNoStateId == to) {
        to = a.AddState();

        const auto distance = description.distance(transition.first, utf8_size - offset);

        if (distance <= description.max_distance()) {
          a.SetFinal(to, {true, distance});
        }

        stack.emplace_back(offset, transition.first, to);
      }

      if (chi && to != default_state) {
        arcs.emplace_back(bytes_ref(entry.utf8, entry.size), to);
        ascii &= (entry.size == 1);
      } else {
        assert(fst::kNoStateId == default_state || to == default_state);
        default_state = to;
      }
    }

    if (INVALID_STATE == default_state && arcs.empty()) {
      // optimization for invalid terminal state
      a.EmplaceArc(state.from, fst::fsa::kRho, INVALID_STATE);
    } else if (INVALID_STATE == default_state && ascii && !a.Final(state.from)) {
      // optimization for ascii only input without default state and weight
      for (auto& arc: arcs) {
        assert(1 == arc.first.size());
        a.EmplaceArc(state.from, arc.first.front(), arc.second);
      }
    } else {
      builder.insert(a, state.from, default_state, arcs.begin(), arcs.end());
    }
  }

#ifdef IRESEARCH_DEBUG
  // ensure resulting automaton is sorted and deterministic
  static constexpr auto EXPECTED_PROPERTIES =
    fst::kIDeterministic |
    fst::kILabelSorted | fst::kOLabelSorted |
    fst::kAcceptor | fst::kUnweighted;
  assert(EXPECTED_PROPERTIES == a.Properties(EXPECTED_PROPERTIES, true));
  UNUSED(EXPECTED_PROPERTIES);

  // ensure invalid state has no outbound transitions
  assert(0 == a.NumArcs(INVALID_STATE));
#endif

  return a;
}

size_t edit_distance(const parametric_description& description,
                     const byte_type* lhs, size_t lhs_size,
                     const byte_type* rhs, size_t rhs_size) {
  assert(description);

  memory::arena<uint32_t, 16> arena;
  memory::arena_vector<uint32_t, decltype(arena)> lhs_chars(arena);
  utf8_utils::utf8_to_utf32<false>(lhs, lhs_size, std::back_inserter(lhs_chars));

  size_t state = 1;  // current parametric state
  size_t offset = 0; // current offset

  for (auto* rhs_end = rhs + rhs_size; rhs < rhs_end; ) {
    const auto c = utf8_utils::next(rhs);

    const auto begin = lhs_chars.begin() + ptrdiff_t(offset);
    const auto end = std::min(begin + ptrdiff_t(description.chi_size()), lhs_chars.end());
    const auto chi = ::chi(begin, end, c);
    const auto& transition = description.transition(state, chi);

    if (INVALID_STATE == transition.first) {
      return description.max_distance() + 1;
    }

    state = transition.first;
    offset += transition.second;
  }

  return description.distance(state, lhs_chars.size() - offset);
}

bool edit_distance(
    size_t& distance,
    const parametric_description& description,
    const byte_type* lhs, size_t lhs_size,
    const byte_type* rhs, size_t rhs_size) {
  assert(description);

  memory::arena<uint32_t, 16> arena;
  memory::arena_vector<uint32_t, decltype(arena)> lhs_chars(arena);
  if (!utf8_utils::utf8_to_utf32<true>(lhs, lhs_size, std::back_inserter(lhs_chars))) {
    return false;
  }

  size_t state = 1;  // current parametric state
  size_t offset = 0; // current offset

  for (auto* rhs_end = rhs + rhs_size; rhs < rhs_end; ) {
    const auto c = utf8_utils::next_checked(rhs, rhs_end);

    if (utf8_utils::INVALID_CODE_POINT == c) {
      return false;
    }

    const auto begin = lhs_chars.begin() + ptrdiff_t(offset);
    const auto end = std::min(begin + ptrdiff_t(description.chi_size()), lhs_chars.end());
    const auto chi = ::chi(begin, end, c);
    const auto& transition = description.transition(state, chi);

    if (INVALID_STATE == transition.first) {
      distance = description.max_distance() + 1;
      return true;
    }

    state = transition.first;
    offset += transition.second;
  }

  distance = description.distance(state, lhs_chars.size() - offset);
  return true;
}

}
