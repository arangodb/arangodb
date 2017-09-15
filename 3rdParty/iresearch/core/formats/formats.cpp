//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "utils/register.hpp"

// list of statically loaded formats via init()
#ifndef IRESEARCH_DLL
  #include "formats_10.hpp"
#endif

#include "formats.hpp"
#include "utils/type_limits.hpp"

NS_LOCAL

const std::string FILENAME_PREFIX("libformat-");

const irs::columnstore_iterator::value_type INVALID {
  irs::type_limits<irs::type_t::doc_id_t>::invalid(),
  irs::bytes_ref::nil
};

const irs::columnstore_iterator::value_type EOFMAX {
  irs::type_limits<irs::type_t::doc_id_t>::eof(),
  irs::bytes_ref::nil
};

class format_register :
  public iresearch::generic_register<iresearch::string_ref, iresearch::format::ptr(*)(), format_register> {
 protected:
  virtual std::string key_to_filename(const key_type& key) const override {
    std::string filename(FILENAME_PREFIX.size() + key.size(), 0);

    std::memcpy(&filename[0], FILENAME_PREFIX.c_str(), FILENAME_PREFIX.size());
    std::memcpy(&filename[0] + FILENAME_PREFIX.size(), key.c_str(), key.size());

    return filename;
  }
}; // format_register

struct empty_column_iterator final : irs::columnstore_iterator {
 public:
  virtual const value_type& value() const override { return EOFMAX; }
  virtual const value_type& seek(irs::doc_id_t) override { return EOFMAX; }
  virtual bool next() override { return false; }
}; // empty_column_iterator

iresearch::columnstore_reader::values_reader_f INVALID_COLUMN =
  [] (irs::doc_id_t, irs::bytes_ref&) { return false; };

NS_END

NS_ROOT

DEFINE_ATTRIBUTE_TYPE(iresearch::term_meta);

postings_writer::~postings_writer() {}
field_writer::~field_writer() {}

postings_reader::~postings_reader() {}
basic_term_reader::~basic_term_reader() {}
term_reader::~term_reader() {}
field_reader::~field_reader() {}

document_mask_writer::~document_mask_writer() {}
document_mask_reader::~document_mask_reader() {}

segment_meta_writer::~segment_meta_writer() {}
segment_meta_reader::~segment_meta_reader() {}

column_meta_writer::~column_meta_writer() {}
column_meta_reader::~column_meta_reader() {}

columnstore_writer::~columnstore_writer() {}
columnstore_reader::~columnstore_reader() {}

/* static */ columnstore_iterator::ptr columnstore_reader::empty_iterator() {
  return columnstore_iterator::make<empty_column_iterator>();
}

/* static */ const columnstore_reader::values_reader_f& columnstore_reader::empty_reader() {
  return INVALID_COLUMN;
}

index_meta_writer::~index_meta_writer() {}
/* static */void index_meta_writer::complete(index_meta& meta) NOEXCEPT {
  meta.last_gen_ = meta.gen_;
}
/* static */ void index_meta_writer::prepare(index_meta& meta) {
  meta.gen_ = meta.next_generation();
}

index_meta_reader::~index_meta_reader() {}

/* static */ void index_meta_reader::complete(
    index_meta& meta,
    uint64_t generation,
    uint64_t counter,
    index_meta::index_segments_t&& segments
) {
  meta.gen_ = generation;
  meta.last_gen_ = generation;
  meta.seg_counter_ = counter;
  meta.segments_ = std::move(segments);
}

format::~format() {}

/*static*/ format::ptr formats::get(const string_ref& name) {
  auto* factory = format_register::instance().get(name);
  return factory ? factory() : nullptr;
}

/*static*/ void formats::init() {
  #ifndef IRESEARCH_DLL
    REGISTER_FORMAT(iresearch::version10::format);
  #endif
}

/*static*/ void formats::load_all(const std::string& path) {
  load_libraries(path, FILENAME_PREFIX, "");
}

/*static*/ bool formats::visit(
  const std::function<bool(const string_ref&)>& visitor
) {
  return format_register::instance().visit(visitor);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               format registration
// -----------------------------------------------------------------------------

format_registrar::format_registrar(
    const format::type_id& type,
    format::ptr(*factory)(),
    const char* source /*= nullptr*/
) {
  auto entry = format_register::instance().set(type.name(), factory);

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering format, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering format, ignoring: type '%s'",
        type.name().c_str()
      );
    }

    IR_STACK_TRACE();
  }
}

format_registrar::operator bool() const NOEXCEPT {
  return registered_;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------