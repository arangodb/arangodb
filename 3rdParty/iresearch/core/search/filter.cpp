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

#include "filter.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class emtpy_query 
/// @brief represent a query returns empty result set 
//////////////////////////////////////////////////////////////////////////////
class empty_query final : public filter::prepared {
 public:
  virtual score_doc_iterator::ptr execute(
      const sub_reader&,
      const order::prepared& ) const override {
    return score_doc_iterator::empty();
  }    
}; // empty_query

// ----------------------------------------------------------------------------
// --SECTION--                                                       Attributes
// ----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(iresearch::score);

// -----------------------------------------------------------------------------
// --SECTION--                                                            filter
// -----------------------------------------------------------------------------

filter::filter(const type_id& type)
  : boost_(boost::no_boost()), type_(&type) {
}

filter::~filter() {}

filter::prepared::ptr filter::prepared::empty() {
  return filter::prepared::make<empty_query>();
}

filter::prepared::prepared(attribute_store&& attrs)
  : attrs_(std::move(attrs)) {
}

filter::prepared::~prepared() {}

// -----------------------------------------------------------------------------
// --SECTION--                                                             empty
// -----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(irs::empty);
DEFINE_FACTORY_DEFAULT(irs::empty);

empty::empty(): filter(empty::type()) {
}

filter::prepared::ptr empty::prepare(
    const index_reader&,
    const order::prepared&,
    boost_t
) const {
  return filter::prepared::empty();
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
