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
#include "collector.hpp"
#include "cost.hpp"
#include "index/index_reader.hpp"
#include "utils/type_limits.hpp"

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
DEFINE_FACTORY_DEFAULT(score);

score::score() { }

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
// --SECTION--                                                             query
// -----------------------------------------------------------------------------

class auto_score_iterator final : public doc_iterator {
public:
  auto_score_iterator( score_doc_iterator::ptr&& it )
    : it_( std::move( it ) ) {
    assert( it_ );
  }

  virtual doc_id_t value() const override {
    return it_->value();
  }

  virtual const attribute_store& attributes() const NOEXCEPT override {
    return it_->attributes();
  }

  virtual bool next() override {
    if ( it_->next() ) {
      it_->score();
      return true;
    }

    return false;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = it_->seek( target );

    if (!type_limits<type_t::doc_id_t>::eof(doc)) {
      it_->score();
    }

    return doc;
  }

private:
  score_doc_iterator::ptr it_;
}; // auto_score_doc_iterator

query::query(query&& rhs) NOEXCEPT
  : filter_( std::move( rhs.filter_ ) ), 
    order_( std::move( rhs.order_ ) ) {
  assert( filter_ );
}

query& query::operator=(query&& rhs) NOEXCEPT {
  if ( this != &rhs ) {
    filter_ = std::move( rhs.filter_ );
    order_ = std::move( rhs.order_ );
  }

  return *this;
}

query query::prepare(
    const index_reader& rdr,
    const filter& filter,
    const order& order) {
  order::prepared porder = order.prepare();
  filter::prepared::ptr pfilter = filter.prepare(rdr, porder);
  return query(std::move(pfilter), std::move(porder));
}

query::query(filter::prepared::ptr&& filter, order::prepared&& order) NOEXCEPT
  : filter_(std::move(filter)), order_(std::move(order)) {
  assert(filter_);
}

doc_iterator::ptr query::execute(const sub_reader& rdr) const {
  if (order_.empty()) {
    return filter_->execute(rdr, order_);
  }

  return doc_iterator::make<auto_score_iterator>(
    filter_->execute(rdr, order_)
    );
}

void query::execute(const index_reader& rdr, collector& c) const {
  for (const auto& sr : rdr) {
    doc_iterator::ptr docs = execute(sr);
    c.prepare(c, docs->attributes());
    for (; docs->next();) {
      c.collect(c, docs->value());
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                               all
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @class all_query
/// @brief compiled all_filter that returns all documents
////////////////////////////////////////////////////////////////////////////////
class all_query : public filter::prepared {
 public:
  class all_iterator : public score_doc_iterator {
   public:
    all_iterator(uint64_t docs_count):
      attrs_(1), // cost
      max_doc_(doc_id_t(type_limits<type_t::doc_id_t>::min() + docs_count - 1)),
      doc_(type_limits<type_t::doc_id_t>::invalid()) {
      attrs_.emplace<cost>()->value(max_doc_);
    }

    virtual doc_id_t value() const {
      return doc_;
    }

    virtual const attribute_store& attributes() const NOEXCEPT {
      return attrs_;
    }

    virtual bool next() override { 
      return !type_limits<type_t::doc_id_t>::eof(seek(doc_ + 1));
    }

    virtual doc_id_t seek(doc_id_t target) override {
      doc_ = target <= max_doc_ ? target : type_limits<type_t::doc_id_t>::eof();
      return doc_;
    }

    virtual void score() override { }

   private:
    attribute_store attrs_;
    doc_id_t max_doc_; // largest valid doc_id
    doc_id_t doc_;
  };

  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared&) const override {
    return score_doc_iterator::make<all_iterator>(rdr.docs_count());
  }
};

DEFINE_FILTER_TYPE(iresearch::all);
DEFINE_FACTORY_DEFAULT(iresearch::all);

all::all() : filter(all::type()) { }

filter::prepared::ptr all::prepare( 
    const index_reader&, 
    const order::prepared&,
    boost_t) const {
  return filter::prepared::make<all_query>();
}

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

NS_END
