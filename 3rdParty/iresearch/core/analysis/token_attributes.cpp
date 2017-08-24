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

#include "shared.hpp"
#include "token_attributes.hpp"
#include "store/store_utils.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                            offset
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::offset);
DEFINE_ATTRIBUTE_TYPE(offset);

// -----------------------------------------------------------------------------
// --SECTION--                                                         increment
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::increment);
DEFINE_ATTRIBUTE_TYPE(increment);

increment::increment() NOEXCEPT
  : basic_attribute<uint32_t>(1U) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    term_attribute
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::term_attribute);
DEFINE_ATTRIBUTE_TYPE(term_attribute);

term_attribute::term_attribute() NOEXCEPT {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           payload
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::payload);
DEFINE_ATTRIBUTE_TYPE(payload);

// -----------------------------------------------------------------------------
// --SECTION--                                                          document
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::document);
DEFINE_ATTRIBUTE_TYPE(document);

document::document() NOEXCEPT:
  basic_attribute<doc_id_t>(type_limits<type_t::doc_id_t>::invalid()) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         frequency
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::frequency);
DEFINE_ATTRIBUTE_TYPE(frequency);

// -----------------------------------------------------------------------------
// --SECTION--                                                granularity_prefix
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::granularity_prefix);
DEFINE_ATTRIBUTE_TYPE(iresearch::granularity_prefix);

// -----------------------------------------------------------------------------
// --SECTION--                                                              norm
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::norm);
DEFINE_ATTRIBUTE_TYPE(norm);
DEFINE_FACTORY_DEFAULT(norm);

const document INVALID_DOCUMENT;

norm::norm() NOEXCEPT {
  reset();
}

void norm::reset() {
  column_ = [](doc_id_t, bytes_ref&){ return false; };
  doc_ = &INVALID_DOCUMENT;
}

bool norm::empty() const {
  return doc_ == &INVALID_DOCUMENT;
}

bool norm::reset(const sub_reader& reader, field_id column, const document& doc) {
  const auto* column_reader = reader.column_reader(column);

  if (!column_reader) {
    return false;
  }

  column_ = column_reader->values();
  doc_ = &doc;
  return true;
}

float_t norm::read() const {
  bytes_ref value;
  if (!column_(doc_->value, value)) {
    return DEFAULT();
  }

  // TODO: create set of helpers to decode float from buffer directly
  bytes_ref_input in(value);
  return read_zvfloat(in);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                          position
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::position);
DEFINE_ATTRIBUTE_TYPE(position);

position::impl::impl(size_t reserve_attrs)
  : attrs_(reserve_attrs) {
}

NS_END
