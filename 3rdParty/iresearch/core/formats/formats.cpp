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

#include "shared.hpp"
#include "utils/register.hpp"

// list of statically loaded formats via init()
#ifndef IRESEARCH_DLL
  #include "formats_10.hpp"
#endif

#include "formats.hpp"
#include "utils/type_limits.hpp"
#include "utils/hash_utils.hpp"

namespace {

const std::string FILENAME_PREFIX("libformat-");

// first - format name
// second - module name, nullptr => matches format name
typedef std::pair<irs::string_ref, irs::string_ref> key_t;

struct equal_to {
  bool operator()(const key_t& lhs, const key_t& rhs) const noexcept {
    return lhs.first == rhs.first;
  }
};

struct hash {
  size_t operator()(const key_t& lhs) const noexcept {
    return irs::hash_utils::hash(lhs.first);
  }
};

class format_register :
  public irs::tagged_generic_register<key_t, irs::format::ptr(*)(), irs::string_ref, format_register, hash, equal_to> {
 protected:
  virtual std::string key_to_filename(const key_type& key) const override {
    auto const& module = key.second.null() ? key.first : key.second;

    std::string filename(FILENAME_PREFIX.size() + module.size(), 0);

    std::memcpy(&filename[0], FILENAME_PREFIX.c_str(), FILENAME_PREFIX.size());

    irs::string_ref::traits_type::copy(
      &filename[0] + FILENAME_PREFIX.size(),
      module.c_str(), module.size());

    return filename;
  }
}; // format_register

iresearch::columnstore_reader::values_reader_f INVALID_COLUMN =
  [] (irs::doc_id_t, irs::bytes_ref&) { return false; };

}

namespace iresearch {

/* static */ const columnstore_reader::values_reader_f& columnstore_reader::empty_reader() {
  return INVALID_COLUMN;
}

/* static */void index_meta_writer::complete(index_meta& meta) noexcept {
  meta.last_gen_ = meta.gen_;
}
/* static */ void index_meta_writer::prepare(index_meta& meta) noexcept {
  meta.gen_ = meta.next_generation();
}

/* static */ void index_meta_reader::complete(
    index_meta& meta,
    uint64_t generation,
    uint64_t counter,
    index_meta::index_segments_t&& segments,
    bstring* payload) {
  meta.gen_ = generation;
  meta.last_gen_ = generation;
  meta.seg_counter_ = counter;
  meta.segments_ = std::move(segments);
  if (payload) {
    meta.payload(std::move(*payload));
  } else {
    meta.payload(bytes_ref::NIL);
  }
}

/*static*/ bool formats::exists(
    const string_ref& name,
    bool load_library /*= true*/) {
  auto const key = std::make_pair(name, string_ref::NIL);
  return nullptr != format_register::instance().get(key, load_library);
}

/*static*/ format::ptr formats::get(
    const string_ref& name,
    const string_ref& module /*= string_ref::NIL*/,
    bool load_library /*= true*/) noexcept {
  try {
    auto const key = std::make_pair(name, module);
    auto* factory = format_register::instance().get(key, load_library);

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
    const std::function<bool(const string_ref&)>& visitor) {
  auto visit_all = [&visitor](const format_register::key_type& key) {
    if (!visitor(key.first)) {
      return false;
    }
    return true;
  };

  return format_register::instance().visit(visit_all);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               format registration
// -----------------------------------------------------------------------------

format_registrar::format_registrar(
    const type_info& type,
    const string_ref& module,
    format::ptr(*factory)(),
    const char* source /*= nullptr*/) {
  string_ref source_ref(source);

  auto entry = format_register::instance().set(
    std::make_pair(type.name(), module),
    factory,
    source_ref.null() ? nullptr : &source_ref
  );

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    const auto key = std::make_pair(type.name(), string_ref::NIL);
    auto* registered_source = format_register::instance().tag(key);

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
  }
}

format_registrar::operator bool() const noexcept {
  return registered_;
}

}
