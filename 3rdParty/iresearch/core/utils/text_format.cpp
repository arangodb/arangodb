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

#include "string.hpp"

#include "text_format.hpp"

NS_LOCAL

static const irs::text_format::type_id CSV("csv");
static const irs::text_format::type_id JSON("json");
static const irs::text_format::type_id TEXT("text");
static const irs::text_format::type_id XML("xml");

NS_END // NS_LOCAL

NS_ROOT
NS_BEGIN(text_format)

const type_id& csv_t() { return CSV; }
const type_id& json_t() { return JSON; }
const type_id& text_t() { return TEXT; }
const type_id& xml_t() { return XML; }

NS_END // text_format
NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------