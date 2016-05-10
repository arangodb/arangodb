////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "VelocyPackDumper.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "Logger/Logger.h"

#include <velocypack/velocypack-common.h>
#include <velocypack/AttributeTranslator.h>
#include "velocypack/Iterator.h"
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
  
void VelocyPackDumper::handleUnsupportedType(VPackSlice const* slice) {
  TRI_string_buffer_t* buffer = _buffer->stringBuffer(); 

  if (options->unsupportedTypeBehavior == VPackOptions::NullifyUnsupportedType) {
    TRI_AppendStringUnsafeStringBuffer(buffer, "null", 4);
    return;
  } else if (options->unsupportedTypeBehavior == VPackOptions::ConvertUnsupportedType) {
    TRI_AppendStringUnsafeStringBuffer(buffer, "\"(non-representable type)\"");
    return;
  }

  throw VPackException(VPackException::NoJsonEquivalent);
}
  
void VelocyPackDumper::appendUInt(uint64_t v) {
  TRI_string_buffer_t* buffer = _buffer->stringBuffer(); 
    
  int res = TRI_ReserveStringBuffer(buffer, 21);
     
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  if (10000000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000000000ULL) % 10);
  }
  if (1000000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000000ULL) % 10);
  }
  if (100000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000000ULL) % 10);
  }
  if (10000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000000ULL) % 10);
  }
  if (1000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000ULL) % 10);
  }
  if (100000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000ULL) % 10);
  }
  if (10000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000ULL) % 10);
  }
  if (1000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000ULL) % 10);
  }
  if (100000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000ULL) % 10);
  }
  if (10000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000ULL) % 10);
  }
  if (1000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000ULL) % 10);
  }
  if (100000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000ULL) % 10);
  }
  if (10000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000ULL) % 10);
  }
  if (1000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000ULL) % 10);
  }
  if (100000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000ULL) % 10);
  }
  if (10000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000ULL) % 10);
  }
  if (1000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000ULL) % 10);
  }
  if (100ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100ULL) % 10);
  }
  if (10ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10ULL) % 10);
  }

  TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v % 10));
}

void VelocyPackDumper::appendDouble(double v) {
  char temp[24];
  int len = fpconv_dtoa(v, &temp[0]);
  
  TRI_string_buffer_t* buffer = _buffer->stringBuffer(); 
    
  int res = TRI_ReserveStringBuffer(buffer, static_cast<size_t>(len));
     
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  TRI_AppendStringUnsafeStringBuffer(buffer, &temp[0], static_cast<size_t>(len));
}

void VelocyPackDumper::dumpInteger(VPackSlice const* slice) {
  if (slice->isType(VPackValueType::UInt)) {
    uint64_t v = slice->getUInt();

    appendUInt(v);
  } else if (slice->isType(VPackValueType::Int)) {
    TRI_string_buffer_t* buffer = _buffer->stringBuffer(); 
    
    int res = TRI_ReserveStringBuffer(buffer, 21);
     
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    int64_t v = slice->getInt();
    if (v == INT64_MIN) {
      TRI_AppendStringUnsafeStringBuffer(buffer, "-9223372036854775808", 20);
      return;
    }
  
    if (v < 0) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '-');
      v = -v;
    }

    if (1000000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000000LL) % 10);
    }
    if (100000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000000LL) % 10);
    }
    if (10000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000000LL) % 10);
    }
    if (1000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000LL) % 10);
    }
    if (100000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000LL) % 10);
    }
    if (10000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000LL) % 10);
    }
    if (1000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000LL) % 10);
    }
    if (100000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000LL) % 10);
    }
    if (10000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000LL) % 10);
    }
    if (1000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000LL) % 10);
    }
    if (100000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000LL) % 10);
    }
    if (10000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000LL) % 10);
    }
    if (1000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000LL) % 10);
    }
    if (100000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000LL) % 10);
    }
    if (10000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000LL) % 10);
    }
    if (1000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000LL) % 10);
    }
    if (100LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100LL) % 10);
    }
    if (10LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10LL) % 10);
    }

    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v % 10));
  } else if (slice->isType(VPackValueType::SmallInt)) {
    TRI_string_buffer_t* buffer = _buffer->stringBuffer(); 
    
    int res = TRI_ReserveStringBuffer(buffer, 21);
     
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    int64_t v = slice->getSmallInt();
    if (v < 0) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '-');
      v = -v;
    }
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + static_cast<char>(v));
  }
}

void VelocyPackDumper::dumpValue(VPackSlice const* slice, VPackSlice const* base) {
  if (base == nullptr) {
    base = slice;
  }
  
  TRI_string_buffer_t* buffer = _buffer->stringBuffer(); 
    
  // alloc at least 16 bytes  
  int res = TRI_ReserveStringBuffer(buffer, 16);
     
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  switch (slice->type()) {
    case VPackValueType::Null: {
      TRI_AppendStringUnsafeStringBuffer(buffer, "null", 4);
      break;
    }

    case VPackValueType::Bool: {
      if (slice->getBool()) {
        TRI_AppendStringUnsafeStringBuffer(buffer, "true", 4);
      } else {
        TRI_AppendStringUnsafeStringBuffer(buffer, "false", 5);
      }
      break;
    }

    case VPackValueType::Array: {
      VPackArrayIterator it(*slice, true);
      TRI_AppendCharUnsafeStringBuffer(buffer, '[');
      while (it.valid()) {
        if (!it.isFirst()) {
          if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
        dumpValue(it.value(), slice);
        it.next();
      }
      if (TRI_AppendCharStringBuffer(buffer, ']') != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      break;
    }

    case VPackValueType::Object: {
      VPackObjectIterator it(*slice, true);
      TRI_AppendCharUnsafeStringBuffer(buffer, '{');
      while (it.valid()) {
        if (!it.isFirst()) {
          if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
        dumpValue(it.key().makeKey(), slice);
        if (TRI_AppendCharStringBuffer(buffer, ':') != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
        dumpValue(it.value(), slice);
        it.next();
      }
      if (TRI_AppendCharStringBuffer(buffer, '}') != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      break;
    }

    case VPackValueType::Double: {
      double const v = slice->getDouble();
      if (std::isnan(v) || !std::isfinite(v)) {
        handleUnsupportedType(slice);
      } else {
        appendDouble(v);
      }
      break;
    }

    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt: {
      dumpInteger(slice);
      break;
    }

    case VPackValueType::String: {
      VPackValueLength len;
      char const* p = slice->getString(len);
      int res;
      if (len == 0) {
        res = TRI_AppendString2StringBuffer(buffer, "\"\"", 2);
      } else {
        res = TRI_AppendCharStringBuffer(buffer, '"');
        res |= TRI_AppendJsonEncodedStringStringBuffer(buffer, p, static_cast<size_t>(len), options->escapeForwardSlashes);
        res |= TRI_AppendCharStringBuffer(buffer, '"');
      }
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      break;
    }
    
    case VPackValueType::External: {
      VPackSlice const external(slice->getExternal());
      dumpValue(&external, base);
      break;
    }
    
    case VPackValueType::Custom: {
      if (options->customTypeHandler == nullptr) {
        throw VPackException(VPackException::NeedCustomTypeHandler);
      } else {
        std::string v = options->customTypeHandler->toString(*slice, nullptr, *base);
        
        int res = TRI_AppendCharStringBuffer(buffer, '"');
        res |= TRI_AppendJsonEncodedStringStringBuffer(buffer, v.c_str(), v.size(), options->escapeForwardSlashes);
        res |= TRI_AppendCharStringBuffer(buffer, '"');
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
      }
      break;
    }

    case VPackValueType::UTCDate: 
    case VPackValueType::None: 
    case VPackValueType::Binary: 
    case VPackValueType::Illegal:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey: 
    case VPackValueType::BCD: {
      handleUnsupportedType(slice);
      break;
    }

  }
}

