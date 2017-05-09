////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;
using namespace arangodb::velocypack;

RocksDBPrefixExtractor::RocksDBPrefixExtractor()
    : _name("ArangoRocksDBPrefixExtractor") {}

RocksDBPrefixExtractor::~RocksDBPrefixExtractor() {}

const char* RocksDBPrefixExtractor::Name() const { return _name.data(); };

rocksdb::Slice RocksDBPrefixExtractor::Transform(
    rocksdb::Slice const& key) const {
  size_t length = _prefixLength[static_cast<uint8_t>(key[0])];
  return rocksdb::Slice(key.data(), length);
}

bool RocksDBPrefixExtractor::InDomain(rocksdb::Slice const& key) const {
  return ((key.size() > 0) &&
          (_prefixLength[static_cast<uint8_t>(key[0])] > 0) &&
          (_prefixLength[static_cast<uint8_t>(key[0])] <= key.size()));
}

bool RocksDBPrefixExtractor::InRange(rocksdb::Slice const& dst) const {
  return ((dst.size() > 0) &&
          (dst.size() == _prefixLength[static_cast<uint8_t>(dst[0])]));
}

size_t RocksDBPrefixExtractor::getPrefixLength(RocksDBEntryType type) {
  return _prefixLength[static_cast<uint8_t>(type)];
}

const size_t RocksDBPrefixExtractor::_prefixLength[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00 - 0x0f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x10 - 0x1f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x20 - 0x2f
    1, 1, 1, 9, 9, 9, 9, 9, 1, 1, 1, 1, 0, 0, 0, 0,  // 0x30 - 0x3f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x40 - 0x4f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x50 - 0x5f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x60 - 0x6f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x70 - 0x7f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80 - 0x8f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90 - 0x9f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xa0 - 0xaf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xb0 - 0xbf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xc0 - 0xcf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xd0 - 0xdf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xe0 - 0xef
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 0xf0 - 0xff
};
