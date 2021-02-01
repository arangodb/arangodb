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
#include "formats/formats.hpp"
#include "utils/type_limits.hpp"
#include "index_meta.hpp"

namespace iresearch {

/* -------------------------------------------------------------------
 * segment_meta
 * ------------------------------------------------------------------*/

segment_meta::segment_meta(const string_ref& name, format::ptr codec)
  : name(name.c_str(), name.size()),
    codec(codec),
    column_store(false) {
}

segment_meta::segment_meta(
    std::string&& name,
    format::ptr codec,
    uint64_t docs_count,
    uint64_t live_docs_count,
    bool column_store,
    segment_meta::file_set&& files,
    size_t size, /* = 0*/
    field_id sort /* = field_limits::invalid() */
) noexcept
  : files(std::move(files)),
    name(std::move(name)),
    docs_count(docs_count),
    live_docs_count(live_docs_count),
    codec(codec),
    size(size),
    sort(sort),
    column_store(column_store) {
}

segment_meta::segment_meta(segment_meta&& rhs)
    noexcept(noexcept(std::is_nothrow_move_constructible_v<file_set>))
  : files(std::move(rhs.files)),
    name(std::move(rhs.name)),
    docs_count(rhs.docs_count),
    live_docs_count(rhs.live_docs_count),
    codec(rhs.codec),
    size(rhs.size),
    version(rhs.version),
    sort(rhs.sort),
    column_store(rhs.column_store) {
  rhs.docs_count = 0;
  rhs.size = 0;
  rhs.sort = field_limits::invalid();
}

segment_meta& segment_meta::operator=(segment_meta&& rhs)
    noexcept(noexcept(std::is_nothrow_move_assignable_v<file_set>)) {
  if (this != &rhs) {
    files = std::move(rhs.files);
    name = std::move(rhs.name);
    docs_count = rhs.docs_count;
    rhs.docs_count = 0;
    live_docs_count = rhs.live_docs_count;
    rhs.live_docs_count = 0;
    codec = rhs.codec;
    rhs.codec = nullptr;
    size = rhs.size;
    rhs.size = 0;
    version = rhs.version;
    sort = rhs.sort;
    rhs.sort = field_limits::invalid();
    column_store = rhs.column_store;
  }

  return *this;
}

bool segment_meta::operator==(const segment_meta& other) const noexcept {
  return this == &other || false == (*this != other);
}

bool segment_meta::operator!=(const segment_meta& other) const noexcept {
  if (this == &other) {
    return false;
  }

  return name != other.name
    || version != other.version
    || docs_count != other.docs_count
    || live_docs_count != other.live_docs_count
    || codec != other.codec
    || size != other.size
    || column_store != other.column_store
    || files != other.files
    || sort != other.sort;
}

/* -------------------------------------------------------------------
 * index_meta
 * ------------------------------------------------------------------*/

index_meta::index_meta()
  : gen_(type_limits<type_t::index_gen_t>::invalid()),
    last_gen_(type_limits<type_t::index_gen_t>::invalid()),
    seg_counter_(0) {
}
index_meta::index_meta(const index_meta& rhs)
  : gen_(rhs.gen_),
    last_gen_(rhs.last_gen_),
    seg_counter_(rhs.seg_counter_.load()),
    segments_(rhs.segments_),
    payload_buf_(rhs.payload_buf_),
    payload_(rhs.payload_.null() ? bytes_ref::NIL : bytes_ref(payload_buf_)) {
}

index_meta::index_meta(index_meta&& rhs) noexcept
  : gen_(std::move(rhs.gen_)),
    last_gen_(std::move(rhs.last_gen_)),
    seg_counter_(rhs.seg_counter_.load()),
    segments_(std::move(rhs.segments_)),
    payload_buf_(std::move(rhs.payload_buf_)),
    payload_(rhs.payload_.null() ? bytes_ref::NIL : bytes_ref(payload_buf_)) {
}

index_meta& index_meta::operator=(index_meta&& rhs) noexcept {
  if (this != &rhs) {
    gen_ = std::move(rhs.gen_);
    last_gen_ = std::move(rhs.last_gen_);
    seg_counter_ = rhs.seg_counter_.load();
    segments_ = std::move(rhs.segments_);
    payload_buf_ = std::move(rhs.payload_buf_);
    payload_ = rhs.payload_.null() ? bytes_ref::NIL : bytes_ref(payload_buf_);
  }

  return *this;
}

bool index_meta::operator==(const index_meta& other) const noexcept {
  if (this == &other) {
    return true;
  }

  if (gen_ != other.gen_
      || last_gen_ != other.last_gen_
      || seg_counter_ != other.seg_counter_
      || segments_.size() != other.segments_.size()
      || payload_ != other.payload_) {
    return false;
  }

  for (size_t i = 0, count = segments_.size(); i < count; ++i) {
    if (segments_[i] != other.segments_[i]) {
      return false;
    }
  }

  return true;
}

uint64_t index_meta::next_generation() const noexcept {
  return type_limits<type_t::index_gen_t>::valid(gen_) ? (gen_ + 1) : 1;
}

/* -------------------------------------------------------------------
 * index_meta::index_segment_t
 * ------------------------------------------------------------------*/

index_meta::index_segment_t::index_segment_t(segment_meta&& meta)
  : meta(std::move(meta)) {
}

bool index_meta::index_segment_t::operator==(
    const index_segment_t& other) const noexcept {
  return this == &other || false == (*this != other);
}

bool index_meta::index_segment_t::operator!=(
    const index_segment_t& other) const noexcept {
  if (this == &other) {
    return false;
  }

  return filename != other.filename || meta != other.meta;
}

}
