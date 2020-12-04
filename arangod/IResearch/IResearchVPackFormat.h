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

#include "analysis/analyzers.hpp"

namespace iresearch {
namespace text_format {

struct vpack {
  static constexpr irs::string_ref type_name() noexcept {
    return "vpack";
  }
};

} // iresearch
} // text_format

#define REGISTER_ANALYZER_VPACK(analyzer_name, factory, normalizer) \
  REGISTER_ANALYZER(analyzer_name, ::iresearch::text_format::vpack, factory, normalizer)

