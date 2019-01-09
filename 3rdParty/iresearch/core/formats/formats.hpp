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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FORMAT_H
#define IRESEARCH_FORMAT_H

#include "shared.hpp"
#include "store/data_output.hpp"
#include "store/directory.hpp"

#include "index/index_meta.hpp"
#include "index/iterators.hpp"

#include "utils/block_pool.hpp"
#include "utils/io_utils.hpp"
#include "utils/string.hpp"
#include "utils/type_id.hpp"
#include "utils/attributes_provider.hpp"

NS_ROOT

struct segment_meta;
struct field_meta;
struct column_meta;
struct flush_state;
struct reader_state;
struct index_output;
struct data_input;
struct index_input;
typedef std::unordered_set<doc_id_t> document_mask;
struct postings_writer;

//////////////////////////////////////////////////////////////////////////////
/// @class term_meta
/// @brief represents metadata associated with the term
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API term_meta : attribute {
  DECLARE_ATTRIBUTE_TYPE();

  term_meta() = default;
  virtual ~term_meta() = default;

  virtual void clear() {
    docs_count = 0;
    freq = 0;
  }

  uint32_t docs_count = 0; // how many documents a particular term contains
  uint32_t freq = 0; // FIXME check whether we can move freq to another place
}; // term_meta

////////////////////////////////////////////////////////////////////////////////
/// @struct postings_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API postings_writer : util::const_attribute_view_provider {
  DECLARE_UNIQUE_PTR(postings_writer);
  DEFINE_FACTORY_INLINE(postings_writer);

  class releaser {
   public:
    explicit releaser(postings_writer* owner = nullptr) NOEXCEPT
      : owner_(owner) {
    }

    inline void operator()(term_meta* meta) const NOEXCEPT;

   private:
    postings_writer* owner_;
  }; // releaser

  typedef std::unique_ptr<term_meta, releaser> state;

  virtual ~postings_writer();
  /* out - corresponding terms utils/utstream */
  virtual void prepare( index_output& out, const flush_state& state ) = 0;  
  virtual void begin_field(const flags& features) = 0;
  virtual state write(doc_iterator& docs) = 0;
  virtual void begin_block() = 0;
  virtual void encode(data_output& out, const term_meta& state) = 0;
  virtual void end() = 0;

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attribute_view::empty_instance();
  }

 protected:
  friend struct term_meta;

  state make_state(term_meta& meta) NOEXCEPT {
    return state(&meta, releaser(this));
  }

  virtual void release(term_meta* meta) NOEXCEPT = 0;
}; // postings_writer

void postings_writer::releaser::operator()(term_meta* meta) const NOEXCEPT {
  assert(owner_ && meta);
  owner_->release(meta);
}

////////////////////////////////////////////////////////////////////////////////
/// @struct field_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API field_writer {
  DECLARE_UNIQUE_PTR(field_writer);
  DEFINE_FACTORY_INLINE(field_writer);

  virtual ~field_writer();
  virtual void prepare(const flush_state& state) = 0;
  virtual void write(const std::string& name, field_id norm, const flags& features, term_iterator& data) = 0;
  virtual void end() = 0;
}; // field_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct postings_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API postings_reader {
  DECLARE_UNIQUE_PTR(postings_reader);
  DEFINE_FACTORY_INLINE(postings_reader);

  virtual ~postings_reader();
  
  // in - corresponding stream
  // features - the set of features available for segment
  virtual void prepare(
    index_input& in, 
    const reader_state& state,
    const flags& features
  ) = 0;

  // parses input stream "in" and populate "attrs" collection
  // with attributes
  virtual void decode(
    data_input& in,
    const flags& features,
    const attribute_view& attrs,
    term_meta& state
  ) = 0;

  virtual doc_iterator::ptr iterator(
    const flags& field,
    const attribute_view& attrs,
    const flags& features
  ) = 0;
}; // postings_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct basic_term_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API basic_term_reader: public util::const_attribute_view_provider {
  virtual ~basic_term_reader();

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
struct IRESEARCH_API term_reader: public util::const_attribute_view_provider {
  DECLARE_UNIQUE_PTR( term_reader);
  DEFINE_FACTORY_INLINE(term_reader);

  virtual ~term_reader();

  virtual seek_term_iterator::ptr iterator() const = 0;

  // returns field metadata
  virtual const field_meta& meta() const = 0;

  // total number of terms
  virtual size_t size() const = 0;

  // total number of documents with at least 1 term in a field
  virtual uint64_t docs_count() const = 0;

  // less significant term
  virtual const bytes_ref& (min)() const = 0;

  // most significant term
  virtual const bytes_ref& (max)() const = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @struct field_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API field_reader {
  DECLARE_UNIQUE_PTR(field_reader);
  DEFINE_FACTORY_INLINE(field_reader);

  virtual ~field_reader();

  virtual void prepare(
    const directory& dir,
    const segment_meta& meta,
    const document_mask& mask
  ) = 0;

  virtual const term_reader* field(const string_ref& field) const = 0;
  virtual field_iterator::ptr iterator() const = 0;
  virtual size_t size() const = 0;
}; // field_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct columnstore_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API columnstore_writer {
  DECLARE_SHARED_PTR(columnstore_writer);

  struct column_output : data_output {
    // resets stream to previous persisted state 
    virtual void reset() = 0; 
  }; // column_output

  // NOTE: doc > type_limits<type_t::doc_id_t>::invalid() && doc < type_limits<type_t::doc_id_t>::eof()
  typedef std::function<column_output&(doc_id_t doc)> values_writer_f;
  typedef std::pair<field_id, values_writer_f> column_t;

  virtual ~columnstore_writer();

  virtual void prepare(directory& dir, const segment_meta& meta) = 0;
  virtual column_t push_column() = 0;
  virtual void rollback() NOEXCEPT = 0;
  virtual bool commit() = 0; // @return was anything actually flushed
}; // columnstore_writer

NS_END

MSVC_ONLY(template class IRESEARCH_API std::function<bool(irs::doc_id_t, irs::bytes_ref&)>); // columnstore_reader::values_reader_f
MSVC_ONLY(template class IRESEARCH_API std::function<iresearch::columnstore_writer::column_output&(iresearch::doc_id_t)>); // columnstore_writer::values_writer_f

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @struct column_meta_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API column_meta_writer {
  DECLARE_SHARED_PTR(column_meta_writer);
  virtual ~column_meta_writer();
  virtual void prepare(directory& dir, const segment_meta& meta) = 0;
  virtual void write(const std::string& name, field_id id) = 0;
  virtual void flush() = 0;
}; // column_meta_writer 

////////////////////////////////////////////////////////////////////////////////
/// @struct column_meta_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API column_meta_reader {
  DECLARE_SHARED_PTR(column_meta_reader);
  virtual ~column_meta_reader();
  /// @returns true if column_meta is present in a segment.
  ///          false - otherwise
  virtual bool prepare(
    const directory& dir, 
    const segment_meta& meta,
    size_t& count, // out parameter
    field_id& max_id // out parameter
  ) = 0;

  // returns false if there is no more data to read
  virtual bool read(column_meta& column) = 0;
}; // column_meta_reader 

////////////////////////////////////////////////////////////////////////////////
/// @struct columnstore_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API columnstore_reader {
  DECLARE_UNIQUE_PTR(columnstore_reader);

  typedef std::function<bool(doc_id_t, bytes_ref&)> values_reader_f;
  typedef std::function<bool(doc_id_t, const bytes_ref&)> values_visitor_f;  

  struct column_reader {
    virtual ~column_reader() = default;

    // returns corresponding column reader
    virtual columnstore_reader::values_reader_f values() const = 0;

    // returns the corresponding column iterator
    // if the column implementation supports document payloads then the latter
    // may be accessed via the 'payload_iterator' attribute
    virtual doc_iterator::ptr iterator() const = 0;

    virtual bool visit(const columnstore_reader::values_visitor_f& reader) const = 0;

    virtual size_t size() const = 0;
  };

  static const values_reader_f& empty_reader();

  virtual ~columnstore_reader();

  /// @returns true if conlumnstore is present in a segment,
  ///          false - otherwise
  /// @throws io_error
  /// @throws index_error
  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta
  ) = 0;

  virtual const column_reader* column(field_id field) const = 0;

  // @returns total number of columns
  virtual size_t size() const = 0;
}; // columnstore_reader

NS_END

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @struct document_mask_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API document_mask_writer {
  DECLARE_MANAGED_PTR(document_mask_writer);
  DEFINE_FACTORY_INLINE(document_mask_writer);

  virtual ~document_mask_writer();

  virtual std::string filename(
    const segment_meta& meta
  ) const = 0;

  virtual void write(
    directory& dir,
    const segment_meta& meta,
    const document_mask& docs_mask
  ) = 0;
}; // document_mask_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct document_mask_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API document_mask_reader {
  DECLARE_MANAGED_PTR(document_mask_reader);
  DEFINE_FACTORY_INLINE(document_mask_reader);

  virtual ~document_mask_reader();

  /// @returns true if there are any deletes in a segment,
  ///          false - otherwise
  /// @throws io_error
  /// @throws index_error
  virtual bool read(
    const directory& dir,
    const segment_meta& meta,
    document_mask& docs_mask
  ) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @struct segment_meta_writer
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API segment_meta_writer {
  DECLARE_MANAGED_PTR(segment_meta_writer);

  virtual ~segment_meta_writer();

  virtual void write(
    directory& dir,
    std::string& filename,
    const segment_meta& meta
  ) = 0;
}; // segment_meta_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct segment_meta_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API segment_meta_reader {
  DECLARE_MANAGED_PTR(segment_meta_reader);

  virtual ~segment_meta_reader();

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
  DECLARE_UNIQUE_PTR(index_meta_writer);
  DEFINE_FACTORY_INLINE(index_meta_writer);

  virtual ~index_meta_writer();
  virtual std::string filename(const index_meta& meta) const = 0;
  virtual bool prepare(directory& dir, index_meta& meta) = 0;
  virtual bool commit() = 0;
  virtual void rollback() NOEXCEPT = 0;
 protected:
  static void complete(index_meta& meta) NOEXCEPT;
  static void prepare(index_meta& meta) NOEXCEPT;
}; // index_meta_writer

////////////////////////////////////////////////////////////////////////////////
/// @struct index_meta_reader
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API index_meta_reader {
  DECLARE_MANAGED_PTR(index_meta_reader);

  virtual ~index_meta_reader();

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
    index_meta::index_segments_t&& segments
  );
}; // index_meta_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct format
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API format {
 public:
  typedef std::shared_ptr<const format> ptr;

  //////////////////////////////////////////////////////////////////////////////
  /// @class type_id
  //////////////////////////////////////////////////////////////////////////////
  class type_id: public iresearch::type_id, util::noncopyable {
   public:
    type_id(const string_ref& name): name_(name) {}
    operator const type_id*() const { return this; }
    const string_ref& name() const { return name_; }

   private:
    string_ref name_;
  };

  format(const type_id& type) NOEXCEPT : type_(&type) {}
  virtual ~format();

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

  const type_id& type() const { return *type_; }

 private:
  const type_id* type_;
}; // format

NS_END

MSVC_ONLY(template class IRESEARCH_API std::shared_ptr<iresearch::format>); // format::ptr

NS_ROOT

struct IRESEARCH_API flush_state {
  directory* dir;
  string_ref name; // segment name
  const flags* features; // segment features
  size_t doc_count;
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
  static bool exists(const string_ref& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find a format by name, or nullptr if not found
  ///        indirect call to <class>::make(...)
  ///        requires use of DECLARE_FACTORY() in class definition
  ///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
  //////////////////////////////////////////////////////////////////////////////
  static format::ptr get(const string_ref& name) NOEXCEPT;

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
// --SECTION--                                                 format definition
// -----------------------------------------------------------------------------

#define DECLARE_FORMAT_TYPE() DECLARE_TYPE_ID(iresearch::format::type_id)
#define DEFINE_FORMAT_TYPE_NAMED(class_type, class_name) DEFINE_TYPE_ID(class_type, iresearch::format::type_id) { \
  static iresearch::format::type_id type(class_name); \
  return type; \
}
#define DEFINE_FORMAT_TYPE(class_type) DEFINE_FORMAT_TYPE_NAMED(class_type, #class_type)

// -----------------------------------------------------------------------------
// --SECTION--                                               format registration
// -----------------------------------------------------------------------------

class IRESEARCH_API format_registrar {
 public:
  format_registrar(
    const format::type_id& type,
    format::ptr(*factory)(),
    const char* source = nullptr
  );
  operator bool() const NOEXCEPT;
 private:
  bool registered_;
};

#define REGISTER_FORMAT__(format_name, line, source) static iresearch::format_registrar format_registrar ## _ ## line(format_name::type(), &format_name::make, source)
#define REGISTER_FORMAT_EXPANDER__(format_name, file, line) REGISTER_FORMAT__(format_name, line, file ":" TOSTRING(line))
#define REGISTER_FORMAT(format_name) REGISTER_FORMAT_EXPANDER__(format_name, __FILE__, __LINE__)

NS_END

#endif
