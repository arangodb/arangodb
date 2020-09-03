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
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/IResearchIdentityAnalyzer.h"
#include "IResearch/IResearchVPackFormat.h"

namespace arangodb {
namespace iresearch {

REGISTER_ANALYZER_VPACK(IdentityAnalyzer, IdentityAnalyzer::make, IdentityAnalyzer::normalize);

IdentityAnalyzer::IdentityAnalyzer() noexcept
  : irs::analysis::analyzer(irs::type<IdentityAnalyzer>::get()),
    _empty(true) {
}


irs::attribute* IdentityAnalyzer::get_mutable(irs::type_info::type_id type) noexcept {
  if (type == irs::type<irs::increment>::id()) {
    return &_inc;
  }

  return type == irs::type<irs::term_attribute>::id()
      ? &_term
      : nullptr;
}

} // iresearch
} // arangodb
