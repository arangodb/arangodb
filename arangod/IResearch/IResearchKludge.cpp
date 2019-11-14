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

namespace arangodb {
namespace iresearch {
namespace kludge {

const char TYPE_DELIMITER = '\0';
const char ANALYZER_DELIMITER = '\1';

irs::string_ref const NULL_SUFFIX("\0_n", 3);
irs::string_ref const BOOL_SUFFIX("\0_b", 3);
irs::string_ref const NUMERIC_SUFFIX("\0_d", 3);

void mangleType(std::string& name) { name += TYPE_DELIMITER; }

void mangleAnalyzer(std::string& name) { name += ANALYZER_DELIMITER; }

void mangleNull(std::string& name) {
  name.append(NULL_SUFFIX.c_str(), NULL_SUFFIX.size());
}

void mangleBool(std::string& name) {
  name.append(BOOL_SUFFIX.c_str(), BOOL_SUFFIX.size());
}

void mangleNumeric(std::string& name) {
  name.append(NUMERIC_SUFFIX.c_str(), NUMERIC_SUFFIX.size());
}

void mangleStringField(
    std::string& name,
    arangodb::iresearch::FieldMeta::Analyzer const& analyzer) {
  name += ANALYZER_DELIMITER;
  name += analyzer._shortName;
}

void demangleStringField(
    std::string& name,
    arangodb::iresearch::FieldMeta::Analyzer const& analyzer) {
  // +1 for preceding '\1'
  auto const suffixSize = 1 + analyzer._shortName.size();

  TRI_ASSERT(name.size() >= suffixSize);
  name.resize(name.size() - suffixSize);
}

}  // namespace kludge
}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
