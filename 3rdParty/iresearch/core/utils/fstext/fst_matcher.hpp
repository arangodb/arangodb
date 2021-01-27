////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FST_MATCHER_H
#define IRESEARCH_FST_MATCHER_H

#include "shared.hpp"

namespace fst {

// This class discards any implicit matches (e.g., the implicit epsilon
// self-loops in the SortedMatcher). Matchers are most often used in
// composition/intersection where the implicit matches are needed
// e.g. for epsilon processing. However, if a matcher is simply being
// used to look-up explicit label matches, this class saves the user
// from having to check for and discard the unwanted implicit matches
// themselves.
template <class MatcherImpl>
class explicit_matcher final : public MatcherBase<typename MatcherImpl::FST::Arc> {
 public:
  typedef typename MatcherImpl::FST FST;
  typedef typename FST::Arc Arc;
  typedef typename Arc::Label Label;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  template<typename... Args>
  explicit explicit_matcher(Args&&... args)
    : matcher_(std::forward<Args>(args)...),
#ifdef IRESEARCH_DEBUG
      match_type_(matcher_.Type(true)) // test fst properties
#else
      match_type_(matcher_.Type(false)) // read type without checks
#endif
  { }

  explicit_matcher(const explicit_matcher& rhs, bool safe = false)
    : matcher_(rhs.matcher_, safe),
      match_type_(rhs.match_type_) {
  }

  explicit_matcher* Copy(bool safe = false) const override {
    return new explicit_matcher(*this, safe);
  }

  MatchType Type(bool test) const override {
    return matcher_.Type(test);
  }

  void SetState(StateId s) override {
    matcher_.SetState(s);
  }

  bool Find(Label match_label) override {
    matcher_.Find(match_label);
    CheckArc();
    return !Done();
  }

  bool Done() const override { return matcher_.Done(); }

  const Arc& Value() const override { return matcher_.Value(); }

  void Next() override {
    matcher_.Next();
    CheckArc();
  }

  Weight Final(StateId s) const override {
    return matcher_.Final(s);
  }

  ssize_t Priority(StateId s) override {
    return  matcher_.Priority(s);
  }

  const FST& GetFst() const override {
    return matcher_.GetFst();
  }

  uint64 Properties(uint64 inprops) const override {
    return matcher_.Properties(inprops);
  }

  uint32 Flags() const override {
    return matcher_.Flags();
  }

 private:
  // Checks current arc if available and explicit. If not available, stops. If
  // not explicit, checks next ones.
  void CheckArc() {
    for (; !matcher_.Done(); matcher_.Next()) {
      const auto& label = match_type_ == MATCH_INPUT ? matcher_.Value().ilabel
                                                     : matcher_.Value().olabel;
      if (label != kNoLabel) return;
    }
  }

  MatcherImpl matcher_;
  MatchType match_type_;  // Type of match requested.
}; // explicit_matcher

} // fst

#endif  // IRESEARCH_FST_MATHCER_H
