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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

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

// -----------------------------------------------------------------------------
// --SECTION--                                                           payload
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(iresearch::payload);
DEFINE_ATTRIBUTE_TYPE(payload);

// -----------------------------------------------------------------------------
// --SECTION--                                                  payload_iterator
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(irs::payload_iterator);
DEFINE_ATTRIBUTE_TYPE(payload_iterator);

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

position::position(size_t reserve_attrs): attrs_(reserve_attrs) {
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------