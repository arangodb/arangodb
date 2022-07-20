////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramo
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include <span>

#include "Aql/AqlValue.h"
#include "utils/type_limits.hpp"

namespace arangodb::iresearch {

constexpr size_t kSearchDocBufSize = sizeof(size_t) + sizeof(irs::doc_id_t);

inline std::string_view encodeSearchDoc(std::span<char, kSearchDocBufSize> buf,
                                        size_t segmentOffset,
                                        irs::doc_id_t docId) noexcept {
  std::memcpy(buf.data(), &segmentOffset, sizeof segmentOffset);
  std::memcpy(buf.data() + sizeof segmentOffset, &docId, sizeof docId);
  return {buf.data(), buf.size()};
}

inline std::pair<size_t, irs::doc_id_t> decodeSearchDoc(
    std::string_view buf) noexcept {
  if (ADB_LIKELY(kSearchDocBufSize == buf.size())) {
    std::pair<size_t, irs::doc_id_t> res;
    std::memcpy(&res.first, buf.data(), sizeof res.first);
    std::memcpy(&res.second, buf.data() + sizeof res.first, sizeof res.second);
    return res;
  }
  return {0, irs::doc_limits::invalid()};
}

}  // namespace arangodb::iresearch
