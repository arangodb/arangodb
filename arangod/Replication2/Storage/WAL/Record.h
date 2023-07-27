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
// type and size into the 16 byte CompressedHeader with the following structure
//   index = 48bits
//   term  = 42bits
//   type  = 6bits
//   size  = 32bits
struct Record {
  struct CompressedHeader;

  struct Header {
    Header() = default;
    explicit Header(CompressedHeader);

    std::uint64_t index;
    std::uint64_t term;
    RecordType type;
    std::uint32_t size;
  };

  struct CompressedHeader {
    CompressedHeader() = default;
    explicit CompressedHeader(Header);

    std::uint64_t indexAndTerm;
    std::uint32_t termAndType;
    std::uint32_t size;

    static constexpr unsigned indexBits = 48;
    static constexpr unsigned termBits = 44;
    static constexpr unsigned typeBits = 4;
    static constexpr unsigned sizeBits = 32;
    static_assert(indexBits + termBits + typeBits + sizeBits == 128);

    std::uint64_t index() const noexcept {
      return indexAndTerm >> numTermBitsInIndexAndTerm;
    }
    std::uint64_t term() const noexcept {
      return ((indexAndTerm & termMask)
              << (CompressedHeader::termBits - numTermBitsInIndexAndTerm)) |
             (termAndType >> CompressedHeader::typeBits);
    }
    RecordType type() const noexcept {
      return static_cast<RecordType>(termAndType & ((1 << typeBits) - 1));
    }

   private:
    static constexpr unsigned numTermBitsInIndexAndTerm =
        (64 - CompressedHeader::indexBits);
    static constexpr std::uint64_t termMask =
        (1 << numTermBitsInIndexAndTerm) - 1;
  };
  static_assert(sizeof(CompressedHeader) == 16);

  static std::uint32_t paddedPayloadSize(std::uint32_t size) {
    // we round to the next multiple of 8
    return (size + 7) & ~7;
  }

  struct Footer {
    std::uint32_t crc32;
    std::uint32_t size;
  };
};

inline Record::Header::Header(CompressedHeader h)
    : index(h.index()), term(h.term()), type(h.type()), size(h.size) {}

inline Record::CompressedHeader::CompressedHeader(Header h)
    : indexAndTerm((h.index << numTermBitsInIndexAndTerm) |
                   (h.term >> (termBits - numTermBitsInIndexAndTerm))),
      termAndType(static_cast<std::uint32_t>(h.term << typeBits) |
                  static_cast<std::uint32_t>(h.type)),
      size(h.size) {}

}  // namespace arangodb::replication2::storage::wal
