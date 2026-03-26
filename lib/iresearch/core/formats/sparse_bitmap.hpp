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

#pragma once

#include "analysis/token_attributes.hpp"
#include "index/iterators.hpp"
#include "search/cost.hpp"
#include "search/prev_doc.hpp"
#include "search/score.hpp"
#include "shared.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitset.hpp"
#include "utils/math_utils.hpp"
#include "utils/type_limits.hpp"

namespace irs {

enum class SparseBitmapVersion {
  kMin = 0,

  // Version supporting access to previous document
  kPrevDoc = 1,

  // Max supported version
  kMax = kPrevDoc
};

struct SparseBitmapWriterOptions {
  explicit SparseBitmapWriterOptions(SparseBitmapVersion version) noexcept
    : track_prev_doc{version >= SparseBitmapVersion::kPrevDoc} {}

  // Track previous document
  bool track_prev_doc;
};

class sparse_bitmap_writer {
 public:
  using value_type = doc_id_t;  // for compatibility with back_inserter

  static constexpr uint32_t kBlockSize = 1 << 16;
  static constexpr uint32_t kNumBlocks = kBlockSize / bits_required<size_t>();

  static_assert(math::is_power2(kBlockSize));

  struct block {
    doc_id_t index;
    uint32_t offset;
  };

  explicit sparse_bitmap_writer(
    index_output& out,
    SparseBitmapVersion version) noexcept(noexcept(out.file_pointer()))
    : out_{&out}, origin_{out.file_pointer()}, opts_{version} {}

  void push_back(doc_id_t value) {
    static_assert(math::is_power2(kBlockSize));
    IRS_ASSERT(doc_limits::valid(value));
    IRS_ASSERT(!doc_limits::eof(value));

    const uint32_t block = value / kBlockSize;

    if (block != block_) {
      flush(block_);
      block_ = block;
    }

    set(value % kBlockSize);
    prev_value_ = value;
  }

  bool erase(doc_id_t value) noexcept {
    if ((value / kBlockSize) < block_) {
      // value is already flushed
      return false;
    }

    reset(value % kBlockSize);
    return true;
  }

  void finish();

  std::span<const block> index() const noexcept { return block_index_; }

  SparseBitmapVersion version() const noexcept {
    static_assert(SparseBitmapVersion::kMin == SparseBitmapVersion{false});
    static_assert(SparseBitmapVersion::kPrevDoc == SparseBitmapVersion{true});
    return SparseBitmapVersion{opts_.track_prev_doc};
  }

 private:
  void flush(uint32_t next_block) {
    const uint32_t popcnt =
      static_cast<uint32_t>(math::popcount(std::begin(bits_), std::end(bits_)));
    if (popcnt) {
      add_block(next_block);
      do_flush(popcnt);
      popcnt_ += popcnt;
      std::memset(bits_, 0, sizeof bits_);
    }
  }

  IRS_FORCE_INLINE void set(doc_id_t value) noexcept {
    IRS_ASSERT(value < kBlockSize);

    irs::set_bit(bits_[value / bits_required<size_t>()],
                 value % bits_required<size_t>());
  }

  IRS_FORCE_INLINE void reset(doc_id_t value) noexcept {
    IRS_ASSERT(value < kBlockSize);

    irs::unset_bit(bits_[value / bits_required<size_t>()],
                   value % bits_required<size_t>());
  }

  IRS_FORCE_INLINE void add_block(uint32_t block_id) {
    const uint64_t offset = out_->file_pointer() - origin_;
    IRS_ASSERT(offset <= std::numeric_limits<uint32_t>::max());

    uint32_t count = 1 + block_id - static_cast<uint32_t>(block_index_.size());

    while (count) {
      block_index_.emplace_back(block{popcnt_, static_cast<uint32_t>(offset)});
      --count;
    }
  }

  void do_flush(uint32_t popcnt);

  index_output* out_;
  uint64_t origin_;
  size_t bits_[kNumBlocks]{};
  std::vector<block> block_index_;
  uint32_t popcnt_{};
  uint32_t block_{};  // last flushed block
  doc_id_t prev_value_{};
  doc_id_t last_in_flushed_block_{};
  SparseBitmapWriterOptions opts_;
};

// Denotes a position of a value associated with a document.
struct value_index : document {
  static constexpr std::string_view type_name() noexcept {
    return "value_index";
  }
};

class sparse_bitmap_iterator : public resettable_doc_iterator {
  static void Noop(index_input* /*in*/) noexcept {}
  static void Delete(index_input* in) noexcept { delete in; }
  // TOOD(MBkkt) unique_ptr isn't optimal here
  using Ptr = std::unique_ptr<index_input, void (*)(index_input*)>;

 public:
  using block_index_t = std::span<const sparse_bitmap_writer::block>;

  struct options {
    // Format version
    SparseBitmapVersion version{SparseBitmapVersion::kMax};

    // Use previous doc tracking
    bool track_prev_doc{true};

    // Use per block index (for dense blocks)
    bool use_block_index{true};

    // Blocks index
    block_index_t blocks;
  };

  sparse_bitmap_iterator(index_input::ptr&& in, const options& opts)
    : sparse_bitmap_iterator{Ptr{in.release(), &Delete}, opts} {}
  sparse_bitmap_iterator(index_input* in, const options& opts)
    : sparse_bitmap_iterator{Ptr{in, &Noop}, opts} {}

  template<typename Cost>
  sparse_bitmap_iterator(index_input::ptr&& in, const options& opts, Cost&& est)
    : sparse_bitmap_iterator{std::move(in), opts} {
    std::get<cost>(attrs_).reset(std::forward<Cost>(est));
  }

  template<typename Cost>
  sparse_bitmap_iterator(index_input* in, const options& opts, Cost&& est)
    : sparse_bitmap_iterator{in, opts} {
    std::get<cost>(attrs_).reset(std::forward<Cost>(est));
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t seek(doc_id_t target) final;

  bool next() final { return !doc_limits::eof(seek(value() + 1)); }

  doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

  void reset() final;

  // The value is undefined for
  // doc_limits::invalid() and doc_limits::eof()
  doc_id_t index() const noexcept {
    return std::get<value_index>(attrs_).value;
  }

 private:
  using block_seek_f = bool (*)(sparse_bitmap_iterator*, doc_id_t);

  template<uint32_t, bool>
  friend struct container_iterator;

  static bool initial_seek(sparse_bitmap_iterator* self, doc_id_t target);

  struct container_iterator_context {
    union {
      // We can also have an inverse sparse container (where we
      // most of values are 0). In this case only zeros can be
      // stored as an array.
      struct {
        const uint16_t* u16data;
        doc_id_t index;
      } sparse;
      struct {
        const size_t* u64data;
        doc_id_t popcnt;
        int32_t word_idx;
        size_t word;
        union {
          const uint16_t* u16data;
          const byte_type* u8data;
        } index;
      } dense;
      struct {
        const void* ignore;
        doc_id_t missing;
      } all;
      const byte_type* u8data;
    };
  };

  using attributes = std::tuple<document, value_index, prev_doc, cost, score>;

  explicit sparse_bitmap_iterator(Ptr&& in, const options& opts);

  void seek_to_block(doc_id_t block);
  void read_block_header();

  container_iterator_context ctx_;
  attributes attrs_;
  Ptr in_;
  std::unique_ptr<byte_type[]> block_index_data_;
  block_seek_f seek_func_;
  block_index_t block_index_;
  uint64_t cont_begin_;
  uint64_t origin_;
  doc_id_t index_{};  // beginning of the block
  doc_id_t index_max_{};
  doc_id_t block_{};
  doc_id_t prev_{};  // previous doc
  const bool use_block_index_;
  const bool prev_doc_written_;
  const bool track_prev_doc_;
};

}  // namespace irs
