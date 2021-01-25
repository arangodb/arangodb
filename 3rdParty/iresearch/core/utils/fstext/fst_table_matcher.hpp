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

#ifndef IRESEARCH_TABLE_MATCHER_H
#define IRESEARCH_TABLE_MATCHER_H

#include <algorithm>

#include "fst/matcher.h"
#include "utils/misc.hpp"
#include "utils/integer.hpp"
#include "utils/std.hpp"

namespace fst {

template<typename F, bool MatchInput = true>
std::vector<typename F::Arc::Label> getStartLabels(const F& fst) {
  std::set<typename F::Arc::Label> labels;
  for (StateIterator<F> siter(fst); !siter.Done(); siter.Next()) {
    const auto state = siter.Value();
    for (ArcIterator<F> aiter(fst, state); !aiter.Done(); aiter.Next()) {
      const auto& arc = aiter.Value();
      labels.emplace(MatchInput ? arc.ilabel : arc.olabel);
    }
  }

  return { labels.begin(), labels.end() };
}

template<typename F, size_t CacheSize = 256, bool MatchInput = true>
class TableMatcher final : public MatcherBase<typename F::Arc> {
 public:
  using FST = F;
  using Arc = typename FST::Arc;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using MatcherBase<Arc>::Flags;
  using MatcherBase<Arc>::Properties;

  static constexpr fst::MatchType MATCH_TYPE = MatchInput
    ? fst::MATCH_INPUT
    : fst::MATCH_OUTPUT;

  // expected FST properties
  static constexpr auto FST_PROPERTIES =
    (MATCH_TYPE == MATCH_INPUT ? kILabelSorted : kOLabelSorted)
    | (MATCH_TYPE == MATCH_INPUT ? kIDeterministic : kODeterministic)
    | kAcceptor;

  explicit TableMatcher(const FST& fst, Label rho)
    : start_labels_(fst::getStartLabels<F, MatchInput>(fst)),
      arc_(kNoLabel, kNoLabel, Weight::NoWeight(), kNoStateId),
      rho_(rho), fst_(&fst),
      error_(fst.Properties(FST_PROPERTIES, true) != FST_PROPERTIES) {
    const size_t numLabels = start_labels_.size();

    // initialize transition table
    ArcIteratorData<Arc> data;
    transitions_.resize(fst.NumStates()*numLabels , kNoStateId);
    for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
      const auto state = siter.Value();

      fst.InitArcIterator(state, &data);

      // fill rho transitions if any
      {
        std::reverse_iterator<decltype(data.arcs)> rbegin(data.arcs + data.narcs);
        std::reverse_iterator<decltype(data.arcs)> rend(data.arcs);

        for (; rbegin != rend; ++rbegin) {
          if (rho_ == get_label(*rbegin)) {
            std::fill_n(transitions_.begin() + state*numLabels, numLabels, rbegin->nextstate);
            break;
          }
        }
      }

      // fill existing transitions
      auto arc = data.arcs;
      auto arc_end = data.arcs + data.narcs;
      auto label = start_labels_.begin();
      auto label_end = start_labels_.end();
      for (; arc != arc_end && label != label_end;) {
        for (; arc != arc_end && get_label(*arc) < *label; ++arc) { }

        if (arc == arc_end) {
          break;
        }

        for (; label != label_end  && get_label(*arc) > *label; ++label) { }

        if (label == label_end) {
          break;
        }

        if (get_label(*arc) == *label) {
          transitions_[state*numLabels + std::distance(start_labels_.begin(), label)] = arc->nextstate;
          ++label;
          ++arc;
        }
      }
    }

    // initialize lookup table for first CacheSize labels,
    // code below is the optimized version of:
    // for (size_t i = 0; i < CacheSize; ++i) {
    //   cached_label_offsets_[i] = find_label_offset(i);
    // }
    auto begin = start_labels_.begin();
    auto end = start_labels_.end();
    for (size_t i = 0, offset = 0;
         i < IRESEARCH_COUNTOF(cached_label_offsets_); ++i) {
      if (begin != end && size_t(*begin) == i) {
        cached_label_offsets_[i] = offset;
        ++offset;
        ++begin;
      } else {
        cached_label_offsets_[i] = numLabels;
      }
    }
  }

  virtual TableMatcher* Copy(bool) const override {
    return new TableMatcher(*this);
  }

  virtual MatchType Type(bool test) const override {
    if constexpr (MATCH_TYPE == MATCH_NONE) {
      return MATCH_TYPE;
    }

    constexpr const auto true_prop = (MATCH_TYPE == MATCH_INPUT)
      ? kILabelSorted
      : kOLabelSorted;

    constexpr const auto false_prop = (MATCH_TYPE == MATCH_INPUT)
      ? kNotILabelSorted
      : kNotOLabelSorted;

    const auto props = fst_->Properties(true_prop | false_prop, test);

    if (props & true_prop) {
      return MATCH_TYPE;
    } else if (props & false_prop) {
      return MATCH_NONE;
    } else {
      return MATCH_UNKNOWN;
    }
  }

  virtual void SetState(StateId s) noexcept final {
    assert(!error_);
    assert(s*start_labels_.size() < transitions_.size());
    state_begin_ = transitions_.data() + s*start_labels_.size();
    state_ = state_begin_;
    state_end_ = state_begin_ + start_labels_.size();
  }

  virtual bool Find(Label label) noexcept final {
    assert(!error_);
    auto label_offset = (size_t(label) < IRESEARCH_COUNTOF(cached_label_offsets_)
                           ? cached_label_offsets_[size_t(label)]
                           : find_label_offset(label));

    if (label_offset == start_labels_.size()) {
      if (start_labels_.back() != rho_) {
        state_ = state_end_;
        return false;
      }

      label_offset = start_labels_.size() - 1;
    }

    state_ = state_begin_ + label_offset;
    assert(state_ < state_end_);
    arc_.nextstate = *state_;
    return arc_.nextstate != kNoStateId;
  }

  virtual bool Done() const noexcept final {
    assert(!error_);
    return state_ == state_end_;
  }

  virtual const Arc& Value() const noexcept final {
    assert(!error_);
    return arc_;
  }

  virtual void Next() noexcept final {
    assert(!error_);

    if (Done()) {
      return;
    }

    ++state_;

    for (; !Done(); ++state_) {
      if (*state_ != kNoLabel) {
        assert(state_ > state_begin_ && state_ < state_end_);
        const auto label = start_labels_[size_t(std::distance(state_begin_, state_))];
        if constexpr (MATCH_TYPE == MATCH_INPUT) {
          arc_.ilabel = label;
        } else {
          arc_.olabel = label;
        }
        arc_.nextstate = *state_;
        return;
      }
    }
  }

  virtual Weight Final(StateId s) const final {
    return MatcherBase<Arc>::Final(s);
  }

  virtual ssize_t Priority(StateId s) final {
    return MatcherBase<Arc>::Priority(s);
  }

  virtual const FST& GetFst() const noexcept override {
    return *fst_;
  }

  virtual uint64 Properties(uint64 inprops) const noexcept override {
    return inprops | (error_ ? kError : 0);
  }

 private:
  template<typename Arc>
  static typename Arc::Label get_label(Arc& arc) {
    if constexpr (MATCH_TYPE == MATCH_INPUT) {
      return arc.ilabel;
    }

    return arc.olabel;
  }

  size_t find_label_offset(Label label) const noexcept {
    const auto it = std::lower_bound(start_labels_.begin(), start_labels_.end(), label);

    if (it == start_labels_.end() || *it != label) {
      return start_labels_.size();
    }

    assert(it != start_labels_.end());
    assert(start_labels_.begin() <= it);
    return size_t(std::distance(start_labels_.begin(), it));
  }

  size_t cached_label_offsets_[CacheSize]{};
  std::vector<Label> start_labels_;
  std::vector<StateId> transitions_;
  Arc arc_;
  Label rho_;
  const FST* fst_;                   // FST for matching
  const Label* state_begin_{};       // Matcher state begin
  const Label* state_end_{};         // Matcher state end
  const Label* state_{};             // Matcher current state
  bool error_;                       // Matcher validity
}; // TableMatcher

} // fst

#endif
