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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_DISJUNCTION_H
#define IRESEARCH_DISJUNCTION_H

#include "conjunction.hpp"
#include "utils/std.hpp"
#include "utils/type_limits.hpp"
#include "index/iterators.hpp"

#include <queue>

NS_ROOT
NS_BEGIN(detail)

// Need this proxy since Microsoft has heap validity check in std::pop_heap.
// Our approach is to refresh top iterator (next or seek) and then remove it
// or move to lead. So we don't need this check.
// It is quite difficult to disable check since it managed by _ITERATOR_DEBUG_LEVEL
// macros which affects ABI (it must be the same for all libs and objs).
template<typename Iterator, typename Pred>
inline void pop_heap(Iterator first, Iterator last, Pred comp) {
  assert(first != last); // pop requires non-empty range

  #ifndef _MSC_VER
    std::pop_heap(first, last, comp);
  #elif _MSC_FULL_VER < 190024000 // < MSVC2015.3
    std::_Pop_heap(std::_Unchecked(first), std::_Unchecked(last), comp);
  #elif _MSC_FULL_VER < 191526726 // < MSVC2017.8
    std::_Pop_heap_unchecked(std::_Unchecked(first), std::_Unchecked(last), comp);
  #else
    std::_Pop_heap_unchecked(first._Unwrapped(), last._Unwrapped(), comp);
  #endif
}

template<typename DocIterator>
void score_add(byte_type* dst, const order::prepared& order, DocIterator& src) {
  typedef void(*add_score_fn_t)(
    const order::prepared& order,
    const irs::score& score,
    byte_type* dst
  );

  static const add_score_fn_t add_score_fns[] = {
    // score != iresearch::score::no_score()
    [](const order::prepared& order, const irs::score& score, byte_type* dst) {
      score.evaluate();
      order.add(dst, score.c_str());
    },

    // score == iresearch::score::no_score()
    [](const order::prepared&, const irs::score&, byte_type*) {
      // NOOP
    }
  };
  const auto* score = src.score;
  assert(score);

  // do not merge scores for irs::score::no_score()
  add_score_fns[&irs::score::no_score() == score](order, *score, dst);
}

NS_END // detail

////////////////////////////////////////////////////////////////////////////////
/// @class basic_disjunction
////////////////////////////////////////////////////////////////////////////////
class basic_disjunction final : public doc_iterator_base {
 public:
  typedef score_iterator_adapter doc_iterator_t;

  basic_disjunction(
      doc_iterator_t&& lhs,
      doc_iterator_t&& rhs,
      const order::prepared& ord = order::prepared::unordered())
    : basic_disjunction(std::move(lhs), std::move(rhs), ord, resolve_overload_tag()) {
    // estimate disjunction
    estimate([this](){
      cost::cost_t est = 0;
      est += cost::extract(lhs_->attributes(), 0);
      est += cost::extract(rhs_->attributes(), 0);
      return est;
    });
  }

  basic_disjunction(
      doc_iterator_t&& lhs,
      doc_iterator_t&& rhs,
      const order::prepared& ord,
      cost::cost_t est)
    : basic_disjunction(std::move(lhs), std::move(rhs), ord, resolve_overload_tag()) {
    // estimate disjunction
    estimate(est);
  }

  virtual doc_id_t value() const NOEXCEPT override {
    return doc_;
  }

  virtual bool next() override {
    next_iterator_impl(lhs_);
    next_iterator_impl(rhs_);
    return !type_limits<type_t::doc_id_t>::eof(doc_ = std::min(lhs_->value(), rhs_->value()));
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (target <= doc_) {
      return doc_;
    }

    if (seek_iterator_impl(lhs_, target) || seek_iterator_impl(rhs_, target)) {
      return doc_ = target;
    }

    return (doc_ = std::min(lhs_->value(), rhs_->value()));
  }

 private:
  struct resolve_overload_tag { };

  basic_disjunction(
      doc_iterator_t&& lhs,
      doc_iterator_t&& rhs,
      const order::prepared& ord,
      resolve_overload_tag)
    : doc_iterator_base(ord),
      lhs_(std::move(lhs)), rhs_(std::move(rhs)),
      doc_(type_limits<type_t::doc_id_t>::invalid()) {

    // prepare score
    if (lhs_.score != &irs::score::no_score()
        && rhs_.score != &irs::score::no_score()) {
      // both sub-iterators has score
      prepare_score([this](byte_type* score) {
        ord_->prepare_score(score);
        score_iterator_impl(lhs_, score);
        score_iterator_impl(rhs_, score);
      });
    } else if (lhs_.score != &irs::score::no_score()) {
      // only left sub-iterator has score
      assert(rhs_.score == &irs::score::no_score());
      prepare_score([this](byte_type* score) {
        ord_->prepare_score(score);
        score_iterator_impl(lhs_, score);
      });
    } else if (rhs_.score != &irs::score::no_score()) {
      // only right sub-iterator has score
      assert(lhs_.score == &irs::score::no_score());
      prepare_score([this](byte_type* score) {
        ord_->prepare_score(score);
        score_iterator_impl(rhs_, score);
      });
    } else {
      assert(lhs_.score == &irs::score::no_score());
      assert(rhs_.score == &irs::score::no_score());
      prepare_score([](byte_type*) {/*NOOP*/});
    }
  }

  bool seek_iterator_impl(doc_iterator_t& it, doc_id_t target) {
    return it->value() < target && target == it->seek(target);
  }

  void next_iterator_impl(doc_iterator_t& it) {
    const auto doc = it->value();

    if (doc_ == doc) {
      it->next();
    } else if (doc < doc_) {
      assert(!type_limits<type_t::doc_id_t>::eof(doc_));
      it->seek(doc_ + 1);
    }
  }

  void score_iterator_impl(doc_iterator_t& it, byte_type* lhs) {
    auto doc = it->value();

    if (doc < doc_) {
      doc = it->seek(doc_);
    }

    if (doc == doc_) {
      const auto* rhs = it.score;
      rhs->evaluate();
      ord_->add(lhs, rhs->c_str());
    }
  }

  doc_iterator_t lhs_;
  doc_iterator_t rhs_;
  doc_id_t doc_;
}; // basic_disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class small_disjunction
/// @brief linear search based disjunction
////////////////////////////////////////////////////////////////////////////////
class small_disjunction : public doc_iterator_base {
 public:
  typedef score_iterator_adapter doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;

  small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      cost::cost_t est)
    : small_disjunction(std::move(itrs), ord, resolve_overload_tag()) {
    // estimate disjunction
    estimate(est);
  }

  explicit small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered())
    : small_disjunction(std::move(itrs), ord, resolve_overload_tag()) {
    // estimate disjunction
    estimate([this](){
      return std::accumulate(
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator_t& rhs) {
          return lhs + cost::extract(rhs->attributes(), 0);
      });
    });
  }

  virtual doc_id_t value() const NOEXCEPT override {
    return doc_;
  }

  bool next_iterator_impl(doc_iterator_t& it) {
    const auto doc = it->value();

    if (doc == doc_) {
      return it->next();
    } else if (doc < doc_) {
      return !type_limits<type_t::doc_id_t>::eof(it->seek(doc_+1));
    }

    return true;
  }

  virtual bool next() override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return false;
    }

    doc_id_t min = type_limits<type_t::doc_id_t>::eof();

    for (auto begin = itrs_.begin(); begin != itrs_.end(); ) {
      auto& it = *begin;
      if (!next_iterator_impl(it)) {
        if (!remove_iterator(it)) {
          doc_ = type_limits<type_t::doc_id_t>::eof();
          return false;
        }
      } else {
        min = std::min(min, it->value());
        ++begin;
      }
    }

    doc_ = min;
    return true;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return doc_;
    }

    doc_id_t min = type_limits<type_t::doc_id_t>::eof();

    for (auto begin = itrs_.begin(); begin != itrs_.end(); ) {
      auto& it = *begin;

      if (it->value() < target) {
        const auto doc = it->seek(target);

        if (doc == target) {
          return doc_ = doc;
        } else if (type_limits<type_t::doc_id_t>::eof(doc)) {
          if (!remove_iterator(it)) {
            // exhausted
            return doc_ = type_limits<type_t::doc_id_t>::eof();
          }
          continue; // don't need to increment 'begin' here
        }
      }

      min = std::min(min, it->value());
      ++begin;
    }

    return (doc_ = min);
  }

 private:
  struct resolve_overload_tag{};

  small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      resolve_overload_tag)
    : doc_iterator_base(ord),
      itrs_(std::move(itrs)),
      doc_(itrs_.empty()
        ? type_limits<type_t::doc_id_t>::eof()
        : type_limits<type_t::doc_id_t>::invalid()) {
    // copy iterators with scores into separate container
    // to avoid extra checks
    scored_itrs_.reserve(itrs_.size());
    for (auto& it : itrs_) {
      if (&irs::score::no_score() != it.score) {
        scored_itrs_.emplace_back(it);
      }
    }

    // prepare score
    if (scored_itrs_.empty()) {
      prepare_score([](byte_type*){ /*NOOP*/ });
    } else {
      prepare_score([this](byte_type* score) {
        ord_->prepare_score(score);

        for (auto& it : scored_itrs_) {
          auto doc = it.it->value();

          if (doc < doc_) {
            doc = it.it->seek(doc_);
          }

          if (doc == doc_) {
            it.score->evaluate();
            ord_->add(score, it.score->c_str());
          }
        }
      });
    }
  }

  bool remove_iterator(doc_iterator_t& it) {
    std::swap(it, itrs_.back());
    itrs_.pop_back();
    return !itrs_.empty();
  }

  doc_iterators_t itrs_;
  doc_iterators_t scored_itrs_; // iterators with scores
  doc_id_t doc_;
}; // small_disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class disjunction
/// @brief heap sort based disjunction
/// ----------------------------------------------------------------------------
///   [0]   <-- begin
///   [1]      |
///   [2]      | head (min doc_id heap)
///   ...      |
///   [n-1] <-- end
///   [n]   <-- lead (accepted iterator)
/// ----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
class disjunction : public doc_iterator_base {
 public:
  typedef small_disjunction small_disjunction_t;
  typedef basic_disjunction basic_disjunction_t;
  typedef score_iterator_adapter doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;

  disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      cost::cost_t est)
    : disjunction(std::move(itrs), ord, resolve_overload_tag()) {
    // estimate disjunction
    estimate(est);
  }

  explicit disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered())
    : disjunction(std::move(itrs), ord, resolve_overload_tag()) {
    // estimate disjunction
    estimate([this](){
      return std::accumulate(
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator_t& rhs) {
          return lhs + cost::extract(rhs->attributes(), 0);
      });
    });
  }

  virtual doc_id_t value() const NOEXCEPT override {
    return doc_;
  }

  virtual bool next() override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return false;
    }

    while (lead()->value() <= doc_) {
      bool const exhausted = lead()->value() == doc_
        ? !lead()->next()
        : type_limits<type_t::doc_id_t>::eof(lead()->seek(doc_ + 1));

      if (exhausted && !remove_lead()) {
        doc_ = type_limits<type_t::doc_id_t>::eof();
        return false;
      } else {
        refresh_lead();
      }
    }

    doc_ = lead()->value();
    return true;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return doc_;
    }

    while (lead()->value() < target) {
      const auto doc = lead()->seek(target);

      if (type_limits<type_t::doc_id_t>::eof(doc) && !remove_lead()) {
        return doc_ = type_limits<type_t::doc_id_t>::eof();
      } else if (doc != target) {
        refresh_lead();
      }
    }

    return doc_ = lead()->value();
  }

 private:
  struct resolve_overload_tag{};

  disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      resolve_overload_tag)
    : doc_iterator_base(ord),
      itrs_(std::move(itrs)),
      doc_(itrs_.empty()
        ? type_limits<type_t::doc_id_t>::eof()
        : type_limits<type_t::doc_id_t>::invalid()) {
    // since we are using heap in order to determine next document,
    // in order to avoid useless make_heap call we expect that all
    // iterators are equal here */
    //assert(irstd::all_equal(itrs_.begin(), itrs_.end()));

    // prepare score
    prepare_score([this](byte_type* score) {
      ord_->prepare_score(score);
      score_impl(score);
    });
  }

  template<typename Iterator>
  inline void push(Iterator begin, Iterator end) {
    // lambda here gives ~20% speedup on GCC
    std::push_heap(begin, end, [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
      return lhs->value() > rhs->value();
    });
  }

  template<typename Iterator>
  inline void pop(Iterator begin, Iterator end) {
    // lambda here gives ~20% speedup on GCC
    detail::pop_heap(begin, end, [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
      return lhs->value() > rhs->value();
    });
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes lead iterator
  /// @returns true - if the disjunction condition still can be satisfied,
  ///          false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  inline bool remove_lead() {
    itrs_.pop_back();

    if (!itrs_.empty()) {
      pop(itrs_.begin(), itrs_.end());
      return true;
    }

    return false;
  }

  inline void refresh_lead() {
    auto begin = itrs_.begin(), end = itrs_.end();
    push(begin, end);
    pop(begin, end);
  }

  inline doc_iterator_t& lead() NOEXCEPT {
    return itrs_.back();
  }

  inline doc_iterator_t& top() NOEXCEPT {
    return itrs_.front();
  }

  void score_impl(byte_type* lhs) {
    assert(!itrs_.empty());

    // hitch all iterators in head to the lead (current doc_)
    auto begin = itrs_.begin(), end = itrs_.end()-1;

    while(begin != end && top()->value() < doc_) {
      const auto doc = top()->seek(doc_);

      if (type_limits<type_t::doc_id_t>::eof(doc)) {
        // remove top
        pop(begin,end);
        std::swap(*--end, itrs_.back());
        itrs_.pop_back();
      } else {
        // refresh top
        pop(begin,end);
        push(begin,end);
      }
    }

    detail::score_add(lhs, *ord_, lead());

    if (top()->value() == doc_) {
      irstd::heap::for_each_if(
        begin, end,
        [this](const doc_iterator_t& it) {
          return it->value() == doc_;
        },
        [this, lhs](doc_iterator_t& it) {
          detail::score_add(lhs, *ord_, it);
      });
    }
  }

  doc_iterators_t itrs_;
  doc_id_t doc_;
}; // disjunction

//////////////////////////////////////////////////////////////////////////////
/// @returns disjunction iterator created from the specified sub iterators
//////////////////////////////////////////////////////////////////////////////
template<typename Disjunction, typename... Args>
doc_iterator::ptr make_disjunction(
    typename Disjunction::doc_iterators_t&& itrs,
    Args&&... args) {
  const auto size = itrs.size();

  switch (size) {
    case 0:
      // empty or unreachable search criteria
      return doc_iterator::empty();
    case 1:
      // single sub-query
      return std::move(itrs.front());
    case 2: {
      typedef typename Disjunction::basic_disjunction_t basic_disjunction_t;

      // simple disjunction
      auto first = itrs.begin();
      auto second = first;
      std::advance(second, 1);

      return doc_iterator::make<basic_disjunction_t>(
        std::move(*first),
        std::move(*second),
        std::forward<Args>(args)...
      );
    }
  }

//  const size_t LINEAR_MERGE_UPPER_BOUND = 5;
//  if (size <= LINEAR_MERGE_UPPER_BOUND) {
//    typedef typename Disjunction::small_disjunction_t small_disjunction_t;
//
//    // small disjunction
//    return doc_iterator::make<small_disjunction_t>(
//      std::move(itrs), std::forward<Args>(args)...
//    );
//  }

  // disjunction
  return doc_iterator::make<Disjunction>(
    std::move(itrs), std::forward<Args>(args)...
  );
}

NS_END // ROOT

#endif // IRESEARCH_DISJUNCTION_H
