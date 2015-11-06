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

#include "velocypack/velocypack-common.h"
#include "velocypack/Builder.h"

using namespace arangodb::velocypack;

void Builder::doActualSort (std::vector<SortEntry>& entries) {
  VELOCYPACK_ASSERT(entries.size() > 1);
  std::sort(entries.begin(), entries.end(), 
    [] (SortEntry const& a, SortEntry const& b) {
      // return true iff a < b:
      uint8_t const* pa = a.nameStart;
      uint64_t sizea = a.nameSize;
      uint8_t const* pb = b.nameStart;
      uint64_t sizeb = b.nameSize;
      size_t const compareLength
          = static_cast<size_t>((std::min)(sizea, sizeb));
      int res = memcmp(pa, pb, compareLength);

      return (res < 0 || (res == 0 && sizea < sizeb));
    });
};

uint8_t const* Builder::findAttrName (uint8_t const* base, uint64_t& len) {
  uint8_t const b = *base;
  if (b >= 0x40 && b <= 0xbe) {
    // short UTF-8 string
    len = b - 0x40;
    return base + 1;
  }
  if (b == 0xbf) {
    // long UTF-8 string
    len = 0;
    // read string length
    for (size_t i = 8; i >= 1; i--) {
      len = (len << 8) + base[i];
    }
    return base + 1 + 8; // string starts here
  }
  throw Exception(Exception::NotImplemented, "Invalid Object key type");
}

void Builder::sortObjectIndexShort (uint8_t* objBase,
                                         std::vector<ValueLength>& offsets) {
  auto cmp = [&] (ValueLength a, ValueLength b) -> bool {
    uint8_t const* aa = objBase + a;
    uint8_t const* bb = objBase + b;
    if (*aa >= 0x40 && *aa <= 0xbe &&
        *bb >= 0x40 && *bb <= 0xbe) {
      // The fast path, short strings:
      uint8_t m = (std::min)(*aa - 0x40, *bb - 0x40);
      int c = memcmp(aa+1, bb+1, static_cast<size_t>(m));
      return (c < 0 || (c == 0 && *aa < *bb));
    }
    else {
      uint64_t lena;
      uint64_t lenb;
      aa = findAttrName(aa, lena);
      bb = findAttrName(bb, lenb);
      uint64_t m = (std::min)(lena, lenb);
      int c = memcmp(aa, bb, m);
      return (c < 0 || (c == 0 && lena < lenb));
    }
  };
  std::sort(offsets.begin(), offsets.end(), cmp);
}

void Builder::sortObjectIndexLong (uint8_t* objBase,
                                        std::vector<ValueLength>& offsets) {

  // on some platforms we can use a thread-local vector
#if __llvm__ == 1
  // nono thread local
  std::vector<Builder::SortEntry> entries;
#elif defined(_WIN32) && defined(_MSC_VER)
  std::vector<Builder::SortEntry> entries;
#else
  // thread local vector for sorting large object attributes
  thread_local std::vector<Builder::SortEntry> entries;
  entries.clear();
#endif
  
  entries.reserve(offsets.size());
  for (ValueLength i = 0; i < offsets.size(); i++) {
    SortEntry e;
    e.offset = offsets[i];
    e.nameStart = findAttrName(objBase + e.offset, e.nameSize);
    entries.push_back(e);
  }
  VELOCYPACK_ASSERT(entries.size() == offsets.size());
  doActualSort(entries);

  // copy back the sorted offsets 
  for (ValueLength i = 0; i < offsets.size(); i++) {
    offsets[i] = entries[i].offset;
  }
}

void Builder::sortObjectIndex (uint8_t* objBase,
                                    std::vector<ValueLength>& offsets) {
  if (offsets.size() > 32) {
    sortObjectIndexLong(objBase, offsets);
  }
  else {
    sortObjectIndexShort(objBase, offsets);
  }
}

void Builder::close () {
  if (_stack.empty()) {
    throw Exception(Exception::BuilderNeedOpenObject);
  }
  ValueLength& tos = _stack.back();
  if (_start[tos] != 0x06 && _start[tos] != 0x0b) {
    throw Exception(Exception::BuilderNeedOpenObject);
  }
  std::vector<ValueLength>& index = _index[_stack.size() - 1];
  if (index.empty()) {
    _start[tos] = (_start[tos] == 0x06) ? 0x01 : 0x0a;
    VELOCYPACK_ASSERT(_pos == tos + 9);
    _pos -= 8;  // no bytelength and number subvalues needed
    _stack.pop_back();
    // Intentionally leave _index[depth] intact to avoid future allocs!
    return;
  }
  // From now on index.size() > 0

  bool needIndexTable = true;
  bool needNrSubs = true;
  if (index.size() == 1) {
    needIndexTable = false;
    if (_start[tos] == 0x06) {
      needNrSubs = false;
    }
    // For objects we leave needNrSubs at true here!
  }
  else if (_start[tos] == 0x06 &&   // an array
           (_pos - tos) - index[0] == index.size() * (index[1] - index[0])) {
    // In this case it could be that all entries have the same length
    // and we do not need an offset table at all:
    bool noTable = true;
    ValueLength subLen = index[1] - index[0];
    for (size_t i = 1; i < index.size() - 1; i++) {
      if (index[i + 1] - index[i] != subLen) {
        noTable = false;
        break;
      }
    }
    if ((_pos - tos) - index[index.size()-1] != subLen) {
      noTable = false;
    }
    if (noTable) {
      needIndexTable = false;
      needNrSubs = false;
    }
  }

  // First determine byte length and its format:
  unsigned int offsetSize;   
        // can be 1, 2, 4 or 8 for the byte width of the offsets,
        // the byte length and the number of subvalues:
  if (_pos - tos + (needIndexTable ? index.size() : 0) 
                 - (needNrSubs ? 6 : 7) <= 0xff) {
    // We have so far used _pos - tos bytes, including the reserved 8
    // bytes for byte length and number of subvalues. In the 1-byte number
    // case we would win back 6 bytes but would need one byte per subvalue
    // for the index table
    offsetSize = 1;
  }
  else if (_pos - tos + (needIndexTable ? 2 * index.size() : 0) <= 0xffff) {
    offsetSize = 2;
  }
  else if (_pos - tos + (needIndexTable ? 4 * index.size() : 0) <= 0xffffffffu) {
    offsetSize = 4;
  }
  else {
    offsetSize = 8;
  }

  // Maybe we need to move down data:
  if (offsetSize == 1) {
    unsigned int targetPos = 3;
    if (! needIndexTable && _start[tos] == 0x06) {
      targetPos = 2;
    }
    if (_pos > (tos + 9)) {
      memmove(_start + tos + targetPos, _start + tos + 9,
              _pos - (tos + 9));
    }
    _pos -= (9 - targetPos);
    for (size_t i = 0; i < index.size(); i++) {
      index[i] -= (9 - targetPos);
    }
  }
  // One could move down things in the offsetSize == 2 case as well,
  // since we only need 4 bytes in the beginning. However, saving these
  // 4 bytes has been sacrificed on the Altar of Performance.

  // Now build the table:
  if (needIndexTable) {
    ValueLength tableBase;
    reserveSpace(offsetSize * index.size() + (offsetSize == 8 ? 8 : 0));
    tableBase = _pos;
    _pos += offsetSize * index.size();
    if (_start[tos] == 0x0b) {  // an object
      if (! options.sortAttributeNames) {
        _start[tos] = 0x0f;  // unsorted
      }
      else if (index.size() >= 2 &&
               options.sortAttributeNames) {
        sortObjectIndex(_start + tos, index);
      }
    }
    for (size_t i = 0; i < index.size(); i++) {
      uint64_t x = index[i];
      for (size_t j = 0; j < offsetSize; j++) {
        _start[tableBase + offsetSize * i + j] = x & 0xff;
        x >>= 8;
      }
    }
  }
  else {  // no index table
    if (_start[tos] == 0x06) {
      _start[tos] = 0x02;
    }
  }
  // Finally fix the byte width in the type byte:
  if (offsetSize > 1) {
    if (offsetSize == 2) {
      _start[tos] += 1;
    }
    else if (offsetSize == 4) {
      _start[tos] += 2;
    }
    else {   // offsetSize == 8
      _start[tos] += 3;
      if (needNrSubs) {
        appendLength(index.size(), 8);
      }
    }
  }

  // Fix the byte length in the beginning:
  ValueLength x = _pos - tos;
  for (unsigned int i = 1; i <= offsetSize; i++) {
    _start[tos + i] = x & 0xff;
    x >>= 8;
  }

  if (offsetSize < 8 && needNrSubs) {
    x = index.size();
    for (unsigned int i = offsetSize + 1; i <= 2 * offsetSize; i++) {
      _start[tos + i] = x & 0xff;
      x >>= 8;
    }
  }

  // And, if desired, check attribute uniqueness:
  if (options.checkAttributeUniqueness && index.size() > 1 &&
      _start[tos] >= 0x0b) {
    // check uniqueness of attribute names
    checkAttributeUniqueness(Slice(_start + tos));
  }

  // Now the array or object is complete, we pop a ValueLength 
  // off the _stack:
  _stack.pop_back();
  // Intentionally leave _index[depth] intact to avoid future allocs!
}

uint8_t* Builder::set (Value const& item) {
  auto const oldPos = _start + _pos;
  auto ctype = item.cType();

  // This method builds a single further VPack item at the current
  // append position. If this is an array or object, then an index
  // table is created and a new ValueLength is pushed onto the stack.
  switch (item.valueType()) {
    case ValueType::None: {
      throw Exception(Exception::BuilderUnexpectedType, "Cannot set a ValueType::None");
    }
    case ValueType::Null: {
      reserveSpace(1);
      _start[_pos++] = 0x18;
      break;
    }
    case ValueType::Bool: {
      if (ctype != Value::CType::Bool) {
        throw Exception(Exception::BuilderUnexpectedValue, "Must give bool for ValueType::Bool");
      }
      reserveSpace(1);
      if (item.getBool()) {
        _start[_pos++] = 0x1a;
      }
      else {
        _start[_pos++] = 0x19;
      }
      break;
    }
    case ValueType::Double: {
      static_assert(sizeof(double) == sizeof(uint64_t),
                    "size of double is not 8 bytes");
      double v = 0.0;
      uint64_t x;
      switch (ctype) {
        case Value::CType::Double:
          v = item.getDouble();
          break;
        case Value::CType::Int64:
          v = static_cast<double>(item.getInt64());
          break;
        case Value::CType::UInt64:
          v = static_cast<double>(item.getUInt64());
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue, "Must give number for ValueType::Double");
      }
      reserveSpace(1 + sizeof(double));
      _start[_pos++] = 0x1b;
      memcpy(&x, &v, sizeof(double));
      appendLength(x, 8);
      break;
    }
    case ValueType::External: {
      if (ctype != Value::CType::VoidPtr) {
        throw Exception(Exception::BuilderUnexpectedValue, "Must give void pointer for ValueType::External");
      }
      reserveSpace(1 + sizeof(void*));
      // store pointer. this doesn't need to be portable
      _start[_pos++] = 0x1d;
      void const* value = item.getExternal();
      memcpy(_start + _pos, &value, sizeof(void*));
      _pos += sizeof(void*);
      break;
    }
    case ValueType::SmallInt: {
      int64_t vv = 0;
      switch (ctype) {
        case Value::CType::Double:
          vv = static_cast<int64_t>(item.getDouble());
          break;
        case Value::CType::Int64:
          vv = item.getInt64();
          break;
        case Value::CType::UInt64:
          vv = static_cast<int64_t>(item.getUInt64());
        default:
          throw Exception(Exception::BuilderUnexpectedValue, "Must give number for ValueType::SmallInt");
      }
      if (vv < -6 || vv > 9) {
        throw Exception(Exception::NumberOutOfRange, "Number out of range of ValueType::SmallInt");
      } 
      reserveSpace(1);
      if (vv >= 0) {
        _start[_pos++] = static_cast<uint8_t>(vv + 0x30);
      }
      else {
        _start[_pos++] = static_cast<uint8_t>(vv + 0x40);
      }
      break;
    }
    case ValueType::Int: {
      int64_t v;
      switch (ctype) {
        case Value::CType::Double:
          v = static_cast<int64_t>(item.getDouble());
          break;
        case Value::CType::Int64:
          v = item.getInt64();
          break;
        case Value::CType::UInt64:
          v = ToInt64(item.getUInt64());
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue, "Must give number for ValueType::Int");
      }
      addInt(v);
      break;
    }
    case ValueType::UInt: {
      uint64_t v = 0;
      switch (ctype) {
        case Value::CType::Double:
          if (item.getDouble() < 0.0) {
            throw Exception(Exception::BuilderUnexpectedValue, "Must give non-negative number for ValueType::UInt");
          }
          v = static_cast<uint64_t>(item.getDouble());
          break;
        case Value::CType::Int64:
          if (item.getInt64() < 0) {
            throw Exception(Exception::BuilderUnexpectedValue, "Must give non-negative number for ValueType::UInt");
          }
          v = static_cast<uint64_t>(item.getInt64());
          break;
        case Value::CType::UInt64:
          v = item.getUInt64();
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue, "Must give number for ValueType::UInt");
      }
      addUInt(v); 
      break;
    }
    case ValueType::UTCDate: {
      int64_t v;
      switch (ctype) {
        case Value::CType::Double:
          v = static_cast<int64_t>(item.getDouble());
          break;
        case Value::CType::Int64:
          v = item.getInt64();
          break;
        case Value::CType::UInt64:
          v = ToInt64(item.getUInt64());
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue, "Must give number for ValueType::UTCDate");
      }
      addUTCDate(v);
      break;
    }
    case ValueType::String: {
      if (ctype != Value::CType::String &&
          ctype != Value::CType::CharPtr) {
        throw Exception(Exception::BuilderUnexpectedValue, "Must give a string or char const* for ValueType::String");
      }
      std::string const* s;
      std::string value;
      if (ctype == Value::CType::String) {
        s = item.getString();
      }
      else {
        value = item.getCharPtr();
        s = &value;
      }
      size_t size = s->size();
      if (size <= 126) {
        // short string
        reserveSpace(1 + size);
        _start[_pos++] = static_cast<uint8_t>(0x40 + size);
        memcpy(_start + _pos, s->c_str(), size);
      }
      else {
        // long string
        reserveSpace(1 + 8 + size);
        _start[_pos++] = 0xbf;
        appendLength(size, 8);
        memcpy(_start + _pos, s->c_str(), size);
      }
      _pos += size;
      break;
    }
    case ValueType::Array: {
      addArray();
      break;
    }
    case ValueType::Object: {
      addObject();
      break;
    }
    case ValueType::Binary: {
      if (ctype != Value::CType::String &&
          ctype != Value::CType::CharPtr) {
        throw Exception(Exception::BuilderUnexpectedValue, "Must provide std::string or char const* for ValueType::Binary");
      }
      std::string const* s;
      std::string value;
      if (ctype == Value::CType::String) {
        s = item.getString();
      }
      else {
        value = item.getCharPtr();
        s = &value;
      }
      ValueLength v = s->size();
      appendUInt(v, 0xbf);
      memcpy(_start + _pos, s->c_str(), v);
      _pos += v;
      break;
    }
    case ValueType::MinKey: {
      reserveSpace(1);
      _start[_pos++] = 0x1e;
      break;
    }
    case ValueType::MaxKey: {
      reserveSpace(1);
      _start[_pos++] = 0x1f;
      break;
    }
    case ValueType::BCD: {
      throw Exception(Exception::NotImplemented);
    }
    case ValueType::Custom: {
      throw Exception(Exception::BuilderUnexpectedType, "Cannot set a ValueType::Custom with this method");
    }
  }
  return oldPos;
}

uint8_t* Builder::set (Slice const& item) {
  ValueLength const l = item.byteSize();
  reserveSpace(l);
  memcpy(_start + _pos, item.start(), l);
  _pos += l;
  return _start + _pos - l;
}

uint8_t* Builder::set (ValuePair const& pair) {
  // This method builds a single further VPack item at the current
  // append position. This is the case for ValueType::ID or
  // ValueType::Binary, which both need two pieces of information
  // to build.
  if (pair.valueType() == ValueType::Binary) {
    uint64_t v = pair.getSize();
    appendUInt(v, 0xbf);
    memcpy(_start + _pos, pair.getStart(), v);
    _pos += v;
    return nullptr;  // unused here
  }
  else if (pair.valueType() == ValueType::String) {
    uint64_t size = pair.getSize();
    if (size > 126) { 
      // long string
      reserveSpace(1 + 8 + size);
      _start[_pos++] = 0xbf;
      appendLength(size, 8);
      _pos += size;
    }
    else {
      // short string
      reserveSpace(1 + size);
      _start[_pos++] = static_cast<uint8_t>(0x40 + size);
      _pos += size;
    }
    // Note that the data is not filled in! It is the responsibility
    // of the caller to fill in 
    //   _start + _pos - size .. _start + _pos - 1
    // with valid UTF-8!
    return _start + _pos - size;
  }
  else if (pair.valueType() == ValueType::Custom) {
    // We only reserve space here, the caller has to fill in the custom type
    uint64_t size = pair.getSize();
    reserveSpace(size);
    uint8_t const* p = pair.getStart();
    if (p != nullptr) {
      memcpy(_start + _pos, p, size);
    }
    _pos += size;
    return _start + _pos - size;
  }
  throw Exception(Exception::BuilderUnexpectedType, "Only ValueType::Binary, ValueType::String and ValueType::Custom are valid for ValuePair argument");
}

void Builder::checkAttributeUniqueness (Slice const obj) const {
  VELOCYPACK_ASSERT(options.checkAttributeUniqueness == true);
  ValueLength const n = obj.length();

  if (obj.isSorted()) {
    // object attributes are sorted
    Slice previous = obj.keyAt(0);
    ValueLength len;
    char const* p = previous.getString(len);

    // compare each two adjacent attribute names
    for (ValueLength i = 1; i < n; ++i) {
      Slice current = obj.keyAt(i);
      if (! current.isString()) {
        throw Exception(Exception::InternalError, "Expecting String key");
      }
      
      ValueLength len2;
      char const* q = current.getString(len2);

      if (len == len2 && memcmp(p, q, len2) == 0) {
        // identical key
        throw Exception(Exception::DuplicateAttributeName);
      }
      // re-use already calculated values for next round
      len = len2;
      p = q;
    }
  }
  else {
    std::unordered_set<std::string> keys;

    for (size_t i = 0; i < n; ++i) {
      Slice key = obj.keyAt(i);
      if (! key.isString()) {
        throw Exception(Exception::InternalError, "Expecting String key");
      }
      
      if (! keys.emplace(key.copyString()).second) {
        throw Exception(Exception::DuplicateAttributeName);
      }
    }
  }
}

uint8_t* Builder::add (std::string const& attrName, Value const& sub) {
  return addInternal<Value>(attrName, sub);
}

uint8_t* Builder::add (std::string const& attrName, Slice const& sub) {
  return addInternal<Slice>(attrName, sub);
}

uint8_t* Builder::add (std::string const& attrName, ValuePair const& sub) {
  return addInternal<ValuePair>(attrName, sub);
}

uint8_t* Builder::add (Value const& sub) {
  return addInternal<Value>(sub);
}

uint8_t* Builder::add (Slice const& sub) {
  return addInternal<Slice>(sub);
}

uint8_t* Builder::add (ValuePair const& sub) {
  return addInternal<ValuePair>(sub);
}

