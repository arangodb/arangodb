////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <unordered_set>
#include <memory>

#include "velocypack/velocypack-common.h"
#include "velocypack/Validator.h"
#include "velocypack/Exception.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

#include "asm-functions.h"

using namespace arangodb::velocypack;

template<bool reverse>
static ValueLength ReadVariableLengthValue(uint8_t const*& p, uint8_t const* end) {
  ValueLength value = 0;
  ValueLength shifter = 0;
  while (true) {
    uint8_t c = *p;
    value += (c & 0x7fU) << shifter;
    shifter += 7;
    if (reverse) {
      --p;
    } else {
      ++p;
    }
    if (!(c & 0x80U)) {
      break;
    }
    if (p == end) {
      throw Exception(Exception::ValidatorInvalidLength, "Compound value length value is out of bounds");
    }
  }
  return value;
}
  
Validator::Validator(Options const* options)
      : options(options), _level(0) {
  if (options == nullptr) {
    throw Exception(Exception::InternalError, "Options cannot be a nullptr");
  }
}

bool Validator::validate(uint8_t const* ptr, std::size_t length, bool isSubPart) {
  if (length == 0) {
    throw Exception(Exception::ValidatorInvalidLength, "length 0 is invalid for any VelocyPack value");
  }

  uint8_t const head = *ptr;

  // type() only reads the first byte, which is safe
  ValueType const type = Slice(ptr).type();

  if (type == ValueType::None && head != 0x00U) {
    // invalid type
    throw Exception(Exception::ValidatorInvalidType);
  }

  // special handling for certain types...
  switch (type) {
    case ValueType::None:
    case ValueType::Null:
    case ValueType::Bool:
    case ValueType::MinKey:
    case ValueType::MaxKey:
    case ValueType::SmallInt:
    case ValueType::Int:
    case ValueType::UInt:
    case ValueType::Double:
    case ValueType::UTCDate:
    case ValueType::Binary:
    case ValueType::Illegal: {
      break;
    }

    case ValueType::String: {
      uint8_t const* p;
      ValueLength len;
      if (head == 0xbfU) {
        // long UTF-8 string. must be at least 9 bytes long so we
        // can read the entire string length safely
        validateBufferLength(1 + 8, length, true);
        len = readIntegerFixed<ValueLength, 8>(ptr + 1);
        p = ptr + 1 + 8;
        validateBufferLength(len + 1 + 8, length, true);
      } else {
        len = head - 0x40U;
        p = ptr + 1;
        validateBufferLength(len + 1, length, true);
      }

      if (options->validateUtf8Strings &&
          !ValidateUtf8String(p, static_cast<std::size_t>(len))) {
        throw Exception(Exception::InvalidUtf8Sequence);
      }
      break;
    }

    case ValueType::Array: {
      ++_level;
      validateArray(ptr, length);
      --_level;
      break;
    }

    case ValueType::Object: {
      ++_level;
      validateObject(ptr, length);
      --_level;
      break;
    }

    case ValueType::BCD: {
      if (options->disallowBCD) {
        throw Exception(Exception::BuilderBCDDisallowed);
      }
      throw Exception(Exception::NotImplemented);
    }
    
    case ValueType::Tagged: {
      if (options->disallowTags) {
        throw Exception(Exception::BuilderTagsDisallowed);
      }
      if (head == 0xee) {
        // 1 byte tag type
        // the actual Slice (without tag) must be at least one byte long
        validateBufferLength(1 + 1 + 1, length, true);
        VELOCYPACK_ASSERT(length > 2);
        ptr += 2;
        length -= 2;
      } else if (head == 0xef) {
        // 8 bytes tag type
        // the actual Slice (without tag) must be at least one byte long
        validateBufferLength(1 + 8 + 1, length, true);
        VELOCYPACK_ASSERT(length > 9);
        ptr += 9;
        length -= 9;
      } else {
        throw Exception(Exception::NotImplemented);
      }
      break;
    }

    case ValueType::External: {
      // check if Externals are forbidden
      if (options->disallowExternals) {
        throw Exception(Exception::BuilderExternalsDisallowed);
      }
      // validate if Slice length exceeds the given buffer
      validateBufferLength(1 + sizeof(void*), length, true);
      // do not perform pointer validation
      break;
    }

    case ValueType::Custom: {
      ValueLength byteSize = 0;

      if (head == 0xf0U) {
        byteSize = 1 + 1;
      } else if (head == 0xf1U) {
        byteSize = 1 + 2;
      } else if (head == 0xf2U) {
        byteSize = 1 + 4;
      } else if (head == 0xf3U) {
        byteSize = 1 + 8;
      } else if (head >= 0xf4U && head <= 0xf6U) {
        validateBufferLength(1 + 1, length, true);
        byteSize = 1 + 1 + readIntegerNonEmpty<ValueLength>(ptr + 1, 1);
        if (byteSize == 1 + 1) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      } else if (head >= 0xf7U && head <= 0xf9U) {
        validateBufferLength(1 + 2, length, true);
        byteSize = 1 + 2 + readIntegerNonEmpty<ValueLength>(ptr + 1, 2);
        if (byteSize == 1 + 2) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      } else if (head >= 0xfaU && head <= 0xfcU) {
        validateBufferLength(1 + 4, length, true);
        byteSize = 1 + 4 + readIntegerNonEmpty<ValueLength>(ptr + 1, 4);
        if (byteSize == 1 + 4) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      } else if (head >= 0xfdU) {
        validateBufferLength(1 + 8, length, true);
        byteSize = 1 + 8 + readIntegerNonEmpty<ValueLength>(ptr + 1, 8);
        if (byteSize == 1 + 8) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      }

      validateSliceLength(ptr, byteSize, isSubPart);
      break;
    }
  }

  // common validation that must happen for all types
  validateSliceLength(ptr, length, isSubPart);
  return true;
}

void Validator::validateArray(uint8_t const* ptr, std::size_t length) {
  uint8_t head = *ptr;

  if (head == 0x13U) {
    // compact array
    validateCompactArray(ptr, length);
  } else if (head >= 0x02U && head <= 0x05U) {
    // array without index table
    validateUnindexedArray(ptr, length);
  } else if (head >= 0x06U && head <= 0x09U) {
    // array with index table
    validateIndexedArray(ptr, length);
  } else if (head == 0x01U) {
    // empty array. always valid
  }
}

void Validator::validateCompactArray(uint8_t const* ptr, std::size_t length) {
  // compact Array without index table
  validateBufferLength(4, length, true);

  uint8_t const* p = ptr + 1;
  // read byteLength
  ValueLength const byteSize = ReadVariableLengthValue<false>(p, p + length);
  if (byteSize > length || byteSize < 4) {
    throw Exception(Exception::ValidatorInvalidLength, "Array length value is out of bounds");
  }

  // read nrItems
  uint8_t const* data = p;
  p = ptr + byteSize - 1;
  ValueLength nrItems = ReadVariableLengthValue<true>(p, ptr + byteSize);
  if (nrItems == 0) {
    throw Exception(Exception::ValidatorInvalidLength, "Array length value is out of bounds");
  }
  ++p;

  // validate the array members
  uint8_t const* e = p;
  p = data;
  while (nrItems-- > 0) {
    validate(p, e - p, true);
    p += Slice(p).byteSize();
  }
}

void Validator::validateUnindexedArray(uint8_t const* ptr, std::size_t length) {
  // Array without index table, with 1-8 bytes lengths, all values with same length
  uint8_t head = *ptr;
  ValueLength const byteSizeLength = 1ULL << (static_cast<ValueLength>(head) - 0x02U);
  validateBufferLength(1 + byteSizeLength + 1, length, true);
  ValueLength const byteSize = readIntegerNonEmpty<ValueLength>(ptr + 1, byteSizeLength);

  if (byteSize > length) {
    throw Exception(Exception::ValidatorInvalidLength, "Array length is out of bounds");
  }

  // look up first member
  uint8_t const* p = ptr + 1 + byteSizeLength;
  uint8_t const* e = p + (8 - byteSizeLength);

  if (e > ptr + byteSize) {
    e = ptr + byteSize;
  }
  while (p < e && *p == '\x00') {
    ++p;
  }

  if (p >= ptr + byteSize) {
    throw Exception(Exception::ValidatorInvalidLength, "Array structure is invalid");
  }

  // check if padding is correct
  if (p != ptr + 1 + byteSizeLength &&
      p != ptr + 1 + byteSizeLength + (8 - byteSizeLength)) {
    throw Exception(Exception::ValidatorInvalidLength, "Array padding is invalid");
  }
  
  validate(p, length - (p - ptr), true);
  ValueLength itemSize = Slice(p).byteSize();
  if (itemSize == 0) {
    throw Exception(Exception::ValidatorInvalidLength, "Array itemSize value is invalid");
  }
  ValueLength nrItems = (byteSize - (p - ptr)) / itemSize;

  if (nrItems == 0) {
    throw Exception(Exception::ValidatorInvalidLength, "Array nrItems value is invalid");
  }
  // we already validated p, so move it forward
  p += itemSize;
  e = ptr + length;
  --nrItems;

  while (nrItems > 0) {
    if (p >= e) {
      throw Exception(Exception::ValidatorInvalidLength, "Array value is out of bounds");
    }
    // validate sub value
    validate(p, e - p, true);
    if (Slice(p).byteSize() != itemSize) {
      // got a sub-object with a different size. this is not allowed
      throw Exception(Exception::ValidatorInvalidLength, "Unexpected Array value length");
    }
    p += itemSize;
    --nrItems;
  } 
}

void Validator::validateIndexedArray(uint8_t const* ptr, std::size_t length) {
  // Array with index table, with 1-8 bytes lengths
  uint8_t head = *ptr;
  ValueLength const byteSizeLength = 1ULL << (static_cast<ValueLength>(head) - 0x06U);
  validateBufferLength(1 + byteSizeLength + byteSizeLength + 1, length, true);
  ValueLength byteSize = readIntegerNonEmpty<ValueLength>(ptr + 1, byteSizeLength);

  if (byteSize > length) {
    throw Exception(Exception::ValidatorInvalidLength, "Array length is out of bounds");
  }

  ValueLength nrItems;
  uint8_t const* indexTable;
  uint8_t const* firstMember;

  if (head == 0x09U) {
    // byte length = 8
    nrItems = readIntegerNonEmpty<ValueLength>(ptr + byteSize - byteSizeLength, byteSizeLength);

    if (nrItems == 0) {
      throw Exception(Exception::ValidatorInvalidLength, "Array nrItems value is invalid");
    }

    indexTable = ptr + byteSize - byteSizeLength - (nrItems * byteSizeLength);
    if (indexTable < ptr + byteSizeLength) {
      throw Exception(Exception::ValidatorInvalidLength, "Array index table is out of bounds");
    }
    
    firstMember = ptr + 1 + byteSizeLength; 
  } else {
    // byte length = 1, 2 or 4
    nrItems = readIntegerNonEmpty<ValueLength>(ptr + 1 + byteSizeLength, byteSizeLength);

    if (nrItems == 0) {
      throw Exception(Exception::ValidatorInvalidLength, "Array nrItems value is invalid");
    }

    // look up first member
    uint8_t const* p = ptr + 1 + byteSizeLength + byteSizeLength;
    uint8_t const* e = p + (8 - byteSizeLength - byteSizeLength);
    if (e > ptr + byteSize) {
      e = ptr + byteSize;
    }
    while (p < e && *p == '\x00') {
      ++p;
    }

    // check if padding is correct
    if (p != ptr + 1 + byteSizeLength + byteSizeLength &&
        p != ptr + 1 + byteSizeLength + byteSizeLength + (8 - byteSizeLength - byteSizeLength)) {
      throw Exception(Exception::ValidatorInvalidLength, "Array padding is invalid");
    }
  
    indexTable = ptr + byteSize - (nrItems * byteSizeLength);
    if (indexTable < ptr + byteSizeLength + byteSizeLength || indexTable < p) {
      throw Exception(Exception::ValidatorInvalidLength, "Array index table is out of bounds");
    }

    firstMember = p;
  }
   
  VELOCYPACK_ASSERT(nrItems > 0); 
  
  ValueLength actualNrItems = 0;
  uint8_t const* member = firstMember;
  while (member < indexTable) {
    validate(member, indexTable - member, true);
    ValueLength offset = readIntegerNonEmpty<ValueLength>(
        indexTable + actualNrItems * byteSizeLength, byteSizeLength);
    if (offset != static_cast<ValueLength>(member - ptr)) {
      throw Exception(Exception::ValidatorInvalidLength, "Array index table is wrong");
    }
  
    member += Slice(member).byteSize();
    ++actualNrItems;
  }

  if (actualNrItems != nrItems) {
    throw Exception(Exception::ValidatorInvalidLength, "Array has more items than in index");
  }
}

void Validator::validateObject(uint8_t const* ptr, std::size_t length) {
  uint8_t head = *ptr;

  if (head == 0x14U) {
    // compact object
    validateCompactObject(ptr, length);
  } else if (head >= 0x0bU && head <= 0x12U) {
    // regular object
    validateIndexedObject(ptr, length);
  } else if (head == 0x0aU) {
    // empty object. always valid
  }
}

void Validator::validateCompactObject(uint8_t const* ptr, std::size_t length) {
  // compact Object without index table
  validateBufferLength(5, length, true);

  uint8_t const* p = ptr + 1;
  // read byteLength
  ValueLength const byteSize = ReadVariableLengthValue<false>(p, p + length);
  if (byteSize > length || byteSize < 5) {
    throw Exception(Exception::ValidatorInvalidLength, "Object length value is out of bounds");
  }

  // read nrItems
  uint8_t const* data = p;
  p = ptr + byteSize - 1;
  ValueLength nrItems = ReadVariableLengthValue<true>(p, ptr + byteSize);
  if (nrItems == 0) {
    throw Exception(Exception::ValidatorInvalidLength, "Object length value is out of bounds");
  }
  ++p;

  // validate the object members
  uint8_t const* e = p;
  p = data;
  while (nrItems-- > 0) {
    // validate key
    validate(p, e - p, true);
    Slice key(p);
    bool isString = key.isString();
    if (!isString) {
      bool const isSmallInt = key.isSmallInt();
      if ((!isSmallInt && !key.isUInt()) || (isSmallInt && key.getSmallInt() <= 0)) {
        throw Exception(Exception::ValidatorInvalidLength, "Invalid object key type");
      }
    }
    ValueLength keySize = key.byteSize();
    // validate key
    if (isString && options->validateUtf8Strings) {
      validate(p, keySize, true);
    }

    // validate value
    p += keySize;
    validate(p, e - p, true);
    p += Slice(p).byteSize();
  }

  // finally check if we are now pointing at the end or not
  if (p != e) {
    throw Exception(Exception::ValidatorInvalidLength, "Object has more members than specified");
  }
}

void Validator::validateIndexedObject(uint8_t const* ptr, std::size_t length) {
  // Object with index table, with 1-8 bytes lengths
  uint8_t head = *ptr;
  ValueLength const byteSizeLength = 1ULL << (static_cast<ValueLength>(head) - 0x0bU);
  validateBufferLength(1 + byteSizeLength + byteSizeLength + 1, length, true);
  ValueLength const byteSize = readIntegerNonEmpty<ValueLength>(ptr + 1, byteSizeLength);

  if (byteSize > length) {
    throw Exception(Exception::ValidatorInvalidLength, "Object length is out of bounds");
  }

  ValueLength nrItems;
  uint8_t const* indexTable;
  uint8_t const* firstMember;

  //if (head == 0x12U) {
  if (head == 0x0eU || head == 0x12U) {
    // byte length = 8
    nrItems = readIntegerNonEmpty<ValueLength>(ptr + byteSize - byteSizeLength, byteSizeLength);

    if (nrItems == 0) {
      throw Exception(Exception::ValidatorInvalidLength, "Object nrItems value is invalid");
    }

    indexTable = ptr + byteSize - byteSizeLength - (nrItems * byteSizeLength);
    if (indexTable < ptr + byteSizeLength) {
      throw Exception(Exception::ValidatorInvalidLength, "Object index table is out of bounds");
    }
    
    firstMember = ptr + byteSize;
  } else {
    // byte length = 1, 2 or 4
    nrItems = readIntegerNonEmpty<ValueLength>(ptr + 1 + byteSizeLength, byteSizeLength);

    if (nrItems == 0) {
      throw Exception(Exception::ValidatorInvalidLength, "Object nrItems value is invalid");
    }

    // look up first member
    uint8_t const* p = ptr + 1 + byteSizeLength + byteSizeLength;
    uint8_t const* e = p + (8 - byteSizeLength - byteSizeLength);
    if (e > ptr + byteSize) {
      e = ptr + byteSize;
    }
    while (p < e && *p == '\x00') {
      ++p;
    }

    // check if padding is correct
    if (p != ptr + 1 + byteSizeLength + byteSizeLength &&
        p != ptr + 1 + byteSizeLength + byteSizeLength + (8 - byteSizeLength - byteSizeLength)) {
      throw Exception(Exception::ValidatorInvalidLength, "Object padding is invalid");
    }
  
    indexTable = ptr + byteSize - (nrItems * byteSizeLength);
    if (indexTable < ptr + byteSizeLength + byteSizeLength || indexTable < p) {
      throw Exception(Exception::ValidatorInvalidLength, "Object index table is out of bounds");
    }

    firstMember = p;
  }

  VELOCYPACK_ASSERT(nrItems > 0);
  
  ValueLength tableBuf[16];    // Fixed space to save offsets found sequentially
  ValueLength* table = tableBuf;
  std::unique_ptr<ValueLength[]> tableGuard;
  std::unique_ptr<std::unordered_set<ValueLength>> offsetSet;
  if (nrItems > 16) {
    if (nrItems <= 128) {
      table = new ValueLength[nrItems];  // throws if bad_alloc
      tableGuard.reset(table);   // for automatic deletion
    } else {
      // if we have even more items, we directly create an unordered_set
      offsetSet.reset(new std::unordered_set<ValueLength>());
    }
  }
  ValueLength actualNrItems = 0;
  uint8_t const* member = firstMember;
  while (member < indexTable) {
    validate(member, indexTable - member, true);

    Slice key(member);
    bool const isString = key.isString();
    if (!isString) {
      bool const isSmallInt = key.isSmallInt();
      if ((!isSmallInt && !key.isUInt()) || (isSmallInt && key.getSmallInt() <= 0)) {
        throw Exception(Exception::ValidatorInvalidLength, "Invalid object key type");
      }
    }

    ValueLength const keySize = key.byteSize();
    if (isString && options->validateUtf8Strings) {
      validate(member, keySize, true);
    }

    uint8_t const* value = member + keySize;
    if (value >= indexTable) {
      throw Exception(Exception::ValidatorInvalidLength, "Object value leaking into index table");
    }
    validate(value, indexTable - value, true);

    ValueLength offset = static_cast<ValueLength>(member - ptr);
    if (nrItems <= 128) {
      table[actualNrItems] = offset;
    } else {
      offsetSet->emplace(offset);
    }

    member += keySize + Slice(value).byteSize();
    ++actualNrItems;

    if (actualNrItems > nrItems) {
      throw Exception(Exception::ValidatorInvalidLength, "Object value has more key/value pairs than announced");
    }
  }

  if (actualNrItems < nrItems) {
    throw Exception(Exception::ValidatorInvalidLength, "Object has fewer items than in index");
  }

  // Finally verify each offset in the index:
  if (nrItems <= 128) {
    for (ValueLength pos = 0; pos < nrItems; ++pos) {
      ValueLength offset = readIntegerNonEmpty<ValueLength>(
          indexTable + pos * byteSizeLength, byteSizeLength);
      // Binary search in sorted index list:
      ValueLength low = 0;
      ValueLength high = nrItems;
      bool found = false;
      while (low < high) {
        ValueLength mid = (low + high) / 2;
        if (offset == table[mid]) {
          found = true;
          break;
        } else if (offset < table[mid]) {
          high = mid;
        } else {   // offset > table[mid]
          low = mid + 1;
        }
      }
      if (!found) {
        throw Exception(Exception::ValidatorInvalidLength, "Object has invalid index offset");
      }
    }
  } else {
    for (ValueLength pos = 0; pos < nrItems; ++pos) {
      ValueLength offset = readIntegerNonEmpty<ValueLength>(
          indexTable + pos * byteSizeLength, byteSizeLength);
      auto i = offsetSet->find(offset);
      if (i == offsetSet->end()) {
        throw Exception(Exception::ValidatorInvalidLength, "Object has invalid index offset");
      }
      offsetSet->erase(i);
    }
  }
}

void Validator::validateBufferLength(std::size_t expected, std::size_t actual, bool isSubPart) {
  if ((expected > actual) ||
      (expected != actual && !isSubPart)) {
    throw Exception(Exception::ValidatorInvalidLength, "given buffer length is unequal to actual length of Slice in buffer");
  }
}

void Validator::validateSliceLength(uint8_t const* ptr, std::size_t length, bool isSubPart) {
  std::size_t actual = static_cast<std::size_t>(Slice(ptr).byteSize());
  validateBufferLength(actual, length, isSubPart);
}
