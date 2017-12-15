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
  #include "text_token_stream.hpp"
#endif

#include "analyzers.hpp"

NS_LOCAL

const std::string FILENAME_PREFIX("libanalyzer-");

class analyzer_register:
  public irs::tagged_generic_register<irs::string_ref, irs::analysis::analyzer::ptr(*)(const irs::string_ref& args), irs::string_ref, analyzer_register> {
 protected:
  virtual std::string key_to_filename(const key_type& key) const override {
    std::string filename(FILENAME_PREFIX.size() + key.size(), 0);

    std::memcpy(&filename[0], FILENAME_PREFIX.c_str(), FILENAME_PREFIX.size());
    std::memcpy(&filename[0] + FILENAME_PREFIX.size(), key.c_str(), key.size());

    return filename;
  }
};

NS_END

NS_ROOT
NS_BEGIN(analysis)

/*static*/ analyzer::ptr analyzers::get(
  const string_ref& name, const string_ref& args
) {
  auto* factory = analyzer_register::instance().get(name);
  return factory ? factory(args) : nullptr;
}

/*static*/ void analyzers::init() {
  #ifndef IRESEARCH_DLL
    REGISTER_ANALYZER_TEXT(irs::analysis::delimited_token_stream, irs::analysis::delimited_token_stream::make);  // match delimited_token_stream.cpp
    REGISTER_ANALYZER_JSON(irs::analysis::text_token_stream, irs::analysis::text_token_stream::make); // match text_token_stream.cpp
    REGISTER_ANALYZER_TEXT(irs::analysis::text_token_stream, irs::analysis::text_token_stream::make); // match text_token_stream.cpp
  #endif
}

/*static*/ void analyzers::load_all(const std::string& path) {
  load_libraries(path, FILENAME_PREFIX, "");
}

/*static*/ bool analyzers::visit(
  const std::function<bool(const string_ref&)>& visitor
) {
  return analyzer_register::instance().visit(visitor);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             analyzer registration
// -----------------------------------------------------------------------------

analyzer_registrar::analyzer_registrar(
    const analyzer::type_id& type,
    const irs::text_format::type_id& args_format,
    analyzer::ptr(*factory)(const irs::string_ref& args),
    const char* source /*= nullptr*/
) {
  irs::string_ref source_ref(source);
  auto entry = analyzer_register::instance().set(
    type.name(),
    factory,
    source_ref.null() ? nullptr : &source_ref
  );

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    auto* registered_source = analyzer_register::instance().tag(type.name());

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

    IR_STACK_TRACE();
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