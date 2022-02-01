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

#ifndef IRESEARCH_COLUMNSTORE2_H
#define IRESEARCH_COLUMNSTORE2_H

#include "shared.hpp"

#include "formats/formats.hpp"
#include "formats/sparse_bitmap.hpp"

#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"

#include "utils/bitpack.hpp"
#include "utils/encryption.hpp"
#include "utils/math_utils.hpp"
#include "utils/simd_utils.hpp"

namespace iresearch {
namespace columnstore2 {

////////////////////////////////////////////////////////////////////////////////
/// @class column
////////////////////////////////////////////////////////////////////////////////
class column final : public irs::column_output {
 public:
  static constexpr size_t kBlockSize = sparse_bitmap_writer::kBlockSize;
  static_assert(math::is_power2(kBlockSize));

  struct context {
    memory_allocator* alloc;
    index_output* data_out;
    encryption::stream* cipher;
    union {
      byte_type* u8buf;
      uint64_t* u64buf;
    };
    bool consolidation;
  }; // context

  struct column_block {
    uint64_t addr;
    uint64_t avg;
    uint64_t data;
    uint64_t last_size;
#ifdef IRESEARCH_DEBUG
    uint64_t size;
#endif
    uint32_t bits;
  }; // column_block

  explicit column(
      const context& ctx,
      field_id id,
      const irs::type_info& compression,
      columnstore_writer::column_finalizer_f&& finalizer,
      compression::compressor::ptr deflater)
    : ctx_{ctx},
      compression_{compression},
      deflater_{std::move(deflater)},
      finalizer_{std::move(finalizer)},
      id_{id} {
    assert(field_limits::valid(id_));
  }

  virtual void write_byte(byte_type b) override {
    data_.stream.write_byte(b);
  }

  virtual void write_bytes(const byte_type* b, size_t size) override {
    data_.stream.write_bytes(b, size);
  }

  virtual void reset() override;

 private:
  friend class writer;

  class address_table {
   public:
    uint64_t back() const noexcept {
      assert(offset_ > offsets_);
      return *(offset_-1);
    }

    void push_back(uint64_t offset) noexcept {
      assert(offset_ >= offsets_);
      assert(offset_ < offsets_ + kBlockSize);
      *offset_++ = offset;
      assert(offset >= offset_[-1]);
    }

    void pop_back() noexcept {
      assert(offset_ > offsets_);
      *--offset_ = 0;
    }

    // returns number of items to be flushed
    uint32_t size() const noexcept {
      assert(offset_ >= offsets_);
      return uint32_t(offset_ - offsets_);
    }

    bool empty() const noexcept {
      return offset_ == offsets_;
    }

    bool full() const noexcept {
      return offset_ == std::end(offsets_);
    }

    void reset() noexcept {
      std::memset(offsets_, 0, sizeof offsets_);
      offset_ = std::begin(offsets_);
    }

    uint64_t* begin() noexcept { return std::begin(offsets_); }
    uint64_t* current() noexcept { return offset_; }
    uint64_t* end() noexcept { return std::end(offsets_); }

   private:
    uint64_t offsets_[kBlockSize]{};
    uint64_t* offset_{offsets_};
  }; // address_table

  void prepare(doc_id_t key);

  bool empty() const noexcept {
    return addr_table_.empty() && !docs_count_;
  }

  void flush() {
    if (!addr_table_.empty()) {
      flush_block();
#ifdef IRESEARCH_DEBUG
      sealed_ = true;
#endif
    }
  }

  void finalize() {
    flush();

    if (finalizer_) {
      name_ = finalizer_(payload_);
    }
  }

  void finish(index_output& index_out);

  string_ref name() const noexcept { return name_; }

  void flush_block();

  context ctx_;
  irs::type_info compression_;
  compression::compressor::ptr deflater_;
  columnstore_writer::column_finalizer_f finalizer_;
  std::vector<column_block> blocks_; // at most 65536 blocks
  memory_output data_{*ctx_.alloc};
  memory_output docs_{*ctx_.alloc};
  sparse_bitmap_writer docs_writer_{docs_.stream};
  address_table addr_table_;
  bstring payload_;
  string_ref name_;
  uint64_t prev_avg_{};
  doc_id_t docs_count_{};
  doc_id_t prev_{}; // last committed doc_id_t
  doc_id_t pend_{}; // last pushed doc_id_t
  field_id id_;
  bool fixed_length_{true};
#ifdef IRESEARCH_DEBUG
  bool sealed_{false};
#endif
}; // column

////////////////////////////////////////////////////////////////////////////////
/// @class writer
////////////////////////////////////////////////////////////////////////////////
class writer final : public columnstore_writer {
 public:
  static constexpr string_ref kDataFormatName = "iresearch_11_columnstore_data";
  static constexpr string_ref kIndexFormatName = "iresearch_11_columnstore_index";
  static constexpr string_ref kDataFormatExt = "csd";
  static constexpr string_ref kIndexFormatExt = "csi";

  explicit writer(bool consolidation);

  virtual void prepare(directory& dir, const segment_meta& meta) override;
  virtual column_t push_column(
      const column_info& info,
      column_finalizer_f finalizer) override;
  virtual bool commit(const flush_state& state) override;
  virtual void rollback() noexcept override;

 private:
  directory* dir_;
  std::string data_filename_;
  memory_allocator* alloc_;
  std::deque<column> columns_; // pointers remain valid
  std::vector<column*> sorted_columns_;
  index_output::ptr data_out_;
  encryption::stream::ptr data_cipher_;
  std::unique_ptr<byte_type[]> buf_;
  bool consolidation_;
}; // writer

////////////////////////////////////////////////////////////////////////////////
/// @enum ColumnType
////////////////////////////////////////////////////////////////////////////////
enum class ColumnType : uint16_t {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief variable length data
  //////////////////////////////////////////////////////////////////////////////
  kSparse = 0,

  //////////////////////////////////////////////////////////////////////////////
  /// @brief no data
  //////////////////////////////////////////////////////////////////////////////
  kMask,

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fixed length data
  //////////////////////////////////////////////////////////////////////////////
  kFixed,

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fixed length data in adjacent blocks
  //////////////////////////////////////////////////////////////////////////////
  kDenseFixed
}; // ColumnType

////////////////////////////////////////////////////////////////////////////////
/// @enum ColumnProperty
////////////////////////////////////////////////////////////////////////////////
enum class ColumnProperty : uint16_t {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Regular column
  //////////////////////////////////////////////////////////////////////////////
  kNormal = 0,

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Encrytped data
  //////////////////////////////////////////////////////////////////////////////
  kEncrypt = 1,

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Annonymous column
  //////////////////////////////////////////////////////////////////////////////
  kNoName = 2
}; // ColumnProperty

ENABLE_BITMASK_ENUM(ColumnProperty);

////////////////////////////////////////////////////////////////////////////////
/// @struct column_header
////////////////////////////////////////////////////////////////////////////////
struct column_header {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief bitmap index offset, 0 if not present
  /// @note 0 - not present, meaning dense column
  //////////////////////////////////////////////////////////////////////////////
  uint64_t docs_index{};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief column identifier
  //////////////////////////////////////////////////////////////////////////////
  field_id id{field_limits::invalid()};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief total number of docs in a column
  //////////////////////////////////////////////////////////////////////////////
  doc_id_t docs_count{};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief min document identifier
  //////////////////////////////////////////////////////////////////////////////
  doc_id_t min{doc_limits::invalid()};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief column properties
  //////////////////////////////////////////////////////////////////////////////
  ColumnProperty props{ColumnProperty::kNormal};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief column type
  //////////////////////////////////////////////////////////////////////////////
  ColumnType type{ColumnType::kSparse};
}; // column_header

////////////////////////////////////////////////////////////////////////////////
/// @class reader
////////////////////////////////////////////////////////////////////////////////
class reader final : public columnstore_reader {
 public:
  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta) override;

  const column_header* header(field_id field) const;

  virtual const column_reader* column(field_id field) const override {
    return field >= columns_.size()
      ? nullptr // can't find column with the specified identifier
      : columns_[field];
  }

  virtual bool visit(const column_visitor_f& visitor) const override;

  virtual size_t size() const override {
    return columns_.size();
  }

 private:
  using column_ptr = std::unique_ptr<column_reader>;

  void prepare_data(
    const directory& dir,
    const std::string& filename);

  void prepare_index(
    const directory& dir,
    const segment_meta& meta,
    const std::string& filename);

  std::vector<column_ptr> sorted_columns_;
  std::vector<const column_ptr::element_type*> columns_;
  encryption::stream::ptr data_cipher_;
  index_input::ptr data_in_;
}; // reader

enum class Version : int32_t {
  kMin = 0,
  kMax = kMin
};

IRESEARCH_API irs::columnstore_writer::ptr make_writer(Version version, bool consolidation);
IRESEARCH_API irs::columnstore_reader::ptr make_reader();

} // columnstore2
} // iresearch

#endif // IRESEARCH_COLUMNSTORE2_H
