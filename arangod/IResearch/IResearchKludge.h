////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_KLUDGE_H
#define ARANGOD_IRESEARCH__IRESEARCH_KLUDGE_H 1

////////////////////////////////////////////////////////////////////////////////
/// @brief common place for all kludges and temporary workarounds required for
///        integration if the IResearch library with ArangoDB
///        NOTE1: all functionality in this file is not nesesarily optimal
///        NOTE2: all functionality in this file is to be considered deprecated
////////////////////////////////////////////////////////////////////////////////

#include "IResearchLinkMeta.h"

namespace arangodb {
namespace iresearch {
namespace kludge {

void mangleType(std::string& name);
void mangleAnalyzer(std::string& name);

void mangleNull(std::string& name);
void mangleBool(std::string& name);
void mangleNumeric(std::string& name);

void mangleField(
  std::string& name,
  iresearch::FieldMeta::Analyzer const& analyzer);

}  // namespace kludge
}  // namespace iresearch
}  // namespace arangodb

#endif
