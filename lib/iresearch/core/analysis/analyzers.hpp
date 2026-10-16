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

#pragma once

#include <velocypack/Slice.h>

#include <functional>

#include "analyzer.hpp"
#include "shared.hpp"
#include "utils/text_format.hpp"

namespace arangodb::velocypack {
class Builder;
}  // namespace arangodb::velocypack
namespace irs::analysis {

using factory_f = analysis::analyzer::ptr (*)(std::string_view args);
using normalizer_f = bool (*)(std::string_view args, std::string& config);

class analyzer_registrar {
 public:
  analyzer_registrar(const type_info& type, const type_info& args_format,
                     factory_f factory, normalizer_f normalizer,
                     const char* source = nullptr);

  operator bool() const noexcept { return registered_; }

 private:
  bool registered_;
};

namespace analyzers {
// Checks whether an analyzer with the specified name is registered.
bool exists(std::string_view name, const type_info& args_format,
            bool load_library = true);

// Normalize arguments for an analyzer specified by name and store them
// in 'out' argument.
// Returns true on success, false - otherwise
bool normalize(std::string& out, std::string_view name,
               const type_info& args_format, std::string_view args,
               bool load_library = true) noexcept;

// Find an analyzer by name, or nullptr if not found
// indirect call to <class>::make(...).
// NOTE: make(...) MUST be defined in CPP to ensire proper code scope
analyzer::ptr get(std::string_view name, const type_info& args_format,
                  std::string_view args, bool load_library = true) noexcept;

// Load all analyzers from plugins directory.
void load_all(std::string_view path);

// Visit all loaded analyzers, terminate early if visitor returns false.
bool visit(
  const std::function<bool(std::string_view, const type_info&)>& visitor);

bool MakeAnalyzer(arangodb::velocypack::Slice input, analyzer::ptr& output);
bool NormalizeAnalyzer(arangodb::velocypack::Slice input,
                       arangodb::velocypack::Builder& output);

}  // namespace analyzers
}  // namespace irs::analysis

#define REGISTER_ANALYZER__(analyzer_name, args_format, factory, normalizer, \
                            line, source)                                    \
  static ::irs::analysis::analyzer_registrar analyzer_registrar##_##line(    \
    ::irs::type<analyzer_name>::get(), ::irs::type<args_format>::get(),      \
    &factory, &normalizer, source)
#define REGISTER_ANALYZER_EXPANDER__(analyzer_name, args_format, factory,    \
                                     normalizer, file, line)                 \
  REGISTER_ANALYZER__(analyzer_name, args_format, factory, normalizer, line, \
                      file ":" IRS_TO_STRING(line))
#define REGISTER_ANALYZER(analyzer_name, args_format, factory, normalizer) \
  REGISTER_ANALYZER_EXPANDER__(analyzer_name, args_format, factory,        \
                               normalizer, __FILE__, __LINE__)
#define REGISTER_ANALYZER_JSON(analyzer_name, factory, normalizer)    \
  REGISTER_ANALYZER(analyzer_name, ::irs::text_format::json, factory, \
                    normalizer)
#define REGISTER_ANALYZER_TEXT(analyzer_name, factory, normalizer)    \
  REGISTER_ANALYZER(analyzer_name, ::irs::text_format::text, factory, \
                    normalizer)
#define REGISTER_ANALYZER_TYPED(analyzer_name, args_format) \
  REGISTER_ANALYZER(analyzer_name, args_format, analyzer_name::make)
#define REGISTER_ANALYZER_VPACK(analyzer_name, factory, normalizer)    \
  REGISTER_ANALYZER(analyzer_name, ::irs::text_format::vpack, factory, \
                    normalizer)
