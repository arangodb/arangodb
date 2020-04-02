////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchCompression.h"
#include "Basics/debugging.h"
#include <utils/lz4compression.hpp>
#ifdef ARANGODB_USE_GOOGLE_TESTS
#include "../tests/IResearch/IResearchTestCompressor.h"
#endif

namespace arangodb {
namespace iresearch {

irs::string_ref columnCompressionToString(irs::compression::type_id const* type) {
  if (ADB_UNLIKELY(type == nullptr)) {
    TRI_ASSERT(false);
    return irs::string_ref::EMPTY;
  }
  auto const& mangled_name = type->name();
  TRI_ASSERT(!mangled_name.empty());
  auto demandled_start = mangled_name.end() - 1;
  while (demandled_start != mangled_name.begin() && *(demandled_start-1) != ':') {
    demandled_start--;
  }
  return irs::string_ref(demandled_start, std::distance(demandled_start, mangled_name.end()));
}

irs::compression::type_id const*  columnCompressionFromString(irs::string_ref const& c) {
  TRI_ASSERT(!c.null());
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (c == "test") {
    return &irs::compression::mock::test_compressor::type();
  }
#endif
  if (c == "lz4") {
    return &irs::compression::lz4::type();
  }
  if (c == "none") {
    return &irs::compression::none::type();
  } 
  return nullptr;
}

irs::compression::type_id const& getDefaultCompression() {
  return irs::compression::lz4::type();
}

}  // namespace iresearch
}  // namespace arangodb

