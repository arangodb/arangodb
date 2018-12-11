////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_TEXT_FORMAT_H
#define IRESEARCH_TEXT_FORMAT_H

#include "noncopyable.hpp"
#include "type_id.hpp"

NS_ROOT
NS_BEGIN(text_format)

////////////////////////////////////////////////////////////////////////////////
/// @class base type_id for all textual format types
////////////////////////////////////////////////////////////////////////////////
class type_id: public irs::type_id, private util::noncopyable {
 public:
  type_id(const string_ref& name): name_(name) {}
  operator const type_id*() const { return this; }
  const string_ref& name() const { return name_; }

 private:
  string_ref name_;
};

// -----------------------------------------------------------------------------
// --SECTION--                                            common textual formats
// -----------------------------------------------------------------------------

#if !defined(_MSC_VER)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-variable"
#endif

////////////////////////////////////////////////////////////////////////////////
/// @class CSV format type https://en.wikipedia.org/wiki/Comma-separated_values
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API const type_id& csv_t();
IRESEARCH_IGNORE_UNUSED static const auto& csv = csv_t();

////////////////////////////////////////////////////////////////////////////////
/// @class jSON format type https://en.wikipedia.org/wiki/JSON
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API const type_id& json_t();
IRESEARCH_IGNORE_UNUSED static const auto& json = json_t();

////////////////////////////////////////////////////////////////////////////////
/// @class raw text format type without any specific encoding
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_IGNORE_UNUSED IRESEARCH_API const type_id& text_t();
static const auto& text = text_t();

////////////////////////////////////////////////////////////////////////////////
/// @class XML format type https://en.wikipedia.org/wiki/XML
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API const type_id& xml_t();
IRESEARCH_IGNORE_UNUSED static const auto& xml = xml_t();

#if !defined(_MSC_VER)
  #pragma GCC diagnostic pop
#endif

NS_END // text_format
NS_END // ROOT

#endif
