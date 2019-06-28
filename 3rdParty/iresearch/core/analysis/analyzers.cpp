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

#include "utils/register.hpp"

// list of statically loaded scorers via init()
#ifndef IRESEARCH_DLL
  #include "delimited_token_stream.hpp"
  #include "ngram_token_stream.hpp"
  #include "text_token_normalizing_stream.hpp"
  #include "text_token_stemming_stream.hpp"
  #include "text_token_stream.hpp"
  #include "token_masking_stream.hpp"
#endif

#include "analyzers.hpp"

NS_LOCAL

struct key {
  key(const irs::string_ref& name,
      const irs::text_format::type_id& args_format)
    : args_format(args_format),
      name(name) {
  }

  bool operator==(const key& other) const NOEXCEPT {
    return &args_format == &other.args_format && name == other.name;
  }

  bool operator!=(const key& other) const NOEXCEPT {
    return !(*this == other);
  }

  const irs::text_format::type_id& args_format;
  const irs::string_ref name;
};

struct value{
  explicit value(
      irs::analysis::factory_f factory = nullptr,
      irs::analysis::normalizer_f normalizer = nullptr)
    : factory(factory),
      normalizer(normalizer) {
  }

  bool empty() const NOEXCEPT { return nullptr == factory;  }

  bool operator==(const value& other) const NOEXCEPT {
    return factory == other.factory && normalizer == other.normalizer;
  }

  bool operator!=(const value& other) const NOEXCEPT {
    return !(*this == other);
  }

  const irs::analysis::factory_f factory;
  const irs::analysis::normalizer_f normalizer;
};
 
NS_END

NS_BEGIN(std)

template<>
struct hash<::key> {
  size_t operator()(const ::key& value) const NOEXCEPT {
    return std::hash<irs::string_ref>()(value.name);
  }
}; // hash

NS_END // std

NS_LOCAL

const std::string FILENAME_PREFIX("libanalyzer-");

class analyzer_register
    : public irs::tagged_generic_register<::key, ::value, irs::string_ref, analyzer_register> {
 protected:
  virtual std::string key_to_filename(const key_type& key) const override {
    auto& name = key.name;
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

NS_END

NS_ROOT
NS_BEGIN(analysis)

/*static*/ bool analyzers::exists(
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    bool load_library /*= true*/
) {
  return !analyzer_register::instance().get(::key(name, args_format), load_library).empty();
}

/*static*/ bool analyzers::normalize(
    std::string& out,
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    const string_ref& args,
    bool load_library /*= true*/
) NOEXCEPT {
  try {
    auto* normalizer = analyzer_register::instance().get(
      ::key(name, args_format),
      load_library
    ).normalizer;

    return normalizer ? normalizer(args, out) : false;
  } catch (...) {
    IR_FRMT_ERROR("Caught exception while normalizing analyzer '%s' arguments", name.c_str());
    IR_LOG_EXCEPTION();
  }

  return false;
}

/*static*/ analyzer::ptr analyzers::get(
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    const string_ref& args,
    bool load_library /*= true*/
) NOEXCEPT {
  try {
    auto* factory = analyzer_register::instance().get(
      ::key(name, args_format),
      load_library
    ).factory;

    return factory ? factory(args) : nullptr;
  } catch (...) {
    IR_FRMT_ERROR("Caught exception while getting an analyzer instance");
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

/*static*/ void analyzers::init() {
  #ifndef IRESEARCH_DLL
    irs::analysis::delimited_token_stream::init();
    irs::analysis::ngram_token_stream::init();
    irs::analysis::text_token_normalizing_stream::init();
    irs::analysis::text_token_stemming_stream::init();
    irs::analysis::text_token_stream::init();
    irs::analysis::token_masking_stream::init();
  #endif
}

/*static*/ void analyzers::load_all(const std::string& path) {
  load_libraries(path, FILENAME_PREFIX, "");
}

/*static*/ bool analyzers::visit(
  const std::function<bool(const string_ref&, const irs::text_format::type_id&)>& visitor
) {
  analyzer_register::visitor_t wrapper = [&visitor](const ::key& key)->bool {
    return visitor(key.name, key.args_format);
  };

  return analyzer_register::instance().visit(wrapper);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             analyzer registration
// -----------------------------------------------------------------------------

analyzer_registrar::analyzer_registrar(
    const analyzer::type_id& type,
    const irs::text_format::type_id& args_format,
    analyzer::ptr(*factory)(const irs::string_ref& args),
    bool(*normalizer)(const irs::string_ref& args, std::string& config),
    const char* source /*= nullptr*/
) {
  irs::string_ref source_ref(source);
  const auto new_entry = ::value(factory, normalizer);
  auto entry = analyzer_register::instance().set(
    ::key(type.name(), args_format),
    new_entry,
    source_ref.null() ? nullptr : &source_ref
  );

  registered_ = entry.second;

  if (!registered_ && new_entry != entry.first) {
    auto* registered_source =
      analyzer_register::instance().tag(::key(type.name(), args_format));

    if (source && registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering analyzer, ignoring: type '%s' from %s, previously from %s",
        type.name().c_str(),
        source,
        registered_source->c_str()
      );
    } else if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering analyzer, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else if (registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering analyzer, ignoring: type '%s', previously from %s",
        type.name().c_str(),
        registered_source->c_str()
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering analyzer, ignoring: type '%s'",
        type.name().c_str()
      );
    }

    IR_LOG_STACK_TRACE();
  }
}

analyzer_registrar::operator bool() const NOEXCEPT {
  return registered_;
}

NS_END // NS_BEGIN(analysis)
NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
