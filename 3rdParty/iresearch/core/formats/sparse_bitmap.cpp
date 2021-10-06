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

#include "sparse_bitmap.hpp"

#include "search/bitset_doc_iterator.hpp"

namespace {

using namespace irs;

////////////////////////////////////////////////////////////////////////////////
/// @brief we use dense container for blocks having
///        more than this number of documents
////////////////////////////////////////////////////////////////////////////////
constexpr uint32_t BITSET_THRESHOLD = (1 << 12) - 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief we don't use index for blocks located at a distance that is
///        closer than this value
////////////////////////////////////////////////////////////////////////////////
constexpr uint32_t BLOCK_SCAN_THRESHOLD = 2;

////////////////////////////////////////////////////////////////////////////////
/// @enum BlockType
////////////////////////////////////////////////////////////////////////////////
enum BlockType : uint32_t {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief dense block is represented as a bitset container
  //////////////////////////////////////////////////////////////////////////////
  BT_DENSE = 0,

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sprase block is represented as an array of values
  //////////////////////////////////////////////////////////////////////////////
  BT_SPARSE
}; // BlockType

////////////////////////////////////////////////////////////////////////////////
/// @enum AccessType
////////////////////////////////////////////////////////////////////////////////
enum AccessType : uint32_t {
  //////////////////////////////////////////////////////////////////////////////
  /// @enum access data via stream API
  //////////////////////////////////////////////////////////////////////////////
  AT_STREAM,

  //////////////////////////////////////////////////////////////////////////////
  /// @enum direct memory access
  //////////////////////////////////////////////////////////////////////////////
  AT_DIRECT,

  //////////////////////////////////////////////////////////////////////////////
  /// @enum aligned direct memory access
  //////////////////////////////////////////////////////////////////////////////
  AT_DIRECT_ALIGNED
}; // AccessType

constexpr size_t DENSE_BLOCK_INDEX_BLOCK_SIZE = 512;
constexpr size_t DENSE_BLOCK_INDEX_NUM_BLOCKS
  = sparse_bitmap_writer::BLOCK_SIZE / DENSE_BLOCK_INDEX_BLOCK_SIZE;
constexpr size_t DENSE_INDEX_BLOCK_SIZE_IN_BYTES
  = DENSE_BLOCK_INDEX_NUM_BLOCKS * sizeof(uint16_t);
constexpr uint32_t DENSE_BLOCK_INDEX_WORDS_PER_BLOCK
    = DENSE_BLOCK_INDEX_BLOCK_SIZE / bits_required<size_t>();

template<size_t N>
void write_block_index(irs::index_output& out, size_t (&bits)[N]) {
  uint16_t popcnt = 0;
  uint16_t index[DENSE_BLOCK_INDEX_NUM_BLOCKS];
  static_assert(DENSE_INDEX_BLOCK_SIZE_IN_BYTES == sizeof index);

  auto* block = reinterpret_cast<byte_type*>(std::begin(index));
  auto begin = std::begin(bits);
  for (; begin != std::end(bits); begin += DENSE_BLOCK_INDEX_WORDS_PER_BLOCK) {
    irs::write<uint16_t>(block, popcnt);

    for (uint32_t i = 0; i < DENSE_BLOCK_INDEX_WORDS_PER_BLOCK; ++i) {
      popcnt += math::math_traits<size_t>::pop(begin[i]);
    }
  }

  out.write_bytes(
    reinterpret_cast<const byte_type*>(std::begin(index)),
    sizeof index);
}

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                              sparse_bitmap_writer
// -----------------------------------------------------------------------------

void sparse_bitmap_writer::finish() {
  flush(block_);

  if (block_index_.size() < BLOCK_SIZE) {
    add_block(block_ ? block_ + 1 : 0);
  }

  // create a sentinel block to issue doc_limits::eof() automatically
  block_ = doc_limits::eof() / BLOCK_SIZE;
  set(doc_limits::eof() % BLOCK_SIZE);
  do_flush(1);
}

void sparse_bitmap_writer::do_flush(uint32_t popcnt) {
  assert(popcnt);
  assert(block_ < BLOCK_SIZE);
  assert(popcnt <= BLOCK_SIZE);

  out_->write_short(static_cast<uint16_t>(block_));
  out_->write_short(static_cast<uint16_t>(popcnt - 1)); // -1 to fit uint16_t

  if (popcnt > BITSET_THRESHOLD) {
    if (popcnt != BLOCK_SIZE) {
      write_block_index(*out_, bits_);

      if constexpr (!is_big_endian()) {
        std::for_each(
          std::begin(bits_), std::end(bits_),
          [](auto& v){ v = numeric_utils::numeric_traits<size_t>::hton(v); });
      }

      out_->write_bytes(
        reinterpret_cast<const byte_type*>(bits_),
        sizeof bits_);
    }
  } else {
    bitset_doc_iterator it(std::begin(bits_), std::end(bits_));

    while (it.next()) {
      out_->write_short(static_cast<uint16_t>(it.value()));
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    block_iterator
// -----------------------------------------------------------------------------

template<uint32_t>
struct container_iterator;

template<>
struct container_iterator<BT_SPARSE> {
  template<AccessType Access>
  static bool seek(sparse_bitmap_iterator* self, doc_id_t target) {
    target &= 0x0000FFFF;

    auto& ctx = self->ctx_.sparse;
    const doc_id_t index_max = self->index_max_;

    for (; self->index_ < index_max; ++self->index_) {
      doc_id_t doc;
      if constexpr (AT_STREAM == Access) {
        doc = self->in_->read_short();
      } else if constexpr (is_big_endian()) {
        if constexpr (AT_DIRECT_ALIGNED == Access) {
          doc = *ctx.u16data;
        } else {
          static_assert(Access == AT_DIRECT);
          std::memcpy(&doc, ctx.u16data, sizeof(uint16_t));
        }
        ++ctx.u16data;
      } else {
        doc = irs::read<uint16_t>(self->ctx_.u8data);
      }

      if (doc >= target) {
        std::get<document>(self->attrs_).value = self->block_ | doc;
        std::get<value_index>(self->attrs_).value = self->index_++;
        return true;
      }
    }

    return false;
  }
};

template<>
struct container_iterator<BT_DENSE> {
  template<AccessType Access>
  static bool seek(sparse_bitmap_iterator* self, doc_id_t target) {
    auto& ctx = self->ctx_.dense;

    const int32_t target_word_idx
      = (target & 0x0000FFFF) / bits_required<size_t>();
    assert(target_word_idx >= ctx.word_idx);

    if (ctx.index.u16data &&
        uint32_t(target_word_idx - ctx.word_idx) >= DENSE_BLOCK_INDEX_WORDS_PER_BLOCK) {
      const size_t index_block = (target & 0x0000FFFF) / DENSE_BLOCK_INDEX_BLOCK_SIZE;

      uint16_t popcnt;
      std::memcpy(&popcnt, &ctx.index.u16data[index_block], sizeof(uint16_t));
      if constexpr (!is_big_endian()) {
        popcnt = (popcnt >> 8) | ((popcnt & 0xFF) << 8);
      }

      const auto word_idx = index_block*DENSE_BLOCK_INDEX_WORDS_PER_BLOCK;
      const auto delta = word_idx - ctx.word_idx;
      assert(delta > 0);

      if constexpr (AT_STREAM == Access) {
        self->in_->seek(self->in_->file_pointer() + (delta-1)*sizeof(size_t));
        ctx.word = self->in_->read_long();
      } else {
        ctx.u64data += delta;
        ctx.word = ctx.u64data[-1];
      }

      ctx.popcnt = ctx.index_base + popcnt + math::math_traits<size_t>::pop(ctx.word);
      ctx.word_idx = word_idx;
    }

    uint32_t word_delta = target_word_idx - ctx.word_idx;

    if constexpr (AT_STREAM == Access) {
      for (; word_delta; --word_delta) {
        ctx.word = self->in_->read_long();
        ctx.popcnt += math::math_traits<size_t>::pop(ctx.word);
      }
      ctx.word_idx = target_word_idx;
    } else {
      if (word_delta) {
        // FIMXE consider using SSE/avx256/avx512 extensions for large skips

        const size_t* end = ctx.u64data + word_delta;
        for (; ctx.u64data < end; ++ctx.u64data) {
          if constexpr (AT_DIRECT_ALIGNED == Access) {
            ctx.word = *ctx.u64data;
          } else {
            static_assert(AT_DIRECT == Access);
            std::memcpy(&ctx.word, ctx.u64data, sizeof(size_t));
          }
          ctx.popcnt += math::math_traits<size_t>::pop(ctx.word);
        }
        if constexpr (!is_big_endian()) {
          ctx.word = numeric_utils::numeric_traits<size_t>::ntoh(ctx.word);
        }
        ctx.word_idx = target_word_idx;
      }
    }

    const size_t left = ctx.word >> (target % bits_required<size_t>());

    if (left) {
      const doc_id_t offset = math::math_traits<decltype(left)>::ctz(left);
      std::get<document>(self->attrs_).value = target + offset;
      std::get<value_index>(self->attrs_).value
        = ctx.popcnt - math::math_traits<decltype(left)>::pop(left);
      return true;
    }

    constexpr int32_t NUM_BLOCKS = sparse_bitmap_writer::NUM_BLOCKS;

    ++ctx.word_idx;
    for (; ctx.word_idx < NUM_BLOCKS; ++ctx.word_idx) {
      if constexpr (AT_STREAM == Access) {
        ctx.word = self->in_->read_long();
      } else {
        if constexpr (AT_DIRECT_ALIGNED == Access) {
          ctx.word = *ctx.u64data;
        } else {
          static_assert(AT_DIRECT == Access);
          std::memcpy(&ctx.word, ctx.u64data, sizeof(size_t));
        }
        ++ctx.u64data;
      }

      if (ctx.word) {
        if constexpr (AT_STREAM != Access && !is_big_endian()) {
          ctx.word = numeric_utils::numeric_traits<size_t>::ntoh(ctx.word);
        }

        const doc_id_t offset = math::math_traits<size_t>::ctz(ctx.word);

        std::get<document>(self->attrs_).value
          = self->block_ + ctx.word_idx * bits_required<size_t>() + offset;
        std::get<value_index>(self->attrs_).value = ctx.popcnt;
        ctx.popcnt += math::math_traits<size_t>::pop(ctx.word);

        return true;
      }
    }

    return false;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                            sparse_bitmap_iterator
// -----------------------------------------------------------------------------

/*static*/ bool sparse_bitmap_iterator::initial_seek(
    sparse_bitmap_iterator* self, doc_id_t target) {
  assert(!doc_limits::valid(self->value()));
  assert(0 == (target & 0xFFFF0000));

  // we can get there iff the very
  // first block is not yet read
  self->read_block_header();
  self->seek(target);
  return true;
}

sparse_bitmap_iterator::sparse_bitmap_iterator(
    memory::managed_ptr<index_input>&& in,
    const options& opts)
  : in_{std::move(in)},
    seek_func_{&sparse_bitmap_iterator::initial_seek},
    block_index_{opts.blocks},
    cont_begin_{in_->file_pointer()},
    origin_{cont_begin_},
    use_block_index_{opts.use_block_index} {
  assert(in_);
}

void sparse_bitmap_iterator::reset() {
  std::get<document>(attrs_).value = irs::doc_limits::invalid();
  seek_func_ = &sparse_bitmap_iterator::initial_seek;
  cont_begin_ = origin_;
  index_max_ = 0;
  in_->seek(cont_begin_);
}

void sparse_bitmap_iterator::read_block_header() {
  block_ = doc_id_t{uint16_t(in_->read_short())} << 16;
  const uint32_t popcnt = 1 + static_cast<uint16_t>(in_->read_short());
  index_ = index_max_;
  index_max_ += popcnt;
  if (popcnt == sparse_bitmap_writer::BLOCK_SIZE) {
    ctx_.all.missing = block_ - index_;
    cont_begin_ = in_->file_pointer();

    seek_func_ = [](sparse_bitmap_iterator* self, doc_id_t target) {
      std::get<document>(self->attrs_).value = target;
      std::get<value_index>(self->attrs_).value = target - self->ctx_.all.missing;
      return true;
    };
  } else if (popcnt <= BITSET_THRESHOLD) {
    constexpr BlockType type = BT_SPARSE;
    const size_t block_size = 2*popcnt;
    cont_begin_ = in_->file_pointer() + block_size;
    ctx_.u8data = in_->read_buffer(block_size, BufferHint::NORMAL);

    // FIXME check alignment
    seek_func_ = ctx_.u8data
      ? &container_iterator<type>::seek<AT_DIRECT>
      : &container_iterator<type>::seek<AT_STREAM>;
  } else {
    constexpr BlockType type = BT_DENSE;
    constexpr size_t block_size
      = sparse_bitmap_writer::BLOCK_SIZE / bits_required<byte_type>();

    ctx_.dense.word_idx = -1;
    ctx_.dense.popcnt = index_;
    ctx_.dense.index_base = index_;
    if (use_block_index_) {
      ctx_.dense.index.u8data = in_->read_buffer(
        DENSE_INDEX_BLOCK_SIZE_IN_BYTES,
        BufferHint::PERSISTENT);

      if (!ctx_.dense.index.u8data) {
        if (!block_index_data_) {
          block_index_data_ = memory::make_unique<byte_type[]>(
            DENSE_INDEX_BLOCK_SIZE_IN_BYTES);
        }

        ctx_.dense.index.u8data = block_index_data_.get();

        in_->read_bytes(block_index_data_.get(),
                        DENSE_INDEX_BLOCK_SIZE_IN_BYTES);
      }
    } else {
      ctx_.dense.index.u8data = nullptr;
      in_->seek(in_->file_pointer() + DENSE_INDEX_BLOCK_SIZE_IN_BYTES);
    }

    cont_begin_ = in_->file_pointer() + block_size;
    ctx_.u8data = in_->read_buffer(block_size, BufferHint::NORMAL);

    // FIXME check alignment
    seek_func_ = ctx_.u8data
      ? &container_iterator<type>::seek<AT_DIRECT>
      : &container_iterator<type>::seek<AT_STREAM>;
  }
}

void sparse_bitmap_iterator::seek_to_block(doc_id_t target) {
  assert(target / sparse_bitmap_writer::BLOCK_SIZE);

  if (block_index_.begin()) {
    assert(!block_index_.empty() && block_index_.end());

    const doc_id_t target_block = target / sparse_bitmap_writer::BLOCK_SIZE;
    if (target_block >= (block_ / sparse_bitmap_writer::BLOCK_SIZE + BLOCK_SCAN_THRESHOLD)) {
      const auto* block = block_index_.begin() + target_block;
      if (block >= block_index_.end()) {
        block = block_index_.end() - 1;
      }

      index_max_ = block->index;
      in_->seek(origin_ + block->offset);
      read_block_header();
      return;
    }
  }

  do {
    in_->seek(cont_begin_);
    read_block_header();
  } while (block_ < target);
}

doc_id_t sparse_bitmap_iterator::seek(doc_id_t target) {
  // FIXME
  if (target <= value()) {
    return value();
  }

  const doc_id_t target_block = target & 0xFFFF0000;
  if (block_ < target_block) {
    seek_to_block(target_block);
  }

  if (block_ == target_block) {
    assert(seek_func_);
    if (seek_func_(this, target)) {
      return value();
    }
    read_block_header();
  }

  assert(seek_func_);
  seek_func_(this, block_);

  return value();
}

} // iresearch
