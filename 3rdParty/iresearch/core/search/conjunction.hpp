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

#ifndef IRESEARCH_CONJUNCTION_H
#define IRESEARCH_CONJUNCTION_H

#include "cost.hpp"
#include "score_doc_iterators.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/type_limits.hpp"

NS_ROOT

template<typename IteratorWrapper>
struct iterator_traits {
  typedef IteratorWrapper wrapper_t;
  
  static cost::cost_t estimate(const wrapper_t& it, cost::cost_t def = cost::MAX) {
    return cost::extract(it->attributes(), def);
  }

  static const byte_type* score(wrapper_t&) {
    return nullptr;
  }

  static bool greater(const wrapper_t& lhs, const wrapper_t& rhs) {
    return lhs->value() > rhs->value();
  }
}; // iterator_traits 

template<typename BaseWrapper>
struct score_wrapper : BaseWrapper {
  // implicit conversion
  score_wrapper(BaseWrapper&& base) NOEXCEPT 
    : BaseWrapper(std::move(base)) {
    auto& scr = (*this)->attributes().template get<iresearch::score>();
    if (scr) {
      score = scr->c_str();
    }
  }

  score_wrapper(score_wrapper&& rhs) NOEXCEPT 
    : BaseWrapper(std::move(rhs)), score(rhs.score) {
    rhs.score = nullptr;
  }  

  score_wrapper& operator=(score_wrapper&& rhs) NOEXCEPT {
    if (this != &rhs) {
      BaseWrapper::operator=(std::move(rhs));
      score = rhs.score;
      rhs.score = nullptr;
    }
    return *this;
  }

  const byte_type* score{};
}; // score_wrapper

template<typename DocIterator>
struct iterator_traits<score_wrapper<DocIterator>> {
  typedef score_wrapper<DocIterator> wrapper_t;

  static cost::cost_t estimate(const wrapper_t& it, cost::cost_t def = cost::MAX) {
    return cost::extract(it->attributes(), def);
  }

  static const byte_type* score(wrapper_t& it) {
    it->score();
    return it.score;
  }

  static bool greater(const wrapper_t& lhs, const wrapper_t& rhs) {
    return lhs->value() > rhs->value();
  }
}; // iterator_traits 

template<typename BaseWrapper>
struct cost_wrapper : BaseWrapper {
  // implicit conversion
  cost_wrapper(BaseWrapper&& it) 
    : BaseWrapper(std::move(it)) {      
    cost = iterator_traits<BaseWrapper>::estimate(*this);
  } 

  cost_wrapper(cost_wrapper&& rhs) NOEXCEPT
    : BaseWrapper(std::move(rhs)),
      cost(rhs.cost) {
     cost = cost::MAX;    
  }

  cost_wrapper& operator=(cost_wrapper&& rhs) NOEXCEPT {
    if (this != &rhs) {
      BaseWrapper::operator=(std::move(rhs));
      cost = rhs.cost;
      rhs.cost = cost::MAX;
    }
    return *this;
  }

  iresearch::cost::cost_t cost;
}; // cost_wrapper

template<typename BaseWrapper>
struct iterator_traits<cost_wrapper<BaseWrapper>> {
  typedef cost_wrapper<BaseWrapper> wrapper_t;

  static cost::cost_t estimate(const wrapper_t& it, cost::cost_t def = cost::MAX) {
    UNUSED(def);
    return it.cost; 
  }

  static const byte_type* score(wrapper_t& it) {
    return iterator_traits<BaseWrapper>::score(it);
  }

  static bool greater(const wrapper_t& lhs, const wrapper_t& rhs) {    
    return iterator_traits<BaseWrapper>::greater(lhs, rhs) 
      || (lhs->value() == rhs->value() && lhs.cost > rhs.cost);
  }
}; // iterator_traits 

NS_BEGIN(detail)

////////////////////////////////////////////////////////////////////////////////
/// @class conjunction
///-----------------------------------------------------------------------------
/// c |  [0] <-- lead (the least cost iterator)
/// o |  [1]    |
/// s |  [2]    | tail (other iterators)
/// t |  ...    |
///   V  [n] <-- end
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<typename IteratorWrapper, typename IteratorTraits = iterator_traits<IteratorWrapper>> 
class conjunction : public score_doc_iterator_base {
 public:
  typedef IteratorWrapper doc_iterator;
  typedef IteratorTraits traits_t;
  typedef std::vector<doc_iterator> doc_iterators_t;
  typedef typename doc_iterators_t::const_iterator iterator;

  conjunction( 
      doc_iterators_t&& itrs, 
      const order::prepared& ord = order::prepared::unordered())
    : score_doc_iterator_base(ord), 
      itrs_(std::move(itrs)) {
    assert(!itrs_.empty());

    // sort subnodes in ascending order by their cost 
    std::sort(itrs_.begin(), itrs_.end(), 
      [](const doc_iterator& lhs, const doc_iterator& rhs) {
        return traits_t::estimate(lhs) < traits_t::estimate(rhs);
    });

    // set front iterator 
    front_ = &itrs_.front();

    // estimate iterator
    attrs_.emplace<cost>()->value(traits_t::estimate(*front_));
  }

  iterator begin() const { return itrs_.begin(); }
  iterator end() const { return itrs_.end(); }

  // size of conjunction
  size_t size() const { return itrs_.size(); }

  virtual void score() override final {
    if (!scr_) return;
    scr_->clear();
    score_impl(scr_->leak());
  }

  virtual doc_id_t value() const override {
    return (*front_)->value();
  }

  virtual bool next() override {      
    if (!(*front_)->next()) {
      return false;
    }

    return !type_limits<type_t::doc_id_t>::eof(converge((*front_)->value()));
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (type_limits<type_t::doc_id_t>::eof(target = (*front_)->seek(target))) {
      return target;
    }

    return converge(target);
  }

 protected: 
  virtual void score_impl(byte_type* lhs) {
    for(auto& it : itrs_) {
      ord_->add(lhs, traits_t::score(it));
    }
  }

 private:
  /* tries to converge front_ and other iterators to the specified target.
   * if it impossible tries to find first convergence place */
  doc_id_t converge(doc_id_t target) {
    for (auto rest = seek_rest(target); target != rest;) {
      target = (*front_)->seek(rest);
      rest = seek_rest(target);
    }

    return target;
  }

  /* seeks all iterators except the 
   * first to the specified target */
  doc_id_t seek_rest(doc_id_t target) {
    if (type_limits<type_t::doc_id_t>::eof(target)) {
      return target;
    }

    for (auto it = itrs_.begin()+1, end = itrs_.end(); it != end; ++it) {
      const auto doc = (*it)->seek(target);

      if (target < doc) {
        return doc;
      }
    }

    return target;
  }

  doc_iterators_t itrs_;
  doc_iterator* front_;
}; // conjunction

//////////////////////////////////////////////////////////////////////////////
/// @returns conjunction iterator created from the specified sub iterators 
//////////////////////////////////////////////////////////////////////////////
template<typename Conjunction, typename... Args>
score_doc_iterator::ptr make_conjunction(
    typename Conjunction::doc_iterators_t&& itrs,
    Args&&... args) {
  typedef Conjunction conjunction_t;

  /* check the size after execution */
  const size_t size = itrs.size();
  if (size < 1) {
    return score_doc_iterator::empty();
  } else if (size < 2) {
    return std::move(itrs.front());
  }

  return score_doc_iterator::make<conjunction_t>(
    std::move(itrs), std::forward<Args>(args)...
  );
}

//////////////////////////////////////////////////////////////////////////////
/// @returns conjunction iterator created from the specified sub queries 
//////////////////////////////////////////////////////////////////////////////
template<typename Conjunction, typename QueryIterator, typename... Args>
score_doc_iterator::ptr make_conjunction(
    const sub_reader& rdr, const order::prepared& ord, 
    QueryIterator begin, QueryIterator end,
    Args&&... args) {
  typedef typename Conjunction::doc_iterator doc_iterator_t;
  typedef typename Conjunction::traits_t traits_t;
  typedef typename std::enable_if<
    std::is_base_of<conjunction<doc_iterator_t, traits_t>, Conjunction>::value, Conjunction
  >::type conjunction_t;

  assert(std::distance(begin, end) >= 0);
  size_t size = size_t(std::distance(begin, end));
    
  /* check the size before the execution */
  if (size < 1) {
    return score_doc_iterator::empty();
  } else if (size < 2) {
    return begin->execute(rdr, ord);
  }

  typename conjunction_t::doc_iterators_t itrs;
  itrs.reserve(size_t(size));
  for(;begin != end; ++begin) {
    auto docs = begin->execute(rdr, ord);

    if (type_limits<type_t::doc_id_t>::eof(docs->value())) {
      // filter out empty iterators
      return score_doc_iterator::empty();
    }

    itrs.emplace_back(std::move(docs));
  }

  return make_conjunction<conjunction_t>(
    std::move(itrs), ord, std::forward<Args>(args)...
  );
}

NS_END // detail
NS_END // ROOT

#endif // IRESEARCH_CONJUNCTION_H
