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

NS_LOCAL

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
  typedef irs::disjunction<irs::doc_iterator::ptr> disjunction_t;

  assert(std::distance(begin, end) >= 0);
  const size_t size = size_t(std::distance(begin, end));

  // check the size before the execution
  if (0 == size) {
    // empty or unreachable search criteria
    return irs::doc_iterator::empty();
  }

  disjunction_t::doc_iterators_t itrs;
  itrs.reserve(size);

  for (;begin != end; ++begin) {
    // execute query - get doc iterator
    auto docs = begin->execute(rdr, ord, ctx);

    // filter out empty iterators
    if (!irs::doc_limits::eof(docs->value())) {
      itrs.emplace_back(std::move(docs));
    }
  }

  return irs::make_disjunction<disjunction_t>(
    std::move(itrs), ord, std::forward<Args>(args)...
  );
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

NS_END // LOCAL

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class boolean_query
/// @brief base class for boolean queries
//////////////////////////////////////////////////////////////////////////////
class boolean_query : public filter::prepared {
 public:
  typedef std::vector<filter::prepared::ptr> queries_t;
  typedef ptr_iterator<queries_t::const_iterator> iterator;

  DECLARE_SHARED_PTR(boolean_query);
  DEFINE_FACTORY_INLINE(boolean_query)

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

    return doc_iterator::make<exclusion>(
      std::move(incl), std::move(excl));
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

    min_match_disjunction<doc_iterator::ptr>::doc_iterators_t itrs;
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
      min_match_disjunction<doc_iterator::ptr>::doc_iterators_t&& itrs,
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
      return memory::make_shared<conjunction_t>(
        conjunction_t::doc_iterators_t(
          std::make_move_iterator(itrs.begin()),
          std::make_move_iterator(itrs.end())
        ), ord);
    }

    // min match disjunction
    assert(min_match_count < size);
    return memory::make_shared<min_match_disjunction<doc_iterator::ptr>>(
      std::move(itrs), min_match_count, ord);
  }

  size_t min_match_count_;
}; // min_match_query

// ----------------------------------------------------------------------------
// --SECTION--                                                   boolean_filter
// ----------------------------------------------------------------------------

boolean_filter::boolean_filter(const type_info& type) noexcept
  : filter(type) {
  all_docs_zero_boost_.boost(0.f);
}

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
  optimize(incl, excl, ord, boost);

  if (incl.empty() && !excl.empty()) {
    // single negative query case
    incl.push_back(&all_docs_);
  }

  return prepare(incl, excl, rdr, ord, boost, ctx);
}

void boolean_filter::group_filters(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& excl) const {
  incl.reserve(size() / 2);
  excl.reserve(incl.capacity());

  for (auto begin = this->begin(), end = this->end(); begin != end; ++begin) {
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
        if (type() == irs::type<Or>::id()) {
          // FIXME: this should have same boost as Not filter.
          // But for now we do not boost negation.
          incl.push_back(&all_docs_zero_boost_);
        }
      } else {
        incl.push_back(res.first);
      }
    } else {
      incl.push_back(&*begin);
    }
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                              And
// ----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(And)

And::And() noexcept
  : boolean_filter(irs::type<And>::get()) {
}

void And::optimize(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& /*excl*/,
    const order::prepared& ord,
    boost_t& boost) const {
  if (incl.empty()) {
    // nothing to do
    return;
  }

  // if include group has empty -> this whole conjunction is empty
  auto incl_empty = std::find_if(incl.begin(), incl.end(),
    [](const irs::filter* filter) {
      return irs::type<irs::empty>::id() == filter->type();
    });

  if (incl_empty != incl.end()) {
    // remove all except one empty
    std::swap(*incl.begin(), *incl_empty);
    incl.erase(incl.begin() + 1, incl.end());
    return;
  }

  boost_t all_boost{ 0 };
  size_t all_count{ 0 };
  std::for_each(
    incl.begin(), incl.end(),
    [&all_count, &all_boost](const irs::filter* filter) {
      if (filter->type() == irs::type<irs::all>::id()) {
        all_count++;
        all_boost += filter->boost();
      }
    });
  if (all_count != 0) {
    const auto non_all_count = incl.size() - all_count;
    if (non_all_count == 0) {
      // only all filters. erase all but first
      if (all_count > 1) {
        incl.erase(incl.begin() + 1, incl.end());
        auto single = const_cast<irs::filter*>(*incl.begin()); // FIXME: remove cast and make optimize non const
        assert(single->type() == irs::type<all>::id());
        single->boost(all_boost);
      }
    } else {
      // Here And differs from Or. Last 'All' should be left in include group only if there is more than one
      // filter of other type. Otherwise this another filter could be container for boost from 'all' filters
      if (1 == non_all_count) {
        auto it = std::remove_if(
          incl.begin(), incl.end(),
          [](const irs::filter* filter) {
            return irs::type<all>::id() == filter->type();
          });
        incl.erase(it, incl.end());
        // let this last filter to hold boost from all removed ones
        auto filter = const_cast<irs::filter*>(*incl.begin()); // FIXME: remove cast and make optimize non const
        filter->boost(filter->boost() + all_boost);
      } else {
        auto first_all = std::find_if(incl.begin(), incl.end(), [](const irs::filter* filter) {
          return irs::type<irs::all>::id() == filter->type();
          });
        auto it = std::remove_if(
          first_all + 1, incl.end(),
          [](const irs::filter* filter) {
            return irs::type<all>::id() == filter->type();
          });
        // let this all hold boost from all removed ones
        auto filter = const_cast<irs::filter*>(*first_all);
        filter->boost(all_boost);
        // remove found `all` filters except one
        incl.erase(it, incl.end());
      }
    }
  }
}

filter::prepared::ptr And::prepare(
    const std::vector<const filter*>& incl,
    const std::vector<const filter*>& excl,
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const {
  boost *= this->boost();

  if (incl.empty()) {
    // empty query
    return prepared::empty();
  } else if (1 == incl.size() && excl.empty()) {
    // single node case
    return incl.front()->prepare(rdr, ord, boost, ctx);
  }

  auto q = and_query::make<and_query>();
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

void Or::optimize(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& /*excl*/,
    const order::prepared& ord,
    boost_t& boost) const {
  if (incl.empty()) {
    // nothing to do
    return;
  }

  if (ord.empty()) {
    // if we have at least one all in include group - all other filters are not necessary
    // in case there is no scoring
    auto incl_all = std::find_if(incl.begin(), incl.end(), [](const irs::filter* filter) {
        return irs::type<irs::all>::id() == filter->type();
      });
    if (incl_all != incl.end() && incl.size() > 1) {
      std::swap(*incl.begin(), *incl_all);
      incl.erase(incl.begin() + 1, incl.end());
      return;
    }
  }

  boost_t all_boost{ 0 };
  size_t all_count{ 0 };
  std::for_each(
    incl.begin(), incl.end(),
    [&all_count, &all_boost](const irs::filter* filter) {
      if (filter->type() == irs::type<irs::all>::id()) {
        all_count++;
        all_boost += filter->boost();
      }
    });
  if (all_count != 0) {
    const auto non_all_count = incl.size() - all_count;
    if (non_all_count == 0) {
      // only all filters. erase all but first
      if (all_count > 1) {
        incl.erase(incl.begin() + 1, incl.end());
        auto single = const_cast<irs::filter*>(*incl.begin()); // FIXME: remove cast and make optimize non const
        assert(single->type() == irs::type<all>::id());
        single->boost(all_boost);
      }
    } else {
      // Here Or differs from And. Last All should be left in include group
      auto first_all = std::find_if(incl.begin(), incl.end(), [](const irs::filter* filter) {
        return irs::type<irs::all>::id() == filter->type();
        });
      auto it = std::remove_if(
        first_all + 1, incl.end(),
        [](const irs::filter* filter) {
          return irs::type<all>::id() == filter->type();
        });

      // let this all hold boost from all removed ones
      auto filter = const_cast<irs::filter*>(*first_all); // FIXME: remove cast and make optimize non const
      filter->boost(all_boost);
      // remove found `all` filters except one
      incl.erase(it, incl.end());
    }
  }

  // find `empty` filters
  auto it = std::remove_if(
    incl.begin(), incl.end(),
    [](const irs::filter* filter) {
      return irs::type<irs::empty>::id() == filter->type();
    });

  // remove found `empty` filters
  incl.erase(it, incl.end());
}

filter::prepared::ptr Or::prepare(
    const std::vector<const filter*>& incl,
    const std::vector<const filter*>& excl,
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const {
  boost *= this->boost();

  if (0 == min_match_count_) {
    // all conditions are satisfied
    return all().prepare(rdr, ord, boost, ctx);
  }

  if (min_match_count_ > incl.size()) {
    // can't satisfy 'min_match_count' conditions
    // having only 'incl.size()' queries
    return prepared::empty();
  }

  if (incl.empty()) {
    // empty query
    return prepared::empty();
  } else if (1 == incl.size() && excl.empty()) {
    // single node case
    return incl.front()->prepare(rdr, ord, boost, ctx);
  }

  assert(min_match_count_ > 0 && min_match_count_ <= incl.size());

  boolean_query::ptr q;
  if (min_match_count_ == incl.size()) {
    q = boolean_query::make<and_query>();
  } else if (1 == min_match_count_) {
    q = boolean_query::make<or_query>();
  } else { // min_match_count > 1 && min_match_count < incl.size()
    q = boolean_query::make<min_match_query>(min_match_count_);
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

    auto q = and_query::make<and_query>();
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

NS_END // ROOT
