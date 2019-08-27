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

NS_ROOT

typedef fst::fsa::Automaton automaton;

template<typename Char>
struct wildcard_traits {
  using char_type = Char;

  // match any string or empty string
  static constexpr Char MATCH_ANY_STRING= Char('%');

  // match any char
  static constexpr Char MATCH_ANY_CHAR = Char('_');

  // denotes beginning of escape sequence
  static constexpr Char ESCAPE = Char('\\');
};

template<
  typename Char,
  typename Traits = wildcard_traits<Char>,
  typename = typename std::enable_if<sizeof(Char) < sizeof(fst::fsa::kMaxLabel)>::type,
  typename = typename std::enable_if<Traits::MATCH_ANY_CHAR != Traits::MATCH_ANY_STRING>::type>
automaton from_wildcard(const irs::basic_string_ref<Char>& expr) {
  automaton a;
  a.ReserveStates(expr.size() + 1);

  automaton::StateId from = a.AddState();
  automaton::StateId to = from;
  a.SetStart(from);

  bool escaped = false;
  auto appendChar = [&escaped, &a, &to, &from](Char c) {
    to = a.AddState();
    a.EmplaceArc(from, c, to);
    from = to;
    escaped = false;
  };

  for (const auto c : expr) {
    switch (c) {
      case Traits::MATCH_ANY_STRING: {
        if (escaped) {
          appendChar(c);
        } else {
          a.EmplaceArc(from, fst::fsa::kRho, from);
        }
        break;
      }
      case Traits::MATCH_ANY_CHAR: {
        if (escaped) {
          appendChar(c);
        } else {
          to = a.AddState();
          a.EmplaceArc(from, fst::fsa::kRho, to);
          from = to;
        }
      } break;
      case Traits::ESCAPE: {
       if (escaped) {
         appendChar(c);
       } else {
         escaped = !escaped;
       }
       break;
      }
      default: {
        appendChar(c);
        break;
      }
    }
  }

  // non-terminated escape sequence
  if (escaped) {
    appendChar(Traits::ESCAPE);
  }

  a.SetFinal(to);

  fst::ArcSort(&a, fst::ILabelCompare<fst::fsa::Transition>());

  automaton res;
  fst::Determinize(a, &res);

  return res;
}

template<typename Char>
bool accept(const automaton& a, const irs::basic_string_ref<Char>& target) {
  typedef fst::SortedMatcher<fst::fsa::Automaton> sorted_matcher_t;
  typedef fst::RhoMatcher<sorted_matcher_t> matcher_t;

  // FIXME optimize rho label lookup (just check last arc)
  matcher_t matcher(a, fst::MatchType::MATCH_INPUT, fst::fsa::kRho);

  auto state = a.Start();
  matcher.SetState(state);

  auto begin = target.begin();
  auto end = target.end();
  for (; begin < end && matcher.Find(*begin); ++begin) {
    state = matcher.Value().nextstate;
    matcher.SetState(state);
  }

  return begin == end && matcher.Final(state);
}

class intersect_term_iterator final : public seek_term_iterator {
 public:
  intersect_term_iterator(const automaton& a, seek_term_iterator::ptr&& it) noexcept
    : a_(&a), it_(std::move(it)) {
    assert(it_);
    value_ = &it_->value();
  }

  virtual const bytes_ref& value() const override {
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

    while (next && accept()) {
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
  bool accept() { return irs::accept(*a_, *value_); }

  const automaton* a_;
  seek_term_iterator::ptr it_;
  const bytes_ref* value_;
}; // intersect_term_iterator

NS_END

#endif
