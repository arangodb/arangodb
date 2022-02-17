////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_SORTED_RANGE_MATCHER_H
#define IRESEARCH_SORTED_RANGE_MATCHER_H

#include "utils/automaton.hpp"
#include "fst/matcher.h"

namespace fst {

// A matcher that expects sorted labels on the side to be matched.
// If match_type == MATCH_INPUT, epsilons match the implicit self-loop
// Arc(kNoLabel, 0, Weight::One(), current_state) as well as any
// actual epsilon transitions. If match_type == MATCH_OUTPUT, then
// Arc(0, kNoLabel, Weight::One(), current_state) is instead matched.
template <class F, fst::MatchType MatchType = MATCH_INPUT>
class SortedRangeExplicitMatcher : public MatcherBase<typename F::Arc> {
 public:
  using FST = F;
  using Arc = typename FST::Arc;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using MatcherBase<Arc>::Flags;
  using MatcherBase<Arc>::Properties;

  // Labels >= binary_label will be searched for by binary search;
  // o.w. linear search is used.
  // This doesn't copy the FST.
  SortedRangeExplicitMatcher(const FST *fst, Label binary_label = 1)
      : fst_(*fst),
        binary_label_(binary_label),
        error_(false) {
  }

  // This makes a copy of the FST.
  SortedRangeExplicitMatcher(const SortedRangeExplicitMatcher<FST> &matcher, bool safe = false)
      : owned_fst_(matcher.fst_.Copy(safe)),
        fst_(*owned_fst_),
        binary_label_(matcher.binary_label_),
        error_(matcher.error_) {
   }

  ~SortedRangeExplicitMatcher() override { Destroy(aiter_, &aiter_pool_); }

  SortedRangeExplicitMatcher<FST> *Copy(bool safe = false) const override {
    return new SortedRangeExplicitMatcher<FST>(*this, safe);
  }

  fst::MatchType Type(bool test) const override {
    if constexpr (MatchType == MATCH_NONE) return MatchType;
    const auto true_prop =
        MatchType == MATCH_INPUT ? kILabelSorted : kOLabelSorted;
    const auto false_prop =
        MatchType == MATCH_INPUT ? kNotILabelSorted : kNotOLabelSorted;
    const auto props = fst_.Properties(true_prop | false_prop, test);
    if (props & true_prop) {
      return MatchType;
    } else if (props & false_prop) {
      return MATCH_NONE;
    } else {
      return MATCH_UNKNOWN;
    }
  }

  void SetState(StateId s) final {
    if (state_ == s) return;
    state_ = s;
    if constexpr (MatchType == MATCH_NONE) {
      FSTERROR() << "SortedMatcher: Bad match type";
      error_ = true;
    }
    Destroy(aiter_, &aiter_pool_);
    aiter_ = new (&aiter_pool_) ArcIterator<FST>(fst_, s);
    aiter_->SetFlags(kArcNoCache, kArcNoCache);
    narcs_ = internal::NumArcs(fst_, s);
  }

  bool Find(Label match_label) final {
    exact_match_ = true;
    if (error_) {
      match_label_ = kNoLabel;
      return false;
    }
    match_label_ = match_label == kNoLabel ? 0 : match_label;
    if (Search()) {
      return true;
    } else {
      return false;
    }
  }

  // Positions matcher to the first position where inserting match_label would
  // maintain the sort order.
  void LowerBound(Label label) {
    exact_match_ = false;
    if (error_) {
      match_label_ = kNoLabel;
      return;
    }
    match_label_ = label;
    Search();
  }

  // After Find(), returns false if no more exact matches.
  // After LowerBound(), returns false if no more arcs.
  bool Done() const final {
    if (aiter_->Done()) return true;
    if (!exact_match_) return false;
    aiter_->SetFlags(MatchType == MATCH_INPUT ?
        kArcILabelValue : kArcOLabelValue,
        kArcValueFlags);
    return GetLabel() != match_label_;
  }

  const Arc &Value() const final {
    aiter_->SetFlags(kArcValueFlags, kArcValueFlags);
    return aiter_->Value();
  }

  void Next() final {
    aiter_->Next();
  }

  Weight Final(StateId s) const final {
    return MatcherBase<Arc>::Final(s);
  }

  ssize_t Priority(StateId s) final {
    return MatcherBase<Arc>::Priority(s);
  }

  const FST &GetFst() const override { return fst_; }

  uint64 Properties(uint64 inprops) const override {
    return inprops | (error_ ? kError : 0);
  }

  size_t Position() const { return aiter_ ? aiter_->Position() : 0; }

 private:
  constexpr Label GetLabel() const noexcept {
    const auto &arc = aiter_->Value();

    if constexpr (MatchType == MATCH_INPUT) {
      return arc.ilabel;
    }

    return arc.olabel;
  }

  bool BinarySearch();
  bool LinearSearch();
  bool Search();

  std::unique_ptr<const FST> owned_fst_;       // FST ptr if owned.
  const FST &fst_;                             // FST for matching.
  StateId state_{kNoStateId};                  // Matcher state.
  ArcIterator<FST> *aiter_{};                  // Iterator for current state.
  Label binary_label_;                         // Least label for binary search.
  Label match_label_{kNoLabel};                // Current label to be matched.
  size_t narcs_{0};                            // Current state arc count.
  MemoryPool<ArcIterator<FST>> aiter_pool_{1}; // Pool of arc iterators.
  bool exact_match_;                           // Exact match or lower bound?
  bool error_;                                 // Error encountered?
};

// Returns true iff match to match_label_. The arc iterator is positioned at the
// lower bound, that is, the first element greater than or equal to
// match_label_, or the end if all elements are less than match_label_.
// If multiple elements are equal to the `match_label_`, returns the rightmost
// one.
template <class FST, fst::MatchType MatchType>
inline bool SortedRangeExplicitMatcher<FST, MatchType>::BinarySearch() {
  size_t size = narcs_;
  if (size == 0) {
    return false;
  }
  size_t high = size - 1;
  while (size > 1) {
    const size_t half = size / 2;
    const size_t mid = high - half;
    aiter_->Seek(mid);
    const fsa::RangeLabel range{GetLabel()};
    if (range.max >= match_label_) {
      high = mid;
    }
    size -= half;
  }
  aiter_->Seek(high);
  const fsa::RangeLabel range{GetLabel()};
  if (range.min <= match_label_ && range.max >= match_label_) {
    return true;
  }
  if (range.max < match_label_) {
    aiter_->Next();
  }
  return false;
}

// Returns true iff match to match_label_, positioning arc iterator at lower
// bound.
template <class FST, fst::MatchType MatchType>
inline bool SortedRangeExplicitMatcher<FST, MatchType>::LinearSearch() {
  for (aiter_->Reset(); !aiter_->Done(); aiter_->Next()) {
    const fsa::RangeLabel range{GetLabel()};
    if (range.min <= match_label_ && range.max >= match_label_) return true;
    if (range.min > match_label_) break;
  }
  return false;
}

// Returns true iff match to match_label_, positioning arc iterator at lower
// bound.
template <class FST, fst::MatchType MatchType>
inline bool SortedRangeExplicitMatcher<FST, MatchType>::Search() {
  aiter_->SetFlags(MatchType == MATCH_INPUT ?
                   kArcILabelValue : kArcOLabelValue,
                   kArcValueFlags);
  if (match_label_ >= binary_label_) {
    return BinarySearch();
  } else {
    return LinearSearch();
  }
}

} // fst

#endif // IRESEARCH_SORTED_RANGE_MATCHER_H

