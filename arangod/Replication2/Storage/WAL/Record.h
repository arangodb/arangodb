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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/Storage/WAL/RecordType.h"

#include <cstdint>

namespace arangodb::replication2::storage::wal {

// An record in the WAL consists of a (compressed) header, the payload and a
// footer.
// We want everything to be 8 byte aligned, so we squeeze the index, term,
// type and size into the 24 byte CompressedHeader with the following structure
//   index = 64bits
//   term  = 44bits
//   tag   = 16bits
//   type  = 4bits
//   size  = 64bits
struct Record {
  struct CompressedHeader;

  struct Header {
    Header() = default;
    explicit Header(CompressedHeader);

    std::uint64_t index = 0;
    std::uint64_t term = 0;
    std::uint16_t tag = 0;
    RecordType type;
    std::uint64_t size = 0;
  };

  struct CompressedHeader {
    CompressedHeader() = default;
    explicit CompressedHeader(Header);

    std::uint64_t index;
    std::uint64_t termTagAndType;
    std::uint64_t size;

    static constexpr unsigned indexBits = 64;
    static constexpr unsigned termBits = 44;
    static constexpr unsigned tagBits = 16;
    static constexpr unsigned typeBits = 4;
    static constexpr unsigned sizeBits = 64;

    static_assert(termBits + tagBits + typeBits == 64);
    static_assert(indexBits + termBits + tagBits + typeBits + sizeBits == 192);

    std::uint64_t term() const noexcept {
      return termTagAndType >> (tagBits + typeBits);
    }
    std::uint16_t tag() const noexcept {
      return (termTagAndType >> typeBits) & ((1 << tagBits) - 1);
    }
    RecordType type() const noexcept {
      return static_cast<RecordType>(termTagAndType & ((1 << typeBits) - 1));
    }
  };
  static_assert(sizeof(CompressedHeader) == 24);

  static std::uint32_t paddedPayloadSize(std::uint32_t size) {
    // we round to the next multiple of 8
    return (size + 7) & ~7;
  }

  struct Footer {
    std::uint32_t crc32 = 0;
    std::uint32_t padding = 0;
    std::uint64_t size = 0;
  };
};

inline Record::Header::Header(CompressedHeader h)
    : index(h.index),
      term(h.term()),
      tag(h.tag()),
      type(h.type()),
      size(h.size) {}

inline Record::CompressedHeader::CompressedHeader(Header h)
    : index(h.index),
      termTagAndType((h.term << (tagBits + typeBits)) | (h.tag << typeBits) |
                     static_cast<std::uint64_t>(h.type)),
      size(h.size) {}

}  // namespace arangodb::replication2::storage::wal
