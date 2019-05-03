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

#ifndef IRESEARCH_SCORERS_H
#define IRESEARCH_SCORERS_H

#include "shared.hpp"
#include "sort.hpp"
#include "utils/text_format.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                 scorer definition
// -----------------------------------------------------------------------------

#define DECLARE_SORT_TYPE() DECLARE_TYPE_ID(::iresearch::sort::type_id)
#define DEFINE_SORT_TYPE_NAMED(class_type, class_name) DEFINE_TYPE_ID(class_type, ::iresearch::sort::type_id) { \
  static ::iresearch::sort::type_id type(class_name); \
  return type; \
}
#define DEFINE_SORT_TYPE(class_type) DEFINE_SORT_TYPE_NAMED(class_type, #class_type)

// -----------------------------------------------------------------------------
// --SECTION--                                               scorer registration
// -----------------------------------------------------------------------------

class IRESEARCH_API scorer_registrar {
 public:
  scorer_registrar(
    const sort::type_id& type,
    const irs::text_format::type_id& args_format,
    sort::ptr(*factory)(const irs::string_ref& args),
    const char* source = nullptr
  );
  operator bool() const NOEXCEPT;
 private:
  bool registered_;
};

#define REGISTER_SCORER__(scorer_name, args_format, factory, line, source) static ::iresearch::scorer_registrar scorer_registrar ## _ ## line(scorer_name::type(), args_format, &factory, source)
#define REGISTER_SCORER_EXPANDER__(scorer_name, args_format, factory, file, line) REGISTER_SCORER__(scorer_name, args_format, factory, line, file ":" TOSTRING(line))
#define REGISTER_SCORER(scorer_name, args_format, factory) REGISTER_SCORER_EXPANDER__(scorer_name, args_format, factory, __FILE__, __LINE__)
#define REGISTER_SCORER_CSV(scorer_name, factory) REGISTER_SCORER(scorer_name, ::iresearch::text_format::csv, factory)
#define REGISTER_SCORER_JSON(scorer_name, factory) REGISTER_SCORER(scorer_name, ::iresearch::text_format::json, factory)
#define REGISTER_SCORER_TEXT(scorer_name, factory) REGISTER_SCORER(scorer_name, ::iresearch::text_format::text, factory)
#define REGISTER_SCORER_XML(scorer_name, factory) REGISTER_SCORER(scorer_name, ::iresearch::text_format::xml, factory)
#define REGISTER_SCORER_TYPED(scorer_name, args_format) REGISTER_SCORER(scorer_name, args_format, scorer_name::make)

// -----------------------------------------------------------------------------
// --SECTION--                                               convinience methods
// -----------------------------------------------------------------------------

class IRESEARCH_API scorers {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether scorer with a specified name is registered
  ////////////////////////////////////////////////////////////////////////////////
  static bool exists(
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    bool load_library = true
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find a scorer by name, or nullptr if not found
  ///        indirect call to <class>::make(...)
  ///        requires use of DECLARE_FACTORY() in class definition
  ///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
  ////////////////////////////////////////////////////////////////////////////////
  static sort::ptr get(
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    const string_ref& args,
    bool load_library = true
  ) NOEXCEPT;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief for static lib reference all known scorers in lib
  ///        for shared lib NOOP
  ///        no explicit call of fn is required, existence of fn is sufficient
  ////////////////////////////////////////////////////////////////////////////////
  static void init();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief load all scorers from plugins directory
  ////////////////////////////////////////////////////////////////////////////////
  static void load_all(const std::string& path);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief visit all loaded scorers, terminate early if visitor returns false
  ////////////////////////////////////////////////////////////////////////////////
  static bool visit(
    const std::function<bool(const string_ref&, const text_format::type_id&)>& visitor
  );

 private:
  scorers() = delete;
};

NS_END

#endif