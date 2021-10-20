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

#include "columnstore2.hpp"

#include "error/error.hpp"
#include "formats/format_utils.hpp"
#include "index/file_names.hpp"
#include "search/all_iterator.hpp"
#include "search/score.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/string_utils.hpp"

namespace {

using namespace irs;
using namespace irs::columnstore2;

using column_ptr = std::unique_ptr<columnstore_reader::column_reader>;
using column_index = std::vector<sparse_bitmap_writer::block>;

std::string data_file_name(string_ref prefix) {
  return file_name(prefix, writer::DATA_FORMAT_EXT);
}

std::string index_file_name(string_ref prefix) {
  return file_name(prefix, writer::INDEX_FORMAT_EXT);
}

void write_header(index_output& out, const column_header& hdr) {
  write_enum(out, hdr.type);
  write_enum(out, hdr.props);
  out.write_int(hdr.min);
  out.write_int(hdr.docs_count);
  out.write_long(hdr.docs_index);
}

column_header read_header(index_input& in) {
  column_header hdr;
  hdr.type = read_enum<ColumnType>(in);
  hdr.props = read_enum<ColumnProperty>(in);
  hdr.min = in.read_int();
  hdr.docs_count = in.read_int();
  hdr.docs_index = in.read_long();
  return hdr;
}

bool is_encrypted(const column_header& hdr) noexcept {
  return ColumnProperty::ENCRYPT == (hdr.props & ColumnProperty::ENCRYPT);
}

void write_bitmap_index(index_output& out, const column_index& blocks) {
  const uint32_t count = static_cast<uint32_t>(blocks.size());

  if (count > 2) {
    out.write_int(count);
    for (auto& block : blocks) {
      out.write_int(block.index);
      out.write_int(block.offset);
    }
  } else {
    out.write_int(0);
  }
}

column_index read_bitmap_index(index_input& in) {
  const uint32_t count = in.read_int();

  if (count > std::numeric_limits<uint16_t>::max()) {
    throw index_error("Invalid number of blocks in column index");
  }

  if (count > 2) {
    column_index blocks(count);

    in.read_bytes(
      reinterpret_cast<byte_type*>(blocks.data()),
      count*sizeof(sparse_bitmap_writer::block));

    if constexpr (!is_big_endian()) {
      for (auto& block : blocks) {
        block.index = numeric_utils::ntoh32(block.index);
        block.offset = numeric_utils::ntoh32(block.offset);
      }
    }

    return blocks;
  }

  return {};
}

void write_blocks_sparse(
    index_output& out,
    const std::vector<column::column_block>& blocks) {
  // FIXME optimize
  for (auto& block : blocks) {
    out.write_long(block.addr);
    out.write_long(block.avg);
    out.write_byte(static_cast<byte_type>(block.bits));
    out.write_long(block.data);
    out.write_long(block.last_size);
  }
}

void write_blocks_dense(
    index_output& out,
    const std::vector<column::column_block>& blocks) {
  // FIXME optimize
  for (auto& block : blocks) {
    out.write_long(block.data);
  }
}

std::vector<uint64_t> read_blocks_dense(
    const column_header& hdr,
    index_input& in) {
  std::vector<uint64_t> blocks(
    math::div_ceil32(hdr.docs_count, column::BLOCK_SIZE));

  in.read_bytes(
    reinterpret_cast<byte_type*>(blocks.data()),
    sizeof(uint64_t)*blocks.size());

  if constexpr (!is_big_endian()) {
    // FIXME simd?
    for (auto& block : blocks) {
      block = numeric_utils::ntoh64(block);
    }
  }

  return blocks;
}

////////////////////////////////////////////////////////////////////////////////
/// @class range_column_iterator
/// @brief iterates over a specified contiguous range of documents
////////////////////////////////////////////////////////////////////////////////
template<typename PayloadReader>
class range_column_iterator final
    : public resettable_doc_iterator,
      private PayloadReader {
 private:
  using payload_reader = PayloadReader;

  using attributes = std::tuple<document, cost, score, irs::payload>;

 public:
  template<typename... Args>
  range_column_iterator(const column_header& header, Args&&... args)
    : payload_reader{std::forward<Args>(args)...},
      min_base_{header.min},
      min_doc_{min_base_},
      max_doc_{min_base_ + header.docs_count - 1} {
    assert(min_doc_ <= max_doc_);
    assert(!doc_limits::eof(max_doc_));
    std::get<irs::cost>(attrs_).reset(header.docs_count);
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::get_mutable(attrs_, type);
  }

  virtual doc_id_t value() const noexcept override {
    return std::get<document>(attrs_).value;
  }

  virtual doc_id_t seek(irs::doc_id_t doc) override {
    if (IRS_LIKELY(min_doc_ <= doc && doc <= max_doc_)) {
      std::get<document>(attrs_).value = doc;
      min_doc_ = doc + 1;
      std::get<irs::payload>(attrs_).value = this->payload(doc - min_base_);
      return doc;
    }

    if (!doc_limits::valid(value())) {
      std::get<document>(attrs_).value = min_doc_++;
      std::get<irs::payload>(attrs_).value = this->payload(value() - min_base_);
      return value();
    }

    if (value() < doc) {
      max_doc_ = doc_limits::invalid();
      min_doc_ = doc_limits::eof();
      std::get<document>(attrs_).value = doc_limits::eof();
      std::get<irs::payload>(attrs_).value = bytes_ref::NIL;
      return doc_limits::eof();
    }

    return value();
  }

  virtual bool next() override {
    if (min_doc_ <= max_doc_) {
      std::get<document>(attrs_).value = min_doc_++;
      std::get<irs::payload>(attrs_).value = this->payload(value() - min_base_);
      return true;
    }

    std::get<document>(attrs_).value = doc_limits::eof();
    std::get<irs::payload>(attrs_).value = bytes_ref::NIL;
    return false;
  }

  virtual void reset() noexcept override {
    min_doc_ = min_base_;
    max_doc_ = min_doc_ + std::get<irs::cost>(attrs_).estimate() - 1;
    std::get<document>(attrs_).value = irs::doc_limits::invalid();
  }

 private:
  doc_id_t min_base_;
  doc_id_t min_doc_;
  doc_id_t max_doc_;
  attributes attrs_;
}; // range_column_iterator

////////////////////////////////////////////////////////////////////////////////
/// @class bitmap_column_iterator
/// @brief iterates over a specified bitmap of documents
////////////////////////////////////////////////////////////////////////////////
template<typename PayloadReader>
class bitmap_column_iterator final
    : public resettable_doc_iterator,
      private PayloadReader {
 private:
  using payload_reader = PayloadReader;

  using attributes = std::tuple<
    attribute_ptr<document>,
    cost,
    attribute_ptr<score>,
    irs::payload>;

 public:
  template<typename... Args>
  bitmap_column_iterator(
      index_input::ptr&& bitmap_in,
      const sparse_bitmap_iterator::options& opts,
      cost::cost_t cost,
      Args&&... args)
    : payload_reader{std::forward<Args>(args)...},
      bitmap_{std::move(bitmap_in), opts, cost} {
    std::get<irs::cost>(attrs_).reset(cost);
    std::get<attribute_ptr<document>>(attrs_) = irs::get_mutable<document>(&bitmap_);
    std::get<attribute_ptr<score>>(attrs_) = irs::get_mutable<score>(&bitmap_);
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::get_mutable(attrs_, type);
  }

  virtual doc_id_t value() const noexcept override {
    return std::get<attribute_ptr<document>>(attrs_).ptr->value;
  }

  virtual doc_id_t seek(irs::doc_id_t doc) override {
    assert(doc_limits::valid(doc) || doc_limits::valid(value()));
    doc = bitmap_.seek(doc);

    if (!doc_limits::eof(doc)) {
      std::get<irs::payload>(attrs_).value = this->payload(bitmap_.index());
      return doc;
    }

    std::get<irs::payload>(attrs_).value = bytes_ref::NIL;
    return doc_limits::eof();
  }

  virtual bool next() override {
    if (bitmap_.next()) {
      std::get<irs::payload>(attrs_).value = this->payload(bitmap_.index());
      return true;
    }

    std::get<irs::payload>(attrs_).value = bytes_ref::NIL;
    return false;
  }

  virtual void reset() override {
    bitmap_.reset();
  }

 private:
  sparse_bitmap_iterator bitmap_;
  attributes attrs_;
}; // bitmap_column_iterator

////////////////////////////////////////////////////////////////////////////////
/// @struct column_base
////////////////////////////////////////////////////////////////////////////////
class column_base : public columnstore_reader::column_reader,
                    private util::noncopyable {
 public:
  column_base(
      const column_header& hdr,
      column_index&& index,
      const index_input& stream,
      encryption::stream* cipher)
    : stream_{&stream},
      cipher_{cipher},
      hdr_{hdr},
      index_{std::move(index)},
      opts_{
        index_.empty()
          ? sparse_bitmap_iterator::block_index_t{}
          : sparse_bitmap_iterator::block_index_t{ index_.data(), index_.size() },  true } {
    assert(!is_encrypted(hdr) || cipher_);
  }

  virtual columnstore_reader::values_reader_f values() const override;

  virtual bool visit(const columnstore_reader::values_visitor_f& visitor) const override;

  virtual doc_id_t size() const noexcept final {
    return hdr_.docs_count;
  }

  const column_header& header() const noexcept {
    return hdr_;
  }

  sparse_bitmap_iterator::options bitmap_iterator_options() const noexcept {
    return opts_;
  }

 protected:
  template<typename Factory>
  doc_iterator::ptr make_iterator(Factory&& f) const;

  const index_input& stream() const noexcept {
    assert(stream_);
    return *stream_;
  }

 private:
  template<typename ValueReader>
  doc_iterator::ptr make_iterator(ValueReader&& f, index_input::ptr&& in) const;

  const index_input* stream_;
  encryption::stream* cipher_;
  column_header hdr_;
  column_index index_;
  sparse_bitmap_iterator::options opts_;
}; // column_base

columnstore_reader::values_reader_f column_base::values() const {
  auto moved_it = make_move_on_copy(this->iterator());
  auto* document = irs::get<irs::document>(*moved_it.value());
  if (!document || doc_limits::eof(document->value)) {
    return columnstore_reader::empty_reader();
  }
  const bytes_ref* payload_value = &bytes_ref::NIL;
  auto* payload = irs::get<irs::payload>(*moved_it.value());
  if (payload) {
    payload_value = &payload->value;
  }

  return [it = moved_it, payload_value, document](doc_id_t doc, bytes_ref& value) {
    if (doc > doc_limits::invalid() && doc < doc_limits::eof()) {
      if (doc < document->value) {
#ifdef IRESEARCH_DEBUG
        auto& impl = dynamic_cast<resettable_doc_iterator&>(*it.value());
#else
        auto& impl = static_cast<resettable_doc_iterator&>(*it.value());
#endif
        impl.reset();
      }

      if (doc == it.value()->seek(doc)) {
        value = *payload_value;
        return true;
      }
    }

    return false;
  };
}

bool column_base::visit(const columnstore_reader::values_visitor_f& visitor) const {
  auto it = this->iterator();

  payload dummy;
  auto* doc = irs::get<document>(*it);
  if (!doc) {
    return false;
  }
  auto* payload = irs::get<irs::payload>(*it);
  if (!payload) {
    payload = &dummy;
  }

  while (it->next()) {
    if (!visitor(doc->value, payload->value)) {
      return false;
    }
  }

  return true;
}

template<typename ValueReader>
doc_iterator::ptr column_base::make_iterator(
    ValueReader&& rdr,
    index_input::ptr&& index_in) const {
  if (!index_in) {
    using iterator_type = range_column_iterator<ValueReader>;

    return memory::make_managed<iterator_type>(header(), std::move(rdr));
  } else {
    index_in->seek(header().docs_index);

    using iterator_type = bitmap_column_iterator<ValueReader>;

    return memory::make_managed<iterator_type>(
      std::move(index_in), bitmap_iterator_options(),
      header().docs_count, std::move(rdr));
  }
}

template<typename Factory>
doc_iterator::ptr column_base::make_iterator(Factory&& f) const {
  assert(header().docs_count);

  index_input::ptr value_in = stream().reopen();

  if (!value_in) {
    // implementation returned wrong pointer
    IR_FRMT_ERROR("Failed to reopen input in: %s", __FUNCTION__);

    throw io_error{"failed to reopen input"};
  }

  index_input::ptr index_in = nullptr;

  if (0 != header().docs_index) {
    index_in = value_in->dup();

    if (!index_in) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to duplicate input in: %s", __FUNCTION__);

      throw io_error{"failed to duplicate input"};
    }
  }

  if (is_encrypted(header())) {
    assert(cipher_);
    return make_iterator(f(std::move(value_in), *cipher_), std::move(index_in));
  } else {
    const byte_type* data = value_in->read_buffer(
      0, value_in->length(),
      BufferHint::PERSISTENT);

    if (data) {
      // direct buffer access
      return make_iterator(f(data), std::move(index_in));
    }

    return make_iterator(f(std::move(value_in)), std::move(index_in));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @class value_direct_reader
////////////////////////////////////////////////////////////////////////////////
class value_direct_reader {
 protected:
  explicit value_direct_reader(const byte_type* data) noexcept
    : data_{data} {
    assert(data);
  }

  bytes_ref value(uint64_t offset, size_t length) noexcept {
    return { data_ + offset, length };
  }

  const byte_type* data_;
}; // value_direct_reader

////////////////////////////////////////////////////////////////////////////////
/// @class value_reader
////////////////////////////////////////////////////////////////////////////////
template<bool Resize>
class value_reader {
 protected:
  value_reader(index_input::ptr data_in, size_t size)
    : buf_(size, 0),
      data_in_{std::move(data_in)} {
  }

  bytes_ref value(uint64_t offset, size_t length) {
    if constexpr (Resize) {
      buf_.resize(length);
    }

    auto* buf = buf_.data();

    [[maybe_unused]] const size_t read = data_in_->read_bytes(offset, buf, length);
    assert(read == length);

    return { buf, length };
  }

  bstring buf_;
  index_input::ptr data_in_;
}; // value_reader

////////////////////////////////////////////////////////////////////////////////
/// @class encrypted_value_reader
////////////////////////////////////////////////////////////////////////////////
template<bool Resize>
class encrypted_value_reader {
 protected:
  encrypted_value_reader(index_input::ptr&& data_in, encryption::stream* cipher, size_t size)
    : buf_(size, 0),
      data_in_{std::move(data_in)},
      cipher_{cipher} {
  }

  bytes_ref value(uint64_t offset, size_t length) {
    if constexpr (Resize) {
      buf_.resize(length);
    }

    auto* buf = buf_.data();

    [[maybe_unused]] const size_t read = data_in_->read_bytes(offset, buf, length);
    assert(read == length);

    [[maybe_unused]] const bool ok = cipher_->decrypt(offset, buf, length);
    assert(ok);

    return { buf, length };
  }

  bstring buf_;
  index_input::ptr data_in_;
  encryption::stream* cipher_;
}; // encrypted_value_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct mask_column
////////////////////////////////////////////////////////////////////////////////
struct mask_column final : public column_base {
  struct payload_reader {
    bytes_ref payload(doc_id_t) noexcept {
      return bytes_ref::NIL;
    }
  }; // payload_reader

  static column_ptr read(
      const column_header& hdr,
      column_index&& index,
      index_input& /*index_in*/,
      const index_input& data_in,
      compression::decompressor::ptr&& /*inflater*/,
      encryption::stream* cipher) {
    return memory::make_unique<mask_column>(hdr, std::move(index), data_in, cipher);
  }

  mask_column(
      const column_header& hdr,
      column_index&& index,
      const index_input& data_in,
      encryption::stream* cipher)
    : column_base{hdr, std::move(index), data_in, cipher} {
    assert(ColumnType::MASK == header().type);
  }

  virtual doc_iterator::ptr iterator() const override;
}; // mask_column

doc_iterator::ptr mask_column::iterator() const {
  if (0 == header().docs_count) {
    // only mask column can be empty
    return doc_iterator::empty();
  }

  if (0 == header().docs_index) {
    return memory::make_managed<range_column_iterator<payload_reader>>(header());
  }

  auto dup = stream().reopen();

  if (!dup) {
    // implementation returned wrong pointer
    IR_FRMT_ERROR("Failed to reopen input in: %s", __FUNCTION__);

    throw io_error{"failed to reopen input"};
  }

  dup->seek(header().docs_index);

  return memory::make_managed<sparse_bitmap_iterator>(
    std::move(dup),
    bitmap_iterator_options(),
    header().docs_count);
}

////////////////////////////////////////////////////////////////////////////////
/// @class dense_fixed_length_column
////////////////////////////////////////////////////////////////////////////////
class dense_fixed_length_column final : public column_base {
 public:
  static column_ptr read(
    const column_header& hdr,
    column_index&& index,
    index_input& index_in,
    const index_input& data_in,
    compression::decompressor::ptr&& inflater,
    encryption::stream* cipher);

  dense_fixed_length_column(
      const column_header& hdr,
      column_index&& index,
      const index_input& data_in,
      compression::decompressor::ptr&& inflater,
      encryption::stream* cipher,
      uint64_t data,
      uint64_t len)
    : column_base{hdr, std::move(index), data_in, cipher},
      inflater_{std::move(inflater)},
      data_{data},
      len_{len} {
    assert(header().docs_count);
    assert(ColumnType::DENSE_FIXED == header().type);
  }

  virtual doc_iterator::ptr iterator() const override;

 private:
  template<typename ValueReader>
  class payload_reader : private ValueReader {
   public:
    template<typename... Args>
    payload_reader(
        uint64_t len, uint64_t data,
        Args&&... args)
      : ValueReader{std::forward<Args>(args)...},
        data_{data},
        len_{len} {
    }

    bytes_ref payload(doc_id_t i) {
      const auto offset = data_ + len_*i;

      return ValueReader::value(offset, len_);
    }

   private:
    uint64_t data_; // where data starts
    uint64_t len_;  // data entry length
  }; // payload_reader

  compression::decompressor::ptr inflater_;
  uint64_t data_;
  uint64_t len_;
}; // dense_fixed_length_column

/*static*/ column_ptr dense_fixed_length_column::read(
    const column_header& hdr,
    column_index&& index,
    index_input& index_in,
    const index_input& data_in,
    compression::decompressor::ptr&& inflater,
    encryption::stream* cipher) {
  const uint64_t data = index_in.read_long();
  const uint64_t len = index_in.read_long();

  return memory::make_unique<dense_fixed_length_column>(
    hdr, std::move(index), data_in,
    std::move(inflater), cipher,
    data, len);
}

doc_iterator::ptr dense_fixed_length_column::iterator() const {
  struct factory {
    payload_reader<encrypted_value_reader<false>> operator()(
        index_input::ptr&& stream, encryption::stream& cipher) const {
      return {ctx->data_, ctx->len_, std::move(stream), &cipher, ctx->data_};
    };

    payload_reader<value_reader<false>> operator()(index_input::ptr&& stream) const {
      return {ctx->data_, ctx->len_, std::move(stream), ctx->data_};
    }

    payload_reader<value_direct_reader> operator()(const byte_type* data) const {
      return {ctx->data_, ctx->len_, data};
    }

    const dense_fixed_length_column* ctx;
  };

  return make_iterator(factory{this});
}

////////////////////////////////////////////////////////////////////////////////
/// @struct fixed_length_column
////////////////////////////////////////////////////////////////////////////////
class fixed_length_column final : public column_base {
 public:
  static column_ptr read(
      const column_header& hdr,
      column_index&& index,
      index_input& index_in,
      const index_input& data_in,
      compression::decompressor::ptr&& inflater,
      encryption::stream* cipher) {
    const uint64_t len = index_in.read_long();
    auto blocks = read_blocks_dense(hdr, index_in);

    return memory::make_unique<fixed_length_column>(
      hdr, std::move(index), data_in,
      std::move(inflater), cipher,
      std::move(blocks), len);
  }

  fixed_length_column(
      const column_header& hdr,
      column_index&& index,
      const index_input& data_in,
      compression::decompressor::ptr&& inflater,
      encryption::stream* cipher,
      std::vector<uint64_t>&& blocks,
      uint64_t len)
    : column_base{hdr, std::move(index), data_in, cipher},
      blocks_{blocks},
      inflater_{std::move(inflater)},
      len_{len} {
    assert(header().docs_count);
    assert(ColumnType::FIXED == header().type);
  }

  virtual doc_iterator::ptr iterator() const override;

 private:
  using column_block = uint64_t;

  template<typename ValueReader>
  class payload_reader : private ValueReader {
   public:
    template<typename... Args>
    payload_reader(
        const column_block* blocks,
        uint64_t len,
        Args&&... args)
      : ValueReader{ std::forward<Args>(args)... },
        blocks_{blocks},
        len_{len} {
    }

    bytes_ref payload(doc_id_t i) {
      const auto block_idx = i / column::BLOCK_SIZE;
      const auto value_idx = i % column::BLOCK_SIZE;

      const auto offset = blocks_[block_idx] + len_*value_idx;

      return ValueReader::value(offset, len_);
    }

   private:
    const column_block* blocks_;
    uint64_t len_;
  }; // payload_reader

  std::vector<uint64_t> blocks_;
  compression::decompressor::ptr inflater_;
  uint64_t len_;
}; // fixed_length_column

doc_iterator::ptr fixed_length_column::iterator() const {
  struct factory {
    payload_reader<encrypted_value_reader<false>> operator()(
        index_input::ptr&& stream,
        encryption::stream& cipher) const {
      return {ctx->blocks_.data(), ctx->len_, std::move(stream), &cipher, ctx->len_};
    };

    payload_reader<value_reader<false>> operator()(index_input::ptr&& stream) const {
      return {ctx->blocks_.data(), ctx->len_, std::move(stream), ctx->len_};
    }

    payload_reader<value_direct_reader> operator()(const byte_type* data) const {
      return {ctx->blocks_.data(), ctx->len_, data};
    }

    const fixed_length_column* ctx;
  };

  return make_iterator(factory{this});
}

////////////////////////////////////////////////////////////////////////////////
/// @struct sparse_column
////////////////////////////////////////////////////////////////////////////////
class sparse_column final : public column_base {
 public:
  struct column_block : column::column_block {
    doc_id_t last;
  };

  static column_ptr read(
      const column_header& hdr,
      column_index&& index,
      index_input& index_in,
      const index_input& data_in,
      compression::decompressor::ptr&& inflater,
      encryption::stream* cipher) {
    auto blocks = read_blocks_sparse(hdr, index_in);

    return memory::make_unique<sparse_column>(
      hdr, std::move(index), data_in,
      std::move(inflater), cipher, std::move(blocks));
  }

  sparse_column(
      const column_header& hdr,
      column_index&& index,
      const index_input& data_in,
      compression::decompressor::ptr&& inflater,
      encryption::stream* cipher,
      std::vector<column_block>&& blocks)
    : column_base{hdr, std::move(index), data_in, cipher},
      blocks_{std::move(blocks)},
      inflater_{std::move(inflater)} {
    assert(header().docs_count);
    assert(ColumnType::SPARSE == header().type);
  }

  virtual doc_iterator::ptr iterator() const override;

 private:
  template<typename ValueReader>
  class payload_reader : private ValueReader {
   public:
    template<typename... Args>
    payload_reader(
        const column_block* blocks,
        Args&&... args)
      : ValueReader{ std::forward<Args>(args)... },
        blocks_{blocks} {
    }

    bytes_ref payload(doc_id_t i);

   private:
    const column_block* blocks_;
  }; // payload_reader

  static std::vector<column_block> read_blocks_sparse(
    const column_header& hder, index_input& in);

  std::vector<column_block> blocks_;
  compression::decompressor::ptr inflater_;
}; // sparse_column

template<typename ValueReader>
bytes_ref sparse_column::payload_reader<ValueReader>::payload(doc_id_t i) {
  const auto& block = blocks_[i / column::BLOCK_SIZE];
  const size_t index = i % column::BLOCK_SIZE;

  if (bitpack::ALL_EQUAL == block.bits) {
    const size_t addr = block.data + block.avg*index;

    size_t length = block.avg;
    if (IRS_UNLIKELY(block.last == index)) {
      length = block.last_size;
    }

    return ValueReader::value(addr, length);
  }

  const size_t block_size = block.bits*sizeof(uint64_t);
  const size_t block_index = index / packed::BLOCK_SIZE_64;
  size_t value_index = index % packed::BLOCK_SIZE_64;

  const size_t addr_offset = block.addr + block_index*block_size;

  const byte_type* addr_buf;
  if constexpr (std::is_same_v<ValueReader, value_direct_reader>) {
    addr_buf = this->data_ + addr_offset;
  } else {
    this->buf_.resize(block_size);
    this->data_in_->read_bytes(addr_offset, this->buf_.data(), block_size);
    addr_buf = this->buf_.c_str();
  }
  const uint64_t start_delta = zig_zag_decode64(
    packed::fastpack_at(reinterpret_cast<const uint64_t*>(addr_buf),
                        value_index, block.bits));
  const uint64_t start = block.avg*index + start_delta;

  size_t length = block.last_size;
  if (IRS_LIKELY(block.last != index)) {
    if (IRS_UNLIKELY(++value_index == 64)) {
      value_index = 0;

      if constexpr (std::is_same_v<ValueReader, value_direct_reader>) {
        addr_buf += block_size;
      } else {
        this->buf_.resize(block_size);
        this->data_in_->read_bytes(this->buf_.data(), block_size);
        addr_buf = this->buf_.c_str();
      }
    }

    const uint64_t end_delta = zig_zag_decode64(
      packed::fastpack_at(reinterpret_cast<const uint64_t*>(addr_buf),
                          value_index, block.bits));
    length = end_delta - start_delta + block.avg;
  }

  const auto offset = block.data + start;

  return ValueReader::value(offset, length);
}

/*static*/ std::vector<sparse_column::column_block> sparse_column::read_blocks_sparse(
    const column_header& hdr, index_input& in) {
  std::vector<sparse_column::column_block> blocks{
    math::div_ceil32(hdr.docs_count, column::BLOCK_SIZE)};

  // FIXME optimize
  for (auto& block : blocks) {
    block.addr = in.read_long();
    block.avg = in.read_long();
    block.bits = in.read_byte();
    block.data = in.read_long();
    block.last_size = in.read_long();
    block.last = column::BLOCK_SIZE - 1;
  }
  blocks.back().last = (hdr.docs_count % column::BLOCK_SIZE - 1);

  return blocks;
}

doc_iterator::ptr sparse_column::iterator() const {
  struct factory {
    payload_reader<encrypted_value_reader<true>> operator()(
        index_input::ptr&& stream,
        encryption::stream& cipher) const {
      return {ctx->blocks_.data(), std::move(stream), &cipher, size_t{0}};
    };

    payload_reader<value_reader<true>> operator()(index_input::ptr&& stream) const {
      return {ctx->blocks_.data(), std::move(stream), size_t{0}};
    }

    payload_reader<value_direct_reader> operator()(const byte_type* data) const {
      return {ctx->blocks_.data(), data};
    }

    const sparse_column* ctx;
  };

  return make_iterator(factory{this});
}

using column_factory_f = column_ptr(*)(
  const column_header&, column_index&&, index_input&,
  const index_input&, compression::decompressor::ptr&&,
  encryption::stream*);

constexpr column_factory_f FACTORIES[] {
  &sparse_column::read,
  &mask_column::read,
  &fixed_length_column::read,
  &dense_fixed_length_column::read };

}

namespace iresearch {
namespace columnstore2 {

// -----------------------------------------------------------------------------
// --SECTION--                                             column implementation
// -----------------------------------------------------------------------------

void column::flush_block() {
  assert(!addr_table_.empty());
  assert(ctx_.data_out);
  data_.stream.flush();

  auto& data_out = *ctx_.data_out;
  auto& block = blocks_.emplace_back();

  block.addr = data_out.file_pointer();
  block.last_size = data_.file.length() - addr_table_.back();

  const uint32_t docs_count = addr_table_.size();
  const uint64_t addr_table_size = math::ceil64(docs_count, packed::BLOCK_SIZE_64);
  auto* begin = addr_table_.begin();
  auto* end = begin + addr_table_size;

  bool all_equal = !data_.file.length();
  if (!all_equal) {
    std::tie(block.data, block.avg, all_equal)
      = encode::avg::encode(begin, addr_table_.current());
  } else {
    block.avg = 0;
    block.data = data_out.file_pointer();
  }

  if (all_equal) {
    assert(simd::all_equal<false>(begin, addr_table_size) && (0 == *begin));
    block.bits = bitpack::ALL_EQUAL;

    // column is fixed length IFF
    // * it is still a fixed length column
    // * values in a block are of the same length including the last one
    // * values in all blocks have the same length
    fixed_length_ = (fixed_length_ &&
                    (block.last_size == block.avg) &&
                    (0 == docs_count_ || block.avg == prev_avg_));
    prev_avg_ = block.avg;
  } else {
    block.bits = packed::maxbits64(begin, end);
    const size_t buf_size = packed::bytes_required_64(addr_table_size, block.bits);
    std::memset(ctx_.u64buf, 0, buf_size);
    packed::pack(begin, end, ctx_.u64buf, block.bits);

    data_out.write_bytes(ctx_.u8buf, buf_size);
    fixed_length_ = false;
  }

  if (data_.file.length()) {
    block.data += data_out.file_pointer();

    if (ctx_.cipher) {
      auto offset = data_out.file_pointer();

      auto encrypt_and_copy
          = [&data_out, cipher = ctx_.cipher, &offset](irs::byte_type* b, size_t len) {
        assert(cipher);

        if (!cipher->encrypt(offset, b, len)) {
          return false;
        }

        data_out.write_bytes(b, len);
        offset += len;
        return true;
      };

      if (!data_.file.visit(encrypt_and_copy)) {
        throw io_error("failed to encrypt columnstore");
      }
    } else {
      data_.file >> data_out;
    }

    data_.stream.seek(0);
    data_.file.reset();
  }

  addr_table_.reset();

  docs_count_ += docs_count;
}

void column::finish(index_output& index_out) {
  assert(ctx_.data_out);

  docs_writer_.finish();
  if (!addr_table_.empty()) {
    flush_block();
  }
  docs_.stream.flush();

  column_header hdr;
  hdr.docs_count = docs_count_;

  memory_index_input in{docs_.file};
  sparse_bitmap_iterator it{&in, {{}, false}};
  if (it.next()) {
    hdr.min = it.value();
  }

  // FIXME how to deal with rollback() and docs_writer_.back()?
  if (docs_count_ && (hdr.min + docs_count_ - doc_limits::min() != pend_)) {
    auto& data_out = *ctx_.data_out;

    // we don't need to store bitmap index in case
    // if every document in a column has a value
    hdr.docs_index = data_out.file_pointer();
    docs_.file >> data_out;
  }

  hdr.props = ColumnProperty::NORMAL;
  if (ctx_.cipher) {
    hdr.props |= ColumnProperty::ENCRYPT;
  }

  if (fixed_length_) {
    if (0 == prev_avg_) {
      hdr.type = ColumnType::MASK;
    } else if (ctx_.consolidation) {
      hdr.type = ColumnType::DENSE_FIXED;
    } else {
      hdr.type = ColumnType::FIXED;
    }
  }

  irs::write_string(index_out, compression_.name());
  write_header(index_out, hdr);

  if (hdr.docs_index) {
    // write bitmap index IFF it's really necessary
    write_bitmap_index(index_out, docs_writer_.index());
  }

  if (ColumnType::SPARSE == hdr.type) {
    write_blocks_sparse(index_out, blocks_);
  } else if (ColumnType::MASK != hdr.type) {
    index_out.write_long(blocks_.front().avg);
    if (ColumnType::DENSE_FIXED == hdr.type) {
      index_out.write_long(blocks_.front().data);
    } else {
      assert(ColumnType::FIXED == hdr.type);
      write_blocks_dense(index_out, blocks_);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                             writer implementation
// -----------------------------------------------------------------------------

writer::writer(bool consolidation)
  : dir_{nullptr},
    alloc_{&memory_allocator::global()},
    buf_{memory::make_unique<byte_type[]>(column::BLOCK_SIZE*sizeof(uint64_t))},
    consolidation_{consolidation} {
}

void writer::prepare(directory& dir, const segment_meta& meta) {
  columns_.clear();

  auto filename = data_file_name(meta.name);
  auto data_out = dir.create(filename);

  if (!data_out) {
    throw io_error{string_utils::to_string(
      "Failed to create file, path: %s",
      filename.c_str())};
  }

  format_utils::write_header(*data_out, DATA_FORMAT_NAME, FORMAT_MIN);

  encryption::stream::ptr data_cipher;
  bstring enc_header;
  auto* enc = dir.attributes().encryption();
  const auto encrypt = irs::encrypt(filename, *data_out, enc, enc_header, data_cipher);
  assert(!encrypt || (data_cipher && data_cipher->block_size()));
  UNUSED(encrypt);


  // noexcept block
  dir_ = &dir;
  alloc_ = &dir.attributes().allocator();
  data_filename_ = std::move(filename);
  data_out_ = std::move(data_out);
  data_cipher_ = std::move(data_cipher);
}

columnstore_writer::column_t writer::push_column(const column_info& info) {
  // FIXME
  // Since current implementation doesn't support custom compression,
  // we ignore the compression option set in column_info.
  // We can potentially implement compression logic later on,
  // thus we make a column aware of compression algrorithm for further
  // extensibility.
  auto compression = irs::type<compression::none>::get(); /* info.compression(); */

  encryption::stream* cipher = info.encryption()
    ? data_cipher_.get()
    : nullptr;

  auto compressor = compression::get_compressor(compression, info.options());

  if (!compressor) {
    compressor = compression::compressor::identity();
  }

  const auto id = columns_.size();
  auto& column = columns_.emplace_back(
    column::context{
      alloc_,
      data_out_.get(),
      cipher,
      { buf_.get() },
      consolidation_ },
    compression,
    std::move(compressor));

  return std::make_pair(id, [&column] (doc_id_t doc) -> column_output& {
    // to avoid extra (and useless in our case) check for block index
    // emptiness in 'writer::column::prepare', we disallow passing
    // doc <= doc_limits::invalid() || doc >= doc_limits::eof()
    assert(doc > doc_limits::invalid() && doc < doc_limits::eof());

    column.prepare(doc);
    return column;
  });
}

bool writer::commit(const flush_state& /*state*/) {
  assert(dir_);

  // remove all empty columns from tail
  while (!columns_.empty() && columns_.back().empty()) {
    columns_.pop_back();
  }

  // remove file if there is no data to write
  if (columns_.empty()) {
    data_out_.reset();

    if (!dir_->remove(data_filename_)) { // ignore error
      IR_FRMT_ERROR("Failed to remove file, path: %s", data_filename_.c_str());
    }

    return false; // nothing to flush
  }

  const irs::string_ref segment_name{
    data_filename_,
    data_filename_.size() - DATA_FORMAT_EXT.size() - 1 };
  auto index_filename = index_file_name(segment_name);
  auto index_out = dir_->create(index_filename);

  if (!index_out) {
    throw io_error{string_utils::to_string(
      "Failed to create file, path: %s",
      index_filename.c_str())};
  }

  format_utils::write_header(*index_out, INDEX_FORMAT_NAME, FORMAT_MIN);

  // flush all remain data including possible
  // empty columns among filled columns
  index_out->write_vlong(columns_.size()); // number of columns
  for (auto& column : columns_) {
    // commit and flush remain blocks
    column.finish(*index_out);
  }

  format_utils::write_footer(*index_out);
  format_utils::write_footer(*data_out_);

  rollback();

  return true;
}

void writer::rollback() noexcept {
  data_filename_.clear();
  dir_ = nullptr;
  data_out_.reset(); // close output
  columns_.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                             reader implementation
// -----------------------------------------------------------------------------

const column_header* reader::header(field_id field) const {
  auto* column = field >= columns_.size()
    ? nullptr // can't find column with the specified identifier
    : columns_[field].get();

  if (column) {
#ifdef IRESEARCH_DEBUG
    auto& impl = dynamic_cast<column_base&>(*column);
#else
    auto& impl = static_cast<column_base&>(*column);
#endif

    return &impl.header();
  }

  return nullptr;
}

void reader::prepare_data(const directory& dir, const std::string& filename) {
  auto data_in = dir.open(filename, irs::IOAdvice::RANDOM);

  if (!data_in) {
    throw io_error{string_utils::to_string(
      "Failed to open file, path: %s",
      filename.c_str())};
  }

  [[maybe_unused]] const auto version =
    format_utils::check_header(
      *data_in,
      writer::DATA_FORMAT_NAME,
      writer::FORMAT_MIN,
      writer::FORMAT_MAX);

  encryption::stream::ptr cipher;
  auto* enc = dir.attributes().encryption();
  if (irs::decrypt(filename, *data_in, enc, cipher)) {
    assert(cipher && cipher->block_size());
  }

  // since columns data are too large
  // it is too costly to verify checksum of
  // the entire file. here we perform cheap
  // error detection which could recognize
  // some forms of corruption
  [[maybe_unused]] const auto checksum = format_utils::read_checksum(*data_in);

  // noexcept
  data_cipher_ = std::move(cipher);
  data_in_ = std::move(data_in);
}

// FIXME return result???
void reader::prepare_index(
    const directory& dir,
    const std::string& filename) {
  auto index_in = dir.open(filename, irs::IOAdvice::READONCE_SEQUENTIAL);

  if (!index_in) {
    throw io_error{string_utils::to_string(
      "Failed to open file, path: %s",
      filename.c_str())};
  }

  const auto checksum = format_utils::checksum(*index_in);

  [[maybe_unused]] const auto version =
    format_utils::check_header(
      *index_in,
      writer::INDEX_FORMAT_NAME,
      writer::FORMAT_MIN,
      writer::FORMAT_MAX);

  std::vector<column_ptr> columns;
  columns.reserve(index_in->read_vlong());

  for (size_t i = 0, size = columns.capacity(); i < size; ++i) {
    const auto compression_id = read_string<std::string>(*index_in);
    auto inflater = compression::get_decompressor(compression_id);

    if (!inflater && !compression::exists(compression_id)) {
      throw index_error{string_utils::to_string(
        "Failed to load compression '%s' for column id=" IR_SIZE_T_SPECIFIER,
        compression_id.c_str(), i)};
    }

    if (inflater && !inflater->prepare(*index_in)) { // FIXME or data_in???
      throw index_error{string_utils::to_string(
        "Failed to prepare compression '%s' for column id=" IR_SIZE_T_SPECIFIER,
        compression_id.c_str(), i)};
    }

    const column_header hdr = read_header(*index_in);

    if (is_encrypted(hdr) && !data_cipher_) {
      throw index_error{string_utils::to_string(
        "Failed to load encrypted column id=" IR_SIZE_T_SPECIFIER " without a cipher",
        i)};
    }

    if (ColumnType::MASK != hdr.type && 0 == hdr.docs_count) {
      throw index_error{string_utils::to_string(
        "Failed to load column id=" IR_SIZE_T_SPECIFIER ", only mask column may be empty",
        i)};
    }

    auto index = hdr.docs_index
      ? read_bitmap_index(*index_in)
      : column_index{};

    const size_t idx = static_cast<size_t>(hdr.type);
    if (IRS_LIKELY(idx < IRESEARCH_COUNTOF(FACTORIES))) {
      auto column = FACTORIES[idx](hdr, std::move(index), *index_in, *data_in_,
                                   std::move(inflater), data_cipher_.get());
      assert(column);
      columns.emplace_back(std::move(column));
    } else {
      throw index_error{string_utils::to_string(
        "Failed to load column id=" IR_SIZE_T_SPECIFIER ", got invalid type=%u",
        i, static_cast<uint32_t>(hdr.type))};
    }
  }

  format_utils::check_footer(*index_in, checksum);

  // noexcept block
  columns_ = std::move(columns);
}

bool reader::prepare(const directory& dir, const segment_meta& meta) {
  bool exists;
  const auto data_filename = data_file_name(meta.name);

  if (!dir.exists(exists, data_filename)) {
    throw io_error{string_utils::to_string(
      "failed to check existence of file, path: %s",
      data_filename.c_str())};
  }

  if (!exists) {
    // possible that the file does not exist
    // since columnstore is optional
    return false;
  }

  prepare_data(dir, data_filename);
  assert(data_in_);

  const auto index_filename = index_file_name(meta.name);

  if (!dir.exists(exists, index_filename)) {
    throw io_error{string_utils::to_string(
      "failed to check existence of file, path: %s",
      index_filename.c_str())};
  }

  if (!exists) {
    // more likely index is currupted
    throw index_error{string_utils::to_string(
      "columnstore index file '%s' is missing",
      index_filename.c_str())};
  }

  prepare_index(dir, index_filename);

  return true;
}

irs::columnstore_writer::ptr make_writer(
    Version /*version*/, bool consolidation) {
  return memory::make_unique<writer>(consolidation);
}

irs::columnstore_reader::ptr make_reader() {
  return memory::make_unique<reader>();
}

}
}
