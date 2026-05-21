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

// We use dense container for blocks having
// more than this number of documents.
constexpr uint32_t kBitSetThreshold = (1 << 12) - 1;

// We don't use index for blocks located at a distance that is
// closer than this value.
constexpr uint32_t kBlockScanThreshold = 2;

enum BlockType : uint32_t {
  // Dense block is represented as a bitset container
  BT_DENSE = 0,

  // Sparse block is represented as an array of values
  BT_SPARSE,

  // Range block is represented as a [Min,Max] pair
  BT_RANGE
};

enum AccessType : uint32_t {
  // Access data via stream API
  AT_STREAM,

  // Direct memory access
  AT_DIRECT,

  // Aligned direct memory access
  AT_DIRECT_ALIGNED
};

constexpr size_t kDenseBlockIndexBlockSize = 512;
constexpr size_t kDenseBlockIndexNumBlocks =
  sparse_bitmap_writer::kBlockSize / kDenseBlockIndexBlockSize;
constexpr size_t kDenseIndexBlockSizeInBytes =
  kDenseBlockIndexNumBlocks * sizeof(uint16_t);
constexpr uint32_t kDenseBlockIndexWordsPerBlock =
  kDenseBlockIndexBlockSize / bits_required<size_t>();

template<size_t N>
void write_block_index(irs::index_output& out, size_t (&bits)[N]) {
  uint16_t popcnt = 0;
  uint16_t index[kDenseBlockIndexNumBlocks];
  static_assert(kDenseIndexBlockSizeInBytes == sizeof index);

  auto* block = reinterpret_cast<byte_type*>(std::begin(index));
  auto begin = std::begin(bits);
  for (; begin != std::end(bits); begin += kDenseBlockIndexWordsPerBlock) {
    irs::write<uint16_t>(block, popcnt);

    for (uint32_t i = 0; i < kDenseBlockIndexWordsPerBlock; ++i) {
      popcnt += std::popcount(begin[i]);
    }
  }

  out.write_bytes(reinterpret_cast<const byte_type*>(std::begin(index)),
                  sizeof index);
}

}  // namespace

namespace irs {

void sparse_bitmap_writer::finish() {
  flush(block_);

  if (block_index_.size() < kBlockSize) {
    add_block(block_ ? block_ + 1 : 0);
  }

  // create a sentinel block to issue doc_limits::eof() automatically
  block_ = doc_limits::eof() / kBlockSize;
  set(doc_limits::eof() % kBlockSize);
  do_flush(1);
}

void sparse_bitmap_writer::do_flush(uint32_t popcnt) {
  IRS_ASSERT(popcnt);
  IRS_ASSERT(block_ < kBlockSize);
  IRS_ASSERT(popcnt <= kBlockSize);

  out_->write_short(static_cast<uint16_t>(block_));
  out_->write_short(static_cast<uint16_t>(popcnt - 1));  // -1 to fit uint16_t

  if (opts_.track_prev_doc) {
    // write last value in the previous block
    out_->write_int(last_in_flushed_block_);
    last_in_flushed_block_ = prev_value_;
  }

  if (popcnt > kBitSetThreshold) {
    if (popcnt != kBlockSize) {
      write_block_index(*out_, bits_);

      if constexpr (!is_big_endian()) {
        std::for_each(std::begin(bits_), std::end(bits_), [](auto& v) {
          v = numeric_utils::numeric_traits<size_t>::hton(v);
        });
      }

      out_->write_bytes(reinterpret_cast<const byte_type*>(bits_),
                        sizeof bits_);
    }
  } else {
    bitset_doc_iterator it(std::begin(bits_), std::end(bits_));

    while (it.next()) {
      out_->write_short(static_cast<uint16_t>(it.value()));
    }
  }
}

template<uint32_t Type, bool TrackPrev>
struct container_iterator;

template<bool TrackPrev>
struct container_iterator<BT_RANGE, TrackPrev> {
  static bool seek(sparse_bitmap_iterator* self, doc_id_t target) noexcept {
    std::get<document>(self->attrs_).value = target;

    auto& index = std::get<value_index>(self->attrs_).value;
    index = target - self->ctx_.all.missing;

    if constexpr (TrackPrev) {
      if (index != self->index_) {
        self->prev_ = target - 1;
      }
    }

    return true;
  }
};

template<bool TrackPrev>
struct container_iterator<BT_SPARSE, TrackPrev> {
  template<AccessType Access>
  static bool seek(sparse_bitmap_iterator* self, doc_id_t target) {
    target &= 0x0000FFFF;

    auto& ctx = self->ctx_.sparse;
    const doc_id_t index_max = self->index_max_;

    [[maybe_unused]] doc_id_t prev = std::get<document>(self->attrs_).value;

    for (; ctx.index < index_max; ++ctx.index) {
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
        std::get<value_index>(self->attrs_).value = ctx.index;

        if constexpr (TrackPrev) {
          if (ctx.index != self->index_) {
            self->prev_ = self->block_ | prev;
          }
        }

        ++ctx.index;

        return true;
      }

      if constexpr (TrackPrev) {
        prev = doc;
      }
    }

    return false;
  }
};

template<>
struct container_iterator<BT_DENSE, false> {
  template<AccessType Access>
  static bool seek(sparse_bitmap_iterator* self, doc_id_t target) {
    auto& ctx = self->ctx_.dense;

    const doc_id_t target_block{target & 0x0000FFFF};
    const int32_t target_word_idx = target_block / bits_required<size_t>();
    IRS_ASSERT(target_word_idx >= ctx.word_idx);

    if (ctx.index.u16data && uint32_t(target_word_idx - ctx.word_idx) >=
                               kDenseBlockIndexWordsPerBlock) {
      const size_t index_block = target_block / kDenseBlockIndexBlockSize;

      uint16_t popcnt;
      std::memcpy(&popcnt, &ctx.index.u16data[index_block], sizeof popcnt);
      if constexpr (!is_big_endian()) {
        popcnt = (popcnt >> 8) | ((popcnt & 0xFF) << 8);
      }

      const auto word_idx = index_block * kDenseBlockIndexWordsPerBlock;
      const auto delta = word_idx - ctx.word_idx;
      IRS_ASSERT(delta > 0);

      if constexpr (AT_STREAM == Access) {
        self->in_->seek(self->in_->file_pointer() +
                        (delta - 1) * sizeof(size_t));
        ctx.word = self->in_->read_long();
      } else {
        ctx.u64data += delta;
        if constexpr (AT_DIRECT_ALIGNED == Access) {
          ctx.word = ctx.u64data[-1];
        } else {
          static_assert(AT_DIRECT == Access);
          std::memcpy(&ctx.word, &ctx.u64data[-1], sizeof ctx.word);
        }
        if constexpr (!is_big_endian()) {
          ctx.word = numeric_utils::numeric_traits<size_t>::ntoh(ctx.word);
        }
      }
      ctx.popcnt = self->index_ + popcnt + std::popcount(ctx.word);
      ctx.word_idx = static_cast<int32_t>(word_idx);
    }

    uint32_t word_delta = target_word_idx - ctx.word_idx;

    if constexpr (AT_STREAM == Access) {
      for (; word_delta; --word_delta) {
        ctx.word = self->in_->read_long();
        ctx.popcnt += std::popcount(ctx.word);
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
            std::memcpy(&ctx.word, ctx.u64data, sizeof ctx.word);
          }
          ctx.popcnt += std::popcount(ctx.word);
        }
        if constexpr (!is_big_endian()) {
          ctx.word = numeric_utils::numeric_traits<size_t>::ntoh(ctx.word);
        }
        ctx.word_idx = target_word_idx;
      }
    }

    const size_t left = ctx.word >> (target % bits_required<size_t>());

    if (left) {
      const doc_id_t offset = std::countr_zero(left);
      std::get<document>(self->attrs_).value = target + offset;

      auto& index = std::get<value_index>(self->attrs_).value;
      index = ctx.popcnt - std::popcount(left);

      return true;
    }

    constexpr int32_t kNumBlocks = sparse_bitmap_writer::kNumBlocks;

    ++ctx.word_idx;
    for (; ctx.word_idx < kNumBlocks; ++ctx.word_idx) {
      if constexpr (AT_STREAM == Access) {
        ctx.word = self->in_->read_long();
      } else {
        if constexpr (AT_DIRECT_ALIGNED == Access) {
          ctx.word = *ctx.u64data;
        } else {
          static_assert(AT_DIRECT == Access);
          std::memcpy(&ctx.word, ctx.u64data, sizeof ctx.word);
        }
        ++ctx.u64data;
      }

      if (ctx.word) {
        if constexpr (AT_STREAM != Access && !is_big_endian()) {
          ctx.word = numeric_utils::numeric_traits<size_t>::ntoh(ctx.word);
        }

        const doc_id_t offset = std::countr_zero(ctx.word);

        std::get<document>(self->attrs_).value =
          self->block_ + ctx.word_idx * bits_required<size_t>() + offset;
        auto& index = std::get<value_index>(self->attrs_).value;
        index = ctx.popcnt;
        ctx.popcnt += std::popcount(ctx.word);

        return true;
      }
    }

    return false;
  }
};

template<>
struct container_iterator<BT_DENSE, true> {
  template<AccessType Access>
  static bool seek(sparse_bitmap_iterator* self, doc_id_t target) {
    const auto res =
      container_iterator<BT_DENSE, false>::seek<Access>(self, target);

    if (std::get<value_index>(self->attrs_).value != self->index_) {
      self->seek_func_ = &container_iterator<BT_DENSE, false>::seek<Access>;
      std::get<irs::prev_doc>(self->attrs_).reset(&seek_prev<Access>, self);
    }

    return res;
  }

 private:
  template<AccessType Access>
  static doc_id_t seek_prev(const void* arg) noexcept(Access != AT_STREAM) {
    const auto* self = static_cast<const sparse_bitmap_iterator*>(arg);
    const auto& ctx = self->ctx_.dense;

    const size_t offs = self->value() % bits_required<size_t>();
    const size_t mask = (size_t{1} << offs) - 1;
    size_t word = ctx.word & mask;
    size_t word_idx = ctx.word_idx;

    if (!word) {
      // FIXME(gnusi): we can use block
      // index to perform huge skips

      if constexpr (AT_STREAM == Access) {
        auto& in = *self->in_;
        // original position
        const auto pos = in.file_pointer();

        auto prev = pos - 2 * sizeof(size_t);
        do {
          in.seek(prev);
          word = in.read_long();
          IRS_ASSERT(word_idx);
          --word_idx;
          prev -= sizeof(size_t);
        } while (!word);
        self->in_->seek(pos);
      } else {
        for (auto* prev_word = ctx.u64data - 1; !word; --word_idx) {
          --prev_word;
          if constexpr (AT_DIRECT_ALIGNED == Access) {
            word = *prev_word;
          } else {
            static_assert(AT_DIRECT == Access);
            std::memcpy(&word, prev_word, sizeof word);
          }
          IRS_ASSERT(word_idx);
        }
        if constexpr (!is_big_endian()) {
          word = numeric_utils::numeric_traits<size_t>::ntoh(word);
        }
      }
    }

    return self->block_ +
           static_cast<doc_id_t>((word_idx + 1) * bits_required<size_t>() -
                                 std::countl_zero(word) - 1);
  }
};

template<BlockType Type>
constexpr auto GetSeekFunc(bool direct, bool track_prev) noexcept {
  auto impl = [&]<AccessType A>(bool track_prev) noexcept {
    if (track_prev) {
      return &container_iterator<Type, true>::template seek<A>;
    } else {
      return &container_iterator<Type, false>::template seek<A>;
    }
  };

  // FIXME(gnusi): check alignment
  if (direct) {
    return impl.template operator()<AT_DIRECT>(track_prev);
  } else {
    return impl.template operator()<AT_STREAM>(track_prev);
  }
}

bool sparse_bitmap_iterator::initial_seek(sparse_bitmap_iterator* self,
                                          doc_id_t target) {
  IRS_ASSERT(!doc_limits::valid(self->value()));
  IRS_ASSERT(0 == (target & 0xFFFF0000));

  // we can get there iff the very
  // first block is not yet read
  self->read_block_header();
  self->seek(target);
  return true;
}

// cppcheck-suppress uninitMemberVarPrivate
sparse_bitmap_iterator::sparse_bitmap_iterator(Ptr&& in, const options& opts)
  : in_{std::move(in)},
    seek_func_{&sparse_bitmap_iterator::initial_seek},
    block_index_{opts.blocks},
    cont_begin_{in_->file_pointer()},
    origin_{cont_begin_},
    use_block_index_{opts.use_block_index},
    prev_doc_written_{opts.version >= SparseBitmapVersion::kPrevDoc},
    track_prev_doc_{prev_doc_written_ && opts.track_prev_doc} {
  IRS_ASSERT(in_);

  if (track_prev_doc_) {
    std::get<prev_doc>(attrs_).reset(
      [](const void* ctx) noexcept {
        return *static_cast<const doc_id_t*>(ctx);
      },
      &prev_);
  }
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
  if (prev_doc_written_) {
    prev_ = in_->read_int();  // last doc in a previously filled block
  }

  if (track_prev_doc_) {
    std::get<prev_doc>(attrs_).reset(
      [](const void* ctx) noexcept {
        return *static_cast<const doc_id_t*>(ctx);
      },
      &prev_);
  }

  if (popcnt == sparse_bitmap_writer::kBlockSize) {
    ctx_.all.missing = block_ - index_;
    cont_begin_ = in_->file_pointer();

    seek_func_ = track_prev_doc_ ? &container_iterator<BT_RANGE, true>::seek
                                 : &container_iterator<BT_RANGE, false>::seek;
  } else if (popcnt <= kBitSetThreshold) {
    const size_t block_size = 2 * popcnt;
    cont_begin_ = in_->file_pointer() + block_size;
    ctx_.u8data = in_->read_buffer(block_size, BufferHint::NORMAL);
    ctx_.sparse.index = index_;

    seek_func_ = GetSeekFunc<BT_SPARSE>(ctx_.u8data, track_prev_doc_);
  } else {
    constexpr size_t block_size =
      sparse_bitmap_writer::kBlockSize / bits_required<byte_type>();

    ctx_.dense.word_idx = -1;
    ctx_.dense.popcnt = index_;
    if (use_block_index_) {
      ctx_.dense.index.u8data =
        in_->read_buffer(kDenseIndexBlockSizeInBytes, BufferHint::PERSISTENT);

      if (!ctx_.dense.index.u8data) {
        if (!block_index_data_) {
          block_index_data_ =
            std::make_unique<byte_type[]>(kDenseIndexBlockSizeInBytes);
        }

        ctx_.dense.index.u8data = block_index_data_.get();

        in_->read_bytes(block_index_data_.get(), kDenseIndexBlockSizeInBytes);
      }
    } else {
      ctx_.dense.index.u8data = nullptr;
      in_->seek(in_->file_pointer() + kDenseIndexBlockSizeInBytes);
    }

    cont_begin_ = in_->file_pointer() + block_size;
    ctx_.u8data = in_->read_buffer(block_size, BufferHint::NORMAL);

    seek_func_ = GetSeekFunc<BT_DENSE>(ctx_.u8data, track_prev_doc_);
  }
}

void sparse_bitmap_iterator::seek_to_block(doc_id_t target) {
  IRS_ASSERT(target / sparse_bitmap_writer::kBlockSize);

  if (!block_index_.empty()) {
    const doc_id_t target_block = target / sparse_bitmap_writer::kBlockSize;
    if (target_block >=
        (block_ / sparse_bitmap_writer::kBlockSize + kBlockScanThreshold)) {
      const auto offset =
        std::min(size_t{target_block}, block_index_.size() - 1);
      const auto& block = block_index_[offset];

      index_max_ = block.index;
      in_->seek(origin_ + block.offset);
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
    IRS_ASSERT(seek_func_);
    if (seek_func_(this, target)) {
      return value();
    }
    read_block_header();
  }

  IRS_ASSERT(seek_func_);
  seek_func_(this, block_);

  return value();
}

}  // namespace irs
