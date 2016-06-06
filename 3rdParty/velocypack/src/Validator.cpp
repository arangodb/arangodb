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

#include "velocypack/velocypack-common.h"
#include "velocypack/Validator.h"
#include "velocypack/Exception.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

using namespace arangodb::velocypack;

bool Validator::validate(uint8_t const* ptr, size_t length, bool isSubPart) const {
  if (length == 0) {
    throw Exception(Exception::ValidatorInvalidLength, "length 0 is invalid for any VelocyPack value");
  }

  uint8_t const head = *ptr;

  // type() only reads the first byte, which is safe
  ValueType const type = Slice(ptr).type();

  if (type == ValueType::None && head != 0x00) {
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
      if (head == 0xbf) {
        // long UTF-8 string. must be at least 9 bytes long so we
        // can read the entire string length safely
        validateBufferLength(1 + 8, length, true);
      } 
      break;
    }

    case ValueType::Array: {
      ValueLength byteLength = 0;
      bool equalSize = false;
      bool hasIndexTable = false;

      if (head >= 0x02 && head <= 0x05) {
        // Array without index table, with 1-8 bytes bytelength, all values with same length
        byteLength = 1 << (head - 0x02);
        equalSize = true;
      } else if (head >= 0x06 && head <= 0x09) {
        // Array with index table, with 1-8 bytes bytelength
        byteLength = 1 << (head - 0x06);
        hasIndexTable = true;
      } 
      
      if (head == 0x13) {
        // compact Array without index table
        validateBufferLength(2, length, true);
        uint8_t const* p = ptr + 1;
        uint8_t const* e = p + length;
        ValueLength shifter = 0;
        while (true) {
          uint8_t c = *p;
          byteLength += (c & 0x7f) << shifter;
          shifter += 7;
          ++p;
          if (!(c & 0x80)) {
            break;
          }
          if (p == e) {
            throw Exception(Exception::ValidatorInvalidLength, "Array length value is out of bounds");
          }
        }
        if (byteLength > length || byteLength < 4) {
          throw Exception(Exception::ValidatorInvalidLength, "Array length value is out of bounds");
        }
        uint8_t const* data = p;
        p = ptr + byteLength - 1;
        ValueLength nrItems = 0;
        shifter = 0;
        while (true) {
          uint8_t c = *p;
          nrItems += (c & 0x7f) << shifter;
          shifter += 7;
          --p;
          if (!(c & 0x80)) {
            break;
          }
          if (p == ptr + byteLength) {
            throw Exception(Exception::ValidatorInvalidLength, "Array length value is out of bounds");
          }
        } 
        if (nrItems == 0) {
          throw Exception(Exception::ValidatorInvalidLength, "Array length value is out of bounds");
        }
        ++p;
        
        // validate the array members 
        e = p;
        p = data;
        while (nrItems-- > 0) { 
          validate(p, e - p, true);
          p += Slice(p).byteSize();
        }
      } else if (byteLength > 0) {
        ValueLength nrItemsLength = 0;
        if (head >= 0x06) {
          nrItemsLength = byteLength;
        }
        validateBufferLength(1 + byteLength + nrItemsLength, length, true);
        ValueLength nrItems = Slice(ptr).length();
        uint8_t const* p = ptr + 1 + byteLength;
        if (!equalSize) {
          p += byteLength;
        }
        uint8_t const* e = ptr + length;
        ValueLength l = 0;
        while (nrItems > 0) {
          if (p >= e) {
            throw Exception(Exception::ValidatorInvalidLength, "Array value is out of bounds");
          }
          // validate sub value
          validate(p, e - p, true);
          ValueLength al = Slice(p).byteSize();
          if (equalSize) {
            if (l == 0) {
              l = al;
            } else if (l != al) {
              throw Exception(Exception::ValidatorInvalidLength, "Unexpected Array value length");
            }
          }
          p += al;
          --nrItems;
        }
        
        if (hasIndexTable) {
          // now also validate index table
          nrItems = Slice(ptr).length();
          for (ValueLength i = 0; i < nrItems; ++i) {
            ValueLength offset = Slice(ptr).getNthOffset(i);
            if (offset < 1 + byteLength + nrItemsLength ||
                offset >= Slice(ptr).byteSize() - nrItems * byteLength) {
              throw Exception(Exception::ValidatorInvalidLength, "Array value offset is out of bounds");
            }
            validate(ptr + offset, length - offset, true);
          }
        }
      }
      break;
    }
    
    case ValueType::Object: {
      ValueLength byteLength = 0;
      if (head >= 0x0b && head <= 0x0e) {
        // Object with index table, with 1-8 bytes bytelength, sorted
        byteLength = 1 << (head - 0x0b);
      } else if (head >= 0x0f && head <= 0x12) {
        // Object with index table, with 1-8 bytes bytelength, unsorted
        byteLength = 1 << (head - 0x0f);
      } else if (head == 0x14) {
        // compact Object without index table
        // TODO
      }

      if (byteLength > 0) {
        validateBufferLength(1 + byteLength, length, true);
        ValueLength nrItems = Slice(ptr).length();
        uint8_t const* p = ptr + 1 + byteLength;
        uint8_t const* e = ptr + length;
        while (nrItems > 0) {
          if (p >= e) {
            throw Exception(Exception::ValidatorInvalidLength, "Object key offset is out of bounds");
          }
          // validate key 
          validate(p, e - p, true);
          // skip over key
          p += Slice(p).byteSize();
          
          if (p >= e) {
            throw Exception(Exception::ValidatorInvalidLength, "Object value offset is out of bounds");
          }
          // validate value
          validate(p, e - p, true);
          // skip over value
          p += Slice(p).byteSize();

          --nrItems;
        }
        
        // now also validate index table
        for (ValueLength i = 0; i < nrItems; ++i) {
          // get offset to key
          ValueLength offset = Slice(ptr).getNthOffset(i);
          if (offset >= length) {
            throw Exception(Exception::ValidatorInvalidLength, "Object key offset is out of bounds");
          }
          // validate length of key
          validate(ptr + offset, length - offset, true);
          // skip over key
          offset += Slice(ptr + offset).byteSize();
          if (offset >= length) {
            throw Exception(Exception::ValidatorInvalidLength, "Object value offset is out of bounds");
          }
          // validate length of value
          validate(ptr + offset, length - offset, true);
        }
      }
      break;
    }

    case ValueType::BCD: {
      throw Exception(Exception::NotImplemented);
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

      if (head == 0xf0) {
        byteSize = 1 + 1;
      } else if (head == 0xf1) {
        byteSize = 1 + 2;
      } else if (head == 0xf2) {
        byteSize = 1 + 4;
      } else if (head == 0xf3) {
        byteSize = 1 + 8;
      } else if (head >= 0xf4 && head <= 0xf6) {
        validateBufferLength(1 + 1, length, true);
        byteSize = 1 + 1 + readInteger<ValueLength>(ptr + 1, 1);
        if (byteSize == 1 + 1) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      } else if (head >= 0xf7 && head <= 0xf9) {
        validateBufferLength(1 + 2, length, true);
        byteSize = 1 + 2 + readInteger<ValueLength>(ptr + 1, 2); 
        if (byteSize == 1 + 2) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      } else if (head >= 0xfa && head <= 0xfc) {
        validateBufferLength(1 + 4, length, true);
        byteSize = 1 + 4 + readInteger<ValueLength>(ptr + 1, 4); 
        if (byteSize == 1 + 4) {
          throw Exception(Exception::ValidatorInvalidLength, "Invalid size for Custom type");
        }
      } else if (head >= 0xfd) {
        validateBufferLength(1 + 8, length, true);
        byteSize = 1 + 8 + readInteger<ValueLength>(ptr + 1, 8); 
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

void Validator::validateBufferLength(size_t expected, size_t actual, bool isSubPart) const {
  if ((expected > actual) ||
      (expected != actual && !isSubPart)) {
    throw Exception(Exception::ValidatorInvalidLength, "given buffer length is unequal to actual length of Slice in buffer");
  }
}
        
void Validator::validateSliceLength(uint8_t const* ptr, size_t length, bool isSubPart) const {
  size_t actual = static_cast<size_t>(Slice(ptr).byteSize());
  validateBufferLength(actual, length, isSubPart);
}
