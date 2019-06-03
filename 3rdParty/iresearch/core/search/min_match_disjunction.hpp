////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MIN_MATCH_DISJUNCTION_H
#define IRESEARCH_MIN_MATCH_DISJUNCTION_H

#include "disjunction.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class min_match_disjunction
///-----------------------------------------------------------------------------
///      [0] <-- begin
///      [1]      |
///      [2]      | head (min doc_id, cost heap)
///      [3]      |
///      [4] <-- lead_
/// c ^  [5]      |
/// o |  [6]      | lead (list of accepted iterators)
/// s |  ...      |
/// t |  [n] <-- end
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
class min_match_disjunction : public doc_iterator_base {
 public:
  struct cost_iterator_adapter : score_iterator_adapter {
    cost_iterator_adapter(irs::doc_iterator::ptr&& it) NOEXCEPT
      : score_iterator_adapter(std::move(it)) {
      est = cost::extract(this->it->attributes(), cost::MAX);
    }

    cost_iterator_adapter(cost_iterator_adapter&& rhs) NOEXCEPT
      : score_iterator_adapter(std::move(rhs)), est(rhs.est) {
    }

    cost_iterator_adapter& operator=(cost_iterator_adapter&& rhs) NOEXCEPT {
      if (this != &rhs) {
        score_iterator_adapter::operator=(std::move(rhs));
        est = rhs.est;
      }
      return *this;
    }

    cost::cost_t est;
  }; // cost_iterator_adapter

  typedef cost_iterator_adapter doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;

  min_match_disjunction(
      doc_iterators_t&& itrs,
      size_t min_match_count = 1,
      const order::prepared& ord = order::prepared::unordered())
    : doc_iterator_base(ord), itrs_(std::move(itrs)),
      min_match_count_(
        std::min(itrs_.size(), std::max(size_t(1), min_match_count))),
      lead_(itrs_.size()), doc_(doc_limits::invalid()) {
    assert(!itrs_.empty());
    assert(min_match_count_ >= 1 && min_match_count_ <= itrs_.size());

    // sort subnodes in ascending order by their cost
    std::sort(
      itrs_.begin(), itrs_.end(),
      [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
        return cost::extract(lhs->attributes(), 0) < cost::extract(rhs->attributes(), 0);
    });

    // make 'document' attribute accessible from outside
    attrs_.emplace(doc_);

    // estimate disjunction
    estimate([this](){
      return std::accumulate(
        // estimate only first min_match_count_ subnodes
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator_t& rhs) {
          return lhs + cost::extract(rhs->attributes(), 0);
      });
    });

    // prepare score
    prepare_score([this](byte_type* score) {
      ord_->prepare_score(score);
      score_impl(score);
    });
  }

  virtual doc_id_t value() const override {
    return doc_.value;
  }

  virtual bool next() override {
    if (doc_limits::eof(doc_.value)) {
      return false;
    }

    while (check_size()) {
      /* start next iteration. execute next for all lead iterators
       * and move them to head */
      if (!pop_lead()) {
        doc_.value = doc_limits::eof();
        return false;
      }

      // make step for all head iterators less or equal current doc (doc_)
      while (top()->value() <= doc_.value) {
        const bool exhausted = top()->value() == doc_.value
          ? !top()->next()
          : doc_limits::eof(top()->seek(doc_.value + 1));

        if (exhausted && !remove_top()) {
          doc_.value = doc_limits::eof();
          return false;
        } else {
          refresh_top();
        }
      }

      // count equal iterators
      const auto top = this->top()->value();

      do {
        add_lead();
        if (lead_ >= min_match_count_) {
          return !doc_limits::eof(doc_.value = top);
        }
      } while (top == this->top()->value());
    }

    doc_.value = doc_limits::eof();
    return false;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (target <= doc_.value) {
      return doc_.value;
    }

    if (doc_limits::eof(doc_.value)) {
      return doc_.value;
    }

    /* execute seek for all lead iterators and
     * move one to head if it doesn't hit the target */
    for (auto it = lead(), end = itrs_.end();it != end;) {
      const auto doc = (*it)->seek(target);

      if (doc == target) {
        // got hit here
        ++it;
        continue;
      }

      if (doc_limits::eof(doc)) {
        --lead_;

        // iterator exhausted
        if (!remove_lead(it)) {
          return (doc_.value = doc_limits::eof());
        }

#ifdef _MSC_VER
        // Microsoft invalidates iterator
        it = lead();
#endif

        // update end
        end = itrs_.end();
      } else { // doc != target
        // move back to head
        push_head(it);
        --lead_;
        ++it;
      }
    }

    // check if we still satisfy search criteria
    if (lead_ >= min_match_count_) {
      return doc_.value = target;
    }

    // main search loop
    for(;;target = top()->value()) {
      while (top()->value() <= target) {
        const auto doc = top()->seek(target);

        if (doc_limits::eof(doc)) {
          // iterator exhausted
          if (!remove_top()) {
            return (doc_.value = doc_limits::eof());
          }
        } else if (doc == target) {
          // valid iterator, doc == target
          add_lead();
          if (lead_ >= min_match_count_) {
            return (doc_.value = target);
          }
        } else {
          // invalid iterator, doc != target
          refresh_top();
        }
      }

      // can't find enough iterators equal to target here.
      // start next iteration. execute next for all lead iterators
      // and move them to head
      if (!pop_lead()) {
        return doc_.value = doc_limits::eof();
      }
    }
  }

 private:
  template<typename Iterator>
  inline void push(Iterator begin, Iterator end) {
    // lambda here gives ~20% speedup on GCC
    std::push_heap(begin, end, [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
      const auto lhs_doc = lhs->value();
      const auto rhs_doc = rhs->value();
      return (lhs_doc > rhs_doc || (lhs_doc == rhs_doc && lhs.est > rhs.est));
    });
  }

  template<typename Iterator>
  inline void pop(Iterator begin, Iterator end) {
    // lambda here gives ~20% speedup on GCC
    detail::pop_heap(begin, end, [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
      const auto lhs_doc = lhs->value();
      const auto rhs_doc = rhs->value();
      return (lhs_doc > rhs_doc || (lhs_doc == rhs_doc && lhs.est > rhs.est));
    });
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief perform step for each iterator in lead group and push it to head
  /// @returns true - if the min_match_count_ condition still can be satisfied,
  ///          false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  bool pop_lead() {
    for (auto it = lead(), end = itrs_.end();it != end;) {
      if (!(*it)->next()) {
        --lead_;

        /* remove iterator */
        if (!remove_lead(it)) {
          return false;
        }

#ifdef _MSC_VER
        /* Microsoft invalidates iterator */
        it = lead();
#endif

        /* update end */
        end = itrs_.end();
      } else {
        // push back to head
        push(itrs_.begin(), ++it);
        --lead_;
      }
    }

    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes iterator from the specified position in lead group
  ///        without moving iterators after the specified iterator
  /// @returns true - if the min_match_count_ condition still can be satisfied,
  ///          false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  template<typename Iterator>
  inline bool remove_lead(Iterator it) {
    if (&*it != &itrs_.back()) {
      std::swap(*it, itrs_.back());
    }
    itrs_.pop_back();
    return check_size();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes iterator from the top of the head without moving
  ///        iterators after the specified iterator
  /// @returns true - if the min_match_count_ condition still can be satisfied,
  ///          false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  inline bool remove_top() {
    auto lead = this->lead();
    pop(itrs_.begin(), lead);
    return remove_lead(--lead);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief refresh the value of the top of the head
  //////////////////////////////////////////////////////////////////////////////
  inline void refresh_top() {
    auto lead = this->lead();
    pop(itrs_.begin(), lead);
    push(itrs_.begin(), lead);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief push the specified iterator from lead group to the head
  //         without movinh iterators after the specified iterator
  //////////////////////////////////////////////////////////////////////////////
  template<typename Iterator>
  inline void push_head(Iterator it) {
    Iterator lead = this->lead();
    if (it != lead) {
      std::swap(*it, *lead);
    }
    ++lead;
    push(itrs_.begin(), lead);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns true - if the min_match_count_ condition still can be satisfied,
  ///          false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  inline bool check_size() const {
    return itrs_.size() >= min_match_count_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns reference to the top of the head
  //////////////////////////////////////////////////////////////////////////////
  inline doc_iterator_t& top() {
    return itrs_.front();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns the first iterator in the lead group
  //////////////////////////////////////////////////////////////////////////////
  inline doc_iterators_t::iterator lead() {
    return itrs_.end() - lead_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds iterator to the lead group
  //////////////////////////////////////////////////////////////////////////////
  inline void add_lead() {
    pop(itrs_.begin(), lead());
    ++lead_;
  }

  inline void score_impl(byte_type* lhs) {
    assert(!itrs_.empty());

    // push all valid iterators to lead
    {
      for(auto lead = this->lead(), begin = itrs_.begin();
          lead != begin && top()->value() <= doc_.value;) {
        // hitch head
        if (top()->value() == doc_.value) {
          // got hit here
          add_lead();
          --lead;
        } else {
          if (doc_limits::eof(top()->seek(doc_.value))) {
            // iterator exhausted
            remove_top();
          } else {
            refresh_top();
          }
        }
      }
    }

    // score lead iterators
    std::for_each(
      lead(), itrs_.end(),
      [this, lhs](doc_iterator_t& it) {
        detail::score_add(lhs, *ord_, it);
    });
  }

  doc_iterators_t itrs_; // sub iterators
  size_t min_match_count_; // minimum number of hits
  size_t lead_; // number of iterators in lead group
  document doc_; // current doc
}; // min_match_disjunction

NS_END // ROOT

#endif // IRESEARCH_MIN_MATCH_DISJUNCTION_H
