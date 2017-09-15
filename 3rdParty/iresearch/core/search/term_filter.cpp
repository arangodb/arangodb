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

#include "term_filter.hpp"
#include "term_query.hpp"
#include "cost.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"

#include <boost/functional/hash.hpp>

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                            by_term implementation
// -----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(by_term);
DEFINE_FACTORY_DEFAULT(by_term);

by_term::by_term() 
  : filter(by_term::type()) {
}

by_term::by_term(const type_id& type)
  : filter( type ) {
}

bool by_term::equals(const filter& rhs) const {
  const by_term& trhs = static_cast<const by_term&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && term_ == trhs.term_;
}

size_t by_term::hash() const {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, fld_);
  ::boost::hash_combine(seed, term_);
  return seed;
}

filter::prepared::ptr by_term::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const {
  return term_query::make(rdr, ord, boost*this->boost(), fld_, term_);
}

NS_END // ROOT
