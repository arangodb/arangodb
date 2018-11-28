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

class format_register :
  public irs::tagged_generic_register<irs::string_ref, irs::format::ptr(*)(), irs::string_ref, format_register> {
 protected:
  virtual std::string key_to_filename(const key_type& key) const override {
    std::string filename(FILENAME_PREFIX.size() + key.size(), 0);

    std::memcpy(
      &filename[0],
      FILENAME_PREFIX.c_str(),
      FILENAME_PREFIX.size()
    );

    irs::string_ref::traits_type::copy(
      &filename[0] + FILENAME_PREFIX.size(),
      key.c_str(),
      key.size()
    );

    return filename;
  }
}; // format_register

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

/* static */ const columnstore_reader::values_reader_f& columnstore_reader::empty_reader() {
  return INVALID_COLUMN;
}

index_meta_writer::~index_meta_writer() {}
/* static */void index_meta_writer::complete(index_meta& meta) NOEXCEPT {
  meta.last_gen_ = meta.gen_;
}
/* static */ void index_meta_writer::prepare(index_meta& meta) NOEXCEPT {
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

/*static*/ bool formats::exists(const string_ref& name) {
  return nullptr != format_register::instance().get(name);
}

/*static*/ format::ptr formats::get(const string_ref& name) NOEXCEPT {
  try {
    auto* factory = format_register::instance().get(name);

    return factory ? factory() : nullptr;
  } catch (...) {
    IR_FRMT_ERROR("Caught exception while getting a format instance");
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

/*static*/ void formats::init() {
#ifndef IRESEARCH_DLL
  irs::version10::init();
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
  irs::string_ref source_ref(source);
  auto entry = format_register::instance().set(
    type.name(),
    factory,
    source_ref.null() ? nullptr : &source_ref
  );

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    auto* registered_source = format_register::instance().tag(type.name());

    if (source && registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering format, ignoring: type '%s' from %s, previously from %s",
        type.name().c_str(),
        source,
        registered_source->c_str()
      );
    } else if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering format, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else if (registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering format, ignoring: type '%s', previously from %s",
        type.name().c_str(),
        registered_source->c_str()
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering format, ignoring: type '%s'",
        type.name().c_str()
      );
    }

    IR_LOG_STACK_TRACE();
  }
}

format_registrar::operator bool() const NOEXCEPT {
  return registered_;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
