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

NS_LOCAL

const irs::doc_id_t INVALID_DOCUMENT = irs::type_limits<irs::type_t::doc_id_t>::invalid();

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                            offset
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::offset);
DEFINE_ATTRIBUTE_TYPE(offset);
DEFINE_FACTORY_DEFAULT(offset);

offset::offset() NOEXCEPT
  : start(0), end(0) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         increment
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::increment);
DEFINE_ATTRIBUTE_TYPE(increment);
DEFINE_FACTORY_DEFAULT(increment);

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
DEFINE_FACTORY_DEFAULT(payload);

payload::payload() NOEXCEPT
  : basic_attribute<bytes_ref>() {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                          document
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::document);
DEFINE_ATTRIBUTE_TYPE(document);
DEFINE_FACTORY_DEFAULT(document);

document::document() NOEXCEPT:
  basic_attribute<const doc_id_t*>(&INVALID_DOCUMENT) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         frequency
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::frequency);
DEFINE_ATTRIBUTE_TYPE(frequency);
DEFINE_FACTORY_DEFAULT(frequency);

frequency::frequency() NOEXCEPT
  : basic_attribute<uint64_t>() {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                granularity_prefix
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::granularity_prefix);
DEFINE_ATTRIBUTE_TYPE(granularity_prefix);
DEFINE_FACTORY_DEFAULT(granularity_prefix);

granularity_prefix::granularity_prefix() NOEXCEPT:
  attribute() {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              norm
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::norm);
DEFINE_ATTRIBUTE_TYPE(norm);
DEFINE_FACTORY_DEFAULT(norm);

norm::norm() NOEXCEPT
  : attribute() {
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
  assert(doc.value);

  const auto* column_reader = reader.column_reader(column);

  if (!column_reader) {
    return false;
  }

  column_ = column_reader->values();
  doc_ = doc.value;
  return true;
}

float_t norm::read() const {
  bytes_ref value;
  if (!column_(*doc_, value)) {
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
DEFINE_FACTORY_DEFAULT(position);

position::position() NOEXCEPT {
}

position::impl::impl(size_t reserve_attrs): attrs_(reserve_attrs) {}

NS_END
