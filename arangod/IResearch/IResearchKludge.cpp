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

#include "IResearchKludge.h"

#include "Basics/Common.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)
NS_BEGIN(kludge)

void mangleNull(std::string& name) {
  static irs::string_ref const SUFFIX("\0_n", 3);
  name.append(SUFFIX.c_str(), SUFFIX.size());
}

void mangleBool(std::string& name) {
  static irs::string_ref const SUFFIX("\0_b", 3);
  name.append(SUFFIX.c_str(), SUFFIX.size());
}

void mangleNumeric(std::string& name) {
  static irs::string_ref const SUFFIX("\0_d", 3);
  name.append(SUFFIX.c_str(), SUFFIX.size());
}

void mangleStringField(std::string& name, TokenizerPoolPtr pool) {
  name += '\0';
  name += pool->name();
}

void unmangleStringField(std::string& name, TokenizerPoolPtr pool) {
  // +1 for preceding '\0'
  auto const suffixSize = 1 + pool->name().size();

  TRI_ASSERT(name.size() >= suffixSize);
  name.resize(name.size() - suffixSize);
}

NS_END // kludge
NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------