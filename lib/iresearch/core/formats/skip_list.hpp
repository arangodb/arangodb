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

#pragma once

#include "store/memory_directory.hpp"
#include "utils/type_limits.hpp"

namespace irs {
// Writer for storing skip-list in a directory
// Example (skip_0 = skip_n = 3):
// clang-format off
//                                                        c         (skip level 2)
//                    c                 c                 c         (skip level 1)
//        x     x     x     x     x     x     x     x     x     x   (skip level 0)
//  d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d (posting list)
//        3     6     9     12    15    18    21    24    27    30  (doc_count)
// clang-format on
// d - document
// x - skip data
// c - skip data with child pointer
class SkipWriter : util::noncopyable {
 public:
  // skip_0: skip interval for level 0
  // skip_n: skip interval for levels 1..n
  SkipWriter(doc_id_t skip_0, doc_id_t skip_n, IResourceManager& rm) noexcept
    : levels_{ManagedTypedAllocator<memory_output>{rm}},
      max_levels_{0},
      skip_0_{skip_0},
      skip_n_{skip_n} {
    IRS_ASSERT(skip_0_);
  }

  // Return number of elements to skip at the 0 level
  doc_id_t Skip0() const noexcept { return skip_0_; }

  // Return number of elements to skip at the levels from 1 to max_levels()
  doc_id_t SkipN() const noexcept { return skip_n_; }

  // Returns number of elements in a skip-list
  size_t MaxLevels() const noexcept { return max_levels_; }

  // Prepares skip_writer capable of writing up to `max_levels` skip levels and
  // `count` elements.
  void Prepare(size_t max_levels, size_t count);

  // Flushes all internal data into the specified output stream
  uint32_t CountLevels() const;
  void FlushLevels(uint32_t num_levels, index_output& out);
  void Flush(index_output& out) { FlushLevels(CountLevels(), out); }

  // Resets skip writer internal state
  void Reset() noexcept {
    for (auto& level : levels_) {
      level.stream.reset();
    }
  }

  // Adds skip at the specified number of elements.
  // `Write` is a functional object is called for every skip allowing users to
  // store arbitrary data for a given level in corresponding output stream
  template<typename Writer>
  void Skip(doc_id_t count, Writer&& write);

 protected:
  ManagedVector<memory_output> levels_;
  size_t max_levels_;
  doc_id_t skip_0_;  // skip interval for 0 level
  doc_id_t skip_n_;  // skip interval for 1..n levels
};

template<typename Writer>
void SkipWriter::Skip(doc_id_t count, Writer&& write) {
  if (0 == (count % skip_0_)) {
    IRS_ASSERT(!levels_.empty());

    uint64_t child = 0;

    // write 0 level
    {
      auto& stream = levels_.front().stream;
      write(0, stream);
      count /= skip_0_;
      child = stream.file_pointer();
    }

    // write levels from 1 to n
    for (size_t i = 1; 0 == count % skip_n_ && i < max_levels_;
         ++i, count /= skip_n_) {
      auto& stream = levels_[i].stream;
      write(i, stream);

      uint64_t next_child = stream.file_pointer();
      stream.write_vlong(child);
      child = next_child;
    }
  }
}

// Base object for searching in skip-lists
class SkipReaderBase : util::noncopyable {
 public:
  // Returns number of elements to skip at the 0 level
  doc_id_t Skip0() const noexcept { return skip_0_; }

  // Return number of elements to skip at the levels from 1 to num_levels()
  doc_id_t SkipN() const noexcept { return skip_n_; }

  // Return number of elements in a skip-list
  size_t NumLevels() const noexcept { return std::size(levels_); }

  // Prepare skip_reader using a specified data stream
  void Prepare(index_input::ptr&& in, doc_id_t left);

  // Reset skip reader internal state
  void Reset();

 protected:
  static constexpr size_t kUndefined = std::numeric_limits<size_t>::max();

  struct Level final {
    Level(index_input::ptr&& stream, doc_id_t step, doc_id_t left,
          uint64_t begin) noexcept;
    Level(Level&&) = default;
    Level& operator=(Level&&) = delete;

    // Level data stream.
    index_input::ptr stream;
    // Where level starts.
    uint64_t begin;
    // Pointer to child level.
    uint64_t child{};
    // Number of documents left at a level.
    // int64_t to be able to go below 0.
    int64_t left;
    // How many docs we jump over with a single skip
    const doc_id_t step;
  };

  static_assert(std::is_nothrow_move_constructible_v<Level>);

  static void SeekToChild(Level& lvl, uint64_t ptr, const Level& prev) {
    IRS_ASSERT(lvl.stream);
    auto& stream = *lvl.stream;
    const auto absolute_ptr = lvl.begin + ptr;
    if (absolute_ptr > stream.file_pointer()) {
      stream.seek(absolute_ptr);
      lvl.left = prev.left + prev.step;
      if (lvl.child != kUndefined) {
        lvl.child = stream.read_vlong();
      }
    }
  }

  SkipReaderBase(doc_id_t skip_0, doc_id_t skip_n) noexcept
    : skip_0_{skip_0}, skip_n_{skip_n} {}

  std::vector<Level> levels_;  // input streams for skip-list levels
  const doc_id_t skip_0_;      // skip interval for 0 level
  const doc_id_t skip_n_;      // skip interval for 1..n levels
  doc_id_t docs_count_{};
};

// The reader for searching in skip-lists written by `SkipWriter`.
// `Read` is a function object is called when reading of next skip. Accepts
// the following parameters: index of the level in a skip-list, where a data
// stream ends, stream where level data resides and  readed key if stream is
// not exhausted, doc_limits::eof() otherwise
template<typename ReaderType>
class SkipReader final : public SkipReaderBase {
 public:
  // skip_0: skip interval for level 0
  // skip_n: skip interval for levels 1..n
  template<typename T>
  SkipReader(doc_id_t skip_0, doc_id_t skip_n, T&& reader)
    : SkipReaderBase{skip_0, skip_n}, reader_{std::forward<T>(reader)} {}

  // Seeks to the specified target.
  // Returns Number of elements skipped from upper bound
  // to the next lower bound.
  doc_id_t Seek(doc_id_t target);

  ReaderType& Reader() noexcept { return reader_; }

 private:
  ReaderType reader_;
};

template<typename Read>
doc_id_t SkipReader<Read>::Seek(doc_id_t target) {
  IRS_ASSERT(!levels_.empty());
  const size_t size = std::size(levels_);
  size_t id = 0;

  // Returns the highest level with the value not less than a target
  for (; id != size; ++id) {
    if (reader_.IsLess(id, target)) {
      break;
    }
  }

  while (id != size) {
    id = reader_.AdjustLevel(id);
    auto& level = levels_[id];
    auto& stream = *level.stream;
    uint64_t child_ptr = level.child;
    const bool is_leaf = (child_ptr == kUndefined);

    if (const auto left = (level.left -= level.step); IRS_LIKELY(left > 0)) {
      reader_.Read(id, stream);
      if (!is_leaf) {
        level.child = stream.read_vlong();
      }
      if (reader_.IsLess(id, target)) {
        continue;
      }
    } else {
      reader_.Seal(id);
    }
    ++id;
    if (!is_leaf) {
      IRS_ASSERT(id < size);
      SeekToChild(levels_[id], child_ptr, level);
      reader_.MoveDown(id);
    }
  }

  auto& level_0 = levels_.back();
  return static_cast<doc_id_t>(level_0.left + level_0.step);
}

}  // namespace irs
