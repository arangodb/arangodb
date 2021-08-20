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

#ifndef IRESEARCH_NORM_H
#define IRESEARCH_NORM_H

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/lz4compression.hpp"

namespace iresearch {

class norm_base {
 public:
  bool reset(const sub_reader& segment, field_id column, const document& doc);
  bool empty() const noexcept;

 protected:
  norm_base() noexcept;

  doc_iterator::ptr column_it_;
  const payload* payload_;
  const document* doc_;
}; // norm_base

static_assert(std::is_nothrow_move_constructible_v<norm_base>);
static_assert(std::is_nothrow_move_assignable_v<norm_base>);

//////////////////////////////////////////////////////////////////////////////
/// @class norm
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API norm : public norm_base {
 public:
  // DO NOT CHANGE NAME
  static constexpr string_ref type_name() noexcept {
    return "norm";
  }

  FORCE_INLINE static constexpr float_t DEFAULT() noexcept {
    return 1.f;
  }

  static void compute(
    const field_stats& stats,
    doc_id_t doc,
    columnstore_writer::values_writer_f& writer);

  float_t read() const;
}; // norm

static_assert(std::is_nothrow_move_constructible_v<norm>);
static_assert(std::is_nothrow_move_assignable_v<norm>);

//////////////////////////////////////////////////////////////////////////////
/// @class norm2
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API norm2 : public norm_base {
 public:
  // DO NOT CHANGE NAME
  static constexpr string_ref type_name() noexcept {
    return "iresearch::norm2";
  }

  static void compute(
      const field_stats& stats,
      doc_id_t doc,
      columnstore_writer::values_writer_f& writer) {
    writer(doc).write_int(stats.len);
  }

  uint32_t read() const {
    assert(column_it_);
    assert(payload_);

    if (IRS_LIKELY(doc_->value == column_it_->seek(doc_->value))) {
      assert(sizeof(uint32_t) == payload_->value.size());
      const auto* value = payload_->value.c_str();
      return irs::read<uint32_t>(value);
    }

    // we should investigate why we failed to find a norm2 value for doc
    assert(false);

    return 1;
  }
}; // norm2

static_assert(std::is_nothrow_move_constructible_v<norm2>);
static_assert(std::is_nothrow_move_assignable_v<norm2>);

} // iresearch

#endif // IRESEARCH_NORM_H
