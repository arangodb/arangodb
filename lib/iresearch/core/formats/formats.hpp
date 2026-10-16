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

#include <function2/function2.hpp>
#include <set>
#include <vector>

#include "formats/seek_cookie.hpp"
#include "index/column_info.hpp"
#include "index/field_meta.hpp"
#include "index/index_features.hpp"
#include "index/index_meta.hpp"
#include "index/index_reader_options.hpp"
#include "index/iterators.hpp"
#include "search/score_function.hpp"
#include "store/data_output.hpp"
#include "store/directory.hpp"
#include "utils/attribute_provider.hpp"
#include "utils/automaton_decl.hpp"
#include "utils/string.hpp"
#include "utils/type_info.hpp"

#include <absl/container/flat_hash_set.h>

namespace irs {

class Comparer;
struct SegmentMeta;
struct field_meta;
struct flush_state;
struct ReaderState;
class index_output;
struct data_input;
struct index_input;
struct postings_writer;
struct Scorer;
struct WandWriter;

using DocumentMask =
  absl::flat_hash_set<doc_id_t,
                      absl::container_internal::hash_default_hash<doc_id_t>,
                      absl::container_internal::hash_default_eq<doc_id_t>,
                      ManagedTypedAllocator<doc_id_t>>;
using DocMap = ManagedVector<doc_id_t>;
using DocMapView = std::span<const doc_id_t>;
using callback_f = std::function<bool(doc_iterator&)>;

using ScoreFunctionFactory =
  std::function<ScoreFunction(const attribute_provider&)>;

struct WanderatorOptions {
  ScoreFunctionFactory factory;
};

struct SegmentWriterOptions {
  const ColumnInfoProvider& column_info;
  const FeatureInfoProvider& feature_info;
  const feature_set_t& scorers_features;
  ScorersView scorers;
  const Comparer* const comparator{};
  IResourceManager& resource_manager{IResourceManager::kNoop};
};

// Represents metadata associated with the term
struct term_meta : attribute {
  static constexpr std::string_view type_name() noexcept {
    return "irs::term_meta";
  }

  void clear() noexcept {
    docs_count = 0;
    freq = 0;
  }

  // How many documents a particular term contains
  uint32_t docs_count = 0;

  // How many times a particular term occur in documents
  uint32_t freq = 0;
};

struct postings_writer {
  using ptr = std::unique_ptr<postings_writer>;

  struct FieldStats {
    uint64_t wand_mask;
    doc_id_t docs_count;
  };

  virtual ~postings_writer() = default;
  // out - corresponding terms stream
  virtual void prepare(index_output& out, const flush_state& state) = 0;
  virtual void begin_field(IndexFeatures index_features,
                           const feature_map_t& features) = 0;
  virtual void write(doc_iterator& docs, term_meta& meta) = 0;
  virtual void begin_block() = 0;
  virtual void encode(data_output& out, const term_meta& state) = 0;
  virtual FieldStats end_field() = 0;
  virtual void end() = 0;
};

struct basic_term_reader : public attribute_provider {
  virtual term_iterator::ptr iterator() const = 0;

  // Returns field metadata
  virtual const field_meta& meta() const = 0;

  // Returns the least significant term
  virtual bytes_view(min)() const = 0;

  // Returns the most significant term
  virtual bytes_view(max)() const = 0;
};

struct field_writer {
  using ptr = std::unique_ptr<field_writer>;

  virtual ~field_writer() = default;
  virtual void prepare(const flush_state& state) = 0;
  virtual void write(const basic_term_reader& reader,
                     const irs::feature_map_t& features) = 0;
  virtual void end() = 0;
};

struct WandInfo {
  uint8_t mapped_index{WandContext::kDisable};
  uint8_t count{0};
};

struct postings_reader {
  using ptr = std::unique_ptr<postings_reader>;
  using term_provider_f = std::function<const term_meta*()>;

  virtual ~postings_reader() = default;

  virtual uint64_t CountMappedMemory() const = 0;

  // in - corresponding stream
  // features - the set of features available for segment
  virtual void prepare(index_input& in, const ReaderState& state,
                       IndexFeatures features) = 0;

  // Parses input block "in" and populate "attrs" collection with
  // attributes.
  // Returns number of bytes read from in.
  virtual size_t decode(const byte_type* in, IndexFeatures features,
                        term_meta& state) = 0;

  // Returns document iterator for a specified 'cookie' and 'features'
  virtual doc_iterator::ptr iterator(IndexFeatures field_features,
                                     IndexFeatures required_features,
                                     const term_meta& meta,
                                     uint8_t wand_count) = 0;

  virtual doc_iterator::ptr wanderator(IndexFeatures field_features,
                                       IndexFeatures required_features,
                                       const term_meta& meta,
                                       const WanderatorOptions& options,
                                       WandContext ctx, WandInfo info) = 0;

  // Evaluates a union of all docs denoted by attribute supplied via a
  // speciified 'provider'. Each doc is represented by a bit in a
  // specified 'bitset'.
  // Returns a number of bits set.
  // It's up to the caller to allocate enough space for a bitset.
  // This API is experimental.
  virtual size_t bit_union(IndexFeatures field_features,
                           const term_provider_f& provider, size_t* set,
                           uint8_t wand_count) = 0;
};

// Expected usage pattern of seek_term_iterator
enum class SeekMode : uint32_t {
  /// Default mode, e.g. multiple consequent seeks are expected
  NORMAL = 0,

  // Only random exact seeks are supported
  RANDOM_ONLY
};

struct term_reader : public attribute_provider {
  using ptr = std::unique_ptr<term_reader>;
  using cookie_provider = std::function<const seek_cookie*()>;

  // `mode` argument defines seek mode for term iterator
  // Returns an iterator over terms for a field.
  virtual seek_term_iterator::ptr iterator(SeekMode mode) const = 0;

  // Read 'count' number of documents containing 'term' to 'docs'
  // Returns number of read documents
  virtual size_t read_documents(bytes_view term,
                                std::span<doc_id_t> docs) const = 0;

  // Returns term metadata for a given 'term'
  virtual term_meta term(bytes_view term) const = 0;

  // Returns an intersection of a specified automaton and term reader.
  virtual seek_term_iterator::ptr iterator(
    automaton_table_matcher& matcher) const = 0;

  // Evaluates a union of all docs denoted by cookies supplied via a
  // speciified 'provider'. Each doc is represented by a bit in a
  // specified 'bitset'.
  // A number of bits set.
  // It's up to the caller to allocate enough space for a bitset.
  // This API is experimental.
  virtual size_t bit_union(const cookie_provider& provider,
                           size_t* bitset) const = 0;

  virtual doc_iterator::ptr postings(const seek_cookie& cookie,
                                     IndexFeatures features) const = 0;

  virtual doc_iterator::ptr wanderator(const seek_cookie& cookie,
                                       IndexFeatures features,
                                       const WanderatorOptions& options,
                                       WandContext context) const = 0;

  // Returns field metadata.
  virtual const field_meta& meta() const = 0;

  // Returns total number of terms.
  virtual size_t size() const = 0;

  // Returns total number of documents with at least 1 term in a field.
  virtual uint64_t docs_count() const = 0;

  // Returns the least significant term.
  virtual bytes_view(min)() const = 0;

  // Returns the most significant term.
  virtual bytes_view(max)() const = 0;

  // Returns true if scorer denoted by the is supported by the field.
  virtual bool has_scorer(uint8_t index) const = 0;
};

struct field_reader {
  using ptr = std::shared_ptr<field_reader>;

  virtual ~field_reader() = default;

  virtual uint64_t CountMappedMemory() const = 0;

  virtual void prepare(const ReaderState& stat) = 0;

  virtual const term_reader* field(std::string_view field) const = 0;
  virtual field_iterator::ptr iterator() const = 0;
  virtual size_t size() const = 0;
};

struct column_output : data_output {
  // Resets stream to previous persisted state
  virtual void reset() = 0;
  // NOTE: doc_limits::invalid() < doc && doc < doc_limits::eof()
  virtual void Prepare(doc_id_t doc) = 0;

#ifdef IRESEARCH_TEST
  column_output& operator()(doc_id_t doc) {
    Prepare(doc);
    return *this;
  }
#endif
};

struct columnstore_writer {
  using ptr = std::unique_ptr<columnstore_writer>;

  // Finalizer can be used to assign name and payload to a column.
  // Returned `std::string_view` must be valid during `commit(...)`.
  using column_finalizer_f =
    fu2::unique_function<std::string_view(bstring& out)>;

  struct column_t {
    field_id id;
    column_output& out;
  };

  virtual ~columnstore_writer() = default;

  virtual void prepare(directory& dir, const SegmentMeta& meta) = 0;
  virtual column_t push_column(const ColumnInfo& info,
                               column_finalizer_f header_writer) = 0;
  virtual void rollback() noexcept = 0;

  // Return was anything actually flushed.
  virtual bool commit(const flush_state& state) = 0;
};

enum class ColumnHint : uint32_t {
  // Nothing special
  kNormal = 0,
  // Open iterator for conosolidation
  kConsolidation = 1,
  // Reading payload isn't necessary
  kMask = 2,
  // Allow accessing prev document
  kPrevDoc = 4
};

ENABLE_BITMASK_ENUM(ColumnHint);

struct column_reader : public memory::Managed {
  // Returns column id.
  virtual field_id id() const = 0;

  // Returns optional column name.
  virtual std::string_view name() const = 0;

  // Returns column header.
  virtual bytes_view payload() const = 0;

  // FIXME(gnusi): implement mode
  //  Returns the corresponding column iterator.
  //  If the column implementation supports document payloads then it
  //  can be accessed via the 'payload' attribute.
  virtual doc_iterator::ptr iterator(ColumnHint hint) const = 0;

  // Returns total number of columns.
  virtual doc_id_t size() const = 0;
};

struct columnstore_reader {
  using ptr = std::unique_ptr<columnstore_reader>;

  using column_visitor_f = std::function<bool(const column_reader&)>;

  struct options {
    // allows to select "hot" columns
    column_visitor_f warmup_column;
    // memory usage accounting
    ResourceManagementOptions resource_manager;
  };

  virtual ~columnstore_reader() = default;

  virtual uint64_t CountMappedMemory() const = 0;

  // Returns true if conlumnstore is present in a segment, false - otherwise.
  // May throw `io_error` or `index_error`.
  virtual bool prepare(const directory& dir, const SegmentMeta& meta,
                       const options& opts = options{}) = 0;

  virtual bool visit(const column_visitor_f& visitor) const = 0;

  virtual const column_reader* column(field_id field) const = 0;

  // Returns total number of columns.
  virtual size_t size() const = 0;
};

struct document_mask_writer : memory::Managed {
  using ptr = memory::managed_ptr<document_mask_writer>;

  virtual std::string filename(const SegmentMeta& meta) const = 0;

  // Return number of bytes written
  virtual size_t write(directory& dir, const SegmentMeta& meta,
                       const DocumentMask& docs_mask) = 0;
};

struct document_mask_reader : memory::Managed {
  using ptr = memory::managed_ptr<document_mask_reader>;

  // Returns true if there are any deletes in a segment,
  // false - otherwise.
  // May throw io_error or index_error
  virtual bool read(const directory& dir, const SegmentMeta& meta,
                    DocumentMask& docs_mask) = 0;
};

struct segment_meta_writer : memory::Managed {
  using ptr = memory::managed_ptr<segment_meta_writer>;

  virtual bool SupportPrimarySort() const noexcept = 0;
  virtual void write(directory& dir, std::string& filename,
                     SegmentMeta& meta) = 0;
};

struct segment_meta_reader : memory::Managed {
  using ptr = memory::managed_ptr<segment_meta_reader>;

  virtual void read(const directory& dir, SegmentMeta& meta,
                    std::string_view filename = {}) = 0;  // null == use meta
};

struct index_meta_writer {
  using ptr = std::unique_ptr<index_meta_writer>;

  virtual ~index_meta_writer() = default;
  virtual bool prepare(directory& dir, IndexMeta& meta,
                       std::string& pending_filename,
                       std::string& filename) = 0;
  virtual bool commit() = 0;
  virtual void rollback() noexcept = 0;
};

struct index_meta_reader : memory::Managed {
  using ptr = memory::managed_ptr<index_meta_reader>;

  virtual bool last_segments_file(const directory& dir,
                                  std::string& name) const = 0;

  // null == use meta
  virtual void read(const directory& dir, IndexMeta& meta,
                    std::string_view filename) = 0;
};

class format {
 public:
  using ptr = std::shared_ptr<const format>;

  virtual ~format() = default;

  virtual index_meta_writer::ptr get_index_meta_writer() const = 0;
  virtual index_meta_reader::ptr get_index_meta_reader() const = 0;

  virtual segment_meta_writer::ptr get_segment_meta_writer() const = 0;
  virtual segment_meta_reader::ptr get_segment_meta_reader() const = 0;

  virtual document_mask_writer::ptr get_document_mask_writer() const = 0;
  virtual document_mask_reader::ptr get_document_mask_reader() const = 0;

  virtual field_writer::ptr get_field_writer(bool consolidation,
                                             IResourceManager& rm) const = 0;
  virtual field_reader::ptr get_field_reader(IResourceManager& rm) const = 0;

  virtual columnstore_writer::ptr get_columnstore_writer(
    bool consolidation, IResourceManager& rm) const = 0;
  virtual columnstore_reader::ptr get_columnstore_reader() const = 0;

  virtual irs::type_info::type_id type() const noexcept = 0;
};

struct flush_state {
  directory* const dir{};
  const DocMap* docmap{};
  const ColumnProvider* columns{};
  // Accumulated segment features
  const feature_set_t* features{};
  // Segment name
  const std::string_view name;  // segment name
  ScorersView scorers;
  const size_t doc_count;
  // Accumulated segment index features
  IndexFeatures index_features{IndexFeatures::NONE};
};

struct ReaderState {
  const directory* dir;
  const SegmentMeta* meta;
  ScorersView scorers;
};

namespace formats {
// Checks whether a format with the specified name is registered.
bool exists(std::string_view name, bool load_library = true);

// Find a format by name, or nullptr if not found
// indirect call to <class>::make(...)
// NOTE: make(...) MUST be defined in CPP to ensire proper code scope
format::ptr get(std::string_view name, std::string_view moduleName = {},
                bool load_library = true) noexcept;

// For static lib reference all known formats in lib
// no explicit call of fn is required, existence of fn is sufficient.
void init();

// Load all formats from plugins directory.
void load_all(std::string_view path);

// Visit all loaded formats, terminate early if visitor returns false.
bool visit(const std::function<bool(std::string_view)>& visitor);

}  // namespace formats
class format_registrar {
 public:
  format_registrar(const type_info& type, std::string_view module,
                   format::ptr (*factory)(), const char* source = nullptr);

  operator bool() const noexcept { return registered_; }

 private:
  bool registered_;
};

#define REGISTER_FORMAT__(format_name, mudule_name, line, source) \
  static ::irs::format_registrar format_registrar##_##line(       \
    ::irs::type<format_name>::get(), mudule_name, &format_name::make, source)
#define REGISTER_FORMAT_EXPANDER__(format_name, mudule_name, file, line) \
  REGISTER_FORMAT__(format_name, mudule_name, line,                      \
                    file ":" IRS_TO_STRING(line))
#define REGISTER_FORMAT_MODULE(format_name, module_name) \
  REGISTER_FORMAT_EXPANDER__(format_name, module_name, __FILE__, __LINE__)
#define REGISTER_FORMAT(format_name) \
  REGISTER_FORMAT_MODULE(format_name, std::string_view{})

}  // namespace irs
