////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

irs::string_ref columnCompressionToString(irs::type_info::type_id type) noexcept {
  if (ADB_UNLIKELY(type == nullptr)) {
    TRI_ASSERT(false);
    return irs::string_ref::EMPTY;
  }
  auto const mangled_name = type().name();
  TRI_ASSERT(!mangled_name.empty());
  auto demangled_start = mangled_name.end() - 1;
  while (demangled_start != mangled_name.begin() && *(demangled_start-1) != ':') {
    demangled_start--;
  }
  return irs::string_ref(demangled_start, std::distance(demangled_start, mangled_name.end()));
}

irs::type_info::type_id columnCompressionFromString(irs::string_ref const& c) noexcept {
  TRI_ASSERT(!c.null());
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (c == "test") {
    return irs::type<irs::compression::mock::test_compressor>::id();
  }
#endif
  if (c == "lz4") {
    return irs::type<irs::compression::lz4>::id();
  }
  if (c == "none") {
    return irs::type<irs::compression::none>::id();
  } 
  return nullptr;
}

irs::type_info::type_id getDefaultCompression() noexcept {
  return irs::type<irs::compression::lz4>::id();
}

}  // namespace iresearch
}  // namespace arangodb

