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

#include <bit>
#include <cstdint>

#include "Assertions/Assert.h"
#include "Replication2/Storage/WAL/RecordType.h"

namespace arangodb::replication2::storage::wal {

#ifdef RECORD_UNIT_TEST
// In order to avoid an ODR violation for `RecordTest.cpp`, we need to put this
// in an anonymous namespace.
namespace {
#endif

// An record in the WAL consists of a (compressed) header, the payload, some
// optional padding and a footer.
// We want everything to be 8 byte aligned, so we squeeze the index, term, type
// and size into the 24 byte CompressedHeader with the following structure
//   index    = 64bits
//   term     = 44bits
//   reserved = 16bits
//   type     = 4bits
//   payloadSize  = 64bits
// The reserved bits can be used in the future, e.g., to include a tag.
// The payload size is the actual size of the payload without padding. Since we
// want everything to be 8 byte aligned, we may have to include some optional
// padding after the payload.
// The footer consists of a crc32, some padding and a size. The crc32 contains
// everything from beginning of the header up to the beginning of the footer,
// i.e., the header, the payload and the padding. The footer size is the
// complete size of the entry, i.e., the header, the payload, the padding and
// the footer. This allows us to scan the WAL backwards.
struct Record {
  struct CompressedHeader;

  static constexpr std::uint8_t alignment = 8;
  static_assert(std::has_single_bit(alignment),
                "alignment must be a power of 2");

  struct Header {
    Header() = default;
    explicit Header(CompressedHeader);

    std::uint64_t index = 0;
    std::uint64_t term = 0;
    std::uint16_t reserved = 0;
    RecordType type;
    std::uint64_t payloadSize = 0;
  };

  struct CompressedHeader {
    CompressedHeader() = default;
    explicit CompressedHeader(Header);

    std::uint64_t index;
    std::uint64_t termAndType;
    std::uint64_t payloadSize;

    static constexpr unsigned indexBits = 64;
    static constexpr unsigned termBits = 44;
    static constexpr unsigned reservedBits = 16;
    static constexpr unsigned typeBits = 4;
    static constexpr unsigned payloadSizeBits = 64;

    static_assert(termBits + reservedBits + typeBits == 64);
    static_assert(indexBits + termBits + reservedBits + typeBits +
                      payloadSizeBits ==
                  192);

    std::uint64_t term() const noexcept {
      return termAndType >> (reservedBits + typeBits);
    }
    std::uint16_t reserved() const noexcept {
      return (termAndType >> typeBits) & ((1 << reservedBits) - 1);
    }
    RecordType type() const noexcept {
      return static_cast<RecordType>(termAndType & ((1 << typeBits) - 1));
    }
  };
  static_assert(sizeof(CompressedHeader) == 24);

  static std::uint64_t paddedPayloadSize(std::uint64_t size) {
    // we round to the next multiple of 8
    return (size + alignment - 1) & ~(alignment - 1);
  }

  struct Footer {
    std::uint32_t crc32 = 0;
    std::uint32_t padding = 0;  // reserved for future use
    std::uint64_t size = 0;
  };
};

inline Record::Header::Header(CompressedHeader h)
    : index(h.index),
      term(h.term()),
      reserved(h.reserved()),
      type(h.type()),
      payloadSize(h.payloadSize) {
  // in preparation for future use the unit tests actually test that the
  // reserved bits are compressed/decompressed correctly
#ifndef RECORD_UNIT_TEST
  TRI_ASSERT(reserved == 0);
#endif
}

inline Record::CompressedHeader::CompressedHeader(Header h)
    : index(h.index),
      termAndType((h.term << (reservedBits + typeBits)) |
                  (h.reserved << typeBits) |
                  static_cast<std::uint64_t>(h.type)),
      payloadSize(h.payloadSize) {
#ifndef RECORD_UNIT_TEST
  TRI_ASSERT(h.reserved == 0);
#endif
}

#ifdef RECORD_UNIT_TEST
}  // anonymous namespace
#endif
}  // namespace arangodb::replication2::storage::wal
