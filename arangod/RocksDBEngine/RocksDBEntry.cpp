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

RocksDBEntry RocksDBEntry::Database(uint64_t id, VPackSlice const& data) {
  return RocksDBEntry(Type::Database, id, 0, data);
}

RocksDBEntry RocksDBEntry::Collection(uint64_t id, VPackSlice const& data) {
  return RocksDBEntry(Type::Collection, id, 0, data);
}

RocksDBEntry RocksDBEntry::Index(uint64_t id, VPackSlice const& data) {
  return RocksDBEntry(Type::Index, id, 0, data);
}

RocksDBEntry RocksDBEntry::Document(uint64_t collectionId, uint64_t revisionId,
                                    VPackSlice const& data) {
  return RocksDBEntry(Type::Document, collectionId, revisionId, data);
}

RocksDBEntry RocksDBEntry::IndexValue(uint64_t indexId, uint64_t revisionId,
                                      VPackSlice const& indexValues) {
  return RocksDBEntry(Type::IndexValue, indexId, revisionId, indexValues);
}

RocksDBEntry RocksDBEntry::UniqueIndexValue(uint64_t indexId,
                                            uint64_t revisionId,
                                            VPackSlice const& indexValues) {
  return RocksDBEntry(Type::UniqueIndexValue, indexId, revisionId, indexValues);
}

RocksDBEntry RocksDBEntry::View(uint64_t id, VPackSlice const& data) {
  return RocksDBEntry(Type::View, id, 0, data);
}

RocksDBEntry RocksDBEntry::CrossReferenceCollection(uint64_t databaseId,
                                                    uint64_t collectionId) {
  return RocksDBEntry(Type::CrossReference, Type::Collection, databaseId,
                      collectionId);
}

RocksDBEntry RocksDBEntry::CrossReferenceIndex(uint64_t databaseId,
                                               uint64_t collectionId,
                                               uint64_t indexId) {
  return RocksDBEntry(Type::CrossReference, Type::Index, databaseId,
                      collectionId, indexId);
}

RocksDBEntry RocksDBEntry::CrossReferenceView(uint64_t databaseId,
                                              uint64_t viewId) {
  return RocksDBEntry(Type::CrossReference, Type::View, databaseId, viewId);
}

RocksDBEntry::Type RocksDBEntry::type() const { return _type; }

uint64_t RocksDBEntry::databaseId() const {
  switch (_type) {
    case Type::Database:
    case Type::CrossReference: {
      return *reinterpret_cast<uint64_t const*>(_keyBuffer.data() +
                                                sizeof(char));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

uint64_t RocksDBEntry::collectionId() const {
  switch (_type) {
    case Type::Collection:
    case Type::Document: {
      return *reinterpret_cast<uint64_t const*>(_keyBuffer.data() +
                                                sizeof(char));
    }

    case Type::CrossReference: {
      Type subtype =
          *reinterpret_cast<Type const*>(_keyBuffer.data() + sizeof(char));
      if ((subtype == Type::Collection) || (subtype == Type::Index)) {
        return *reinterpret_cast<uint64_t const*>(
            _keyBuffer.data() + (2 * sizeof(char)) + sizeof(uint64_t));
      } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
      }
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

uint64_t RocksDBEntry::indexId() const {
  switch (_type) {
    case Type::Index:
    case Type::IndexValue:
    case Type::UniqueIndexValue: {
      return *reinterpret_cast<uint64_t const*>(_keyBuffer.data() +
                                                sizeof(char));
    }

    case Type::CrossReference: {
      Type subtype =
          *reinterpret_cast<Type const*>(_keyBuffer.data() + sizeof(char));
      if (subtype == Type::Index) {
        return *reinterpret_cast<uint64_t const*>(
            _keyBuffer.data() + (2 * sizeof(char)) + (2 * sizeof(uint64_t)));
      } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
      }
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

uint64_t RocksDBEntry::viewId() const {
  switch (_type) {
    case Type::View: {
      return *reinterpret_cast<uint64_t const*>(_keyBuffer.data() +
                                                sizeof(char));
    }

    case Type::CrossReference: {
      Type subtype =
          *reinterpret_cast<Type const*>(_keyBuffer.data() + sizeof(char));
      if (subtype == Type::View) {
        return *reinterpret_cast<uint64_t const*>(
            _keyBuffer.data() + (2 * sizeof(char)) + sizeof(uint64_t));
      } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
      }
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

uint64_t RocksDBEntry::revisionId() const {
  switch (_type) {
    case Type::Document: {
      return *reinterpret_cast<uint64_t const*>(
          _keyBuffer.data() + sizeof(char) + sizeof(uint64_t));
    }

    case Type::IndexValue: {
      return *reinterpret_cast<uint64_t const*>(
          _keyBuffer.data() + (_keyBuffer.size() - sizeof(uint64_t)));
    }

    case Type::UniqueIndexValue: {
      return *reinterpret_cast<uint64_t const*>(_valueBuffer.data());
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

VPackSlice const RocksDBEntry::indexedValues() const {
  switch (_type) {
    case Type::IndexValue:
    case Type::UniqueIndexValue: {
      return VPackSlice(*reinterpret_cast<VPackSlice const*>(
          _keyBuffer.data() + sizeof(char) + sizeof(uint64_t)));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

VPackSlice const RocksDBEntry::data() const {
  switch (_type) {
    case Type::Database:
    case Type::Collection:
    case Type::Index:
    case Type::Document:
    case Type::View: {
      return VPackSlice(
          *reinterpret_cast<VPackSlice const*>(_valueBuffer.data()));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

std::string const& RocksDBEntry::key() const { return _keyBuffer; }

std::string const& RocksDBEntry::value() const { return _valueBuffer; }

std::string& RocksDBEntry::valueBuffer() { return _valueBuffer; }

RocksDBEntry::RocksDBEntry(RocksDBEntry::Type type, RocksDBEntry::Type subtype,
                           uint64_t first, uint64_t second, uint64_t third)
    : _type(type), _keyBuffer(), _valueBuffer() {
  TRI_ASSERT(_type == Type::CrossReference);

  switch (subtype) {
    case Type::Collection:
    case Type::View: {
      size_t length = (2 * sizeof(char)) + (2 * sizeof(uint64_t));
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      _keyBuffer.push_back(static_cast<char>(subtype));
      _keyBuffer.append(std::to_string(first));
      _keyBuffer.append(std::to_string(second));

      break;
    }

    case Type::Index: {
      size_t length = (2 * sizeof(char)) + (3 * sizeof(uint64_t));
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      _keyBuffer.push_back(static_cast<char>(subtype));
      _keyBuffer.append(std::to_string(first));
      _keyBuffer.append(std::to_string(second));
      _keyBuffer.append(std::to_string(third));

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBEntry::RocksDBEntry(RocksDBEntry::Type type, uint64_t first,
                           uint64_t second, VPackSlice const& slice)
    : _type(type), _keyBuffer(), _valueBuffer() {
  TRI_ASSERT(_type != Type::CrossReference);
  switch (_type) {
    case Type::Database:
    case Type::Collection:
    case Type::Index:
    case Type::View: {
      size_t length = sizeof(char) + sizeof(uint64_t);
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      _keyBuffer.append(std::to_string(first));

      _valueBuffer.reserve(static_cast<size_t>(slice.byteSize()));
      _valueBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                          static_cast<size_t>(slice.byteSize()));

      break;
    }

    case Type::Document: {
      size_t length = sizeof(char) + (2 * sizeof(uint64_t));
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      _keyBuffer.append(std::to_string(first));
      _keyBuffer.append(std::to_string(second));

      _valueBuffer.reserve(static_cast<size_t>(slice.byteSize()));
      _valueBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                          static_cast<size_t>(slice.byteSize()));

      break;
    }

    case Type::IndexValue: {
      size_t length = sizeof(char) + static_cast<size_t>(slice.byteSize()) +
                      (2 * sizeof(uint64_t));
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      _keyBuffer.append(std::to_string(first));
      _keyBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                        static_cast<size_t>(slice.byteSize()));
      _keyBuffer.append(std::to_string(second));

      break;
    }

    case Type::UniqueIndexValue: {
      size_t length = sizeof(char) + static_cast<size_t>(slice.byteSize()) +
                      sizeof(uint64_t);
      _keyBuffer.reserve(length);
      _keyBuffer.push_back(static_cast<char>(_type));
      _keyBuffer.append(std::to_string(first));
      _keyBuffer.append(reinterpret_cast<char const*>(slice.begin()),
                        static_cast<size_t>(slice.byteSize()));

      _valueBuffer.reserve(sizeof(uint64_t));
      _valueBuffer.append(reinterpret_cast<char const*>(&second),
                          sizeof(uint64_t));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}
