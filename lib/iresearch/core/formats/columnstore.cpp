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

#include "columnstore.hpp"

#include <tuple>

#include "analysis/token_attributes.hpp"
#include "formats/format_utils.hpp"
#include "formats/formats.hpp"
#include "index/file_names.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "store/memory_directory.hpp"
#include "store/store_avg_utils.hpp"
#include "store/store_utils.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitpack.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/encryption.hpp"
#include "utils/hash_set_utils.hpp"
#include "utils/iterator.hpp"
#include "utils/log.hpp"
#include "utils/lz4compression.hpp"
#include "utils/object_pool.hpp"
#include "utils/type_limits.hpp"

// ----------------------------------------------------------------------------
// --SECTION--                                               columnstore format
// ----------------------------------------------------------------------------
// |Header|
// |Compressed block #0|
// |Compressed block #1|
// |Compressed block #2|
// |Compressed block #1| <-- Columnstore data blocks
// |Compressed block #1|
// |Compressed block #1|
// |Compressed block #2|
// ...
// |Last block #0 key|Block #0 offset|
// |Last block #1 key|Block #1 offset| <-- Columnstore block index
// |Last block #2 key|Block #2 offset|
// ...
// |Footer|
// ----------------------------------------------------------------------------

namespace {

using namespace irs;
using columnstore::ColumnMetaVersion;

irs::bytes_view kDummy;  // placeholder for visiting logic in columnstore

//////////////////////////////////////////////////////////////////////////////
/// @struct column_meta
/// @brief represents column metadata
//////////////////////////////////////////////////////////////////////////////
struct column_meta {
 public:
  column_meta() = default;

  column_meta(column_meta&&) = delete;
  column_meta(const column_meta&) = delete;
  column_meta& operator=(column_meta&&) = delete;
  column_meta& operator=(const column_meta&) = delete;

  std::string name;
  field_id id{field_limits::invalid()};
};

struct format_traits {
  IRS_FORCE_INLINE static void pack32(const uint32_t* IRS_RESTRICT decoded,
                                      uint32_t* IRS_RESTRICT encoded,
                                      size_t size, uint32_t bits) noexcept {
    IRS_ASSERT(encoded);
    IRS_ASSERT(decoded);
    IRS_ASSERT(size);
    irs::packed::pack(decoded, decoded + size, encoded, bits);
  }

  IRS_FORCE_INLINE static void pack64(const uint64_t* IRS_RESTRICT decoded,
                                      uint64_t* IRS_RESTRICT encoded,
                                      size_t size, uint32_t bits) noexcept {
    IRS_ASSERT(encoded);
    IRS_ASSERT(decoded);
    IRS_ASSERT(size);
    irs::packed::pack(decoded, decoded + size, encoded, bits);
  }
};

// ----------------------------------------------------------------------------
// --SECTION--                                                 Format constants
// ----------------------------------------------------------------------------

constexpr uint32_t INDEX_BLOCK_SIZE = 1024;
constexpr size_t MAX_DATA_BLOCK_SIZE = 8192;

/// @brief Column flags
/// @note by default we treat columns as a variable length sparse columns
enum ColumnProperty : uint32_t {
  CP_SPARSE = 0,
  CP_DENSE = 1,               // keys can be presented as an array indices
  CP_FIXED = 1 << 1,          // fixed length colums
  CP_MASK = 1 << 2,           // column contains no data
  CP_COLUMN_DENSE = 1 << 3,   // column index is dense
  CP_COLUMN_ENCRYPT = 1 << 4  // column contains encrypted data
};

ENABLE_BITMASK_ENUM(ColumnProperty);

bool is_good_compression_ratio(size_t raw_size,
                               size_t compressed_size) noexcept {
  // check if compressed is less than 12.5%
  return compressed_size < raw_size - (raw_size / 8U);
}

ColumnProperty write_compact(index_output& out, bstring& encode_buf,
                             encryption::stream* cipher,
                             compression::compressor& compressor,
                             bstring& data) {
  if (data.empty()) {
    out.write_byte(0);  // zig_zag_encode32(0) == 0
    return CP_MASK;
  }

  // compressor can only handle size of int32_t, so can use the negative flag as
  // a compression flag
  const bytes_view compressed =
    compressor.compress(data.data(), data.size(), encode_buf);

  if (is_good_compression_ratio(data.size(), compressed.size())) {
    IRS_ASSERT(compressed.size() <=
               static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    irs::write_zvint(out, int32_t(compressed.size()));  // compressed size
    if (cipher) {
      cipher->encrypt(out.file_pointer(),
                      const_cast<irs::byte_type*>(compressed.data()),
                      compressed.size());
    }
    out.write_bytes(compressed.data(), compressed.size());
    irs::write_zvlong(out, data.size() - MAX_DATA_BLOCK_SIZE);  // original size
  } else {
    IRS_ASSERT(data.size() <=
               static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    irs::write_zvint(
      out, int32_t(0) - int32_t(data.size()));  // -ve to mark uncompressed
    if (cipher) {
      cipher->encrypt(out.file_pointer(), data.data(), data.size());
    }
    out.write_bytes(data.c_str(), data.size());
  }

  return CP_SPARSE;
}

void read_compact(irs::index_input& in, irs::encryption::stream* cipher,
                  irs::compression::decompressor* decompressor,
                  irs::bstring& encode_buf, irs::bstring& decode_buf) {
  const auto size = irs::read_zvint(in);

  if (!size) {
    return;
  }

  const size_t buf_size = std::abs(size);

  // -ve to mark uncompressed
  if (size < 0) {
    decode_buf.resize(
      buf_size);  // Ensure that we have enough space to store decompressed data

    [[maybe_unused]] const auto read =
      in.read_bytes(decode_buf.data(), buf_size);
    IRS_ASSERT(read == buf_size);

    if (cipher) {
      cipher->decrypt(in.file_pointer() - buf_size, decode_buf.data(),
                      buf_size);
    }

    return;
  }

  if (IRS_UNLIKELY(!decompressor)) {
    throw irs::index_error{
      absl::StrCat("While reading compact, error: can't decompress "
                   "block of size ",
                   size, " without decompressor")};
  }

  // Try direct buffer access
  const byte_type* buf =
    cipher ? nullptr
           : in.read_buffer(buf_size + bytes_io<uint64_t>::const_max_vsize,
                            BufferHint::NORMAL);

  uint64_t buff_size = 0;
  if (buf) {
    const byte_type* ptr = buf + buf_size;
    buff_size = zvread<uint64_t>(ptr);
  } else {
    encode_buf.resize(buf_size);

    [[maybe_unused]] const auto read =
      in.read_bytes(encode_buf.data(), buf_size);
    IRS_ASSERT(read == buf_size);

    if (cipher) {
      cipher->decrypt(in.file_pointer() - buf_size, encode_buf.data(),
                      buf_size);
    }

    buf = encode_buf.c_str();
    buff_size = irs::read_zvlong(in);
  }

  // Ensure that we have enough space to store decompressed data
  decode_buf.resize(buff_size + MAX_DATA_BLOCK_SIZE);

  const auto decoded = decompressor->decompress(
    buf, buf_size, decode_buf.data(), decode_buf.size());

  if (IsNull(decoded)) {
    throw irs::index_error("error while reading compact");
  }
}

class meta_writer final {
 public:
  static constexpr std::string_view FORMAT_NAME = "iresearch_10_columnmeta";
  static constexpr std::string_view FORMAT_EXT = "cm";

  explicit meta_writer(ColumnMetaVersion version) noexcept : version_(version) {
    IRS_ASSERT(version >= ColumnMetaVersion::MIN &&
               version <= ColumnMetaVersion::MAX);
  }

  void prepare(directory& dir, std::string_view meta);
  void write(std::string_view name, field_id id);
  void flush();

 private:
  encryption::stream::ptr out_cipher_;
  index_output::ptr out_;
  size_t count_{};     // Number of written objects
  field_id max_id_{};  // The highest column id written (optimization for vector
                       // resize on read to highest id)
  ColumnMetaVersion version_;
};

void meta_writer::prepare(directory& dir, std::string_view segment) {
  auto filename = irs::file_name(segment, meta_writer::FORMAT_EXT);
  IRS_ASSERT(0 ==
             count_);  // Make sure there were no writes or flush was called

  out_ = dir.create(filename);

  if (!out_) {
    throw io_error{absl::StrCat("Failed to create file, path: ", filename)};
  }

  format_utils::write_header(*out_, FORMAT_NAME,
                             static_cast<int32_t>(version_));

  if (version_ > ColumnMetaVersion::MIN) {
    bstring enc_header;
    auto* enc = dir.attributes().encryption();

    if (irs::encrypt(filename, *out_, enc, enc_header, out_cipher_)) {
      IRS_ASSERT(out_cipher_ && out_cipher_->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        DEFAULT_ENCRYPTION_BUFFER_SIZE, out_cipher_->block_size());

      out_ = index_output::make<encrypted_output>(std::move(out_), *out_cipher_,
                                                  blocks_in_buffer);
    }
  }
}

void meta_writer::write(std::string_view name, field_id id) {
  IRS_ASSERT(out_);
  out_->write_vlong(id);
  write_string(*out_, name);
  ++count_;
  max_id_ = std::max(max_id_, id);
}

void meta_writer::flush() {
  IRS_ASSERT(out_);

  if (out_cipher_) {
    auto& out = static_cast<encrypted_output&>(*out_);
    out.flush();
    out_ = out.release();
  }

  out_->write_long(count_);   // write total number of written objects
  out_->write_long(max_id_);  // write highest column id written
  format_utils::write_footer(*out_);
  out_.reset();
  count_ = 0;
}

class meta_reader final {
 public:
  bool prepare(const directory& dir, const SegmentMeta& meta, size_t& count,
               field_id& max_id);
  bool read(column_meta& column);

 private:
  encryption::stream::ptr in_cipher_;
  index_input::ptr in_;
  size_t count_{0};
  field_id max_id_{0};
};

bool meta_reader::prepare(const directory& dir, const SegmentMeta& meta,
                          size_t& count, field_id& max_id) {
  const auto filename = irs::file_name(meta.name, meta_writer::FORMAT_EXT);

  bool exists;

  if (!dir.exists(exists, filename)) {
    throw io_error{
      absl::StrCat("Failed to check existence of file, path: ", filename)};
  }

  if (!exists) {
    // column meta is optional
    return false;
  }

  in_ = dir.open(filename, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in_) {
    throw io_error{absl::StrCat("Failed to open file, path: ", filename)};
  }

  const auto checksum = format_utils::checksum(*in_);

  constexpr size_t kFooterLength = sizeof(uint64_t)    // count
                                   + sizeof(uint64_t)  // max id
                                   + format_utils::kFooterLen;

  const size_t length = in_->length();

  if (length < kFooterLength) {
    throw index_error{absl::StrCat("Invalid column id: ", max_id,
                                   " footer length, "
                                   "got ",
                                   length, ", path: ", filename)};
  }

  // seek to start of meta footer (before count and max_id)
  in_->seek(length - kFooterLength);
  count = in_->read_long();   // read number of objects to read
  max_id = in_->read_long();  // read highest column id written

  if (max_id >= std::numeric_limits<size_t>::max()) {
    throw index_error{
      absl::StrCat("Invalid max column id: ", max_id, ", path: ", filename)};
  }

  format_utils::check_footer(*in_, checksum);

  in_->seek(0);

  const ColumnMetaVersion version{
    format_utils::check_header(*in_, meta_writer::FORMAT_NAME,
                               static_cast<int32_t>(ColumnMetaVersion::MIN),
                               static_cast<int32_t>(ColumnMetaVersion::MAX))};

  if (version > ColumnMetaVersion::MIN) {
    encryption* enc = dir.attributes().encryption();

    if (irs::decrypt(filename, *in_, enc, in_cipher_)) {
      IRS_ASSERT(in_cipher_ && in_cipher_->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        DEFAULT_ENCRYPTION_BUFFER_SIZE, in_cipher_->block_size());

      in_ = std::make_unique<encrypted_input>(std::move(in_), *in_cipher_,
                                              blocks_in_buffer, kFooterLength);
    }
  }

  count_ = count;
  max_id_ = max_id;
  return true;
}

bool meta_reader::read(column_meta& column) {
  if (!count_) {
    return false;
  }

  const auto id = in_->read_vlong();

  IRS_ASSERT(id <= max_id_);
  column.name = read_string<std::string>(*in_);
  column.id = id;
  --count_;

  return true;
}

}  // namespace

namespace irs {
namespace columnstore {

template<size_t Size>
class index_block {
 public:
  static constexpr size_t SIZE = Size;

  void push_back(doc_id_t key, uint64_t offset) noexcept {
    IRS_ASSERT(key_ >= keys_);
    IRS_ASSERT(key_ < keys_ + Size);
    *key_++ = key;
    IRS_ASSERT(key >= key_[-1]);
    IRS_ASSERT(offset_ >= offsets_);
    IRS_ASSERT(offset_ < offsets_ + Size);
    *offset_++ = offset;
    IRS_ASSERT(offset >= offset_[-1]);
  }

  void pop_back() noexcept {
    IRS_ASSERT(key_ > keys_);
    *key_-- = 0;
    IRS_ASSERT(offset_ > offsets_);
    *offset_-- = 0;
  }

  // returns total number of items
  uint32_t total() const noexcept { return flushed() + size(); }

  // returns total number of flushed items
  uint32_t flushed() const noexcept { return flushed_; }

  // returns number of items to be flushed
  uint32_t size() const noexcept {
    IRS_ASSERT(key_ >= keys_);
    return uint32_t(key_ - keys_);
  }

  bool empty() const noexcept { return keys_ == key_; }

  bool full() const noexcept { return key_ == std::end(keys_); }

  doc_id_t min_key() const noexcept { return *keys_; }

  doc_id_t max_key() const noexcept {
    // if this->empty(), will point to last offset
    // value which is 0 in this case
    return *(key_ - 1);
  }

  uint64_t max_offset() const noexcept {
    IRS_ASSERT(offset_ > offsets_);
    return *(offset_ - 1);
  }

  ColumnProperty flush(data_output& out, uint64_t* buf) {
    if (empty()) {
      return CP_DENSE | CP_FIXED;
    }

    const auto num_elements = this->size();

    ColumnProperty props = CP_SPARSE;

    // write keys
    {
      // adjust number of elements to pack to the nearest value
      // that is multiple of the block size
      const auto block_size = math::ceil32(num_elements, packed::BLOCK_SIZE_32);
      IRS_ASSERT(block_size >= num_elements);

      IRS_ASSERT(std::is_sorted(keys_, key_));
      const auto stats = encode::avg::encode(keys_, key_);
      const auto bits = encode::avg::write_block(
        [&](const uint32_t* IRS_RESTRICT decoded,
            uint32_t* IRS_RESTRICT encoded, uint32_t bits) {
          format_traits::pack32(decoded, encoded, block_size, bits);
        },
        out, stats.first, stats.second, keys_, block_size,
        reinterpret_cast<uint32_t*>(buf));

      if (1 == stats.second && 0 == keys_[0] && bitpack::rl(bits)) {
        props |= CP_DENSE;
      }
    }

    // write offsets
    {
      // adjust number of elements to pack to the nearest value
      // that is multiple of the block size
      const auto block_size = math::ceil64(num_elements, packed::BLOCK_SIZE_64);
      IRS_ASSERT(block_size >= num_elements);

      IRS_ASSERT(std::is_sorted(offsets_, offset_));
      const auto stats = encode::avg::encode(offsets_, offset_);
      const auto bits = encode::avg::write_block(
        &format_traits::pack64, out, std::get<0>(stats), std::get<1>(stats),
        offsets_, block_size, buf);

      if (0 == offsets_[0] && bitpack::rl(bits)) {
        props |= CP_FIXED;
      }
    }

    flushed_ += num_elements;

    // reset pointers and clear data
    key_ = keys_;
    std::memset(keys_, 0, sizeof keys_);
    offset_ = offsets_;
    std::memset(offsets_, 0, sizeof offsets_);

    return props;
  }

 private:
  // order is important (see max_key())
  uint64_t offsets_[Size]{};
  doc_id_t keys_[Size]{};
  uint64_t* offset_{offsets_};
  doc_id_t* key_{keys_};
  uint32_t flushed_{};  // number of flushed items
};

//////////////////////////////////////////////////////////////////////////////
/// @class writer
//////////////////////////////////////////////////////////////////////////////
class writer final : public irs::columnstore_writer {
 public:
  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = 1;

  static constexpr std::string_view FORMAT_NAME = "iresearch_10_columnstore";
  static constexpr std::string_view FORMAT_EXT = "cs";

  explicit writer(Version version, ColumnMetaVersion meta_version) noexcept
    : meta_writer_{meta_version},
      buf_(2 * MAX_DATA_BLOCK_SIZE, 0),
      dir_(nullptr),
      version_(version) {
    static_assert(
      2 * MAX_DATA_BLOCK_SIZE >= INDEX_BLOCK_SIZE * sizeof(uint64_t),
      "buffer is not big enough");

    IRS_ASSERT(version >= Version::MIN && version <= Version::MAX);
  }

  void prepare(directory& dir, const SegmentMeta& meta) final;
  // Current implmentation doesn't support column headers
  column_t push_column(const ColumnInfo& info,
                       column_finalizer_f /*writer*/) final;
  bool commit(const flush_state& state) final;
  void rollback() noexcept final;

 private:
  class column final : public irs::column_output {
   public:
    explicit column(writer& ctx, field_id id, const irs::type_info& type,
                    columnstore_writer::column_finalizer_f&& finalizer,
                    compression::compressor::ptr compressor,
                    encryption::stream* cipher)
      : ctx_(&ctx),
        comp_type_(type),
        comp_(std::move(compressor)),
        finalizer_{std::move(finalizer)},
        cipher_(cipher),
        id_(id),
        blocks_index_(IResourceManager::kNoop),
        block_buf_(2 * MAX_DATA_BLOCK_SIZE, 0) {
      IRS_ASSERT(comp_);   // ensured by `push_column'
      block_buf_.clear();  // reset size to '0'
    }

    void Prepare(doc_id_t key) final {
      IRS_ASSERT(doc_limits::invalid() < key);
      IRS_ASSERT(key < doc_limits::eof());
      IRS_ASSERT(key >= block_index_.max_key());

      if (key <= block_index_.max_key()) {
        // less or equal to previous key
        return;
      }

      // flush block if we've overcome MAX_DATA_BLOCK_SIZE size
      // or reached the end of the index block
      if (block_buf_.size() >= MAX_DATA_BLOCK_SIZE || block_index_.full()) {
        flush_block();
      }

      block_index_.push_back(key, block_buf_.size());
    }

    bool empty() const noexcept { return !block_index_.total(); }

    std::string_view name() const noexcept { return name_; }

    field_id id() const noexcept { return id_; }

    void finish(bstring& tmp) {
      auto& out = *ctx_->data_out_;

      if (finalizer_) {
        name_ = finalizer_(tmp);
      }

      // evaluate overall column properties
      auto column_props = blocks_props_;
      if (0 != (column_props_ & CP_DENSE)) {
        column_props |= CP_COLUMN_DENSE;
      }
      if (cipher_) {
        column_props |= CP_COLUMN_ENCRYPT;
      }

      write_enum(out, column_props);
      if (ctx_->version_ > Version::MIN) {
        write_string(out, comp_type_.name());
        comp_->flush(out);  // flush compression dependent data
      }
      out.write_vint(block_index_.total());  // total number of items
      out.write_vint(max_);                  // max column key
      out.write_vint(avg_block_size_);       // avg data block size
      out.write_vint(avg_block_count_);      // avg number of elements per block
      out.write_vint(column_index_.total());  // total number of index blocks
      blocks_index_.file >> out;              // column blocks index
    }

    void flush() {
      // do not take into account last block
      const auto blocks_count = std::max(1U, column_index_.total());
      avg_block_count_ = block_index_.flushed() / blocks_count;
      avg_block_size_ = static_cast<uint32_t>(length_ / blocks_count);

      // we don't care of tail block size
      prev_block_size_ = block_buf_.size();

      // commit and flush remain blocks
      flush_block();

      // finish column blocks index
      IRS_ASSERT(ctx_->buf_.size() >= INDEX_BLOCK_SIZE * sizeof(uint64_t));
      auto* buf = reinterpret_cast<uint64_t*>(ctx_->buf_.data());
      column_index_.flush(blocks_index_.stream, buf);
      blocks_index_.stream.flush();
    }

    void write_byte(byte_type b) final { block_buf_ += b; }

    void write_bytes(const byte_type* b, size_t size) final {
      block_buf_.append(b, size);
    }

    void reset() final {
      if (block_index_.empty()) {
        // nothing to reset
        return;
      }

      // reset to previous offset
      block_buf_.resize(block_index_.max_offset());
      block_index_.pop_back();
    }

   private:
    void flush_block() {
      if (block_index_.empty()) {
        // nothing to flush
        return;
      }

      // refresh column properties
      // column is dense IFF
      // - all blocks are dense
      // - there are no gaps between blocks
      // - all data blocks have the same length
      column_props_ &= ColumnProperty{
        1 == (block_index_.min_key() - max_) &&
        (!doc_limits::valid(max_) || prev_block_size_ == block_buf_.size())};

      // update max element
      max_ = block_index_.max_key();

      auto& out = *ctx_->data_out_;

      // write first block key & where block starts
      column_index_.push_back(block_index_.min_key(), out.file_pointer());

      IRS_ASSERT(ctx_->buf_.size() >= INDEX_BLOCK_SIZE * sizeof(uint64_t));
      auto* buf = reinterpret_cast<uint64_t*>(ctx_->buf_.data());

      if (column_index_.full()) {
        column_index_.flush(blocks_index_.stream, buf);
      }

      // flush current block

      // write total number of elements in the block
      out.write_vint(block_index_.size());

      // write block index, compressed data and aggregate block properties
      // note that order of calls is important here, since it is not defined
      // which expression should be evaluated first in the following example:
      //   const auto res = expr0() | expr1();
      // otherwise it would violate format layout
      auto block_props = block_index_.flush(out, buf);
      block_props |=
        write_compact(out, ctx_->buf_, cipher_, *comp_, block_buf_);

      prev_block_size_ = block_buf_.size();
      length_ += prev_block_size_;

      // refresh blocks properties
      blocks_props_ &= block_props;
      // reset buffer stream after flush
      block_buf_.clear();

      // refresh column properties
      // column is dense IFF
      // - all blocks are dense
      // - there are no gaps between blocks
      // - all data blocks have the same length
      column_props_ &= ColumnProperty(0 != (block_props & CP_DENSE));
    }

    writer* ctx_;  // writer context
    irs::type_info comp_type_;
    compression::compressor::ptr comp_;  // compressor used for column
    columnstore_writer::column_finalizer_f finalizer_;
    encryption::stream* cipher_;
    field_id id_;
    uint64_t length_{};  // size of all data blocks in the column
    uint64_t prev_block_size_{};
    index_block<INDEX_BLOCK_SIZE>
      block_index_;  // current block index (per document key/offset)
    index_block<INDEX_BLOCK_SIZE>
      column_index_;              // column block index (per block key/offset)
    memory_output blocks_index_;  // blocks index
    bstring block_buf_;           // data buffer
    ColumnProperty blocks_props_{
      CP_DENSE | CP_FIXED | CP_MASK};  // aggregated column blocks properties
    ColumnProperty column_props_{
      CP_DENSE};                  // aggregated column block index properties
    uint32_t avg_block_count_{};  // average number of items per block (tail
                                  // block is not taken into account since it
                                  // may skew distribution)
    uint32_t
      avg_block_size_{};  // average size of the block (tail block is not taken
                          // into account since it may skew distribution)
    doc_id_t max_{doc_limits::invalid()};  // max key (among flushed blocks)
    std::string_view name_;
  };

  void flush_meta(const flush_state& meta);

  meta_writer meta_writer_;
  std::deque<column> columns_;  // pointers remain valid
  std::vector<std::reference_wrapper<const column>> sorted_columns_;
  bstring buf_;  // reusable temporary buffer for packing/compression
  index_output::ptr data_out_;
  std::string filename_;
  directory* dir_;
  encryption::stream::ptr data_out_cipher_;
  Version version_;
};

void writer::prepare(directory& dir, const SegmentMeta& meta) {
  columns_.clear();

  auto filename = file_name(meta.name, writer::FORMAT_EXT);
  auto data_out = dir.create(filename);

  if (!data_out) {
    throw io_error{absl::StrCat("Failed to create file, path: ", filename)};
  }

  format_utils::write_header(*data_out, FORMAT_NAME,
                             static_cast<int32_t>(version_));

  encryption::stream::ptr data_out_cipher;

  if (version_ > Version::MIN) {
    bstring enc_header;
    auto* enc = dir.attributes().encryption();

    const auto encrypt =
      irs::encrypt(filename, *data_out, enc, enc_header, data_out_cipher);
    IRS_ASSERT(!encrypt || (data_out_cipher && data_out_cipher->block_size()));
    IRS_IGNORE(encrypt);
  }

  // noexcept block
  dir_ = &dir;
  data_out_ = std::move(data_out);
  data_out_cipher_ = std::move(data_out_cipher);
  filename_ = std::move(filename);
}

columnstore_writer::column_t writer::push_column(const ColumnInfo& info,
                                                 column_finalizer_f finalizer) {
  encryption::stream* cipher;
  irs::type_info compression;

  if (version_ > Version::MIN) {
    compression = info.compression;
    cipher = info.encryption ? data_out_cipher_.get() : nullptr;
  } else {
    // we don't support encryption and custom
    // compression for 'FORMAT_MIN' version
    compression = irs::type<compression::lz4>::get();
    cipher = nullptr;
  }

  auto compressor = compression::get_compressor(compression, info.options);

  if (!compressor) {
    compressor = compression::compressor::identity();
  }

  const auto id = columns_.size();
  auto& column =
    columns_.emplace_back(*this, id, info.compression, std::move(finalizer),
                          std::move(compressor), cipher);

  return {id, column};
}

bool writer::commit(const flush_state& state) {
  IRS_ASSERT(dir_);

  // Remove all empty columns from tail.
  while (!columns_.empty() && columns_.back().empty()) {
    columns_.pop_back();
  }

  // Remove file if there is no data to write.
  if (columns_.empty()) {
    data_out_.reset();

    if (!dir_->remove(filename_)) {  // Ignore error.
      IRS_LOG_ERROR(absl::StrCat("Failed to remove file, path: ", filename_));
    }

    return false;  // Nothing to flush.
  }

  // Flush all remain data including possible
  // empty columns amongst the filled ones.
  for (auto& column : columns_) {
    column.flush();
  }

  // Where blocks index start.
  const auto block_index_ptr = data_out_->file_pointer();

  data_out_->write_vlong(columns_.size());  // Number of columns.

  // Dummy buffer, current implementation doesn't support column payload.
  bstring tmp;

  for (auto& column : columns_) {
    tmp.clear();
    column.finish(tmp);
  }

  data_out_->write_long(block_index_ptr);
  format_utils::write_footer(*data_out_);

  flush_meta(state);

  rollback();

  return true;
}

void writer::rollback() noexcept {
  filename_.clear();
  dir_ = nullptr;
  data_out_.reset();  // close output
  columns_.clear();   // ensure next flush (without prepare(...)) will use the
                      // section without 'data_out_'
  sorted_columns_.clear();
}

void writer::flush_meta(const flush_state& meta) {
  // ensure columns are sorted
  IRS_ASSERT(sorted_columns_.empty());
  sorted_columns_.reserve(columns_.size());
  std::copy_if(std::begin(columns_), std::end(columns_),
               std::back_inserter(sorted_columns_),
               [](auto& column) noexcept { return !IsNull(column.name()); });
  std::sort(std::begin(sorted_columns_), std::end(sorted_columns_),
            [](const writer::column& lhs, const writer::column& rhs) noexcept {
              IRS_ASSERT(!IsNull(rhs.name()));
              return lhs.name() < rhs.name();
            });

  // flush columns meta
  meta_writer_.prepare(*dir_, meta.name);
  for (const writer::column& column : sorted_columns_) {
    IRS_ASSERT(!IsNull(column.name()));
    meta_writer_.write(column.name(), column.id());
  }
  meta_writer_.flush();
}

template<typename Block, typename Allocator>
class block_cache : irs::util::noncopyable {
 public:
  explicit block_cache(const Allocator& alloc = Allocator()) : cache_(alloc) {}
  block_cache(block_cache&&) = default;

  template<typename... Args>
  Block& emplace_back(Args&&... args) {
    cache_.emplace_back(std::forward<Args>(args)...);
    return cache_.back();
  }

  void pop_back() noexcept { cache_.pop_back(); }

 private:
  std::deque<Block, Allocator> cache_;  // pointers remain valid
};

template<typename Block, typename Allocator>
struct block_cache_traits {
  typedef Block block_t;
  typedef
    typename std::allocator_traits<Allocator>::template rebind_alloc<block_t>
      allocator_t;
  typedef block_cache<Block, allocator_t> cache_t;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                            Blocks
// -----------------------------------------------------------------------------

class sparse_block : util::noncopyable {
 private:
  struct ref {
    doc_id_t key{doc_limits::eof()};
    uint64_t offset{0};
  };

 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      next_ = std::lower_bound(
        begin_, end_, doc,
        [](const ref& lhs, doc_id_t rhs) { return lhs.key < rhs; });

      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (next_ == end_) {
        return false;
      }

      value_ = next_->key;
      const auto vbegin = next_->offset;

      begin_ = next_;
      const auto vend = (++next_ == end_ ? data_->size() : next_->offset);

      IRS_ASSERT(vend >= vbegin);
      IRS_ASSERT(payload_ != &kDummy);
      *payload_ = bytes_view(data_->c_str() + vbegin,  // start
                             vend - vbegin);           // length
      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      payload_ = &kDummy;
      next_ = begin_ = end_;
    }

    void reset(const sparse_block& block, irs::payload& payload) noexcept {
      value_ = doc_limits::invalid();
      payload.value = {};
      payload_ = &payload.value;
      next_ = begin_ = std::begin(block.index_);
      end_ = block.end_;
      data_ = &block.data_;

      IRS_ASSERT(std::is_sorted(
        begin_, end_,
        [](const sparse_block::ref& lhs, const sparse_block::ref& rhs) {
          return lhs.key < rhs.key;
        }));
    }

    bool operator==(const sparse_block& rhs) const noexcept {
      return data_ == &rhs.data_;
    }

   private:
    irs::bytes_view* payload_{&kDummy};
    irs::doc_id_t value_{doc_limits::invalid()};
    const sparse_block::ref* next_{};  // next position
    const sparse_block::ref* begin_{};
    const sparse_block::ref* end_{};
    const bstring* data_{};
  };

  void load(index_input& in, compression::decompressor* decomp,
            encryption::stream* cipher, bstring& buf) {
    const uint32_t size = in.read_vint();  // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'sparse_block' found in columnstore");
    }

    auto begin = std::begin(index_);

    // read keys
    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint32_t*>(buf.data()),
      [begin](uint32_t key) mutable {
        begin->key = key;
        ++begin;
      });

    // read offsets
    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint64_t*>(buf.data()),
      [begin](uint64_t offset) mutable {
        begin->offset = offset;
        ++begin;
      });

    // read data
    read_compact(in, cipher, decomp, buf, data_);
    end_ = index_ + size;
  }

 private:
  // TODO: use single memory block for both index & data

  // all blocks except the tail are going to be fully filled,
  // so we don't track size of each block here since we could
  // waste just INDEX_BLOCK_SIZE*sizeof(ref)-1 per column
  // in the worst case
  ref index_[INDEX_BLOCK_SIZE];
  bstring data_;
  const ref* end_{std::end(index_)};
};

// cppcheck-suppress noConstructor
class dense_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      // before the current element
      if (doc <= value_) {
        doc = value_;
      }

      // FIXME refactor
      it_ = begin_ + doc - base_;

      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (it_ >= end_) {
        // after the last element
        return false;
      }

      value_ = base_ + static_cast<uint32_t>(std::distance(begin_, it_));
      next_value();

      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      payload_ = &kDummy;
      it_ = begin_ = end_;
    }

    void reset(const dense_block& block, irs::payload& payload) noexcept {
      value_ = block.base_;
      payload.value = {};
      payload_ = &payload.value;
      it_ = begin_ = std::begin(block.index_);
      end_ = block.end_;
      data_ = &block.data_;
      base_ = block.base_;
    }

    bool operator==(const dense_block& rhs) const noexcept {
      return data_ == &rhs.data_;
    }

   private:
    // note that function increments 'it_'
    void next_value() noexcept {
      const auto vbegin = *it_;
      const auto vend = (++it_ == end_ ? data_->size() : *it_);

      IRS_ASSERT(vend >= vbegin);
      IRS_ASSERT(payload_ != &kDummy);
      *payload_ = bytes_view(data_->c_str() + vbegin,  // start
                             vend - vbegin);           // length
    }

    irs::bytes_view* payload_{&kDummy};
    irs::doc_id_t value_{doc_limits::invalid()};
    const uint32_t* begin_{};
    const uint32_t* it_{};
    const uint32_t* end_{};
    const bstring* data_{};
    doc_id_t base_{};
  };

  void load(index_input& in, compression::decompressor* decomp,
            encryption::stream* cipher, bstring& buf) {
    const uint32_t size = in.read_vint();  // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'dense_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, base_, avg) || 1 != avg) {
      throw index_error{
        absl::StrCat("Invalid RL encoding in 'dense_block', base_key=", base_,
                     " avg_delta=", avg)};
    }

    // read data offsets
    auto begin = std::begin(index_);

    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint64_t*>(buf.data()),
      [begin](uint64_t offset) mutable {
        *begin = static_cast<uint32_t>(offset);
        ++begin;
      });

    // read data
    read_compact(in, cipher, decomp, buf, data_);
    end_ = index_ + size;
  }

 private:
  // TODO: use single memory block for both index & data

  // all blocks except the tail are going to be fully filled,
  // so we don't track size of each block here since we could
  // waste just INDEX_BLOCK_SIZE*sizeof(ref)-1 per column
  // in the worst case
  uint32_t index_[INDEX_BLOCK_SIZE];
  bstring data_;
  uint32_t* end_{index_};
  doc_id_t base_{0};
};

class dense_fixed_offset_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      if (doc < value_next_) {
        if (!doc_limits::valid(value_)) {
          return next();
        }

        // don't seek backwards
        return true;
      }

      value_next_ = doc;
      return next();
    }

    const doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (value_next_ >= value_end_) {
        seal();
        return false;
      }

      value_ = value_next_++;
      const auto offset = (value_ - value_min_) * avg_length_;

      IRS_ASSERT(payload_ != &kDummy);
      *payload_ =
        bytes_view(data_.data() + offset,
                   value_ == value_back_ ? data_.size() - offset : avg_length_);

      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      value_next_ = doc_limits::eof();
      value_min_ = doc_limits::eof();
      value_end_ = doc_limits::eof();
      payload_ = &kDummy;
    }

    void reset(const dense_fixed_offset_block& block,
               irs::payload& payload) noexcept {
      avg_length_ = block.avg_length_;
      data_ = block.data_;
      payload.value = {};
      payload_ = &payload.value;
      value_ = doc_limits::invalid();
      value_next_ = block.base_key_;
      value_min_ = block.base_key_;
      value_end_ = value_min_ + block.size_;
      value_back_ = value_end_ - 1;
    }

    bool operator==(const dense_fixed_offset_block& rhs) const noexcept {
      return data_.data() == rhs.data_.c_str();
    }

   private:
    uint64_t avg_length_{};  // average value length
    bytes_view data_;
    irs::bytes_view* payload_{&kDummy};
    doc_id_t value_{doc_limits::invalid()};       // current value
    doc_id_t value_next_{doc_limits::invalid()};  // next value
    doc_id_t value_min_{};                        // min doc_id
    doc_id_t value_end_{};                        // after the last valid doc id
    doc_id_t value_back_{};                       // last valid doc id
  };

  void load(index_input& in, compression::decompressor* decomp,
            encryption::stream* cipher, bstring& buf) {
    size_ = in.read_vint();  // total number of entries in a block

    if (!size_) {
      throw index_error(
        "Empty 'dense_fixed_offset_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, base_key_, avg) || 1 != avg) {
      throw index_error{absl::StrCat(
        "Invalid RL encoding in 'dense_fixed_offset_block', base_key=",
        base_key_, "avg_delta=", avg)};
    }

    // fixed length block must be encoded with RL encoding
    if (!encode::avg::read_block_rl32(in, base_offset_, avg_length_)) {
      throw index_error{absl::StrCat(
        "Invalid RL encoding in 'dense_fixed_offset_block', base_offset=",
        base_key_, "avg_length=", avg_length_)};
    }

    // read data
    read_compact(in, cipher, decomp, buf, data_);
  }

 private:
  doc_id_t base_key_{};     // base key
  uint32_t base_offset_{};  // base offset
  uint32_t avg_length_{};   // entry length
  doc_id_t size_{};         // total number of entries
  bstring data_;
};

class sparse_mask_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      it_ = std::lower_bound(begin_, end_, doc);

      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (it_ == end_) {
        return false;
      }

      begin_ = it_;
      value_ = *it_++;
      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      it_ = begin_ = end_;
    }

    void reset(const sparse_mask_block& block, irs::payload& payload) noexcept {
      value_ = doc_limits::invalid();
      payload.value = {};  // mask block doesn't have payload
      it_ = begin_ = std::begin(block.keys_);
      end_ = begin_ + block.size_;

      IRS_ASSERT(std::is_sorted(begin_, end_));
    }

    bool operator==(const sparse_mask_block& rhs) const noexcept {
      return end_ == (rhs.keys_ + rhs.size_);
    }

   private:
    irs::doc_id_t value_{doc_limits::invalid()};
    const doc_id_t* it_{};
    const doc_id_t* begin_{};
    const doc_id_t* end_{};
  };

  sparse_mask_block() noexcept {
    std::fill(std::begin(keys_), std::end(keys_), doc_limits::eof());
  }

  void load(index_input& in, compression::decompressor* /*decomp*/,
            encryption::stream* /*cipher*/, bstring& buf) {
    size_ = in.read_vint();  // total number of entries in a block

    if (!size_) {
      throw index_error("Empty 'sparse_mask_block' found in columnstore");
    }

    auto begin = std::begin(keys_);

    encode::avg::visit_block_packed_tail(
      in, size_, reinterpret_cast<uint32_t*>(buf.data()),
      [begin](uint32_t key) mutable { *begin++ = key; });

    // mask block has no data, so all offsets should be equal to 0
    if (!encode::avg::check_block_rl64(in, 0)) {
      throw index_error("'sparse_mask_block' expected to contain no data");
    }
  }

 private:
  // all blocks except the tail one are going to be fully filled,
  // so we store keys in a fixed length array since we could
  // waste just INDEX_BLOCK_SIZE*sizeof(ref)-1 per column
  // in the worst case
  doc_id_t keys_[INDEX_BLOCK_SIZE];
  doc_id_t size_{};  // number of documents in a block
};

class dense_mask_block {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      if (doc < doc_) {
        if (!doc_limits::valid(value_)) {
          return next();
        }

        // don't seek backwards
        return true;
      }

      doc_ = doc;
      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (doc_ >= max_) {
        seal();
        return false;
      }

      value_ = doc_++;

      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      doc_ = max_;
      block_ = nullptr;
    }

    void reset(const dense_mask_block& block, irs::payload& payload) noexcept {
      block_ = &block;
      payload.value = {};  // mask block doesn't have payload
      doc_ = block.min_;
      max_ = block.max_;
    }

    bool operator==(const dense_mask_block& rhs) const noexcept {
      return block_ == &rhs;
    }

   private:
    const dense_mask_block* block_{};
    doc_id_t value_{doc_limits::invalid()};
    doc_id_t doc_{doc_limits::invalid()};
    doc_id_t max_{doc_limits::invalid()};
  };

  dense_mask_block() noexcept
    : min_(doc_limits::invalid()), max_(doc_limits::invalid()) {}

  void load(index_input& in, compression::decompressor* /*decomp*/,
            encryption::stream* /*cipher*/, bstring& /*buf*/) {
    const auto size = in.read_vint();  // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'dense_mask_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, min_, avg) || 1 != avg) {
      throw index_error{
        absl::StrCat("Invalid RL encoding in 'dense_mask_block', "
                     "base_key=",
                     min_, " avg_delta=", avg)};
    }

    // mask block has no data, so all offsets should be equal to 0
    if (!encode::avg::check_block_rl64(in, 0)) {
      throw index_error{"'dense_mask_block' expected to contain no data"};
    }

    max_ = min_ + size;
  }

 private:
  doc_id_t min_;
  doc_id_t max_;
};

template<typename Allocator = std::allocator<sparse_block>>
class read_context
  : public block_cache_traits<sparse_block, Allocator>::cache_t,
    public block_cache_traits<dense_block, Allocator>::cache_t,
    public block_cache_traits<dense_fixed_offset_block, Allocator>::cache_t,
    public block_cache_traits<sparse_mask_block, Allocator>::cache_t,
    public block_cache_traits<dense_mask_block, Allocator>::cache_t {
 public:
  using ptr = std::shared_ptr<read_context>;

  static ptr make(const index_input& stream, encryption::stream* cipher) {
    auto clone = stream.reopen();  // reopen thead-safe stream

    if (!clone) {
      // implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen columnstore input");

      throw io_error{"Failed to reopen columnstore input"};
    }

    return std::make_shared<read_context>(std::move(clone), cipher);
  }

  read_context(index_input::ptr&& in, encryption::stream* cipher,
               const Allocator& alloc = Allocator())
    : block_cache_traits<sparse_block, Allocator>::cache_t(
        typename block_cache_traits<sparse_block, Allocator>::allocator_t(
          alloc)),
      block_cache_traits<dense_block, Allocator>::cache_t(
        typename block_cache_traits<dense_block, Allocator>::allocator_t(
          alloc)),
      block_cache_traits<dense_fixed_offset_block, Allocator>::cache_t(
        typename block_cache_traits<dense_fixed_offset_block,
                                    Allocator>::allocator_t(alloc)),
      block_cache_traits<sparse_mask_block, Allocator>::cache_t(
        typename block_cache_traits<sparse_mask_block, Allocator>::allocator_t(
          alloc)),
      block_cache_traits<dense_mask_block, Allocator>::cache_t(
        typename block_cache_traits<dense_mask_block, Allocator>::allocator_t(
          alloc)),
      buf_(INDEX_BLOCK_SIZE * sizeof(uint32_t), 0),
      stream_(std::move(in)),
      cipher_(cipher) {}

  template<typename Block, typename... Args>
  Block& emplace_back(uint64_t offset, compression::decompressor* decomp,
                      bool decrypt, Args&&... args) {
    // cppcheck-suppress constVariable
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;

    // add cache entry
    auto& block{cache.emplace_back(std::forward<Args>(args)...)};

    try {
      load(block, decomp, decrypt, offset);
    } catch (...) {
      // failed to load block
      pop_back<Block>();

      throw;
    }

    return block;
  }

  template<typename Block>
  void load(Block& block, compression::decompressor* decomp, bool decrypt,
            uint64_t offset) {
    stream_->seek(offset);  // seek to the offset
    block.load(*stream_, decomp, decrypt ? cipher_ : nullptr, buf_);
  }

  template<typename Block>
  void pop_back() noexcept {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;
    cache.pop_back();
  }

 private:
  bstring buf_;  // temporary buffer for decoding/unpacking
  index_input::ptr stream_;
  encryption::stream* cipher_;  // options cipher stream
};

typedef read_context<> read_context_t;

class context_provider : private util::noncopyable {
 public:
  explicit context_provider(size_t max_pool_size)
    : pool_(std::max(size_t(1), max_pool_size)) {}

  void prepare(index_input::ptr&& stream,
               encryption::stream::ptr&& cipher) noexcept {
    IRS_ASSERT(stream);

    stream_ = std::move(stream);
    cipher_ = std::move(cipher);
  }

  bounded_object_pool<read_context_t>::ptr get_context() const {
    return pool_.emplace(*stream_, cipher_.get());
  }

 private:
  mutable bounded_object_pool<read_context_t> pool_;
  encryption::stream::ptr cipher_;
  index_input::ptr stream_;
};

// in case of success caches block pointed
// instance, nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t& load_block(const context_provider& ctxs,
                                             compression::decompressor* decomp,
                                             bool decrypt, BlockRef& ref) {
  typedef typename BlockRef::block_t block_t;

  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();
    IRS_ASSERT(ctx);

    // load block
    const auto& block =
      ctx->template emplace_back<block_t>(ref.offset, decomp, decrypt);

    // mark block as loaded
    if (ref.pblock.compare_exchange_strong(cached, &block)) {
      cached = &block;
    } else {
      // already cached by another thread
      ctx->template pop_back<block_t>();
    }
  }

  // cppcheck-suppress invalidLifetime
  return *cached;
}

// in case of success caches block pointed
// by 'ref' in the specified 'block' and
// retuns a pointer to cached instance,
// nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t& load_block(
  const context_provider& ctxs, compression::decompressor* decomp, bool decrypt,
  const BlockRef& ref, typename BlockRef::block_t& block) {
  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();
    IRS_ASSERT(ctx);

    ctx->load(block, decomp, decrypt, ref.offset);

    cached = &block;
  }

  return *cached;
}

////////////////////////////////////////////////////////////////////////////////
/// @class column
////////////////////////////////////////////////////////////////////////////////
class column : public irs::column_reader, private util::noncopyable {
 public:
  using ptr = std::unique_ptr<column>;

  explicit column(field_id id, ColumnProperty props) noexcept
    : id_{id}, encrypted_(0 != (props & CP_COLUMN_ENCRYPT)) {}

  field_id id() const final { return id_; }

  std::string_view name() const final {
    return name_.has_value() ? name_.value() : std::string_view{};
  }

  bytes_view payload() const final {
    // Implementation doesn't support column headers.
    return {};
  }

  virtual void read(data_input& in, uint64_t* /*buf*/,
                    compression::decompressor::ptr decomp) {
    count_ = in.read_vint();
    max_ = in.read_vint();
    avg_block_size_ = in.read_vint();
    avg_block_count_ = in.read_vint();
    if (!avg_block_count_) {
      avg_block_count_ = count_;
    }
    decomp_ = std::move(decomp);
  }

  void set_name(std::string&& name) noexcept { name_ = std::move(name); }

  bool encrypted() const noexcept { return encrypted_; }
  doc_id_t max() const noexcept { return max_; }
  doc_id_t size() const noexcept final { return count_; }
  bool empty() const noexcept { return 0 == size(); }
  uint32_t avg_block_size() const noexcept { return avg_block_size_; }
  uint32_t avg_block_count() const noexcept { return avg_block_count_; }
  compression::decompressor* decompressor() const noexcept {
    return decomp_.get();
  }

 protected:
  // same as size() but returns uint32_t to avoid type convertions
  uint32_t count() const noexcept { return count_; }

 private:
  compression::decompressor::ptr decomp_;
  doc_id_t max_{doc_limits::eof()};
  uint32_t count_{};
  uint32_t avg_block_size_{};
  uint32_t avg_block_count_{};
  field_id id_;
  std::optional<std::string> name_;
  bool encrypted_{false};  // cached encryption mark
};

template<typename Column>
class column_iterator : public irs::doc_iterator {
 private:
  using attributes = std::tuple<document, cost, score, payload>;

 public:
  typedef Column column_t;
  typedef typename Column::block_t block_t;
  typedef typename block_t::iterator block_iterator_t;

  explicit column_iterator(const column_t& column,
                           const typename column_t::block_ref* begin,
                           const typename column_t::block_ref* end, bool cache)
    : begin_(begin),
      seek_origin_(begin),
      end_(end),
      column_(&column),
      cache_(cache) {
    std::get<cost>(attrs_).reset(column.size());
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

  doc_id_t seek(irs::doc_id_t doc) final {
    begin_ = column_->find_block(seek_origin_, end_, doc);

    if (!next_block()) {
      return value();
    }

    if (!block_.seek(doc)) {
      // reached the end of block,
      // advance to the next one
      while (next_block() && !block_.next()) {
      }
    }

    std::get<document>(attrs_).value = block_.value();
    return value();
  }

  bool next() final {
    while (!block_.next()) {
      if (!next_block()) {
        return false;
      }
    }

    std::get<document>(attrs_).value = block_.value();
    return true;
  }

 private:
  typedef typename column_t::refs_t refs_t;

  bool next_block() {
    auto& doc = std::get<document>(attrs_);
    auto& payload = std::get<irs::payload>(attrs_);

    if (begin_ == end_) {
      // reached the end of the column
      block_.seal();
      seek_origin_ = end_;
      payload.value = {};
      doc.value = irs::doc_limits::eof();

      return false;
    }

    try {
      const auto& cached =
        cache_ ? load_block(*column_->ctxs_, column_->decompressor(),
                            column_->encrypted(), *begin_)
               : load_block(*column_->ctxs_, column_->decompressor(),
                            column_->encrypted(), *begin_, cached_block_);

      if (block_ != cached || &cached == &cached_block_) {
        block_.reset(cached, payload);
      }
    } catch (...) {
      // unable to load block, seal the iterator
      block_.seal();
      begin_ = end_;
      payload.value = {};
      doc.value = irs::doc_limits::eof();

      throw;
    }

    seek_origin_ = begin_++;

    return true;
  }

  block_iterator_t block_;
  attributes attrs_;
  const typename column_t::block_ref* begin_;
  const typename column_t::block_ref* seek_origin_;
  const typename column_t::block_ref* end_;
  const column_t* column_;
  block_t cached_block_;
  bool cache_;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                           Columns
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @class sparse_column
////////////////////////////////////////////////////////////////////////////////
template<typename Block>
class sparse_column final : public column {
 public:
  typedef sparse_column column_t;
  typedef Block block_t;

  static column::ptr make(const context_provider& ctxs, field_id id,
                          ColumnProperty props) {
    return std::make_unique<column_t>(ctxs, id, props);
  }

  sparse_column(const context_provider& ctxs, field_id id, ColumnProperty props)
    : column(id, props), ctxs_(&ctxs) {}

  void read(data_input& in, uint64_t* buf,
            compression::decompressor::ptr decomp) final {
    column::read(in, buf, std::move(decomp));  // read common header

    uint32_t blocks_count =
      in.read_vint();  // total number of column index blocks

    std::vector<block_ref> refs(blocks_count + 1);  // +1 for upper bound

    auto begin = refs.begin();
    while (blocks_count >= INDEX_BLOCK_SIZE) {
      encode::avg::visit_block_packed(in, INDEX_BLOCK_SIZE,
                                      reinterpret_cast<uint32_t*>(buf),
                                      [begin](uint32_t key) mutable {
                                        begin->key = key;
                                        ++begin;
                                      });

      encode::avg::visit_block_packed(in, INDEX_BLOCK_SIZE, buf,
                                      [begin](uint64_t offset) mutable {
                                        begin->offset = offset;
                                        ++begin;
                                      });

      begin += INDEX_BLOCK_SIZE;
      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      encode::avg::visit_block_packed_tail(in, blocks_count,
                                           reinterpret_cast<uint32_t*>(buf),
                                           [begin](uint32_t key) mutable {
                                             begin->key = key;
                                             ++begin;
                                           });

      encode::avg::visit_block_packed_tail(in, blocks_count, buf,
                                           [begin](uint64_t offset) mutable {
                                             begin->offset = offset;
                                             ++begin;
                                           });

      begin += blocks_count;
    }

    // upper bound
    if (this->max() < doc_limits::eof()) {
      begin->key = this->max() + 1;
    } else {
      begin->key = doc_limits::eof();
    }
    begin->offset = address_limits::invalid();

    refs_ = std::move(refs);
  }

  irs::doc_iterator::ptr iterator(ColumnHint hint) const final {
    typedef column_iterator<column_t> iterator_t;

    if (empty()) {
      return irs::doc_iterator::empty();
    }

    return memory::make_managed<iterator_t>(
      *this, refs_.data(),
      refs_.data() + refs_.size() - 1,  // -1 for upper bound
      ColumnHint::kConsolidation != (hint & ColumnHint::kConsolidation));
  }

 private:
  friend class column_iterator<column_t>;

  struct block_ref : util::noncopyable {
    typedef typename column_t::block_t block_t;

    block_ref() = default;

    block_ref(block_ref&& other) noexcept
      : key(std::move(other.key)),
        offset(std::move(other.offset)),
        pblock{other.pblock.exchange(
          nullptr)} {  // no std::move(...) for std::atomic<...>
    }

    doc_id_t key;                                // min key in a block
    uint64_t offset;                             // block offset
    mutable std::atomic<const block_t*> pblock;  // pointer to cached block
  };

  typedef std::vector<block_ref> refs_t;

  const block_ref* find_block(const block_ref* begin, const block_ref* end,
                              doc_id_t key) const noexcept {
    IRS_IGNORE(end);

    if (key <= begin->key) {
      return begin;
    }

    const auto rbegin =
      irstd::make_reverse_iterator(refs_.data() + refs_.size());  // upper bound
    const auto rend = irstd::make_reverse_iterator(begin);
    const auto it = std::lower_bound(
      rbegin, rend, key,
      [](const block_ref& lhs, doc_id_t rhs) { return lhs.key > rhs; });

    if (it == rend) {
      return &*rbegin;
    }

    return it.base() - 1;
  }

  typename refs_t::const_iterator find_block(doc_id_t key) const noexcept {
    if (key <= refs_.front().key) {
      return refs_.begin();
    }

    const auto rbegin = refs_.rbegin();  // upper bound
    const auto rend = refs_.rend();
    const auto it = std::lower_bound(
      rbegin, rend, key,
      [](const block_ref& lhs, doc_id_t rhs) { return lhs.key > rhs; });

    if (it == rend) {
      return refs_.end() - 1;  // -1 for upper bound
    }

    return irstd::to_forward(it);
  }

  const context_provider* ctxs_;
  refs_t refs_;  // blocks index
};

////////////////////////////////////////////////////////////////////////////////
/// @class dense_fixed_offset_column
////////////////////////////////////////////////////////////////////////////////
template<typename Block>
class dense_fixed_offset_column final : public column {
 public:
  typedef dense_fixed_offset_column column_t;
  typedef Block block_t;

  static column::ptr make(const context_provider& ctxs, field_id id,
                          ColumnProperty props) {
    return std::make_unique<column_t>(ctxs, id, props);
  }

  dense_fixed_offset_column(const context_provider& ctxs, field_id id,
                            ColumnProperty prop)
    : column(id, prop), ctxs_(&ctxs) {}

  void read(data_input& in, uint64_t* buf,
            compression::decompressor::ptr decomp) final {
    column::read(in, buf, std::move(decomp));  // read common header

    size_t blocks_count =
      in.read_vint();  // total number of column index blocks

    std::vector<block_ref> refs(blocks_count);

    auto begin = refs.begin();
    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl32(in, this->avg_block_count())) {
        throw index_error(
          "Invalid RL encoding in 'dense_fixed_offset_column' (keys)");
      }

      encode::avg::visit_block_packed(in, INDEX_BLOCK_SIZE, buf,
                                      [begin](uint64_t offset) mutable {
                                        begin->offset = offset;
                                        ++begin;
                                      });

      begin += INDEX_BLOCK_SIZE;
      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      const auto avg_block_count = blocks_count > 1
                                     ? this->avg_block_count()
                                     : 0U;  // in this case avg == 0

      if (!encode::avg::check_block_rl32(in, avg_block_count)) {
        throw index_error(
          "Invalid RL encoding in 'dense_fixed_offset_column' (keys)");
      }

      encode::avg::visit_block_packed_tail(in, blocks_count, buf,
                                           [begin](uint64_t offset) mutable {
                                             begin->offset = offset;
                                             ++begin;
                                           });

      begin += blocks_count;
    }

    refs_ = std::move(refs);
    min_ = this->max() - this->count() + 1;
  }

  irs::doc_iterator::ptr iterator(ColumnHint hint) const final {
    typedef column_iterator<column_t> iterator_t;

    if (empty()) {
      return irs::doc_iterator::empty();
    }

    return memory::make_managed<iterator_t>(
      *this, refs_.data(), refs_.data() + refs_.size(),
      ColumnHint::kConsolidation != (hint & ColumnHint::kConsolidation));
  }

 private:
  friend class column_iterator<column_t>;

  struct block_ref {
    typedef typename column_t::block_t block_t;

    block_ref() = default;

    block_ref(block_ref&& other) noexcept
      : offset(std::move(other.offset)),
        pblock{other.pblock.exchange(
          nullptr)} {  // no std::move(...) for std::atomic<...>
    }

    uint64_t offset;  // need to store base offset since blocks may not be
                      // located sequentially
    mutable std::atomic<const block_t*> pblock;
  };

  typedef std::vector<block_ref> refs_t;

  const block_ref* find_block(const block_ref* begin, const block_ref* end,
                              doc_id_t key) const noexcept {
    const auto min =
      min_ + this->avg_block_count() * std::distance(refs_.data(), begin);

    if (key < min) {
      return begin;
    }

    if ((key -= min_) >= this->count()) {
      return end;
    }

    const auto block_idx = key / this->avg_block_count();
    IRS_ASSERT(block_idx < refs_.size());

    return refs_.data() + block_idx;
  }

  typename refs_t::const_iterator find_block(doc_id_t key) const noexcept {
    if (key < min_) {
      return refs_.begin();
    }

    if ((key -= min_) >= this->count()) {
      return refs_.end();
    }

    const auto block_idx = key / this->avg_block_count();
    IRS_ASSERT(block_idx < refs_.size());

    return std::begin(refs_) + block_idx;
  }

  const context_provider* ctxs_;
  refs_t refs_;
  doc_id_t min_{};  // min key
};

template<>
class dense_fixed_offset_column<dense_mask_block> final : public column {
 public:
  typedef dense_fixed_offset_column column_t;

  static column::ptr make(const context_provider&, field_id id,
                          ColumnProperty props) {
    return std::make_unique<column_t>(id, props);
  }

  explicit dense_fixed_offset_column(field_id id, ColumnProperty prop) noexcept
    : column(id, prop) {}

  void read(data_input& in, uint64_t* buf,
            compression::decompressor::ptr decomp) final {
    // we treat data in blocks as "garbage" which could be
    // potentially removed on merge, so we don't validate
    // column properties using such blocks

    column::read(in, buf, std::move(decomp));  // read common header

    uint32_t blocks_count =
      in.read_vint();  // total number of column index blocks

    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl32(in, this->avg_block_count())) {
        throw index_error(
          "Invalid RL encoding in "
          "'dense_fixed_offset_column<dense_mask_block>' (keys)");
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed(in, INDEX_BLOCK_SIZE, buf,
                                      [](uint64_t) {});

      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      const auto avg_block_count = blocks_count > 1
                                     ? this->avg_block_count()
                                     : 0;  // in this case avg == 0

      if (!encode::avg::check_block_rl32(in, avg_block_count)) {
        throw index_error(
          "Invalid RL encoding in "
          "'dense_fixed_offset_column<dense_mask_block>' (keys)");
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed_tail(in, blocks_count, buf,
                                           [](uint64_t) {});
    }

    min_ = this->max() - this->count();
  }

  irs::doc_iterator::ptr iterator(ColumnHint hint) const final;

 private:
  class column_iterator : public irs::doc_iterator {
   private:
    using attributes = std::tuple<document, cost, score>;

   public:
    explicit column_iterator(const column_t& column) noexcept
      : min_(1 + column.min_), max_(column.max()) {
      std::get<cost>(attrs_).reset(column.size());
    }

    attribute* get_mutable(irs::type_info::type_id type) noexcept final {
      return irs::get_mutable(attrs_, type);
    }

    irs::doc_id_t value() const noexcept final {
      return std::get<document>(attrs_).value;
    }

    irs::doc_id_t seek(irs::doc_id_t doc) noexcept final {
      // cppcheck-suppress shadowFunction
      auto& value = std::get<document>(attrs_);

      if (doc < min_) {
        if (!doc_limits::valid(value.value)) {
          next();
        }

        return this->value();
      }

      min_ = doc;
      next();

      return value.value;
    }

    bool next() noexcept final {
      // cppcheck-suppress shadowFunction
      auto& value = std::get<document>(attrs_);

      if (min_ > max_) {
        value.value = doc_limits::eof();

        return false;
      }

      value.value = min_++;

      return true;
    }

   private:
    attributes attrs_;
    doc_id_t min_{doc_limits::invalid()};
    doc_id_t max_{doc_limits::invalid()};
  };

  doc_id_t min_{};  // min key (less than any key in column)
};

irs::doc_iterator::ptr dense_fixed_offset_column<dense_mask_block>::iterator(
  ColumnHint) const {
  return empty() ? irs::doc_iterator::empty()
                 : memory::make_managed<column_iterator>(*this);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                 column factories
// ----------------------------------------------------------------------------

typedef std::function<column::ptr(const context_provider& ctxs, field_id id,
                                  ColumnProperty prop)>
  column_factory_f;
//     Column      |          Blocks
const column_factory_f COLUMN_FACTORIES[]{
  // CP_COLUMN_DENSE | CP_MASK CP_FIXED CP_DENSE
  &sparse_column<sparse_block>::make,  //       0         |    0        0 0
  &sparse_column<dense_block>::make,   //       0         |    0        0 1
  &sparse_column<sparse_block>::make,  //       0         |    0        1 0
  &sparse_column<dense_fixed_offset_block>::make,  //       0         |    0 1 1
  nullptr,
  /* invalid properties, should never happen */  //       0         |    1 0 0
  nullptr,
  /* invalid properties, should never happen */  //       0         |    1 0 1
  &sparse_column<sparse_mask_block>::make,  //       0         |    1        1 0
  &sparse_column<dense_mask_block>::make,   //       0         |    1        1 1

  &sparse_column<sparse_block>::make,  //       1         |    0        0 0
  &sparse_column<dense_block>::make,   //       1         |    0        0 1
  &sparse_column<sparse_block>::make,  //       1         |    0        1 0
  &dense_fixed_offset_column<dense_fixed_offset_block>::make,  //       1 |    0
                                                               //       1 1
  nullptr,
  /* invalid properties, should never happen */  //       1         |    1 0 0
  nullptr,
  /* invalid properties, should never happen */  //       1         |    1 0 1
  &sparse_column<sparse_mask_block>::make,  //       1         |    1        1 0
  &dense_fixed_offset_column<dense_mask_block>::make  //       1         |    1
                                                      //       1        1
};

//////////////////////////////////////////////////////////////////////////////
/// @class reader
//////////////////////////////////////////////////////////////////////////////
class reader final : public columnstore_reader, public context_provider {
 public:
  explicit reader(size_t pool_size = 16) : context_provider(pool_size) {}

  uint64_t CountMappedMemory() const final {
    // We don't support it for old columnstore
    return 0;
  }

  bool prepare(const directory& dir, const SegmentMeta& meta,
               const options& opts = options{}) final;

  const column_reader* column(field_id field) const final;

  bool visit(const column_visitor_f& visitor) const final;

  size_t size() const noexcept final { return columns_.size(); }

 private:
  static bool read_meta(const directory& dir, const SegmentMeta& meta,
                        std::vector<column::ptr>& columns,
                        std::vector<const class column*>& sorted_columns);

  std::vector<column::ptr> columns_;
  std::vector<const class column*> sorted_columns_;
};

bool reader::read_meta(const directory& dir, const SegmentMeta& meta,
                       std::vector<column::ptr>& columns,
                       std::vector<const class column*>& sorted_columns) {
  size_t count{};
  irs::field_id max_id{};
  meta_reader reader;

  if (!reader.prepare(dir, meta, count, max_id)) {
    // no column meta found in a segment
    return false;
  }

  sorted_columns.reserve(columns.size());
  for (column_meta col_meta; reader.read(col_meta);) {
    if (col_meta.id >= columns.size()) {
      throw index_error{absl::StrCat("Invalid column '", col_meta.name,
                                     "' id in segment '", meta.name, "'")};
    }

    IRS_ASSERT(col_meta.id == columns[col_meta.id]->id());
    auto& column = columns[col_meta.id];
    column->set_name(std::move(col_meta.name));
    sorted_columns.emplace_back(column.get());
  }

  for (auto& column : columns) {
    const auto name = column->name();

    if (IsNull(name)) {
      sorted_columns.emplace_back(column.get());
    }
  }

  // column meta exists
  return true;
}

bool reader::prepare(const directory& dir, const SegmentMeta& meta,
                     const columnstore_reader::options&) {
  const auto filename = file_name(meta.name, writer::FORMAT_EXT);
  bool exists;

  if (!dir.exists(exists, filename)) {
    throw io_error{
      absl::StrCat("Failed to check existence of file, path: ", filename)};
  }

  if (!exists) {
    // possible that the file does not exist since columnstore is optional
    return false;
  }

  // open columstore stream
  auto stream = dir.open(filename, irs::IOAdvice::RANDOM);

  if (!stream) {
    throw io_error{absl::StrCat("Failed to open file, path: ", filename)};
  }

  // check header
  const auto version = format_utils::check_header(
    *stream, writer::FORMAT_NAME, writer::FORMAT_MIN, writer::FORMAT_MAX);

  encryption::stream::ptr cipher;

  if (version > writer::FORMAT_MIN) {
    auto* enc = dir.attributes().encryption();

    if (irs::decrypt(filename, *stream, enc, cipher)) {
      IRS_ASSERT(cipher && cipher->block_size());
    }
  }

  // since columns data are too large
  // it is too costly to verify checksum of
  // the entire file. here we perform cheap
  // error detection which could recognize
  // some forms of corruption
  format_utils::read_checksum(*stream);

  // seek to data start
  stream->seek(stream->length() - format_utils::kFooterLen - sizeof(uint64_t));
  stream->seek(stream->read_long());  // seek to blocks index

  const size_t count = stream->read_vlong();

  if (count >= field_limits::invalid()) {
    throw index_error{
      absl::StrCat("Too many columns in the columnstore (", count, ")")};
  }

  uint64_t buf[INDEX_BLOCK_SIZE];  // temporary buffer for bit packing
  std::vector<column::ptr> columns;
  columns.reserve(count);
  for (field_id i = 0, size = static_cast<field_id>(count); i < size; ++i) {
    // read column properties
    const auto props = read_enum<ColumnProperty>(*stream);
    const auto factory_id = (props & (~CP_COLUMN_ENCRYPT));

    if (factory_id >= std::size(COLUMN_FACTORIES)) {
      throw index_error{absl::StrCat(
        "Failed to load column id=", i,
        ", got invalid properties=", static_cast<uint32_t>(props))};
    }

    // create column
    const auto& factory = COLUMN_FACTORIES[factory_id];

    if (!factory) {
      static_assert(
        std::is_same_v<std::underlying_type_t<ColumnProperty>, uint32_t>);

      throw index_error{
        absl::StrCat("Failed to open column id=", static_cast<uint32_t>(props),
                     ", properties=", i)};
    }

    column::ptr column = factory(*this, i, props);

    if (!column) {
      throw index_error{absl::StrCat("Factory failed to create column id=", i)};
    }

    compression::decompressor::ptr decomp;

    if (version > writer::FORMAT_MIN) {
      const auto compression_id = read_string<std::string>(*stream);
      decomp = compression::get_decompressor(compression_id);

      if (!decomp && !compression::exists(compression_id)) {
        throw index_error{absl::StrCat("Failed to load compression '",
                                       compression_id, "' for column id=", i)};
      }

      if (decomp && !decomp->prepare(*stream)) {
        throw index_error{absl::StrCat("Failed to prepare compression '",
                                       compression_id, "' for column id=", i)};
      }
    } else {
      // we don't support encryption and custom
      // compression for 'FORMAT_MIN' version
      decomp =
        compression::get_decompressor(irs::type<compression::lz4>::get());
      IRS_ASSERT(decomp);
    }

    try {
      column->read(*stream, buf, std::move(decomp));
    } catch (...) {
      IRS_LOG_ERROR(absl::StrCat("Failed to load column id=", i));

      throw;
    }

    // noexcept since space has been already reserved
    columns.emplace_back(std::move(column));
  }

  decltype(sorted_columns_) sorted_columns;
  read_meta(dir, meta, columns, sorted_columns);

  // noexcept
  context_provider::prepare(std::move(stream), std::move(cipher));
  columns_ = std::move(columns);
  sorted_columns_ = std::move(sorted_columns);

  return true;
}

bool reader::visit(const column_visitor_f& visitor) const {
  for (auto* column : sorted_columns_) {
    IRS_ASSERT(column);
    if (!visitor(*column)) {
      return false;
    }
  }
  return true;
}

const irs::column_reader* reader::column(field_id field) const {
  return field >= columns_.size()
           ? nullptr  // can't find column with the specified identifier
           : columns_[field].get();
}

irs::columnstore_writer::ptr make_writer(Version version,
                                         ColumnMetaVersion meta_version) {
  return std::make_unique<columnstore::writer>(version, meta_version);
}

irs::columnstore_reader::ptr make_reader() {
  return std::make_unique<columnstore::reader>();
}

}  // namespace columnstore
}  // namespace irs
