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

#ifndef IRESEARCH_SKIP_LIST_H
#define IRESEARCH_SKIP_LIST_H

#include "store/memory_directory.hpp"
#include "utils/type_limits.hpp"
#include <functional>

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class skip_writer
/// @brief writer for storing skip-list in a directory
///
/// Example (skip_0 = skip_n = 3):
///
///                                                        c         (skip level 2)
///                    c                 c                 c         (skip level 1)
///        x     x     x     x     x     x     x     x     x     x   (skip level 0)
///  d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d (posting list)
///        3     6     9     12    15    18    21    24    27    30  (doc_count)
///
/// d - document
/// x - skip data
/// c - skip data with child pointer
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API skip_writer: util::noncopyable {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief this function will be called every skip allowing users to store
  /// arbitrary data for a given level in corresponding output stream
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<void(size_t, index_output&)> write_f;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor
  /// @param skip_0 skip interval for level 0
  /// @param skip_n skip interval for levels 1..n
  //////////////////////////////////////////////////////////////////////////////
  skip_writer(
    size_t skip_0,
    size_t skip_n
  ) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns number of elements to skip at the 0 level
  //////////////////////////////////////////////////////////////////////////////
  size_t skip_0() const noexcept { return skip_0_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns number of elements to skip at the levels from 1 to num_levels()
  //////////////////////////////////////////////////////////////////////////////
  size_t skip_n() const noexcept { return skip_n_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns number of elements in a skip-list
  //////////////////////////////////////////////////////////////////////////////
  size_t num_levels() const noexcept { return levels_.size(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepares skip_writer
  /// @param max_levels maximum number of levels in a skip-list
  /// @param count total number of elements to store in a skip-list
  /// @param write write function
  /// @param alloc memory file allocator
  //////////////////////////////////////////////////////////////////////////////
  void prepare(
    size_t max_levels,
    size_t count,
    const write_f& write = nop,
    const memory_allocator& alloc = memory_allocator::global()
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds skip at the specified number of elements
  /// @param count number of elements to skip
  //////////////////////////////////////////////////////////////////////////////
  void skip(size_t count);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flushes all internal data into the specified output stream
  /// @param out output stream
  //////////////////////////////////////////////////////////////////////////////
  void flush(index_output& out);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief resets skip reader internal state
  //////////////////////////////////////////////////////////////////////////////
  void reset() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns true if skip_writer was succesfully prepared
  //////////////////////////////////////////////////////////////////////////////
  explicit operator bool() const noexcept {
    return static_cast<bool>(write_);
  }

 private:
  static void nop(size_t, index_output&) { }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN

  std::vector<memory_output> levels_;
  size_t skip_0_; // skip interval for 0 level
  size_t skip_n_; // skip interval for 1..n levels
  write_f write_; // write function
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // skip_writer

////////////////////////////////////////////////////////////////////////////////
/// @class skip_reader
/// @brief base write for searching in skip-lists in a directory 
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API skip_reader: util::noncopyable {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief function will be called when reading of next skip 
  /// @param index of the level in a skip-list
  /// @param stream where level data resides 
  /// @returns readed key if stream is not exhausted, NO_MORE_DOCS otherwise
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<doc_id_t(size_t, index_input&)> read_f;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor
  /// @param skip_0 skip interval for level 0
  /// @param skip_n skip interval for levels 1..n
  //////////////////////////////////////////////////////////////////////////////
  skip_reader(size_t skip_0, size_t skip_n) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns number of elements to skip at the 0 level
  //////////////////////////////////////////////////////////////////////////////
  size_t skip_0() const noexcept { return skip_0_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns number of elements to skip at the levels from 1 to num_levels()
  //////////////////////////////////////////////////////////////////////////////
  size_t skip_n() const noexcept { return skip_n_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns number of elements in a skip-list
  //////////////////////////////////////////////////////////////////////////////
  size_t num_levels() const noexcept { return levels_.size(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepares skip_reader
  /// @param in source data stream
  /// @param max_levels maximum number of levels in a skip-list
  /// @param count total number of elements to store in a skip-list
  /// @param read read function
  //////////////////////////////////////////////////////////////////////////////
  void prepare(
    index_input::ptr&& in,
    const read_f& read = nop);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief seeks to the specified target
  /// @param target target to find
  /// @returns number of elements skipped
  //////////////////////////////////////////////////////////////////////////////
  size_t seek(doc_id_t target);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief resets skip reader internal state
  //////////////////////////////////////////////////////////////////////////////
  void reset();

  //////////////////////////////////////////////////////////////////////////////
  /// @returns true if skip_reader was succesfully prepared
  //////////////////////////////////////////////////////////////////////////////
  explicit operator bool() const noexcept  {
    return static_cast<bool>(read_);
  }

 private:
  struct level final : public index_input {
    level(
      index_input::ptr&& stream,
      size_t step,
      uint64_t begin,
      uint64_t end,
      uint64_t child = 0,
      size_t skipped = 0,
      doc_id_t doc = doc_limits::invalid()) noexcept;
    level(level&&) = default;
    level& operator=(level&&) = delete;

    ptr dup() const override;
    uint8_t read_byte() override;
    size_t read_bytes(byte_type* b, size_t count) override;
    const byte_type* read_buffer(size_t size, BufferHint hint) override {
      return stream->read_buffer(size, hint);
    }
    ptr reopen() const override;
    size_t file_pointer() const override;
    size_t length() const override;
    bool eof() const override;
    void seek(size_t pos) override;
    int64_t checksum(size_t offset) const override;

    index_input::ptr stream; // level data stream
    uint64_t begin; // where current level starts
    uint64_t end; // where current level ends
    uint64_t child{}; // pointer to current child level
    size_t step{}; // how many docs we jump over with a single skip
    size_t skipped{}; // number of sipped documents
    doc_id_t doc{ doc_limits::invalid() }; // current key
  };

  static_assert(std::is_nothrow_move_constructible_v<level>);

  typedef std::vector<level> levels_t;

  static void load_level(levels_t& levels, index_input::ptr&& stream, size_t step);
  static doc_id_t nop(size_t, index_input&) { return doc_limits::invalid(); }
  static void seek_skip(skip_reader::level& level, uint64_t ptr, size_t skipped);

  void read_skip(skip_reader::level& level);
  levels_t::iterator find_level(doc_id_t);

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  read_f read_;
  levels_t levels_; // input streams for skip-list levels
  size_t skip_0_; // skip interval for 0 level
  size_t skip_n_; // skip interval for 1..n levels
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // skip_reader

}

#endif
