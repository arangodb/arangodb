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
#include "formats/formats.hpp"
#include "formats/sparse_bitmap.hpp"
#include "resource_manager.hpp"
#include "shared.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/encryption.hpp"
#include "utils/math_utils.hpp"

namespace irs {
namespace columnstore2 {

enum class Version : int32_t { kMin = 0, kMax = kMin };

class column final : public irs::column_output {
 public:
  static constexpr uint32_t kBlockSize = sparse_bitmap_writer::kBlockSize;
  static_assert(math::is_power2(kBlockSize));

  struct context {
    index_output* data_out;
    encryption::stream* cipher;
    union {
      byte_type* u8buf;
      uint64_t* u64buf;
    };
    bool consolidation;
    SparseBitmapVersion version;
  };

  struct column_block {
    uint64_t addr;
    uint64_t avg;
    uint64_t data;
    uint64_t last_size;
#ifdef IRESEARCH_DEBUG
    uint64_t size;
#endif
    uint32_t bits;
  };

  explicit column(const context& ctx, field_id id,
                  const irs::type_info& compression,
                  columnstore_writer::column_finalizer_f&& finalizer,
                  compression::compressor::ptr deflater,
                  IResourceManager& resource_manager);

  void write_byte(byte_type b) final { data_.stream.write_byte(b); }

  void write_bytes(const byte_type* b, size_t size) final {
    data_.stream.write_bytes(b, size);
  }

  void reset() final;

 private:
  friend class writer;

public:
  class address_table {
   public:
    address_table(ManagedTypedAllocator<uint64_t> alloc) : offsets_(alloc) { }

    ~address_table() = default;

    bool back(uint64_t& backValue) const noexcept {
      IRS_ASSERT(!offsets_.empty() && current_pos_ > 0);
      if (offsets_.empty() || 0 == current_pos_)
        return false;

      backValue = offsets_[current_pos_ - 1];
      return true;
    }

    //  TODO:
    //  The present users of the push/pop functions
    //  assume that these functions will always succeed,
    //  so they don't check for failures.
    bool push_back(uint64_t offset) noexcept {
      IRS_ASSERT(current_pos_ < column::kBlockSize);

      if (!(current_pos_ < column::kBlockSize))
        return false;

      if (offsets_.size() <= static_cast<decltype(offsets_)::size_type>(current_pos_))
        offsets_.resize(current_pos_ * 2 + 1);

      offsets_[current_pos_++] = offset;
      return true;
    }

    bool pop_back() {
      IRS_ASSERT(!offsets_.empty() && current_pos_ > 0);
      if (offsets_.empty() || 0 == current_pos_)
        return false;

      --current_pos_;
      return true;
    }

    uint32_t size() const noexcept {
      return current_pos_;
    }

    //  Allocates more space if required.
    //  Doesn't shrink if max_elem < size.
    bool grow_size(uint32_t max_elems) {
      IRS_ASSERT(max_elems <= column::kBlockSize);
      if (!(max_elems <= column::kBlockSize))
        return false;

      if (max_elems > static_cast<uint32_t>(offsets_.size()))
        offsets_.resize(max_elems);

      return true;
    }

    bool empty() const noexcept { return 0 == current_pos_; }

    bool full() const noexcept { return current_pos_ == column::kBlockSize; }

    void reset() noexcept {
      offsets_.clear();
      current_pos_ = 0;
    }

    uint64_t* begin() noexcept { return &offsets_[0]; }

    //  current() points to the next location in the table
    //  where new data will be added.
    //  Call grow_size() to allocate space before using current()
    uint64_t* current() noexcept { return &offsets_[current_pos_]; }

   private:
    std::vector<uint64_t, ManagedTypedAllocator<uint64_t>> offsets_;
    uint32_t current_pos_ { 0 };
  };

private:
  void Prepare(doc_id_t key) final;

  bool empty() const noexcept { return addr_table_.empty() && !docs_count_; }

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

  std::string_view name() const noexcept { return name_; }

  void flush_block();

  context ctx_;
  irs::type_info compression_;
  compression::compressor::ptr deflater_;
  columnstore_writer::column_finalizer_f finalizer_;
  ManagedVector<column_block> blocks_;  // at most 65536 blocks
  memory_output data_;
  memory_output docs_;
  sparse_bitmap_writer docs_writer_{docs_.stream, ctx_.version};
  address_table addr_table_;
  bstring payload_;
  std::string_view name_;
  uint64_t prev_avg_{};
  doc_id_t docs_count_{};
  doc_id_t prev_{};  // last committed doc_id_t
  doc_id_t pend_{};  // last pushed doc_id_t
  field_id id_;
  bool fixed_length_{true};
#ifdef IRESEARCH_DEBUG
  bool sealed_{false};
#endif
};

class writer final : public columnstore_writer {
 public:
  static constexpr std::string_view kDataFormatName =
    "iresearch_11_columnstore_data";
  static constexpr std::string_view kIndexFormatName =
    "iresearch_11_columnstore_index";
  static constexpr std::string_view kDataFormatExt = "csd";
  static constexpr std::string_view kIndexFormatExt = "csi";

  writer(Version version, IResourceManager& resource_manager,
         bool consolidation);
  ~writer() override;

  void prepare(directory& dir, const SegmentMeta& meta) final;
  column_t push_column(const ColumnInfo& info,
                       column_finalizer_f finalizer) final;
  bool commit(const flush_state& state) final;
  void rollback() noexcept final;

 private:
  directory* dir_;
  std::string data_filename_;
  std::deque<column, ManagedTypedAllocator<column>>
    columns_;  // pointers remain valid
  std::vector<column*> sorted_columns_;
  index_output::ptr data_out_;
  encryption::stream::ptr data_cipher_;
  byte_type* buf_;
  Version ver_;
  bool consolidation_;
};

enum class ColumnType : uint16_t {
  // Variable length data
  kSparse = 0,

  // No data
  kMask,

  // Fixed length data
  kFixed,

  // Fixed length data in adjacent blocks
  kDenseFixed
};

enum class ColumnProperty : uint16_t {
  // Regular column
  kNormal = 0,

  // Encrytped data
  kEncrypt = 1,

  // Annonymous column
  kNoName = 2,

  // Support accessing previous document
  kPrevDoc = 4
};

ENABLE_BITMASK_ENUM(ColumnProperty);

struct column_header {
  // Bitmap index offset, 0 if not present.
  // 0 - not present, meaning dense column
  uint64_t docs_index{};

  // Column identifier
  field_id id{field_limits::invalid()};

  // Total number of docs in a column
  doc_id_t docs_count{};

  // Min document identifier
  doc_id_t min{doc_limits::invalid()};

  // Column properties
  ColumnProperty props{ColumnProperty::kNormal};

  // Column type
  ColumnType type{ColumnType::kSparse};
};

class reader final : public columnstore_reader {
 public:
  uint64_t CountMappedMemory() const final {
    return data_in_ != nullptr ? data_in_->CountMappedMemory() : 0;
  }

  bool prepare(const directory& dir, const SegmentMeta& meta,
               const options& opts = options{}) final;

  const column_header* header(field_id field) const;

  const column_reader* column(field_id field) const final {
    return field >= columns_.size()
             ? nullptr  // can't find column with the specified identifier
             : columns_[field];
  }

  bool visit(const column_visitor_f& visitor) const final;

  size_t size() const final { return columns_.size(); }

 private:
  using column_ptr = memory::managed_ptr<column_reader>;

  void prepare_data(const directory& dir, std::string_view filename);

  void prepare_index(const directory& dir, const SegmentMeta& meta,
                     std::string_view filename, std::string_view data_filename,
                     const options& opts);

  std::vector<column_ptr> sorted_columns_;
  std::vector<const column_ptr::element_type*> columns_;
  encryption::stream::ptr data_cipher_;
  index_input::ptr data_in_;
};

irs::columnstore_writer::ptr make_writer(Version version, bool consolidation,
                                         IResourceManager& rm);
irs::columnstore_reader::ptr make_reader();

}  // namespace columnstore2
}  // namespace irs
