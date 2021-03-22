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

#ifndef IRESEARCH_FORMAT_H
#define IRESEARCH_FORMAT_H

#include <absl/container/flat_hash_set.h>

#include "shared.hpp"
#include "store/data_output.hpp"
#include "store/directory.hpp"

#include "index/index_meta.hpp"
#include "index/column_info.hpp"
#include "index/iterators.hpp"

#include "utils/io_utils.hpp"
#include "utils/string.hpp"
#include "utils/type_info.hpp"
#include "utils/attribute_provider.hpp"
#include "utils/automaton_decl.hpp"

namespace iresearch {

struct segment_meta;
struct field_meta;
struct column_meta;
struct flush_state;
struct reader_state;
struct index_output;
struct data_input;
struct index_input;
struct postings_writer;

using document_mask = absl::flat_hash_set<doc_id_t> ;
using doc_map = std::vector<doc_id_t>;
using callback_f = std::function<bool(doc_iterator&)>;

//////////////////////////////////////////////////////////////////////////////
/// @class term_meta
/// @brief represents metadata associated with the term
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API term_meta : attribute {
  static constexpr string_ref type_name() noexcept {
    return "iresearch::term_meta";
  }

  void clear() noexcept {
    docs_count = 0;
    freq = 0;
  }

  uint32_t docs_count = 0; // how many documents a particular term contains
  uint32_t freq = 0; // FIXME check whether we can move freq to another place
}; // term_meta

////////////////////////////////////////////////////////////////////////////////
/// @struct postings_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API postings_writer : attribute_provider {
  using ptr = std::unique_ptr<postings_writer>;

  class releaser {
   public:
    explicit releaser(postings_writer* owner = nullptr) noexcept
      : owner_(owner) {
    }

    inline void operator()(term_meta* meta) const noexcept;

   private:
    postings_writer* owner_;
  }; // releaser

  typedef std::unique_ptr<term_meta, releaser> state;

  virtual ~postings_writer() = default;
  /* out - corresponding terms utils/utstream */
  virtual void prepare( index_output& out, const flush_state& state ) = 0;  
  virtual void begin_field(const flags& features) = 0;
  virtual state write(doc_iterator& docs) = 0;
  virtual void begin_block() = 0;
  virtual void encode(data_output& out, const term_meta& state) = 0;
  virtual void end() = 0;

 protected:
  friend struct term_meta;

  state make_state(term_meta& meta) noexcept {
    return state(&meta, releaser(this));
  }

  virtual void release(term_meta* meta) noexcept = 0;
}; // postings_writer

void postings_writer::releaser::operator()(term_meta* meta) const noexcept {
  assert(owner_ && meta);
  owner_->release(meta);
}

////////////////////////////////////////////////////////////////////////////////
/// @struct field_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API field_writer {
  using ptr = std::unique_ptr<field_writer>;

  virtual ~field_writer() = default;
  virtual void prepare(const flush_state& state) = 0;
  virtual void write(const std::string& name, field_id norm, const flags& features, term_iterator& data) = 0;
  virtual void end() = 0;
}; // field_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct postings_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API postings_reader {
  using ptr = std::unique_ptr<postings_reader>;
  using term_provider_f = std::function<const term_meta*()>;

  virtual ~postings_reader() = default;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @arg in - corresponding stream
  /// @arg features - the set of features available for segment
  //////////////////////////////////////////////////////////////////////////////
  virtual void prepare(
    index_input& in, 
    const reader_state& state,
    const flags& features) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses input block "in" and populate "attrs" collection with attributes
  /// @returns number of bytes read from in
  //////////////////////////////////////////////////////////////////////////////
  virtual size_t decode(
    const byte_type* in,
    const flags& features,
    term_meta& state) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns a document iterator for a specified 'cookie' and 'features'
  //////////////////////////////////////////////////////////////////////////////
  virtual doc_iterator::ptr iterator(
    const flags& field,
    const flags& features,
    const term_meta& meta) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief evaluates a union of all docs denoted by attribute supplied via a
  ///        speciified 'provider'. Each doc is represented by a bit in a
  ///        specified 'bitset'.
  /// @returns a number of bits set
  /// @note it's up to the caller to allocate enough space for a bitset
  /// @note this API is experimental
  //////////////////////////////////////////////////////////////////////////////
  virtual size_t bit_union(
    const flags& field,
    const term_provider_f& provider,
    size_t* set) = 0;
}; // postings_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct basic_term_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API basic_term_reader : public attribute_provider {
  virtual ~basic_term_reader() = default;

  virtual term_iterator::ptr iterator() const = 0;

  // returns field metadata
  virtual const field_meta& meta() const = 0;

  // least significant term
  virtual const bytes_ref& (min)() const = 0;

  // most significant term
  virtual const bytes_ref& (max)() const = 0;
}; // basic_term_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct term_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API term_reader: public attribute_provider {
  using ptr = std::unique_ptr<term_reader>;
  using cookie_provider = std::function<const seek_term_iterator::seek_cookie*()>;

  virtual ~term_reader() = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns an iterator over terms for a field
  //////////////////////////////////////////////////////////////////////////////
  virtual seek_term_iterator::ptr iterator() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns an intersection of a specified automaton and term reader
  //////////////////////////////////////////////////////////////////////////////
  virtual seek_term_iterator::ptr iterator(automaton_table_matcher& matcher) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief evaluates a union of all docs denoted by cookies supplied via a
  ///        speciified 'provider'. Each doc is represented by a bit in a
  ///        specified 'bitset'.
  /// @returns a number of bits set
  /// @note it's up to the caller to allocate enough space for a bitset
  /// @note this API is experimental
  //////////////////////////////////////////////////////////////////////////////
  virtual size_t bit_union(const cookie_provider& provider,
                           size_t* bitset) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns field metadata
  //////////////////////////////////////////////////////////////////////////////
  virtual const field_meta& meta() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns total number of terms
  //////////////////////////////////////////////////////////////////////////////
  virtual size_t size() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns total number of documents with at least 1 term in a field
  //////////////////////////////////////////////////////////////////////////////
  virtual uint64_t docs_count() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns the least significant term
  //////////////////////////////////////////////////////////////////////////////
  virtual const bytes_ref& (min)() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns the most significant term
  //////////////////////////////////////////////////////////////////////////////
  virtual const bytes_ref& (max)() const = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @struct field_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API field_reader {
  using ptr = std::unique_ptr<field_reader>;

  virtual ~field_reader() = default;

  virtual void prepare(
    const directory& dir,
    const segment_meta& meta,
    const document_mask& mask) = 0;

  virtual const term_reader* field(const string_ref& field) const = 0;
  virtual field_iterator::ptr iterator() const = 0;
  virtual size_t size() const = 0;
}; // field_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct columnstore_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API columnstore_writer {
  using ptr = std::unique_ptr<columnstore_writer>;

  struct column_output : data_output {
    // resets stream to previous persisted state 
    virtual void reset() = 0; 
  }; // column_output

  // NOTE: doc > type_limits<type_t::doc_id_t>::invalid() && doc < type_limits<type_t::doc_id_t>::eof()
  typedef std::function<column_output&(doc_id_t doc)> values_writer_f;
  typedef std::pair<field_id, values_writer_f> column_t;

  virtual ~columnstore_writer() = default;

  virtual void prepare(directory& dir, const segment_meta& meta) = 0;
  virtual column_t push_column(const column_info& info) = 0;
  virtual void rollback() noexcept = 0;
  virtual bool commit() = 0; // @return was anything actually flushed
}; // columnstore_writer

}

MSVC_ONLY(template class IRESEARCH_API std::function<bool(irs::doc_id_t, irs::bytes_ref&)>;) // columnstore_reader::values_reader_f
MSVC_ONLY(template class IRESEARCH_API std::function<irs::columnstore_writer::column_output&(irs::doc_id_t)>;) // columnstore_writer::values_writer_f

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @struct column_meta_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API column_meta_writer {
  using ptr = std::unique_ptr<column_meta_writer>;

  virtual ~column_meta_writer() = default;
  virtual void prepare(directory& dir, const segment_meta& meta) = 0;
  virtual void write(const std::string& name, field_id id) = 0;
  virtual void flush() = 0;
}; // column_meta_writer 

////////////////////////////////////////////////////////////////////////////////
/// @struct column_meta_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API column_meta_reader {
  using ptr = std::unique_ptr<column_meta_reader>;

  virtual ~column_meta_reader() = default;
  /// @returns true if column_meta is present in a segment.
  ///          false - otherwise
  virtual bool prepare(
    const directory& dir, 
    const segment_meta& meta,
    /*out*/ size_t& count,
    /*out*/ field_id& max_id) = 0;

  // returns false if there is no more data to read
  virtual bool read(column_meta& column) = 0;
}; // column_meta_reader 

////////////////////////////////////////////////////////////////////////////////
/// @struct columnstore_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API columnstore_reader {
  using ptr = std::unique_ptr<columnstore_reader>;

  typedef std::function<bool(doc_id_t, bytes_ref&)> values_reader_f;
  typedef std::function<bool(doc_id_t, const bytes_ref&)> values_visitor_f;  

  struct column_reader {
    virtual ~column_reader() = default;

    // returns corresponding column reader
    virtual columnstore_reader::values_reader_f values() const = 0;

    // returns the corresponding column iterator
    // if the column implementation supports document payloads then the latter
    // may be accessed via the 'payload' attribute
    virtual doc_iterator::ptr iterator() const = 0;

    virtual bool visit(const columnstore_reader::values_visitor_f& reader) const = 0;

    virtual size_t size() const = 0;
  };

  static const values_reader_f& empty_reader();

  virtual ~columnstore_reader() = default;

  /// @returns true if conlumnstore is present in a segment,
  ///          false - otherwise
  /// @throws io_error
  /// @throws index_error
  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta) = 0;

  virtual const column_reader* column(field_id field) const = 0;

  // @returns total number of columns
  virtual size_t size() const = 0;
}; // columnstore_reader

}

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @struct document_mask_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API document_mask_writer {
  using ptr = memory::managed_ptr<document_mask_writer>;

  virtual ~document_mask_writer() = default;

  virtual std::string filename(const segment_meta& meta) const = 0;

  virtual void write(
    directory& dir,
    const segment_meta& meta,
    const document_mask& docs_mask) = 0;
}; // document_mask_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct document_mask_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API document_mask_reader {
  using ptr = memory::managed_ptr<document_mask_reader>;

  virtual ~document_mask_reader() = default;

  /// @returns true if there are any deletes in a segment,
  ///          false - otherwise
  /// @throws io_error
  /// @throws index_error
  virtual bool read(
    const directory& dir,
    const segment_meta& meta,
    document_mask& docs_mask) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @struct segment_meta_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API segment_meta_writer {
  using ptr = memory::managed_ptr<segment_meta_writer>;

  virtual ~segment_meta_writer() = default;

  virtual void write(
    directory& dir,
    std::string& filename,
    const segment_meta& meta) = 0;
}; // segment_meta_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct segment_meta_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API segment_meta_reader {
  using ptr = memory::managed_ptr<segment_meta_reader>;

  virtual ~segment_meta_reader() = default;

  virtual void read(
    const directory& dir,
    segment_meta& meta,
    const string_ref& filename = string_ref::NIL // null == use meta
  ) = 0;
}; // segment_meta_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct index_meta_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API index_meta_writer {
  using ptr = std::unique_ptr<index_meta_writer>;

  virtual ~index_meta_writer() = default;
  virtual std::string filename(const index_meta& meta) const = 0;
  virtual bool prepare(directory& dir, index_meta& meta) = 0;
  virtual bool commit() = 0;
  virtual void rollback() noexcept = 0;
 protected:
  static void complete(index_meta& meta) noexcept;
  static void prepare(index_meta& meta) noexcept;
}; // index_meta_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct index_meta_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API index_meta_reader {
  using ptr = memory::managed_ptr<index_meta_reader>;

  virtual ~index_meta_reader() = default;

  virtual bool last_segments_file(
    const directory& dir, std::string& name
  ) const = 0;

  virtual void read(
    const directory& dir, 
    index_meta& meta,
    const string_ref& filename = string_ref::NIL // null == use meta
  ) = 0;

 protected:
  static void complete(
    index_meta& meta,
    uint64_t generation,
    uint64_t counter,
    index_meta::index_segments_t&& segments,
    bstring* payload_buf
  );
}; // index_meta_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct format
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API format {
 public:
  typedef std::shared_ptr<const format> ptr;

  explicit format(const type_info& type) noexcept : type_(type) {}
  virtual ~format() = default;

  virtual index_meta_writer::ptr get_index_meta_writer() const = 0;
  virtual index_meta_reader::ptr get_index_meta_reader() const = 0;

  virtual segment_meta_writer::ptr get_segment_meta_writer() const = 0;
  virtual segment_meta_reader::ptr get_segment_meta_reader() const = 0;

  virtual document_mask_writer::ptr get_document_mask_writer() const = 0;
  virtual document_mask_reader::ptr get_document_mask_reader() const = 0;

  virtual field_writer::ptr get_field_writer(bool volatile_state) const = 0;
  virtual field_reader::ptr get_field_reader() const = 0;

  virtual column_meta_writer::ptr get_column_meta_writer() const = 0;
  virtual column_meta_reader::ptr get_column_meta_reader() const = 0;

  virtual columnstore_writer::ptr get_columnstore_writer() const = 0;
  virtual columnstore_reader::ptr get_columnstore_reader() const = 0;

  const type_info& type() const { return type_; }

 private:
  type_info type_;
}; // format

}

MSVC_ONLY(template class IRESEARCH_API std::shared_ptr<irs::format>;) // format::ptr

namespace iresearch {

struct IRESEARCH_API flush_state {
  directory* dir;
  string_ref name; // segment name
  const flags* features; // segment features
  size_t doc_count;
  const doc_map* docmap;
};

struct IRESEARCH_API reader_state {
  const directory* dir;
  const segment_meta* meta;
};

// -----------------------------------------------------------------------------
// --SECTION--                                               convinience methods
// -----------------------------------------------------------------------------

class IRESEARCH_API formats {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether a format with the specified name is registered
  ////////////////////////////////////////////////////////////////////////////////
  static bool exists(const string_ref& name, bool load_library = true);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find a format by name, or nullptr if not found
  ///        indirect call to <class>::make(...)
  ///        requires use of DECLARE_FACTORY() in class definition
  ///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
  //////////////////////////////////////////////////////////////////////////////
  static format::ptr get(
    const string_ref& name,
    const string_ref& module = string_ref::NIL,
    bool load_library = true) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief for static lib reference all known formats in lib
  ///        for shared lib NOOP
  ///        no explicit call of fn is required, existence of fn is sufficient
  ////////////////////////////////////////////////////////////////////////////////
  static void init();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief load all formats from plugins directory
  ////////////////////////////////////////////////////////////////////////////////
  static void load_all(const std::string& path);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief visit all loaded formats, terminate early if visitor returns false
  ////////////////////////////////////////////////////////////////////////////////
  static bool visit(const std::function<bool(const string_ref&)>& visitor);

 private:
  formats() = delete;
};

// -----------------------------------------------------------------------------
// --SECTION--                                               format registration
// -----------------------------------------------------------------------------

class IRESEARCH_API format_registrar {
 public:
  format_registrar(
    const type_info& type,
    const string_ref& module,
    format::ptr(*factory)(),
    const char* source = nullptr);

  operator bool() const noexcept;

 private:
  bool registered_;
};

#define REGISTER_FORMAT__(format_name, mudule_name, line, source) \
  static ::iresearch::format_registrar format_registrar ## _ ## line(::iresearch::type<format_name>::get(), mudule_name, &format_name::make, source)
#define REGISTER_FORMAT_EXPANDER__(format_name, mudule_name, file, line) REGISTER_FORMAT__(format_name, mudule_name, line, file ":" TOSTRING(line))
#define REGISTER_FORMAT_MODULE(format_name, module_name) REGISTER_FORMAT_EXPANDER__(format_name, module_name, __FILE__, __LINE__)
#define REGISTER_FORMAT(format_name) REGISTER_FORMAT_MODULE(format_name, irs::string_ref::NIL)

}

#endif
