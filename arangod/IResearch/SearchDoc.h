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

namespace iresearch {

struct sub_reader;

}

namespace arangodb::iresearch {

constexpr size_t kSearchDocBufSize = sizeof(size_t) + sizeof(irs::doc_id_t);

class SearchDoc {
 public:
  static SearchDoc decode(std::string_view buf) noexcept {
    if (ADB_LIKELY(kSearchDocBufSize == buf.size())) {
      SearchDoc res;
      std::memcpy(&res._segment, buf.data(), sizeof res._segment);
      std::memcpy(&res._doc, buf.data() + sizeof res._segment, sizeof res._doc);
      return res;
    }
    return {};
  }

  constexpr SearchDoc() = default;

  SearchDoc(irs::sub_reader const& segment, irs::doc_id_t doc) noexcept
      : _segment{&segment}, _doc{doc} {}

  irs::sub_reader const* segment() const noexcept { return _segment; }

  irs::doc_id_t doc() const noexcept { return _doc; }

  bool isValid() const noexcept {
    return _segment && irs::doc_limits::valid(_doc);
  }

  constexpr auto operator<=>(const SearchDoc&) const noexcept = default;

  constexpr bool operator==(const SearchDoc& rhs) const noexcept {
    return _segment == rhs._segment && _doc == rhs._doc;
  }

  std::string_view encode(
      std::span<char, kSearchDocBufSize> buf) const noexcept {
    std::memcpy(buf.data(), static_cast<void const*>(&_segment),
                sizeof _segment);
    std::memcpy(buf.data() + sizeof _segment, &_doc, sizeof _doc);
    return {buf.data(), buf.size()};
  }

 private:
  irs::sub_reader const* _segment{};
  irs::doc_id_t _doc{irs::doc_limits::invalid()};
};

}  // namespace arangodb::iresearch
