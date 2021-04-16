////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#ifndef ARANGOD_IRESEARCH_VPACK_TERM_ATTRIBUTE
#define ARANGOD_IRESEARCH_VPACK_TERM_ATTRIBUTE
#include <utils/attributes.hpp>
#include <utils/bit_utils.hpp>
#include <velocypack/Slice.h>

namespace arangodb {
namespace iresearch {

struct VPackTermAttribute final : irs::attribute {
  static constexpr irs::string_ref type_name() noexcept { return "vpack_term_attribute"; }

  ::arangodb::velocypack::Slice value;
};

} // namespace iresearch
} // namespace arangodb

#endif // ARANGOD_IRESEARCH_VPACK_TERM_ATTRIBUTE
