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

#include "boolean_filter.hpp"

#include <boost/functional/hash.hpp>

#include "conjunction.hpp"
#include "disjunction.hpp"
#include "min_match_disjunction.hpp"
#include "exclusion.hpp"

namespace {

// first - pointer to the innermost not "not" node
// second - collapsed negation mark
std::pair<const irs::filter*, bool> optimize_not(const irs::Not& node) {
  bool neg = true;
  const irs::filter* inner = node.filter();
  while (inner && inner->type() == irs::type<irs::Not>::id()) {
    neg = !neg;
    inner = static_cast<const irs::Not*>(inner)->filter();
  }

  return std::make_pair(inner, neg);
}

const irs::all all_docs_zero_boost = []() {irs::all a; a.boost(0); return a;}();
//////////////////////////////////////////////////////////////////////////////
/// @returns disjunction iterator created from the specified queries
//////////////////////////////////////////////////////////////////////////////
template<typename QueryIterator, typename... Args>
irs::doc_iterator::ptr make_disjunction(
    const irs::sub_reader& rdr,
    const irs::order::prepared& ord,
    const irs::attribute_provider* ctx,
    QueryIterator begin,
    QueryIterator end,
    Args&&... args) {
  using scored_disjunction_t = irs::scored_disjunction_iterator<irs::doc_iterator::ptr>;
  using disjunction_t = irs::disjunction_iterator<irs::doc_iterator::ptr>;

  assert(std::distance(begin, end) >= 0);
  const size_t size = size_t(std::distance(begin, end));

  // check the size before the execution
  if (0 == size) {
    // empty or unreachable search criteria
    return irs::doc_iterator::empty();
  }

  scored_disjunction_t::doc_iterators_t itrs;
  itrs.reserve(size);

  for (;begin != end; ++begin) {
    // execute query - get doc iterator
    auto docs = begin->execute(rdr, ord, ctx);

    // filter out empty iterators
    if (!irs::doc_limits::eof(docs->value())) {
      itrs.emplace_back(std::move(docs));
    }
  }

  if (ord.empty()) {
    return irs::make_disjunction<disjunction_t>(
      std::move(itrs), ord, std::forward<Args>(args)...);
  }

  return irs::make_disjunction<scored_disjunction_t>(
    std::move(itrs), ord, std::forward<Args>(args)...);
}

//////////////////////////////////////////////////////////////////////////////
/// @returns conjunction iterator created from the specified queries
//////////////////////////////////////////////////////////////////////////////
template<typename QueryIterator, typename... Args>
irs::doc_iterator::ptr make_conjunction(
    const irs::sub_reader& rdr,
    const irs::order::prepared& ord,
    const irs::attribute_provider* ctx,
    QueryIterator begin,
    QueryIterator end,
    Args&&... args) {
  typedef irs::conjunction<irs::doc_iterator::ptr> conjunction_t;

  assert(std::distance(begin, end) >= 0);
  const size_t size = std::distance(begin, end);

  // check size before the execution
  switch (size) {
    case 0:
      return irs::doc_iterator::empty();
    case 1:
      return begin->execute(rdr, ord, ctx);
  }

  conjunction_t::doc_iterators_t itrs;
  itrs.reserve(size);

  for (;begin != end; ++begin) {
    auto docs = begin->execute(rdr, ord, ctx);

    // filter out empty iterators
    if (irs::doc_limits::eof(docs->value())) {
      return irs::doc_iterator::empty();
    }

    itrs.emplace_back(std::move(docs));
  }

  return irs::make_conjunction<conjunction_t>(
     std::move(itrs), ord, std::forward<Args>(args)...
  );
}

} // LOCAL

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @class boolean_query
/// @brief base class for boolean queries
//////////////////////////////////////////////////////////////////////////////
class boolean_query : public filter::prepared {
 public:
  typedef std::vector<filter::prepared::ptr> queries_t;
  typedef ptr_iterator<queries_t::const_iterator> iterator;

  boolean_query() noexcept : excl_(0) { }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider* ctx) const override {
    if (empty()) {
      return doc_iterator::empty();
    }

    assert(excl_);
    auto incl = execute(rdr, ord, ctx, begin(), begin() + excl_);

    // exclusion part does not affect scoring at all
    auto excl = ::make_disjunction(rdr, order::prepared::unordered(), ctx,
                                   begin() + excl_, end());

    // got empty iterator for excluded
    if (doc_limits::eof(excl->value())) {
      // pure conjunction/disjunction
      return incl;
    }

    return memory::make_managed<exclusion>(std::move(incl), std::move(excl));
  }

  virtual void prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost,
      const attribute_provider* ctx,
      const std::vector<const filter*>& incl,
      const std::vector<const filter*>& excl) {
    boolean_query::queries_t queries;
    queries.reserve(incl.size() + excl.size());

    // apply boost to the current node
    this->boost(boost);

    // prepare included
    for (const auto* filter : incl) {
      queries.emplace_back(filter->prepare(rdr, ord, boost, ctx));
    }

    // prepare excluded
    for (const auto* filter : excl) {
      // exclusion part does not affect scoring at all
      queries.emplace_back(filter->prepare(
        rdr, order::prepared::unordered(), irs::no_boost(), ctx));
    }

    // nothrow block
    queries_ = std::move(queries);
    excl_ = incl.size();
  }

  iterator begin() const { return iterator(queries_.begin()); }
  iterator excl_begin() const { return iterator(queries_.begin() + excl_); }
  iterator end() const { return iterator(queries_.end()); }

  bool empty() const { return queries_.empty(); }
  size_t size() const { return queries_.size(); }

 protected:
  virtual doc_iterator::ptr execute(
    const sub_reader& rdr,
    const order::prepared& ord,
    const attribute_provider* ctx,
    iterator begin,
    iterator end) const = 0;

 private:
  // 0..excl_-1 - included queries
  // excl_..queries.end() - excluded queries
  queries_t queries_;
  // index of the first excluded query
  size_t excl_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class and_query
/// @brief represent a set of queries joint by "And"
//////////////////////////////////////////////////////////////////////////////
class and_query final : public boolean_query {
 public:
  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider* ctx,
      iterator begin,
      iterator end) const override {
    return ::make_conjunction(rdr, ord, ctx, begin, end);
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class or_query
/// @brief represent a set of queries joint by "Or"
//////////////////////////////////////////////////////////////////////////////
class or_query final : public boolean_query {
 public:
  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider* ctx,
      iterator begin,
      iterator end) const override {
    return ::make_disjunction(rdr, ord, ctx, begin, end);
  }
}; // or_query

//////////////////////////////////////////////////////////////////////////////
/// @class min_match_query
/// @brief represent a set of queries joint by "Or" with the specified
/// minimum number of clauses that should satisfy criteria
//////////////////////////////////////////////////////////////////////////////
class min_match_query final : public boolean_query {
 private:
  using scored_disjunction_t = scored_min_match_iterator<doc_iterator::ptr>;
  using disjunction_t = min_match_iterator<doc_iterator::ptr>; // FIXME use FAST version

 public:
  explicit min_match_query(size_t min_match_count)
    : min_match_count_(min_match_count) {
    assert(min_match_count_ > 1);
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider* ctx,
      iterator begin,
      iterator end) const override {
    assert(std::distance(begin, end) >= 0);
    const size_t size = size_t(std::distance(begin, end));

    // 1 <= min_match_count
    size_t min_match_count = std::max(size_t(1), min_match_count_);

    // check the size before the execution
    if (0 == size || min_match_count > size) {
      // empty or unreachable search criteria
      return doc_iterator::empty();
    } else if (min_match_count == size) {
      // pure conjunction
      return ::make_conjunction(rdr, ord, ctx, begin, end);
    }

    // min_match_count <= size
    min_match_count = std::min(size, min_match_count);

    disjunction_t::doc_iterators_t itrs;
    itrs.reserve(size);

    for (;begin != end; ++begin) {
      // execute query - get doc iterator
      auto docs = begin->execute(rdr, ord, ctx);

      // filter out empty iterators
      if (!doc_limits::eof(docs->value())) {
        itrs.emplace_back(std::move(docs));
      }
    }

    return make_min_match_disjunction(
      std::move(itrs), ord, min_match_count);
  }

 private:
  static doc_iterator::ptr make_min_match_disjunction(
      disjunction_t::doc_iterators_t&& itrs,
      const order::prepared& ord,
      size_t min_match_count) {
    const auto size = min_match_count > itrs.size() ? 0 : itrs.size();

    switch (size) {
      case 0:
        // empty or unreachable search criteria
        return doc_iterator::empty();
      case 1:
        // single sub-query
        return std::move(itrs.front());
    }

    if (min_match_count == size) {
      typedef conjunction<doc_iterator::ptr> conjunction_t;

      // pure conjunction
      return memory::make_managed<conjunction_t>(
        conjunction_t::doc_iterators_t(
          std::make_move_iterator(itrs.begin()),
          std::make_move_iterator(itrs.end())), ord);
    }

    // min match disjunction
    assert(min_match_count < size);

    if (ord.empty()) {
      return memory::make_managed<disjunction_t>(std::move(itrs), min_match_count);
    }

    return memory::make_managed<scored_disjunction_t>(std::move(itrs), min_match_count, ord);
  }

  size_t min_match_count_;
}; // min_match_query

// ----------------------------------------------------------------------------
// --SECTION--                                                   boolean_filter
// ----------------------------------------------------------------------------

boolean_filter::boolean_filter(const type_info& type) noexcept
  : filter(type) {}

size_t boolean_filter::hash() const noexcept {
  size_t seed = 0;

  ::boost::hash_combine(seed, filter::hash());
  std::for_each(
    filters_.begin(), filters_.end(),
    [&seed](const filter::ptr& f){
      ::boost::hash_combine(seed, *f);
  });

  return seed;
}

bool boolean_filter::equals(const filter& rhs) const noexcept {
  const boolean_filter& typed_rhs = static_cast< const boolean_filter& >( rhs );

  return filter::equals(rhs)
    && filters_.size() == typed_rhs.size()
    && std::equal(begin(), end(), typed_rhs.begin());
}

filter::prepared::ptr boolean_filter::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const {
  // determine incl/excl parts
  std::vector<const filter*> incl;
  std::vector<const filter*> excl;
  
  group_filters(incl, excl);

  const irs::all all_docs_no_boost;
  if (incl.empty() && !excl.empty()) {
    // single negative query case
    incl.push_back(&all_docs_no_boost);
  }

  return prepare(incl, excl, rdr, ord, boost, ctx);
}

void boolean_filter::group_filters(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& excl) const {
  incl.reserve(size() / 2);
  excl.reserve(incl.capacity());

  const irs::filter* empty_filter{ nullptr };
  const auto is_or = type() == irs::type<Or>::id();
  for (auto begin = this->begin(), end = this->end(); begin != end; ++begin) {
    if (irs::type<irs::empty>::id() == begin->type()) {
      empty_filter = &*begin;
      continue;
    }
    if (irs::type<Not>::id() == begin->type()) {
#ifdef IRESEARCH_DEBUG
      const auto& not_node = dynamic_cast<const Not&>(*begin);
#else
      const auto& not_node = static_cast<const Not&>(*begin);
#endif
      const auto res = optimize_not(not_node);

      if (!res.first) {
        continue;
      }

      if (res.second) {
        if (irs::type<all>::id() == res.first->type()) {
          // not all -> empty result
          incl.clear();
          return;
        }
        excl.push_back(res.first);
        if (is_or) {
          // FIXME: this should have same boost as Not filter.
          // But for now we do not boost negation.
          incl.push_back(&all_docs_zero_boost);
        }
      } else {
        incl.push_back(res.first);
      }
    } else {
      incl.push_back(&*begin);
    }
  }
  if (empty_filter != nullptr) {
    incl.push_back(empty_filter);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                              And
// ----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(And)

And::And() noexcept
  : boolean_filter(irs::type<And>::get()) {
}

filter::prepared::ptr And::prepare(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& excl,
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const {

  //optimization step
  // if include group empty itself or has 'empty' -> this whole conjunction is empty
  if (incl.empty() || incl.back()->type() == irs::type<irs::empty>::id()) {
    return prepared::empty();
  }

  irs::all cumulative_all;
  boost_t all_boost{ 0 };
  size_t all_count{ 0 };
  for (auto filter : incl) {
    if (filter->type() == irs::type<irs::all>::id()) {
      all_count++;
      all_boost += filter->boost();
    }
  }
  if (all_count != 0) {
    const auto non_all_count = incl.size() - all_count;
    auto it = std::remove_if(
      incl.begin(), incl.end(),
      [](const irs::filter* filter) {
        return irs::type<all>::id() == filter->type();
      });
    incl.erase(it, incl.end());
    // Here And differs from Or. Last 'All' should be left in include group only if there is more than one
    // filter of other type. Otherwise this another filter could be container for boost from 'all' filters
    if (1 == non_all_count) {
      // let this last filter hold boost from all removed ones
      // so we aggregate in external boost values from removed all filters
      // If we will not optimize resulting boost will be:
      //   boost * OR_BOOST * ALL_BOOST + boost * OR_BOOST * LEFT_BOOST
      // We could adjust only 'boost' so we recalculate it as
      // new_boost =  ( boost * OR_BOOST * ALL_BOOST + boost * OR_BOOST * LEFT_BOOST) / (OR_BOOST * LEFT_BOOST)
      // so when filter will be executed resulting boost will be:
      // new_boost * OR_BOOST * LEFT_BOOST. If we substitute new_boost back we will get
      // ( boost * OR_BOOST * ALL_BOOST + boost * OR_BOOST * LEFT_BOOST) - original non-optimized boost value
      auto left_boost = (*incl.begin())->boost();
      if (this->boost() != 0 && left_boost != 0 && !ord.empty()) {
        boost = (boost * this->boost() * all_boost + boost * this->boost() * left_boost)
          / (left_boost * this->boost());
      } else {
        boost = 0;
      }
    } else {
      // create new 'all' with boost from all removed
      cumulative_all.boost(all_boost);
      incl.push_back(&cumulative_all);
    }
  }
  boost *= this->boost();
  if (1 == incl.size() && excl.empty()) {
    // single node case
    return incl.front()->prepare(rdr, ord, boost, ctx);
  }
  auto q = memory::make_managed<and_query>();
  q->prepare(rdr, ord, boost, ctx, incl, excl);
  return q;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                               Or
// ----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(Or)

Or::Or() noexcept
  : boolean_filter(irs::type<Or>::get()),
    min_match_count_(1) {
}


filter::prepared::ptr Or::prepare(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& excl,
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const {
  // preparing
  boost *= this->boost();

  if (0 == min_match_count_) { // only explicit 0 min match counts!
    // all conditions are satisfied
    return all().prepare(rdr, ord, boost, ctx);
  }

  if (!incl.empty() && incl.back()->type() == irs::type<irs::empty>::id()) {
    incl.pop_back();
  }

  if (incl.empty()) {
    return prepared::empty();
  }

  irs::all cumulative_all;
  size_t optimized_match_count = 0;
  // Optimization steps

  boost_t all_boost{ 0 };
  size_t all_count{ 0 };
  const irs::filter* incl_all{ nullptr };
  for (auto filter : incl) {
    if (filter->type() == irs::type<irs::all>::id()) {
      all_count++;
      all_boost += filter->boost();
      incl_all = filter;
    }
  }
  if (all_count != 0) {
    if (ord.empty() && incl.size() > 1 && min_match_count_ <= all_count) {
      // if we have at least one all in include group - all other filters are not necessary
      // in case there is no scoring and 'all' count satisfies  min_match
      assert(incl_all != nullptr);
      incl.resize(1);
      incl.front() = incl_all;
      optimized_match_count = all_count - 1;
    } else {
      // Here Or differs from And. Last All should be left in include group
      auto it = std::remove_if(
        incl.begin(), incl.end(),
        [](const irs::filter* filter) {
          return irs::type<all>::id() == filter->type();
        });
      incl.erase(it, incl.end());
      // create new 'all' with boost from all removed
      cumulative_all.boost(all_boost);
      incl.push_back(&cumulative_all);
      optimized_match_count = all_count - 1;
    }
  }
  // check strictly less to not roll back to 0 min_match (we`ve handled this case above!)
  // single 'all' left -> it could contain boost we want to preserve
  const auto adjusted_min_match_count = (optimized_match_count < min_match_count_) ?
                                        min_match_count_ - optimized_match_count :
                                        1;

  if (adjusted_min_match_count > incl.size()) {
    // can't satisfy 'min_match_count' conditions
    // having only 'incl.size()' queries
    return prepared::empty();
  }

  if (1 == incl.size() && excl.empty()) {
    // single node case
    return incl.front()->prepare(rdr, ord, boost, ctx);
  }

  assert(adjusted_min_match_count > 0 && adjusted_min_match_count <= incl.size());

  memory::managed_ptr<boolean_query> q;
  if (adjusted_min_match_count == incl.size()) {
    q = memory::make_managed<and_query>();
  } else if (1 == adjusted_min_match_count) {
    q = memory::make_managed<or_query>();
  } else { // min_match_count > 1 && min_match_count < incl.size()
    q = memory::make_managed<min_match_query>(adjusted_min_match_count);
  }

  q->prepare(rdr, ord, boost, ctx, incl, excl);
  return q;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                              Not
// ----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(Not)

Not::Not() noexcept
  : irs::filter(irs::type<Not>::get()) {
}

filter::prepared::ptr Not::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const {
  const auto res = optimize_not(*this);

  if (!res.first) {
    return prepared::empty();
  }

  boost *= this->boost();

  if (res.second) {
    all all_docs;
    const std::vector<const irs::filter*> incl { &all_docs };
    const std::vector<const irs::filter*> excl { res.first };

    auto q = memory::make_managed<and_query>();
    q->prepare(rdr, ord, boost, ctx, incl, excl);
    return q;
  }

  // negation has been optimized out
  return res.first->prepare(rdr, ord, boost, ctx);
}

size_t Not::hash() const noexcept {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  if (filter_) {
    ::boost::hash_combine<const irs::filter&>(seed, *filter_);
  }
  return seed;
}

bool Not::equals(const irs::filter& rhs) const noexcept {
  const Not& typed_rhs = static_cast<const Not&>(rhs);
  return filter::equals(rhs)
    && ((!empty() && !typed_rhs.empty() && *filter_ == *typed_rhs.filter_)
       || (empty() && typed_rhs.empty()));
}

} // ROOT
