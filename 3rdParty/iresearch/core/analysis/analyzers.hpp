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

class IRESEARCH_API analyzer_registrar {
 public:
  analyzer_registrar(
    const analyzer::type_id& type,
    analyzer::ptr(*factory)(const iresearch::string_ref& args),
    const char* source = nullptr
  );
  operator bool() const NOEXCEPT;
 private:
  bool registered_;
};

#define REGISTER_ANALYZER__(analyzer_name, line, source) static iresearch::analysis::analyzer_registrar analyzer_registrar ## _ ## line(analyzer_name::type(), &analyzer_name::make, source)
#define REGISTER_ANALYZER_EXPANDER__(analyzer_name, file, line) REGISTER_ANALYZER__(analyzer_name, line, file ":" TOSTRING(line))
#define REGISTER_ANALYZER(analyzer_name) REGISTER_ANALYZER_EXPANDER__(analyzer_name, __FILE__, __LINE__)

// -----------------------------------------------------------------------------
// --SECTION--                                               convinience methods
// -----------------------------------------------------------------------------

class IRESEARCH_API analyzers {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find an analyzer by name, or nullptr if not found
  ///        indirect call to <class>::make(...)
  ///        requires use of DECLARE_FACTORY_DEFAULT() in class definition
  ///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
  ////////////////////////////////////////////////////////////////////////////////
  static analyzer::ptr get(const string_ref& name, const string_ref& args);

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
  static bool visit(const std::function<bool(const string_ref&)>& visitor);

 private:
  analyzers() = delete;
};

NS_END // NS_BEGIN(analysis)
NS_END

#endif