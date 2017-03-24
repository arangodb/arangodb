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

#include "RocksDBEngine/RocksDBEntry.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::velocypack;

RocksDBEntry RocksDBEntry::Database(TRI_voc_tick_t databaseId, VPackSlice const& data) {
  return RocksDBEntry(RocksDBEntryType::Database, databaseId, data);
}

RocksDBEntry RocksDBEntry::Collection(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, VPackSlice const& data) {
  return RocksDBEntry(RocksDBEntryType::Collection, databaseId, collectionId, data);
}

RocksDBEntry RocksDBEntry::Index(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, TRI_idx_iid_t indexId, VPackSlice const& data) {
  return RocksDBEntry(RocksDBEntryType::Index, databaseId, collectionId, indexId, data);
}

RocksDBEntry RocksDBEntry::Document(uint64_t collectionId, TRI_voc_rid_t revisionId,
                                    VPackSlice const& data) {
  return RocksDBEntry(RocksDBEntryType::Document, collectionId, revisionId, data);
}

RocksDBEntry RocksDBEntry::IndexValue(uint64_t indexId, TRI_voc_rid_t revisionId,
                                      VPackSlice const& indexValues) {
  return RocksDBEntry(RocksDBEntryType::IndexValue, indexId, revisionId, indexValues);
}

RocksDBEntry RocksDBEntry::UniqueIndexValue(uint64_t indexId,
                                            TRI_voc_rid_t revisionId,
                                            VPackSlice const& indexValues) {
  return RocksDBEntry(RocksDBEntryType::UniqueIndexValue, indexId, revisionId, indexValues);
}

RocksDBEntry RocksDBEntry::View(TRI_voc_tick_t databaseId, TRI_voc_cid_t viewId, VPackSlice const& data) {
  return RocksDBEntry(RocksDBEntryType::View, databaseId, viewId, data);
}

RocksDBEntryType RocksDBEntry::type() const { return _type; }

TRI_voc_tick_t RocksDBEntry::databaseId() const {
  switch (_type) {
    case RocksDBEntryType::Database: {
      return uint64FromPersistent(_keyBuffer.data() + sizeof(char));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_cid_t RocksDBEntry::collectionId() const {
  switch (_type) {
    case RocksDBEntryType::Collection: {
      return uint64FromPersistent(_keyBuffer.data() + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_cid_t RocksDBEntry::viewId() const {
  switch (_type) {
    case RocksDBEntryType::View: {
      return uint64FromPersistent(_keyBuffer.data() + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}


TRI_idx_iid_t RocksDBEntry::indexId() const {
  switch (_type) {
    case RocksDBEntryType::Index: {
      return uint64FromPersistent(_keyBuffer.data() + sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_rid_t RocksDBEntry::revisionId() const {
  switch (_type) {
    case RocksDBEntryType::Document: {
      return uint64FromPersistent(_keyBuffer.data() + sizeof(char) + sizeof(uint64_t));
    }

    case RocksDBEntryType::IndexValue: {
      return *reinterpret_cast<TRI_voc_rid_t const*>(
          _keyBuffer.data() + (_keyBuffer.size() - sizeof(uint64_t)));
    }

    case RocksDBEntryType::UniqueIndexValue: {
      return *reinterpret_cast<TRI_voc_rid_t const*>(_valueBuffer.data());
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

VPackSlice RocksDBEntry::indexedValues() const {
  switch (_type) {
    case RocksDBEntryType::IndexValue:
    case RocksDBEntryType::UniqueIndexValue: {
      return VPackSlice(_keyBuffer.data() + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

VPackSlice RocksDBEntry::data() const {
  switch (_type) {
    case RocksDBEntryType::Database:
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::Index:
    case RocksDBEntryType::Document:
    case RocksDBEntryType::View: {
      return VPackSlice(_valueBuffer.data());
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

std::string const& RocksDBEntry::key() const { return _keyBuffer; }

std::string const& RocksDBEntry::value() const { return _valueBuffer; }

std::string& RocksDBEntry::valueBuffer() { return _valueBuffer; }

RocksDBEntry::RocksDBEntry(RocksDBEntryType type, uint64_t first, VPackSlice const& slice)
    : _type(type), _keyBuffer(), _valueBuffer() {
  switch (_type) {
    case RocksDBEntryType::Database: {
      size_t length = sizeof(char) + sizeof(uint64_t);
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_keyBuffer, first); // databaseId

      _valueBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                          static_cast<size_t>(slice.byteSize()));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}
  
RocksDBEntry::RocksDBEntry(RocksDBEntryType type, uint64_t first,
                           uint64_t second, VPackSlice const& slice)
    : _type(type), _keyBuffer(), _valueBuffer() {
  switch (_type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View: {
      size_t length = sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t);
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_keyBuffer, first); // databaseId
      uint64ToPersistent(_keyBuffer, second); // viewId / collectionId
      
      _valueBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                          static_cast<size_t>(slice.byteSize()));

      break;
    }

    case RocksDBEntryType::Document: {
      size_t length = sizeof(char) + (2 * sizeof(uint64_t));
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_keyBuffer, first);
      uint64ToPersistent(_keyBuffer, second);

      _valueBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                          static_cast<size_t>(slice.byteSize()));
      break;
    }

    case RocksDBEntryType::IndexValue: {
      size_t length = sizeof(char) + static_cast<size_t>(slice.byteSize()) +
                      (2 * sizeof(uint64_t));
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_keyBuffer, first);
      _keyBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                        static_cast<size_t>(slice.byteSize()));
      _keyBuffer.append(std::to_string(second));

      break;
    }

    case RocksDBEntryType::UniqueIndexValue: {
      size_t length = sizeof(char) + static_cast<size_t>(slice.byteSize()) +
                      sizeof(uint64_t);
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_keyBuffer, first);
      _keyBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                        static_cast<size_t>(slice.byteSize()));

      _valueBuffer.append(reinterpret_cast<char const*>(&second),
                          sizeof(uint64_t));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBEntry::RocksDBEntry(RocksDBEntryType type, uint64_t first,
                           uint64_t second, uint64_t third, VPackSlice const& slice)
    : _type(type), _keyBuffer(), _valueBuffer() {
  switch (_type) {
    case RocksDBEntryType::Index: {
      size_t length = sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_keyBuffer, first); // databaseId
      uint64ToPersistent(_keyBuffer, second); // collectionId
      uint64ToPersistent(_keyBuffer, third); // indexId
      
      _valueBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                          static_cast<size_t>(slice.byteSize()));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

bool RocksDBEntry::isSameDatabase(RocksDBEntryType type, TRI_voc_tick_t id, rocksdb::Slice const& slice) {
  switch (type) {
    case RocksDBEntryType::Collection: 
    case RocksDBEntryType::View: {
      TRI_ASSERT(slice.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
      return id == uint64FromPersistent(slice.data() + sizeof(char));
    }
    
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

uint64_t RocksDBEntry::uint64FromPersistent(char const* p) {
  uint64_t value = 0;
  uint64_t x = 0;
  char const* end = p + sizeof(uint64_t);
  do {
    value += static_cast<uint64_t>(*p++) << x;
    x += 8;
  } while (p < end);
  return value;
}

void RocksDBEntry::uint64ToPersistent(char* p, uint64_t value) {
  char* end = p + sizeof(uint64_t);
  do {
    *p++ = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  } while (p < end);
}

void RocksDBEntry::uint64ToPersistent(std::string& p, uint64_t value) {
  size_t len = 0;
  do {
    p.push_back(static_cast<char>(value & 0xff));
    value >>= 8;
  } while (++len < sizeof(uint64_t));
}
