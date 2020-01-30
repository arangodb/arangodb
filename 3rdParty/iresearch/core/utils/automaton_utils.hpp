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

#ifndef IRESEARCH_AUTOMATON_UTILS_H
#define IRESEARCH_AUTOMATON_UTILS_H

#include "automaton.hpp"
#include "formats/formats.hpp"
#include "search/filter.hpp"

NS_ROOT

inline automaton match_any_char() {
 automaton a;
 const auto start = a.AddState();
 const auto finish = a.AddState();
 a.SetStart(start);
 a.EmplaceArc(start, fst::fsa::kRho, finish);
 a.SetFinal(finish);

 return a;
}

inline automaton match_any() {
 automaton a;
 const auto start = a.AddState();
 a.SetStart(start);
 a.EmplaceArc(start, fst::fsa::kRho, start);
 a.SetFinal(start);

 return a;
}

inline automaton match_char(automaton::Arc::Label c) {
 automaton a;
 const auto start = a.AddState();
 a.SetStart(start);
 a.EmplaceArc(start, c, start);
 a.SetFinal(start);

 return a;
}

template<typename Char, typename Matcher>
bool accept(const automaton& a, Matcher& matcher, const irs::basic_string_ref<Char>& target) {
  auto state = a.Start();
  matcher.SetState(state);

  auto begin = target.begin();
  const auto end = target.end();

  for (; begin < end && matcher.Find(*begin); ++begin) {
    state = matcher.Value().nextstate;
    matcher.SetState(state);
  }

  return begin == end && a.Final(state);
}

template<typename Char>
bool accept(const automaton& a, const irs::basic_string_ref<Char>& target) {
  typedef fst::RhoMatcher<fst::fsa::AutomatonMatcher> matcher_t;

  // FIXME optimize rho label lookup (just check last arc)
  matcher_t matcher(a, fst::MatchType::MATCH_INPUT, fst::fsa::kRho);
  return accept(a, matcher, target);
}

class automaton_term_iterator final : public seek_term_iterator {
 public:
  automaton_term_iterator(const automaton& a, seek_term_iterator::ptr&& it)
    : a_(&a), matcher_(a_, fst::MatchType::MATCH_INPUT, fst::fsa::kRho), it_(std::move(it)) {
    assert(it_);
    value_ = &it_->value();
  }

  virtual const bytes_ref& value() const noexcept override {
    return *value_;
  }

  virtual doc_iterator::ptr postings(const irs::flags& features) const override {
    return it_->postings(features);
  }

  virtual void read() override {
    it_->read();
  }

  virtual bool next() override {
    bool next = it_->next();

    while (next && !accept()) {
      next = it_->next();
    }

    return next;
  }

  virtual const attribute_view& attributes() const noexcept override {
    return it_->attributes();
  }

  virtual SeekResult seek_ge(const bytes_ref& target) override {
    it_->seek_ge(target);

    if (accept()) {
      return SeekResult::FOUND;
    }

    return next() ? SeekResult::NOT_FOUND : SeekResult::END;
  }

  virtual bool seek(const bytes_ref& target) override {
    return SeekResult::FOUND == seek_ge(target);
  }

  virtual bool seek(const bytes_ref& target, const seek_cookie& cookie) override {
    return it_->seek(target, cookie);
  }

  virtual seek_cookie::ptr cookie() const override {
    return it_->cookie();
  }

 private:
  typedef fst::RhoMatcher<fst::fsa::AutomatonMatcher> matcher_t;

  bool accept() { return irs::accept(*a_, matcher_, *value_); }

  const automaton* a_;
  matcher_t matcher_;
  seek_term_iterator::ptr it_;
  const bytes_ref* value_;
}; // automaton_term_iterator

IRESEARCH_API filter::prepared::ptr prepare_automaton_filter(
  const string_ref& field,
  const automaton& acceptor,
  size_t scored_terms_limit,
  const index_reader& index,
  const order::prepared& order,
  boost_t boost);

NS_END

#endif
