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

// list of statically loaded scorers via init()
#ifndef IRESEARCH_DLL
  #include "tfidf.hpp"
  #include "bm25.hpp"
#endif

#include "utils/register.hpp"
#include "scorers.hpp"

NS_LOCAL

const std::string FILENAME_PREFIX("libscorer-");

class scorer_register:
  public irs::generic_register<irs::string_ref, irs::sort::ptr(*)(const irs::string_ref& args), scorer_register> {
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

/*static*/ bool scorers::exists(const string_ref& name) {
  return scorer_register::instance().get(name);
}

/*static*/ sort::ptr scorers::get(
    const string_ref& name,
    const string_ref& args
) {
  auto* factory = scorer_register::instance().get(name);

  return factory ? factory(args) : nullptr;
}

/*static*/ void scorers::init() {
  #ifndef IRESEARCH_DLL
    REGISTER_SCORER_JSON(irs::bm25_sort, irs::bm25_sort::make); // match bm25.cpp
    REGISTER_SCORER_JSON(irs::tfidf_sort, irs::tfidf_sort::make); // match tfidf.cpp
  #endif
}

/*static*/ void scorers::load_all(const std::string& path) {
  load_libraries(path, FILENAME_PREFIX, "");
}

/*static*/ bool scorers::visit(
  const std::function<bool(const string_ref&)>& visitor
) {
  return scorer_register::instance().visit(visitor);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               scorer registration
// -----------------------------------------------------------------------------

scorer_registrar::scorer_registrar(
    const sort::type_id& type,
    const irs::text_format::type_id& args_format,
    sort::ptr(*factory)(const irs::string_ref& args),
    const char* source /*= nullptr*/
) {
  auto entry = scorer_register::instance().set(type.name(), factory);

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering scorer, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering scorer, ignoring: type '%s'",
        type.name().c_str()
      );
    }

    IR_STACK_TRACE();
  }}

scorer_registrar::operator bool() const NOEXCEPT {
  return registered_;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
