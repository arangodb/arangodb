////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "norm.hpp"
#include "store/store_utils.hpp"
#include "utils/bytes_utils.hpp"

namespace {

const irs::document INVALID_DOCUMENT;

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                         norm_base
// -----------------------------------------------------------------------------

norm_base::norm_base() noexcept
  : payload_(nullptr),
    doc_(&INVALID_DOCUMENT) {
}

bool norm_base::empty() const noexcept {
  return doc_ == &INVALID_DOCUMENT;
}

bool norm_base::reset(
    const sub_reader& reader,
    field_id column,
    const document& doc) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                              norm
// -----------------------------------------------------------------------------

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
void norm::compute(
    const field_stats& stats,
    doc_id_t doc,
    columnstore_writer::values_writer_f& writer) {
  if (stats.len > 0) {
    const float_t value = 1.f / float_t(std::sqrt(double_t(stats.len)));
    if (value != norm::DEFAULT()) {
      auto& stream = writer(doc);
      write_zvfloat(stream, value);
    }
  }
}

REGISTER_ATTRIBUTE(norm);

// -----------------------------------------------------------------------------
// --SECTION--                                                             norm2
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(norm2);

} // iresearch
