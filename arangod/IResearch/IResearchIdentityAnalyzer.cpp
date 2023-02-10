////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/IResearchIdentityAnalyzer.h"

namespace arangodb {
namespace iresearch {

bool IdentityAnalyzer::normalize(std::string_view /*args*/, std::string& out) {
  out.resize(VPackSlice::emptyObjectSlice().byteSize());
  std::memcpy(&out[0], VPackSlice::emptyObjectSlice().begin(), out.size());
  return true;
}

irs::analysis::analyzer::ptr IdentityAnalyzer::make(std::string_view /*args*/) {
  return std::make_unique<IdentityAnalyzer>();
}

bool IdentityAnalyzer::normalize_json(std::string_view /*args*/,
                                      std::string& out) {
  out = "{}";
  return true;
}

irs::analysis::analyzer::ptr IdentityAnalyzer::make_json(
    std::string_view /*args*/) {
  return std::make_unique<IdentityAnalyzer>();
}

}  // namespace iresearch
}  // namespace arangodb
