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

#include "shared.hpp"
#include "token_attributes.hpp"
#include "store/store_utils.hpp"

namespace {

struct empty_position final : irs::position {
  virtual void reset() override { }
  virtual bool next() override { return false; }
  virtual attribute* get_mutable(irs::type_info::type_id) noexcept override {
    return nullptr;
  }
};

empty_position NO_POSITION;
const irs::document INVALID_DOCUMENT;

}

////////////////////////////////////////////////////////////////////////////////
/// !!! DO NOT MODIFY value in DEFINE_ATTRIBUTE_TYPE(...) as it may break
/// already created indexes !!!
////////////////////////////////////////////////////////////////////////////////

namespace iresearch {

REGISTER_ATTRIBUTE(offset);
REGISTER_ATTRIBUTE(increment);
REGISTER_ATTRIBUTE(term_attribute);
REGISTER_ATTRIBUTE(payload);
REGISTER_ATTRIBUTE(document);
REGISTER_ATTRIBUTE(frequency);
REGISTER_ATTRIBUTE(iresearch::granularity_prefix);

// -----------------------------------------------------------------------------
// --SECTION--                                                              norm
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(norm);

norm::norm() noexcept
  : payload_(nullptr),
    doc_(&INVALID_DOCUMENT) {
}

void norm::clear() noexcept {
  column_it_.reset();
  payload_ = nullptr;
  doc_ = &INVALID_DOCUMENT;
}

bool norm::empty() const noexcept {
  return doc_ == &INVALID_DOCUMENT;
}

bool norm::reset(const sub_reader& reader, field_id column, const document& doc) {
  const auto* column_reader = reader.column_reader(column);

  if (!column_reader) {
    return false;
  }

  column_it_ = column_reader->iterator();
  if (!column_it_) {
    return false;
  }

  payload_ = irs::get<irs::payload>(*column_it_);
  if (!payload_) {
    return false;
  }
  doc_ = &doc;
  return true;
}

float_t norm::read() const {
  assert(column_it_);
  if (doc_->value != column_it_->seek(doc_->value)) {
    return DEFAULT();
  }
  assert(payload_);
  // TODO: create set of helpers to decode float from buffer directly
  bytes_ref_input in(payload_->value);
  return read_zvfloat(in);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                          position
// -----------------------------------------------------------------------------

/*static*/ irs::position* position::empty() noexcept { return &NO_POSITION; }

REGISTER_ATTRIBUTE(position);

}
