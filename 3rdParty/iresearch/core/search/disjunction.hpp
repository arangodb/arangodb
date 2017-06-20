//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_DISJUNCTION_H
#define IRESEARCH_DISJUNCTION_H

#include "conjunction.hpp"
#include "utils/std.hpp"
#include "utils/type_limits.hpp"
#include "index/iterators.hpp"
#include <queue>
#include <boost/compressed_pair.hpp>

NS_ROOT
NS_LOCAL

/* Need this proxy since Microsoft has heap validity check in std::pop_heap.
 * Our approach is to refresh top iterator (next or seek) and then remove it
 * or move to lead. So we don't need this check. 
 * It is quite difficult to disable check since it manages by _ITERATOR_DEBUG_LEVEL
 * macros which is affect ABI (it must be the same for all libs and objs). */
template<typename Iterator, typename Pred>
inline void pop_heap(Iterator first, Iterator last, Pred comp) {
  #ifdef _MSC_VER
    if (1 < std::distance(first, last)) {
      #if _MSC_FULL_VER < 190024000
        std::_Pop_heap(std::_Unchecked(first), std::_Unchecked(last), comp);
      #else
        std::_Pop_heap_unchecked(std::_Unchecked(first), std::_Unchecked(last), comp);
      #endif
    }
  #else
    std::pop_heap(first, last, comp);
  #endif
}

NS_END // LOCAL

NS_BEGIN(detail) 

////////////////////////////////////////////////////////////////////////////////
/// @class basic_disjunction
////////////////////////////////////////////////////////////////////////////////
template<typename IteratorWrapper, typename IteratorTraits = iterator_traits<IteratorWrapper>>
class basic_disjunction final : public score_doc_iterator_base {
 public:
  typedef IteratorWrapper doc_iterator;
  typedef IteratorTraits traits_t;

  basic_disjunction(
      doc_iterator&& lhs, doc_iterator&& rhs,
      const order::prepared& ord = order::prepared::unordered()) NOEXCEPT
    : score_doc_iterator_base(ord),
      lhs_(std::move(lhs)), rhs_(std::move(rhs)), 
      doc_(type_limits<type_t::doc_id_t>::invalid()) {
    // estimate disjunction
    attrs_.emplace<cost>()->rule([this](){
      cost::cost_t est = 0;
      est += traits_t::estimate(lhs_, 0);
      est += traits_t::estimate(rhs_, 0);
      return est;
    });
  }

  virtual void score() override {
    if (!scr_) return;
    scr_->clear();
    score_impl(scr_->leak());
  }

  virtual doc_id_t value() const override {
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
  bool seek_iterator_impl(doc_iterator& it, doc_id_t target) {
    return it->value() < target && target == it->seek(target);
  }

  void next_iterator_impl(doc_iterator& it) {
    const auto doc = it->value();

    if (doc_ == doc) {
      it->next();
    } else if (doc < doc_) {
      it->seek(doc_ + 1);
    }
  }

  void score_iterator_impl(doc_iterator& it, byte_type* lhs) {
    auto doc = it->value();

    if (doc < doc_) {
      doc = it->seek(doc_);
    }

    if (doc == doc_) {
      ord_->add(lhs, traits_t::score(it));
    }
  }

  void score_impl(byte_type* lhs) {
    score_iterator_impl(lhs_, lhs);
    score_iterator_impl(rhs_, lhs);
  }

  doc_iterator lhs_;
  doc_iterator rhs_;
  doc_id_t doc_;
}; // basic_disjunction 

////////////////////////////////////////////////////////////////////////////////
/// @class disjunction
///-----------------------------------------------------------------------------
///   [0]   <-- begin
///   [1]      |
//    [2]      | head (min doc_id heap)
///   ...      |
///   [n-1] <-- end
///   [n]   <-- lead (accepted iterator)
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<
  typename IteratorWrapper, 
  typename IteratorTraits = iterator_traits<IteratorWrapper>
> class disjunction : public score_doc_iterator_base {
public:
  typedef IteratorWrapper doc_iterator;
  typedef IteratorTraits traits_t;
  typedef std::vector<doc_iterator> doc_iterators_t;

  //TODO: remove delegating ctor
  disjunction(
      doc_iterators_t&& itrs, 
      const order::prepared& ord,
      cost::cost_t est) 
    : disjunction(std::move(itrs), ord) {
    /* estimate disjunction */
    attrs_.emplace<cost>()->value(est);
  }

  explicit disjunction(
      doc_iterators_t&& itrs, 
      const order::prepared& ord = order::prepared::unordered())
    : score_doc_iterator_base(ord),
      itrs_(std::move(itrs)),
      doc_(type_limits<type_t::doc_id_t>::invalid()) {
    assert(!itrs_.empty());

    // since we are using heap in order to determine next document,
    // in order to avoid useless make_heap call we expect that all
    // iterators are equal here */
    //assert(irstd::all_equal(itrs_.begin(), itrs_.end()));

    // estimate disjunction
    attrs_.emplace<cost>()->rule([this](){
      return std::accumulate(
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator& rhs) {
          return lhs + traits_t::estimate(rhs, 0);
      });
    });
  }

  virtual void score() override {
    if (!scr_) return;
    scr_->clear();
    score_impl(scr_->leak());
  }

  virtual doc_id_t value() const override {
    return doc_;
  }

  virtual bool next() override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return false;
    }

    if (itrs_.empty()) {
      doc_ = type_limits<type_t::doc_id_t>::eof();
      return false;
    }

    for(;lead()->value() == doc_;) {
      if (lead()->next()) {
        refresh_lead();
      } else if (!remove_lead()) {
        doc_ = type_limits<type_t::doc_id_t>::eof();
        return false;
      }
    }

    doc_ = lead()->value();
    return true;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return doc_;
    }

    if (itrs_.empty()) {
      return (doc_ = type_limits<type_t::doc_id_t>::eof());
    }

    while (lead()->value() < target) {
      const auto doc = lead()->seek(target);

      if (type_limits<type_t::doc_id_t>::eof(doc) && !remove_lead()) {
        return doc_ = doc;
      } else if (doc != target) {
        refresh_lead();
      }
    }

    return doc_ = lead()->value();
  }

 protected:
  virtual void score_add_impl(byte_type* dst, doc_iterator& src) {
    ord_->add(dst, traits_t::score(src));
  }

 private:
  template<typename Iterator>
  inline void push(Iterator begin, Iterator end) {
    std::push_heap(begin, end, traits_t::greater); 
  }

  template<typename Iterator>
  inline void pop(Iterator begin, Iterator end) {
    iresearch::pop_heap(begin, end, traits_t::greater); 
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes lead iterator
  /// @returns true - if the disjunction condition still can be satisfied,
  //           false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  inline bool remove_lead() {
    itrs_.pop_back();
    pop(itrs_.begin(), itrs_.end());
    return !itrs_.empty();
  }

  inline void refresh_lead() {
    auto begin = itrs_.begin(), end = itrs_.end();
    push(begin, end);
    pop(begin, end);
  }

  inline doc_iterator& lead() {
    return itrs_.back();
  }

  inline doc_iterator& top() {
    return itrs_.front();
  }

  inline void score_impl(byte_type* lhs) {
    assert(!itrs_.empty());

    /* hitch all iterators in head to the lead (current doc_) */
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

    score_add_impl(lhs, lead());

    if (top()->value() == doc_) {
      irstd::heap::for_each_if(
        begin, end,
        [this](const doc_iterator& it) {
          return it->value() == doc_;
        },
        [this, lhs](doc_iterator& it) {
          score_add_impl(lhs, it);
      });
    }
  }

  doc_iterators_t itrs_;
  doc_id_t doc_;
}; // disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class min_match_disjunction
///-----------------------------------------------------------------------------
///      [0] <-- begin
///      [1]      |
//       [2]      | head (min doc_id, cost heap)
///      [3]      |
///      [4] <-- lead_
/// c ^  [5]      |
/// o |  [6]      | lead (list of accepted iterators)
/// s |  ...      |
/// t |  [n] <-- end
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<
  typename IteratorWrapper, 
  typename IteratorTraits = iterator_traits<IteratorWrapper>
> class min_match_disjunction : public score_doc_iterator_base {
 public:
  typedef IteratorTraits traits_t;
  typedef disjunction<IteratorWrapper, IteratorTraits> disjunction_t;
  typedef typename disjunction_t::doc_iterator doc_iterator;
  typedef typename disjunction_t::doc_iterators_t doc_iterators_t;

  min_match_disjunction(
      doc_iterators_t&& itrs,
      size_t min_match_count = 1,
      const order::prepared& ord = order::prepared::unordered())
    : score_doc_iterator_base(ord), itrs_(std::move(itrs)),
      min_match_count_(
        std::min(itrs_.size(), std::max(size_t(1), min_match_count))),
      lead_(itrs_.size()), doc_(type_limits<type_t::doc_id_t>::invalid()) {
    assert(!itrs_.empty());
    assert(min_match_count_ >= 1 && min_match_count_ <= itrs_.size());

    // sort subnodes in ascending order by their cost
    std::sort(
      itrs_.begin(), itrs_.end(),
      [](const doc_iterator& lhs, const doc_iterator& rhs) {
        return traits_t::estimate(lhs) < traits_t::estimate(rhs);
    });

    // estimate disjunction
    attrs_.emplace<cost>()->rule([this](){
      return std::accumulate(
        // estimate only first min_match_count_ subnodes
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator& rhs) {
          return lhs + traits_t::estimate(rhs, 0);
      });
    });
  }

  virtual void score() override {
    if (!scr_) return;
    scr_->clear();
    score_impl(scr_->leak());
  }

  virtual doc_id_t value() const override {
    return doc_;
  }

  virtual bool next() override {
    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return false;
    }

    while (check_size()) {
      /* start next iteration. execute next for all lead iterators
       * and move them to head */
      if (!pop_lead()) {
        doc_ = type_limits<type_t::doc_id_t>::eof();
        return false;
      }

      // make step for all head iterators less or equal current doc (doc_)
      while (top()->value() <= doc_) {
        const bool exhausted = top()->value() == doc_
          ? !top()->next()
          : type_limits<type_t::doc_id_t>::eof(top()->seek(doc_ + 1));

        if (exhausted && !remove_top()) {
          doc_ = type_limits<type_t::doc_id_t>::eof();
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
          return !type_limits<type_t::doc_id_t>::eof(doc_ = top);
        }
      } while (top == this->top()->value());
    }

    doc_ = type_limits<type_t::doc_id_t>::eof();
    return false;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (target <= doc_) {
      return doc_;
    }

    if (type_limits<type_t::doc_id_t>::eof(doc_)) {
      return doc_;
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

      if (type_limits<type_t::doc_id_t>::eof(doc)) {
        --lead_;

        // iterator exhausted
        if (!remove_lead(it)) {
          return (doc_ = type_limits<type_t::doc_id_t>::eof());
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
      return doc_ = target;
    }

    // main search loop
    for(;;target = top()->value()) {
      while (top()->value() <= target) {
        const auto doc = top()->seek(target);

        if (type_limits<type_t::doc_id_t>::eof(doc)) {
          // iterator exhausted
          if (!remove_top()) {
            return (doc_ = type_limits<type_t::doc_id_t>::eof());
          }
        } else if (doc == target) {
          // valid iterator, doc == target
          add_lead();
          if (lead_ >= min_match_count_) {
            return (doc_ = target);
          }
        } else {
          // invalid iterator, doc != target
          refresh_top();
        }
      }

      /* can't find enough iterators equal to target here.
       * start next iteration. execute next for all lead iterators
       * and move them to head */
      if (!pop_lead()) {
        return doc_ = type_limits<type_t::doc_id_t>::eof();
      }
    }
  }

 private:
  template<typename Iterator>
  inline void push(Iterator begin, Iterator end) {
    std::push_heap(begin, end, traits_t::greater);
  }

  template<typename Iterator>
  inline void pop(Iterator begin, Iterator end) {
    iresearch::pop_heap(begin, end, traits_t::greater);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief perform step for each iterator in lead group and push it to head
  /// @returns true - if the min_match_count_ condition still can be satisfied,
  //           false - otherwise
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
  //           false - otherwise
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
  //           false - otherwise
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
  //           false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  inline bool check_size() const {
    return itrs_.size() >= min_match_count_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns reference to the top of the head
  //////////////////////////////////////////////////////////////////////////////
  inline doc_iterator& top() {
    return itrs_.front();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns the first iterator in the lead group
  //////////////////////////////////////////////////////////////////////////////
  inline typename doc_iterators_t::iterator lead() {
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
          lead != begin && top()->value() <= doc_;) {
        // hitch head
        if (top()->value() == doc_) {
          // got hit here
          add_lead();
          --lead;
        } else {
          if (type_limits<type_t::doc_id_t>::eof(top()->seek(doc_))) {
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
      [this, lhs](doc_iterator& it) {
      ord_->add(lhs, traits_t::score(it));
    });
  }

  doc_iterators_t itrs_; /* sub iterators */
  size_t min_match_count_; /* minimum number of hits */
  size_t lead_; /* number of iterators in lead group */
  doc_id_t doc_; // current doc
}; // min_match_disjunction

//////////////////////////////////////////////////////////////////////////////
/// @returns disjunction iterator created from the specified sub iterators
//////////////////////////////////////////////////////////////////////////////
template<typename Disjunction, typename... Args>
score_doc_iterator::ptr make_disjunction(
    typename Disjunction::doc_iterators_t&& itrs,
    size_t min_match_count,
    Args&&... args) {
  typedef typename Disjunction::doc_iterator doc_iterator_t;
  typedef typename Disjunction::traits_t traits_t;
  typedef typename std::enable_if<
    std::is_base_of<disjunction<doc_iterator_t, traits_t>, Disjunction>::value, Disjunction
  >::type disjunction_t;
  typedef basic_disjunction<doc_iterator_t, traits_t> basic_disjunction_t;
  typedef min_match_disjunction<doc_iterator_t, traits_t> min_match_disjunction_t;
  typedef conjunction<doc_iterator_t, traits_t> conjunction_t;

  auto size = itrs.size();
  /* check the size after execution */
  if (0 == size || min_match_count > size) {
    /* empty or unreachable search criteria */
    return score_doc_iterator::empty();
  } else if (1 == size) {
    /* single sub-query */
    return std::move(itrs.front());
  } else if (min_match_count == size) {
    /* pure conjunction */
    return score_doc_iterator::make<conjunction_t>(
      std::move(itrs), std::forward<Args>(args)...
    );
  }
  
  if (min_match_count > 1) {
    /* min match disjunction */
    assert(min_match_count < size);
    return score_doc_iterator::make<min_match_disjunction_t>(
      std::move(itrs), min_match_count, std::forward<Args>(args)...
    );
  }

  if (2 == size) {
    /* simple disjunction */
    auto begin = itrs.begin();
    auto first = std::move(*begin); ++begin;
    auto second = std::move(*begin);
    return score_doc_iterator::make<basic_disjunction_t>(
      std::move(first), std::move(second), std::forward<Args>(args)...
    );
  }

  /* disjunction */
  return score_doc_iterator::make<disjunction_t>(
    std::move(itrs), std::forward<Args>(args)...
  );
}

//////////////////////////////////////////////////////////////////////////////
/// @returns disjunction iterator created from the specified sub queries 
//////////////////////////////////////////////////////////////////////////////
template<typename Disjunction, typename QueryIterator, typename... Args>
score_doc_iterator::ptr make_disjunction(
    const sub_reader& rdr, const order::prepared& ord,
    QueryIterator begin, QueryIterator end,
    size_t min_match_count,
    Args&&... args) {
  typedef typename Disjunction::doc_iterator doc_iterator_t;
  typedef typename Disjunction::traits_t traits_t;
  typedef typename std::enable_if<
    std::is_base_of<disjunction<doc_iterator_t, traits_t>, Disjunction>::value, Disjunction
  >::type disjunction_t;
  typedef conjunction<doc_iterator_t, traits_t> conjunction_t;

  assert(std::distance(begin, end) >= 0);
  const size_t size = size_t(std::distance(begin, end));

  /* 1 <= min_match_count */
  min_match_count = std::max(size_t(1), min_match_count);

  /* check the size before the execution */
  if (0 == size || min_match_count > size) {
    /* empty or unreachable search criteria */
    return score_doc_iterator::empty();
  } else if (min_match_count == size) {
    /* pure conjunction */
    return make_conjunction<conjunction_t>(rdr, ord, begin, end, std::forward<Args>(args)...); 
  }

  /* min_match_count <= size */
  min_match_count = std::min(size, min_match_count);

  typename disjunction_t::doc_iterators_t itrs;
  itrs.reserve(size);
  for(;begin != end; ++begin) {
    /* execute query - get doc iterator */
    auto docs = begin->execute(rdr, ord);

    // filter out empty iterators
    if (!type_limits<type_t::doc_id_t>::eof(docs->value())) {
      itrs.emplace_back(std::move(docs));
    }
  } 

  return make_disjunction<disjunction_t>(
    std::move(itrs), min_match_count, ord, std::forward<Args>(args)... 
  );
}

NS_END // detail
NS_END // ROOT

#endif // IRESEARCH_DISJUNCTION_H
