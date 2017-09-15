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

#include "utils/register.hpp"

// list of statically loaded scorers via init()
#ifndef IRESEARCH_DLL
  #include "text_token_stream.hpp"
#endif

#include "analyzers.hpp"

NS_LOCAL

const std::string FILENAME_PREFIX("libanalyzer-");

class analyzer_register:
  public iresearch::generic_register<iresearch::string_ref, iresearch::analysis::analyzer::ptr(*)(const iresearch::string_ref& args), analyzer_register> {
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
    REGISTER_ANALYZER(iresearch::analysis::text_token_stream);
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
    analyzer::ptr(*factory)(const iresearch::string_ref& args),
    const char* source /*= nullptr*/
) {
  auto entry = analyzer_register::instance().set(type.name(), factory);

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering analyzer, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
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