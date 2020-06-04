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

#include "type_id.hpp"

NS_ROOT
NS_BEGIN(text_format)

// -----------------------------------------------------------------------------
// --SECTION--                                            common textual formats
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @class CSV format type https://en.wikipedia.org/wiki/Comma-separated_values
////////////////////////////////////////////////////////////////////////////////
struct csv {
  static constexpr string_ref type_name() noexcept {
    return "csv";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @class jSON format type https://en.wikipedia.org/wiki/JSON
////////////////////////////////////////////////////////////////////////////////
struct json {
  static constexpr string_ref type_name() noexcept {
    return "json";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @class raw text format type without any specific encoding
////////////////////////////////////////////////////////////////////////////////
struct text {
  static constexpr string_ref type_name() noexcept {
    return "text";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @class XML format type https://en.wikipedia.org/wiki/XML
////////////////////////////////////////////////////////////////////////////////
struct xml {
  static constexpr string_ref type_name() noexcept {
    return "xml";
  }
};

NS_END // text_format
NS_END // ROOT

#endif
