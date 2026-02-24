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

#pragma once

#include "absl/strings/str_cat.h"
#include "analysis/token_attributes.hpp"
#include "formats/formats.hpp"
#include "fst/closure.h"
#include "search/filter.hpp"
#include "utils/automaton.hpp"
#include "utils/fstext/fst_sorted_range_matcher.hpp"
#include "utils/fstext/fst_states_map.hpp"
#include "utils/fstext/fst_table_matcher.hpp"
#include "utils/hash_utils.hpp"
#include "utils/utf8_utils.hpp"

namespace irs {

#ifdef IRESEARCH_DEBUG
inline constexpr bool kTestAutomatonProps = true;
#else
inline constexpr bool kTestAutomatonProps = false;
#endif
inline constexpr auto kRhoLabel = RangeLabel::From(0x80, 0xBF);
// C0..C1 -- not allowed
// F5..FF -- not allowed
// It doesn't account 4 special cases:
// E0 A0..BF 80..BF
// ED 80..9F 80..BF
// F0 90..BF 80..BF 80..BF
// F4 80..8F 80..BF 80..BF
inline constexpr auto kUTF8Byte1 = RangeLabel::From(0x00, 0x7F);
inline constexpr auto kUTF8Byte2 = RangeLabel::From(0xC2, 0xDF);
inline constexpr auto kUTF8Byte3 = RangeLabel::From(0xE0, 0xEF);
inline constexpr auto kUTF8Byte4 = RangeLabel::From(0xF0, 0xF4);

struct filter_visitor;

inline auto MakeAutomatonMatcher(const automaton& a,
                                 bool test_props = kTestAutomatonProps) {
  return automaton_table_matcher{a, test_props};
}

template<typename Char, typename Matcher>
inline automaton::Weight Match(Matcher& matcher,
                               basic_string_view<Char> target) {
  auto state = matcher.GetFst().Start();
  matcher.SetState(state);

  auto begin = target.begin();
  const auto end = target.end();

  for (; begin < end && matcher.Find(*begin); ++begin) {
    state = matcher.Value().nextstate;
    matcher.SetState(state);
  }

  return begin == end ? matcher.Final(state) : automaton::Weight::Zero();
}

template<typename Char>
inline automaton::Weight Accept(const automaton& a,
                                basic_string_view<Char> target) {
  using Matcher =
    fst::SortedRangeExplicitMatcher<automaton, fst::MatchType::MATCH_INPUT>;

  Matcher matcher{&a};
  return Match(matcher, target);
}

class AutomatonTermIterator : public seek_term_iterator {
 public:
  AutomatonTermIterator(const automaton& a, seek_term_iterator::ptr&& it)
    : a_(&a), matcher_(a_), it_(std::move(it)) {
    IRS_ASSERT(it_);
    const auto* term = irs::get<term_attribute>(*it_);
    IRS_ASSERT(term);
    value_ = &term->value;
  }

  bytes_view value() const noexcept final { return *value_; }

  doc_iterator::ptr postings(IndexFeatures features) const final {
    return it_->postings(features);
  }

  void read() final { it_->read(); }

  bool next() final {
    bool next = it_->next();

    while (next && !Accept()) {
      next = it_->next();
    }

    return next;
  }

  attribute* get_mutable(type_info::type_id type) noexcept final {
    return it_->get_mutable(type);
  }

  SeekResult seek_ge(bytes_view target) final {
    it_->seek_ge(target);

    if (Accept()) {
      return SeekResult::FOUND;
    }

    return next() ? SeekResult::NOT_FOUND : SeekResult::END;
  }

  bool seek(bytes_view target) final {
    return SeekResult::FOUND == seek_ge(target);
  }

  seek_cookie::ptr cookie() const final { return it_->cookie(); }

 private:
  bool Accept() { return Match(matcher_, *value_); }

  const automaton* a_;
  fst::SortedRangeExplicitMatcher<automaton> matcher_;
  seek_term_iterator::ptr it_;
  const bytes_view* value_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief add a new transition to "to" state denoted by "label" or expands
///        the last already existing one
//////////////////////////////////////////////////////////////////////////////
inline void AddOrExpandArc(automaton& a, automaton::StateId from,
                           RangeLabel label, automaton::StateId to) {
  IRS_ASSERT(fst::kILabelSorted == a.Properties(fst::kILabelSorted, true));

  fst::ArcIteratorData<automaton::Arc> data;
  a.InitArcIterator(from, &data);

  if (data.narcs) {
    auto& prev = const_cast<automaton::Arc&>(data.arcs[data.narcs - 1]);
    IRS_ASSERT(prev < label);
    if (prev.nextstate == to && label.min - prev.max <= 1) {
      prev.ilabel = RangeLabel::From(prev.min, label.max);
      return;
    }
  }

  a.EmplaceArc(from, label, to);
}

/// helper class for building minimal acyclic binary automaton from a specified
/// root, a default (rho) state and a set of arcs with UTF-8 encoded labels
class Utf8TransitionsBuilder {
 public:
  Utf8TransitionsBuilder() : states_map_{16, StateEmplace{weight_}} {}

  template<typename Iterator>
  void Insert(automaton& a, automaton::StateId from,
              automaton::StateId rho_state, Iterator begin, Iterator end) {
    last_ = kEmptyStringView<irs::byte_type>;
    states_map_.reset();

    // we inherit weight from 'from' node to all intermediate states
    // that were created by transitions builder
    weight_ = a.Final(from);

    // 'from' state is already a part of automaton
    states_[0].id = from;

    std::fill(std::begin(rho_states_), std::end(rho_states_), rho_state);

    if (rho_state != fst::kNoStateId) {
      // create intermediate default states if necessary
      a.SetFinal(rho_states_[1] = a.AddState(), weight_);
      a.SetFinal(rho_states_[2] = a.AddState(), weight_);
      a.SetFinal(rho_states_[3] = a.AddState(), weight_);
    }

    for (; begin != end; ++begin) {
      // we expect sorted input
      IRS_ASSERT(last_ <= static_cast<bytes_view>(std::get<0>(*begin)));

      const auto& label = std::get<0>(*begin);
      Insert(a, label.data(), label.size(), std::get<1>(*begin));
      last_ = static_cast<bytes_view>(label);
    }

    Finish(a, from);
  }

 private:
  struct State;

  struct Arc : public RangeLabel, private util::noncopyable {
    Arc(automaton::Arc::Label label, State* target) noexcept
      : RangeLabel{label}, target{target} {}

    Arc(automaton::Arc::Label label, automaton::StateId target) noexcept
      : RangeLabel{label}, id{target} {}

    bool operator==(const automaton::Arc& rhs) const noexcept {
      return ilabel == rhs.ilabel && id == rhs.nextstate;
    }

    union {
      State* target;
      automaton::StateId id;
    };
  };

  struct State : private util::noncopyable {
    void Clear() noexcept {
      id = fst::kNoStateId;
      arcs.clear();
    }

    void AddRhoArc(uint32_t min, uint32_t max, automaton::StateId rho) {
      if (fst::kNoStateId == rho) {
        return;
      }

      if (!arcs.empty()) {
        min = arcs.back().max + 1;
      }

      if (min < max) {
        arcs.emplace_back(RangeLabel::From(min, max - 1), rho);
      }
    }

    automaton::StateId id{fst::kNoStateId};
    std::vector<Arc> arcs;
  };

  static_assert(std::is_nothrow_move_constructible_v<State>);

  struct StateHash {
    size_t operator()(const State& s, const automaton& fst) const noexcept {
      if (fst::kNoStateId != s.id) {
        return operator()(s.id, fst);
      }

      size_t hash = 0;
      const auto& arcs = s.arcs;

      for (const auto& arc : arcs) {
        hash = hash_combine(hash, arc.ilabel);
        // cppcheck-suppress redundantAssignment
        hash = hash_combine(hash, arc.id);
      }

      return hash;
    }

    size_t operator()(automaton::StateId id,
                      const automaton& fst) const noexcept {
      fst::ArcIteratorData<automaton::Arc> arcs;
      fst.InitArcIterator(id, &arcs);

      const auto* begin = arcs.arcs;
      const auto* end = arcs.arcs + arcs.narcs;

      size_t hash = 0;
      for (; begin != end; ++begin) {
        hash = hash_combine(hash, begin->ilabel);
        // cppcheck-suppress redundantAssignment
        hash = hash_combine(hash, begin->nextstate);
      }

      return hash;
    }
  };

  struct StateEqual {
    bool operator()(const State& lhs, automaton::StateId rhs,
                    const automaton& fst) const noexcept {
      if (lhs.id != fst::kNoStateId) {
        // already a part of automaton
        return lhs.id == rhs;
      }

      fst::ArcIteratorData<automaton::Arc> rarcs;
      fst.InitArcIterator(rhs, &rarcs);

      if (lhs.arcs.size() != rarcs.narcs) {
        return false;
      }

      const auto* rarc = rarcs.arcs;
      for (const auto& larc : lhs.arcs) {
        if (larc != *rarc) {
          return false;
        }
        ++rarc;
      }

      return true;
    }
  };

  class StateEmplace {
   public:
    explicit StateEmplace(const automaton::Weight& weight) noexcept
      : weight_(&weight) {}

    automaton::StateId operator()(const State& s, automaton& fst) const {
      auto id = s.id;

      if (id == fst::kNoStateId) {
        id = fst.AddState();
        fst.SetFinal(id, *weight_);
      }

      for (const auto& a : s.arcs) {
        fst.EmplaceArc(id, a.ilabel, a.id);
      }

      return id;
    }

   private:
    const automaton::Weight* weight_;
  };

  using AutomatonStatesMap =
    fst_states_map<automaton, State, StateEmplace, StateHash, StateEqual,
                   fst::kNoStateId>;

  void Minimize(automaton& a, size_t prefix);

  void Insert(automaton& a, const byte_type* label, size_t size,
              automaton::StateId to);

  void Finish(automaton& a, automaton::StateId from);

  // FIXME use a memory pool for arcs
  automaton::Weight weight_;
  automaton::StateId rho_states_[4];
  State states_[utf8_utils::kMaxCharSize + 1];  // +1 for root state
  AutomatonStatesMap states_map_;
  bytes_view last_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief validate a specified automaton and print message on error
//////////////////////////////////////////////////////////////////////////////
inline bool Validate(const automaton& a,
                     bool test_props = kTestAutomatonProps) {
  if (a.Properties(automaton_table_matcher::FST_PROPERTIES, test_props) !=
      fst::kError) {
    return true;
  }
  IRS_LOG_ERROR(absl::StrCat(
    "Expected deterministic, epsilon-free acceptor, got the following "
    "properties ",
    a.Properties(automaton_table_matcher::FST_PROPERTIES, false)));
  return false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief generalized field visitation logic for automaton based filters
/// @param segment segment reader
/// @param field term reader
/// @param matcher input matcher
/// @param visitor visitor
//////////////////////////////////////////////////////////////////////////////
template<typename Visitor>
void Visit(const SubReader& segment, const term_reader& reader,
           automaton_table_matcher& matcher, Visitor& visitor) {
  IRS_ASSERT(fst::kError != matcher.Properties(0));
  auto terms = reader.iterator(matcher);

  if (IRS_UNLIKELY(!terms)) {
    return;
  }

  if (terms->next()) {
    visitor.prepare(segment, reader, *terms);

    do {
      terms->read();

      visitor.visit(kNoBoost);
    } while (terms->next());
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief establish UTF-8 labeled connection between specified source and
///        target states
//////////////////////////////////////////////////////////////////////////////
void Utf8EmplaceArc(automaton& a, automaton::StateId from, bytes_view label,
                    automaton::StateId to);

//////////////////////////////////////////////////////////////////////////////
/// @brief establish default connnection between specified source (from) and
///        and target (to)
//////////////////////////////////////////////////////////////////////////////
void Utf8EmplaceRhoArc(automaton& a, automaton::StateId from,
                       automaton::StateId to);

inline automaton MakeChar(bytes_view c) {
  automaton a;
  a.AddStates(2);
  a.SetStart(0);
  a.SetFinal(1);
  Utf8EmplaceArc(a, 0, c, 1);
  return a;
}

inline automaton MakeAny() {
  automaton a;
  a.AddStates(2);
  a.SetStart(0);
  a.SetFinal(1);
  Utf8EmplaceRhoArc(a, 0, 1);
  return a;
}

inline automaton MakeAll() {
  // TODO(MBkkt) speedup: We can just make such automaton
  // S ->  0..FF -> F
  // F -> epsilon -> S
  auto a = MakeAny();
  fst::Closure(&a, fst::ClosureType::CLOSURE_STAR);
  return a;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief instantiate compiled filter based on a specified automaton, field
///        and other properties
/// @param field field name
/// @param matcher input matcher
/// @param scored_terms_limit score as many terms
/// @param index index reader
/// @param order compiled order
/// @param bool query boost
/// @returns compiled filter
//////////////////////////////////////////////////////////////////////////////
filter::prepared::ptr PrepareAutomatonFilter(const PrepareContext& ctx,
                                             std::string_view field,
                                             const automaton& acceptor,
                                             size_t scored_terms_limit);

}  // namespace irs
