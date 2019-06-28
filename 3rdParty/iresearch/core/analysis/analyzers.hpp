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

#ifndef IRESEARCH_ANALYZERS_H
#define IRESEARCH_ANALYZERS_H

#include "shared.hpp"
#include "analyzer.hpp"
#include "utils/text_format.hpp"

NS_ROOT
NS_BEGIN(analysis)

// -----------------------------------------------------------------------------
// --SECTION--                                               analyzer definition
// -----------------------------------------------------------------------------

#define DECLARE_ANALYZER_TYPE() DECLARE_TYPE_ID(iresearch::analysis::analyzer::type_id)
#define DEFINE_ANALYZER_TYPE_NAMED(class_type, class_name) DEFINE_TYPE_ID(class_type, iresearch::analysis::analyzer::type_id) { \
  static iresearch::analysis::analyzer::type_id type(class_name); \
  return type; \
}
#define DEFINE_ANALYZER_TYPE(class_type) DEFINE_ANALYZER_TYPE_NAMED(class_type, #class_type)

// -----------------------------------------------------------------------------
// --SECTION--                                             analyzer registration
// -----------------------------------------------------------------------------

typedef irs::analysis::analyzer::ptr(*factory_f)(const irs::string_ref& args);
typedef bool(*normalizer_f)(const irs::string_ref& args, std::string& config);

class IRESEARCH_API analyzer_registrar {
 public:
   analyzer_registrar(
    const analyzer::type_id& type,
    const irs::text_format::type_id& args_format,
    factory_f factory,
    normalizer_f normalizer,
    const char* source = nullptr
  );
  operator bool() const NOEXCEPT;
 private:
  bool registered_;
};

#define REGISTER_ANALYZER__(analyzer_name, args_format, factory, normalizer, line, source) static iresearch::analysis::analyzer_registrar analyzer_registrar ## _ ## line(analyzer_name::type(), args_format, &factory, &normalizer, source)
#define REGISTER_ANALYZER_EXPANDER__(analyzer_name, args_format, factory, normalizer, file, line) REGISTER_ANALYZER__(analyzer_name, args_format, factory, normalizer, line, file ":" TOSTRING(line))
#define REGISTER_ANALYZER(analyzer_name, args_format, factory, normalizer) REGISTER_ANALYZER_EXPANDER__(analyzer_name, args_format, factory, normalizer, __FILE__, __LINE__)
#define REGISTER_ANALYZER_CSV(analyzer_name, factory, normalizer) REGISTER_ANALYZER(analyzer_name, ::iresearch::text_format::csv, factory, normalizer)
#define REGISTER_ANALYZER_JSON(analyzer_name, factory, normalizer) REGISTER_ANALYZER(analyzer_name, ::iresearch::text_format::json, factory, normalizer)
#define REGISTER_ANALYZER_TEXT(analyzer_name, factory, normalizer) REGISTER_ANALYZER(analyzer_name, ::iresearch::text_format::text, factory, normalizer)
#define REGISTER_ANALYZER_XML(analyzer_name, factory, normalizer) REGISTER_ANALYZER(analyzer_name, ::iresearch::text_format::xml, factory, normalizer)
#define REGISTER_ANALYZER_TYPED(analyzer_name, args_format) REGISTER_ANALYZER(analyzer_name, args_format, analyzer_name::make)

// -----------------------------------------------------------------------------
// --SECTION--                                               convinience methods
// -----------------------------------------------------------------------------

class IRESEARCH_API analyzers {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether an analyzer with the specified name is registered
  ////////////////////////////////////////////////////////////////////////////////
  static bool exists(
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    bool load_library = true
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief normalized arguments for an analyzer specified by name and store them
  ///        in 'out' argument
  /// @returns true on success, false - otherwise
  ////////////////////////////////////////////////////////////////////////////////
  static bool normalize(
    std::string& out,
    const string_ref& name,
    const irs::text_format::type_id& args_format,
    const string_ref& args,
    bool load_library = true
  ) NOEXCEPT;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find an analyzer by name, or nullptr if not found
  ///        indirect call to <class>::make(...)
  ///        requires use of DECLARE_FACTORY() in class definition
  ///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
  ////////////////////////////////////////////////////////////////////////////////
  static analyzer::ptr get(
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
  /// @brief load all analyzers from plugins directory
  ////////////////////////////////////////////////////////////////////////////////
  static void load_all(const std::string& path);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief visit all loaded analyzers, terminate early if visitor returns false
  ////////////////////////////////////////////////////////////////////////////////
  static bool visit(
    const std::function<bool(const string_ref&, const irs::text_format::type_id&)>& visitor
  );

 private:
  analyzers() = delete;
};

NS_END // NS_BEGIN(analysis)
NS_END

#endif
