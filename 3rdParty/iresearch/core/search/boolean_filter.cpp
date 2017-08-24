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

#include "all_filter.hpp"
#include "boolean_filter.hpp"
#include "disjunction.hpp"
#include "conjunction.hpp"
#include "exclusion.hpp"
#include <boost/functional/hash.hpp>

NS_ROOT
NS_BEGIN(detail)

/* first - pointer to the innermost not "not" node 
 * second - optimized inversion */
std::pair<const filter*, bool> optimize_not( const Not& node ) {
  bool neg = true;
  const filter* inner = node.filter();
  for ( ; inner && inner->type() == Not::type(); ) {
    neg = !neg;
    inner = static_cast< const Not* >( inner )->filter();
  }

  return std::make_pair( inner, neg );
}

NS_END // detail

//////////////////////////////////////////////////////////////////////////////
/// @class boolean_query
/// @brief base class for boolean queries 
//////////////////////////////////////////////////////////////////////////////
class boolean_query : public filter::prepared {
 public:
  typedef std::vector<filter::prepared::ptr> queries_t;
  typedef ptr_iterator<queries_t::const_iterator> iterator;

  DECLARE_SPTR(boolean_query);
  DECLARE_FACTORY(boolean_query);

  boolean_query() : excl_(0)  { }

  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord ) const override {
    typedef detail::disjunction<score_doc_iterator::ptr> disjunction_t;

    if (empty()) {
      return score_doc_iterator::empty();
    }

    assert(excl_);
    auto incl = execute(rdr, ord, begin(), begin() + excl_);
    auto excl = detail::make_disjunction<disjunction_t>(
      rdr, ord, begin() + excl_, end(), 1 
    );

    // got empty iterator for excluded
    if (type_limits<type_t::doc_id_t>::eof(excl->value())) {
      // pure conjunction/disjunction
      return incl;
    }

    return score_doc_iterator::make<detail::exclusion>(
      std::move(incl), std::move(excl) 
    );
  }

  virtual void prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost::boost_t boost,
      const std::vector<const filter*>& incl,
      const std::vector<const filter*>& excl) {
    boolean_query::queries_t queries;
    queries.reserve(incl.size() + excl.size());

    /* apply boost to the current node */
    boost::apply(this->attributes(), boost);

    /* prepare included */
    std::transform(
      incl.begin(), incl.end(),
      irstd::back_emplacer(queries),
      [&rdr, &ord, boost] (const filter* sub) {
        return sub->prepare(rdr, ord, boost);
    });

    /* prepare excluded */
    std::transform(
      excl.begin(), excl.end(),
      irstd::back_emplacer(queries),
      [&rdr, &ord] (const filter* sub) {
        return sub->prepare(rdr, ord);
    });

    /* nothrow block */
    queries_ = std::move(queries);
    excl_ = incl.size();
  }

  iterator begin() const { return iterator(queries_.begin()); }
  iterator excl_begin() const { return iterator(queries_.begin() + excl_); }
  iterator end() const { return iterator(queries_.end()); }

  bool empty() const { return queries_.empty(); }
  size_t size() const { return queries_.size(); }

protected:
  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      iterator begin,
      iterator end) const = 0;

 private:
  /* 0..excl_-1 - included queries
   * excl_..queries.end() - excluded queries */
  queries_t queries_;
  /* index of the first excluded query */
  size_t excl_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class and_query
/// @brief represent a set of queries joint by "And"
//////////////////////////////////////////////////////////////////////////////
class and_query final : public boolean_query {
public:
  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      iterator begin, 
      iterator end) const override {
    typedef score_wrapper<score_doc_iterator::ptr> score_wrapper_t;
    typedef detail::conjunction<score_wrapper_t> conjunction_t;

    return detail::make_conjunction<conjunction_t>(
      rdr, ord, begin, end
    );
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class or_query
/// @brief represent a set of queries joint by "Or"
//////////////////////////////////////////////////////////////////////////////
class or_query final : public boolean_query {
 public:
  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      iterator begin,
      iterator end) const override {
    typedef score_wrapper<score_doc_iterator::ptr> score_wrapper_t;
    typedef detail::disjunction<score_wrapper_t> disjunction_t;

    return detail::make_disjunction<disjunction_t>(
      rdr, ord, begin, end, 1
    );
  }
}; // or_query

//////////////////////////////////////////////////////////////////////////////
/// @class min_match_query
/// @brief represent a set of queries joint by "Or" with the specified
/// minimum number of clauses that should satisfy criteria
//////////////////////////////////////////////////////////////////////////////
class min_match_query final : public boolean_query {
 public:
  min_match_query(size_t min_match_count)
    : min_match_count_(min_match_count) {
    assert(min_match_count_ > 1);
  }

  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      iterator begin, iterator end) const override {
   typedef score_wrapper<score_doc_iterator::ptr> score_wrapper_t;
   typedef cost_wrapper<score_wrapper_t> cost_wrapper_t;
   typedef detail::disjunction<cost_wrapper_t> disjunction_t;

   return detail::make_disjunction<disjunction_t>(
     rdr, ord, begin, end, min_match_count_
   );
  }

 private:
  size_t min_match_count_;
}; // min_match_query

// ----------------------------------------------------------------------------
// --SECTION--                                                   boolean_filter 
// ----------------------------------------------------------------------------

boolean_filter::boolean_filter(const type_id& type)
  : filter(type) {
}

size_t boolean_filter::hash() const {
  size_t seed = 0; 

  ::boost::hash_combine<const filter&>( seed, *this );
  std::for_each(
    filters_.begin(), filters_.end(),
    [&seed](const filter::ptr& f){ 
      ::boost::hash_combine( seed, *f );
  } );

  return seed;
}

bool boolean_filter::equals( const filter& rhs ) const {
  const boolean_filter& typed_rhs = static_cast< const boolean_filter& >( rhs );

  return filter::equals( rhs )
    && filters_.size() == typed_rhs.size()
    && std::equal(begin(), end(), typed_rhs.begin() );
}

void boolean_filter::group_filters(
    std::vector<const filter*>& incl,
    std::vector<const filter*>& excl) const {
  incl.reserve(size() / 2);
  excl.reserve(incl.capacity());
  for (auto begin = this->begin(), end = this->end(); begin != end; ++begin) {
    const Not* not_node = begin.safe_as<Not>();
    if (not_node) {
      auto res = detail::optimize_not(*not_node);
      if (!res.first) {
        continue;
      }

      if (res.second) {
        excl.push_back(res.first);
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

DEFINE_FILTER_TYPE(And);
DEFINE_FACTORY_DEFAULT(And);

And::And() : boolean_filter(And::type()) {}

filter::prepared::ptr And::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const {
  /* determine incl/excl parts */
  std::vector<const filter*> incl;
  std::vector<const filter*> excl;
  group_filters(incl, excl);

  /* single negative query case */
  all all_docs;
  if (incl.empty() & !excl.empty()) {
    incl.push_back(&all_docs);
  }

  auto q = and_query::make<and_query>();
  q->prepare(rdr, ord, this->boost()*boost, incl, excl);
  return q;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                               Or 
// ----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(Or);
DEFINE_FACTORY_DEFAULT(Or);

Or::Or() : boolean_filter(Or::type()), min_match_count_(1) {}

filter::prepared::ptr Or::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const {
  /* determine incl/excl parts */
  std::vector<const filter*> incl;
  std::vector<const filter*> excl;
  group_filters(incl, excl);

  /* single negative query case */
  all all_docs;
  if (incl.empty() & !excl.empty()) {
    incl.push_back(&all_docs);
  }
  size_t min_match_count = std::max(size_t(1), min_match_count_);

  boolean_query::ptr q;
  if (min_match_count >= incl.size()) {
    q = boolean_query::make<and_query>();
  } else if (min_match_count == 1) {
    q = boolean_query::make<or_query>();
  } else { /* min_match_count > 1 && min_match_count < incl.size() */
    q = boolean_query::make<min_match_query>(min_match_count);
  }

  q->prepare(rdr, ord, this->boost()*boost, incl, excl);
  return q;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                              Not 
// ----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(Not);
DEFINE_FACTORY_DEFAULT(Not);

Not::Not() : iresearch::filter(Not::type()) {}

filter::prepared::ptr Not::prepare(
    const index_reader&,
    const order::prepared&,
    boost_t) const {
  return filter::prepared::empty();
}

size_t Not::hash() const {
  size_t seed = 0;
  ::boost::hash_combine<const iresearch::filter&>( seed, *this );
  ::boost::hash_combine<const iresearch::filter&>( seed, *filter_ );
  return seed;
}

bool Not::equals( const iresearch::filter& rhs ) const {
  const Not& typed_rhs = static_cast< const Not& >( rhs );
  return filter::equals( rhs ) 
    && ((!empty() && !typed_rhs.empty() && *filter_ == *typed_rhs.filter_)
       || (empty() && typed_rhs.empty()));
}

NS_END // root

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------