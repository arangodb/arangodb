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

#include "scorers.hpp"

#include "shared.hpp"
// list of statically loaded scorers via init()
#ifndef IRESEARCH_DLL
  #include "tfidf.hpp"
  #include "bm25.hpp"
  #include "boost_sort.hpp"
#endif
#include "utils/register.hpp"
#include "utils/hash_utils.hpp"

namespace {

struct entry_key_t {
  irs::type_info args_format_;
  const irs::string_ref name_;

  entry_key_t(
      const irs::string_ref& name,
      const irs::type_info& args_format)
    : args_format_(args_format), name_(name) {
  }

  bool operator==(const entry_key_t& other) const noexcept {
    return args_format_ == other.args_format_ && name_ == other.name_;
  }
};

}

namespace std {

template<>
struct hash<entry_key_t> {
  size_t operator()(const entry_key_t& value) const {
    return irs::hash_combine(
          std::hash<irs::type_info::type_id>()(value.args_format_.id()),
          std::hash<irs::string_ref>()(value.name_));
  }
}; // hash

} // std

namespace {

const std::string FILENAME_PREFIX("libscorer-");

class scorer_register:
  public irs::tagged_generic_register<entry_key_t, irs::sort::ptr(*)(const irs::string_ref& args), irs::string_ref, scorer_register> {
 protected:
  virtual std::string key_to_filename(const key_type& key) const override {
    auto& name = key.name_;
    std::string filename(FILENAME_PREFIX.size() + name.size(), 0);

    std::memcpy(
      &filename[0],
      FILENAME_PREFIX.c_str(),
      FILENAME_PREFIX.size()
    );

    irs::string_ref::traits_type::copy(
      &filename[0] + FILENAME_PREFIX.size(),
      name.c_str(),
      name.size()
    );

    return filename;
  }
};

}

namespace iresearch {

/*static*/ bool scorers::exists(
    const string_ref& name,
    const type_info& args_format,
    bool load_library /*= true*/) {
  return nullptr != scorer_register::instance().get(entry_key_t(name, args_format),load_library);
}

/*static*/ sort::ptr scorers::get(
    const string_ref& name,
    const type_info& args_format,
    const string_ref& args,
    bool load_library /*= true*/) noexcept {
  try {
    auto* factory = scorer_register::instance().get(
      entry_key_t(name, args_format), load_library
    );

    return factory ? factory(args) : nullptr;
  } catch (...) {
    IR_FRMT_ERROR("Caught exception while getting a scorer instance");
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

/*static*/ void scorers::init() {
  #ifndef IRESEARCH_DLL
    irs::bm25_sort::init();
    irs::tfidf_sort::init();
    irs::boost_sort::init();
  #endif
}

/*static*/ void scorers::load_all(const std::string& path) {
  load_libraries(path, FILENAME_PREFIX, "");
}

/*static*/ bool scorers::visit(
    const std::function<bool(const string_ref&, const type_info&)>& visitor) {
  scorer_register::visitor_t wrapper = [&visitor](const entry_key_t& key)->bool {
    return visitor(key.name_, key.args_format_);
  };

  return scorer_register::instance().visit(wrapper);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               scorer registration
// -----------------------------------------------------------------------------

scorer_registrar::scorer_registrar(
    const type_info& type,
    const type_info& args_format,
    sort::ptr(*factory)(const irs::string_ref& args),
    const char* source /*= nullptr*/) {
  irs::string_ref source_ref(source);
  auto entry = scorer_register::instance().set(
    entry_key_t(type.name(), args_format),
    factory,
    source_ref.null() ? nullptr : &source_ref);

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    auto* registered_source =
      scorer_register::instance().tag(entry_key_t(type.name(), args_format));

    if (source && registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering scorer, ignoring: type '%s' from %s, previously from %s",
        type.name().c_str(),
        source,
        registered_source->c_str()
      );
    } else if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering scorer, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else if (registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering scorer, ignoring: type '%s', previously from %s",
        type.name().c_str(),
        registered_source->c_str()
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering scorer, ignoring: type '%s'",
        type.name().c_str()
      );
    }
  }}

scorer_registrar::operator bool() const noexcept {
  return registered_;
}

}
