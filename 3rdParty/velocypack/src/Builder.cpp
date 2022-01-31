////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <memory>
#include <string_view>
#include <unordered_set>

#include "velocypack/velocypack-common.h"
#include "velocypack/Builder.h"
#include "velocypack/Dumper.h"
#include "velocypack/Iterator.h"
#include "velocypack/Sink.h"

using namespace arangodb::velocypack;

namespace {
  
// checks whether a memmove operation is allowed to get rid of the padding
bool isAllowedToMemmove(Options const* options, uint8_t const* start, 
                        std::vector<ValueLength>::iterator indexStart, 
                        std::vector<ValueLength>::iterator indexEnd,
                        ValueLength offsetSize) {
  VELOCYPACK_ASSERT(offsetSize == 1 || offsetSize == 2);

  if (options->paddingBehavior == Options::PaddingBehavior::NoPadding || 
      (offsetSize == 1 && options->paddingBehavior == Options::PaddingBehavior::Flexible)) {
    std::size_t const distance = std::distance(indexStart, indexEnd);
    std::size_t const n = (std::min)(std::size_t(8 - 2 * offsetSize), distance);
    for (std::size_t i = 0; i < n; i++) {
      if (start[indexStart[i]] == 0x00) {
        return false;
      }
    }
    return true;
  }

  return false;
}

uint8_t determineArrayType(bool needIndexTable, ValueLength offsetSize) {
  uint8_t type;
  // Now build the table:
  if (needIndexTable) {
    type = 0x06;
  } else {  // no index table
    type = 0x02;
  }
  // Finally fix the byte width in the type byte:
  if (offsetSize == 2) {
    type += 1;
  } else if (offsetSize == 4) {
    type += 2;
  } else if (offsetSize == 8) {
    type += 3;
  }
  return type;
}

constexpr ValueLength linearAttributeUniquenessCutoff = 4;
  
// struct used when sorting index tables for objects:
struct SortEntry {
  uint8_t const* nameStart;
  uint64_t nameSize;
  uint64_t offset;
};

// minimum allocation done for the sortEntries vector
// this is used to overallocate memory so we can avoid some follow-up
// reallocations
constexpr size_t minSortEntriesAllocation = 32;


#ifndef VELOCYPACK_NO_THREADLOCALS

// thread-local, reusable buffer used for sorting medium to big index entries
thread_local std::unique_ptr<std::vector<SortEntry>> sortEntries;

// thread-local, reusable set to track usage of duplicate keys
thread_local std::unique_ptr<std::unordered_set<std::string_view>> duplicateKeys;

#endif


// Find the actual bytes of the attribute name of the VPack value
// at position base, also determine the length len of the attribute.
// This takes into account the different possibilities for the format
// of attribute names:
uint8_t const* findAttrName(uint8_t const* base, uint64_t& len) {
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
    for (std::size_t i = 8; i >= 1; i--) {
      len = (len << 8) + base[i];
    }
    return base + 1 + 8;  // string starts here
  }

  // translate attribute name
  return findAttrName(arangodb::velocypack::Slice(base).makeKey().start(), len);
}

bool checkAttributeUniquenessUnsortedBrute(ObjectIterator& it) {
  std::array<std::string_view, linearAttributeUniquenessCutoff> keys;

  do {
    // key(true) guarantees a String as returned type
    std::string_view key = it.key(true).stringView();

    ValueLength index = it.index();
    // compare with all other already looked-at keys
    for (ValueLength i = 0; i < index; ++i) {
      if (VELOCYPACK_UNLIKELY(keys[i] == key)) {
        return false;
      }
    }
    keys[index] = key;
    it.next();

  } while (it.valid());

  return true;
}

bool checkAttributeUniquenessUnsortedSet(ObjectIterator& it) {
#ifndef VELOCYPACK_NO_THREADLOCALS
  std::unique_ptr<std::unordered_set<std::string_view>>& tmp = ::duplicateKeys;

  if (::duplicateKeys == nullptr) {
    ::duplicateKeys = std::make_unique<std::unordered_set<std::string_view>>();
  } else {
    ::duplicateKeys->clear();
  }
#else
  auto tmp = std::make_unique<std::unordered_set<std::string_view>>();
#endif

  do {
    Slice key = it.key(true);
    // key(true) guarantees a String as returned type
    VELOCYPACK_ASSERT(key.isString());
    if (VELOCYPACK_UNLIKELY(!tmp->emplace(key.stringView()).second)) {
      // identical key
      return false;
    }
    it.next();
  } while (it.valid());

  return true;
}

} // namespace

// create an empty Builder, using default Options 
Builder::Builder()
      : _buffer(std::make_shared<Buffer<uint8_t>>()),
        _bufferPtr(_buffer.get()),
        _start(_bufferPtr->data()),
        _pos(0),
        _arena(),
        _stack(_arena),
        _keyWritten(false),
        options(&Options::Defaults) {
  // do a full initial allocation in the arena, so we can maximize its usage
  _stack.reserve(arenaSize / sizeof(decltype(_stack)::value_type));
}
  
// create an empty Builder, using Options 
Builder::Builder(Options const* opts)
      : Builder() {
  if (VELOCYPACK_UNLIKELY(opts == nullptr)) {
    throw Exception(Exception::InternalError, "Options cannot be a nullptr");
  }
  options = opts;
}

// create an empty Builder, using an existing buffer and default Options
Builder::Builder(std::shared_ptr<Buffer<uint8_t>> buffer)
      : _buffer(std::move(buffer)), 
        _bufferPtr(_buffer.get()), 
        _start(nullptr),
        _pos(0), 
        _arena(),
        _stack(_arena),
        _keyWritten(false), 
        options(&Options::Defaults) {
  if (VELOCYPACK_UNLIKELY(_bufferPtr == nullptr)) {
    throw Exception(Exception::InternalError, "Buffer cannot be a nullptr");
  }
  _start = _bufferPtr->data();
  _pos = _bufferPtr->size();
  
  // do a full initial allocation in the arena, so we can maximize its usage
  _stack.reserve(arenaSize / sizeof(decltype(_stack)::value_type));
}
  
// create an empty Builder, using an existing buffer
Builder::Builder(std::shared_ptr<Buffer<uint8_t>> buffer, Options const* opts)
      : Builder(std::move(buffer)) {
  if (VELOCYPACK_UNLIKELY(opts == nullptr)) {
    throw Exception(Exception::InternalError, "Options cannot be a nullptr");
  }
  options = opts;
}

// create a Builder that uses an existing Buffer and options. 
// the Builder will not claim ownership for its Buffer
Builder::Builder(Buffer<uint8_t>& buffer) noexcept
      : _bufferPtr(&buffer), 
        _start(_bufferPtr->data()),
        _pos(buffer.size()), 
        _arena(),
        _stack(_arena),
        _keyWritten(false), 
        options(&Options::Defaults) {
  // do a full initial allocation in the arena, so we can maximize its usage
  _stack.reserve(arenaSize / sizeof(decltype(_stack)::value_type));
}
  
// create a Builder that uses an existing Buffer. the Builder will not
// claim ownership for its Buffer
Builder::Builder(Buffer<uint8_t>& buffer, Options const* opts)
      : Builder(buffer) {
  if (VELOCYPACK_UNLIKELY(opts == nullptr)) {
    throw Exception(Exception::InternalError, "Options cannot be a nullptr");
  }
  options = opts;
}
  
// populate a Builder from a Slice
Builder::Builder(Slice slice, Options const* options)
      : Builder(options) {
  add(slice);
}

Builder::Builder(Builder const& that)
      : _bufferPtr(nullptr),
        _start(nullptr),
        _pos(that._pos),
        _arena(),
        _stack(_arena),
        _indexes(that._indexes),
        _keyWritten(that._keyWritten),
        options(that.options) {
  VELOCYPACK_ASSERT(options != nullptr);

  _stack = that._stack;

  if (that._buffer == nullptr) {
    _bufferPtr = that._bufferPtr;
  } else {
    _buffer = std::make_shared<Buffer<uint8_t>>(*that._buffer);
    _bufferPtr = _buffer.get();
  }
        
  if (_bufferPtr != nullptr) {
    _start = _bufferPtr->data();
  }
  
  // do a full initial allocation in the arena, so we can maximize its usage
  _stack.reserve(arenaSize / sizeof(decltype(_stack)::value_type));
}

Builder& Builder::operator=(Builder const& that) {
  if (this != &that) {
    if (that._buffer == nullptr) {
      _buffer.reset();
      _bufferPtr = that._bufferPtr;
    } else {
      _buffer = std::make_shared<Buffer<uint8_t>>(*that._buffer);
      _bufferPtr = _buffer.get();
    }
    if (_bufferPtr == nullptr) {
      _start = nullptr;
    } else {
      _start = _bufferPtr->data();
    }
    _pos = that._pos;
    _stack = that._stack;
    _indexes = that._indexes;
    _keyWritten = that._keyWritten;
    options = that.options;
  }
  VELOCYPACK_ASSERT(options != nullptr);
  return *this;
}

Builder::Builder(Builder&& that) noexcept
    : _buffer(std::move(that._buffer)),
      _bufferPtr(nullptr),
      _start(nullptr),
      _pos(that._pos),
      _arena(),
      _stack(_arena),
      _indexes(std::move(that._indexes)),
      _keyWritten(that._keyWritten),
      options(that.options) {
      
  // do a full initial allocation in the arena, so we can maximize its usage
  _stack.reserve(arenaSize / sizeof(decltype(_stack)::value_type));
  _stack = std::move(that._stack);
  
  if (_buffer != nullptr) {
    _bufferPtr = _buffer.get();
  } else {
    _bufferPtr = that._bufferPtr;
  }
  if (_bufferPtr != nullptr) {
    _start = _bufferPtr->data();
  }
  VELOCYPACK_ASSERT(that._buffer == nullptr);
  that._bufferPtr = nullptr;
  that.clear();
}

Builder& Builder::operator=(Builder&& that) noexcept {
  if (this != &that) {
    _buffer = std::move(that._buffer);
    if (_buffer != nullptr) {
      _bufferPtr = _buffer.get();
    } else {
      _bufferPtr = that._bufferPtr;
    }
    if (_bufferPtr != nullptr) {
      _start = _bufferPtr->data();
    } else {
      _start = nullptr;
    }
    _pos = that._pos;
    // do a full initial allocation in the arena, so we can maximize its usage
    _stack.reserve(arenaSize / sizeof(decltype(_stack)::value_type));
    _stack = std::move(that._stack);
    _indexes = std::move(that._indexes);
    _keyWritten = that._keyWritten;
    options = that.options;
    VELOCYPACK_ASSERT(that._buffer == nullptr);
    that._bufferPtr = nullptr;
    that.clear();
  }
  return *this;
}

std::string Builder::toString() const {
  Options opts;
  opts.prettyPrint = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper::dump(slice(), &sink, &opts);
  return buffer;
}

std::string Builder::toJson() const {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper::dump(slice(), &sink);
  return buffer;
}
  
void Builder::sortObjectIndexShort(uint8_t* objBase,
                                   std::vector<ValueLength>::iterator indexStart,
                                   std::vector<ValueLength>::iterator indexEnd) const {
  std::sort(indexStart, indexEnd, [objBase](ValueLength const& a, ValueLength const& b) {
    uint8_t const* aa = objBase + a;
    uint8_t const* bb = objBase + b;
    if (*aa >= 0x40 && *aa <= 0xbe && *bb >= 0x40 && *bb <= 0xbe) {
      // The fast path, short strings:
      uint8_t m = (std::min)(*aa - 0x40, *bb - 0x40);
      int c = std::memcmp(aa + 1, bb + 1, checkOverflow(m));
      return (c < 0 || (c == 0 && *aa < *bb));
    } else {
      uint64_t lena;
      uint64_t lenb;
      aa = findAttrName(aa, lena);
      bb = findAttrName(bb, lenb);
      uint64_t m = (std::min)(lena, lenb);
      int c = std::memcmp(aa, bb, checkOverflow(m));
      return (c < 0 || (c == 0 && lena < lenb));
    }
  });
}

void Builder::sortObjectIndexLong(uint8_t* objBase,
                                  std::vector<ValueLength>::iterator indexStart,
                                  std::vector<ValueLength>::iterator indexEnd) const {
#ifndef VELOCYPACK_NO_THREADLOCALS
  std::unique_ptr<std::vector<SortEntry>>& tmp = ::sortEntries;

  // start with clean sheet in case the previous run left something
  // in the vector (e.g. when bailing out with an exception)
  if (::sortEntries == nullptr) {
    ::sortEntries = std::make_unique<std::vector<SortEntry>>();
  } else {
    ::sortEntries->clear();
  }
#else
  auto tmp = std::make_unique<std::vector<SortEntry>>();
#endif

  std::size_t const n = std::distance(indexStart, indexEnd);
  VELOCYPACK_ASSERT(n > 1);
  tmp->reserve(std::max(::minSortEntriesAllocation, n));
  for (std::size_t i = 0; i < n; i++) {
    SortEntry e;
    e.offset = indexStart[i];
    e.nameStart = ::findAttrName(objBase + e.offset, e.nameSize);
    tmp->push_back(e);
  }
  VELOCYPACK_ASSERT(tmp->size() == n);
  std::sort(tmp->begin(), tmp->end(), [](SortEntry const& a,
                                         SortEntry const& b)
#ifdef VELOCYPACK_64BIT
    noexcept
#endif
  {
    // return true iff a < b:
    uint64_t sizea = a.nameSize;
    uint64_t sizeb = b.nameSize;
    std::size_t const compareLength = checkOverflow((std::min)(sizea, sizeb));
    int res = std::memcmp(a.nameStart, b.nameStart, compareLength);

    return (res < 0 || (res == 0 && sizea < sizeb));
  });

  // copy back the sorted offsets
  for (std::size_t i = 0; i < n; i++) {
    indexStart[i] = (*tmp)[i].offset;
  }
}

Builder& Builder::closeEmptyArrayOrObject(ValueLength pos, bool isArray) {
  // empty Array or Object
  _start[pos] = (isArray ? 0x01 : 0x0a);
  VELOCYPACK_ASSERT(_pos == pos + 9);
  rollback(8); // no bytelength and number subvalues needed
  closeLevel();
  return *this;
}

bool Builder::closeCompactArrayOrObject(ValueLength pos, bool isArray,
                                        std::vector<ValueLength>::iterator indexStart,
                                        std::vector<ValueLength>::iterator indexEnd) {
  std::size_t const n = std::distance(indexStart, indexEnd);

  // use compact notation
  ValueLength nLen =
      getVariableValueLength(static_cast<ValueLength>(n));
  VELOCYPACK_ASSERT(nLen > 0);
  ValueLength byteSize = _pos - (pos + 8) + nLen;
  VELOCYPACK_ASSERT(byteSize > 0);
  ValueLength bLen = getVariableValueLength(byteSize);
  byteSize += bLen;
  if (getVariableValueLength(byteSize) != bLen) {
    byteSize += 1;
    bLen += 1;
  }

  if (bLen < 9) {
    // can only use compact notation if total byte length is at most 8 bytes
    // long
    _start[pos] = (isArray ? 0x13 : 0x14);
    ValueLength targetPos = 1 + bLen;

    if (_pos > (pos + 9)) {
      ValueLength len = _pos - (pos + 9);
      memmove(_start + pos + targetPos, _start + pos + 9, checkOverflow(len));
    }

    // store byte length
    VELOCYPACK_ASSERT(byteSize > 0);
    storeVariableValueLength<false>(_start + pos + 1, byteSize);

    // need additional memory for storing the number of values
    if (nLen > 8 - bLen) {
      reserve(nLen);
    }
    storeVariableValueLength<true>(_start + pos + byteSize - 1,
                                   static_cast<ValueLength>(n));

    rollback(8);
    advance(nLen + bLen);

    closeLevel();
    return true;
  }
  return false;
}

Builder& Builder::closeArray(ValueLength pos, 
                             std::vector<ValueLength>::iterator indexStart,
                             std::vector<ValueLength>::iterator indexEnd) {
  std::size_t const n = std::distance(indexStart, indexEnd);
  VELOCYPACK_ASSERT(n > 0);

  bool needIndexTable = true;
  bool needNrSubs = true;

  if (n == 1) {
    // just one array entry
    needIndexTable = false;
    needNrSubs = false;
  } else if ((_pos - pos) - indexStart[0] == n * (indexStart[1] - indexStart[0])) {
    // In this case it could be that all entries have the same length
    // and we do not need an offset table at all:
    bool buildIndexTable = false;
    ValueLength const subLen = indexStart[1] - indexStart[0];
    if ((_pos - pos) - indexStart[n - 1] != subLen) {
      buildIndexTable = true;
    } else {
      for (std::size_t i = 1; i < n - 1; i++) {
        if (indexStart[i + 1] - indexStart[i] != subLen) {
          // different lengths
          buildIndexTable = true;
          break;
        }
      }
    }
        
    if (!buildIndexTable) {
      needIndexTable = false;
      needNrSubs = false;
    }
  }

  VELOCYPACK_ASSERT(needIndexTable == needNrSubs);

  // First determine byte length and its format:
  unsigned int offsetSize;
  // can be 1, 2, 4 or 8 for the byte width of the offsets,
  // the byte length and the number of subvalues:
  bool allowMemmove = ::isAllowedToMemmove(options, _start + pos, indexStart, indexEnd, 1);
  if (_pos - pos + 
      (needIndexTable ? n : 0) - 
      (allowMemmove ? (needNrSubs ? 6 : 7) : 0) <= 0xff) {
    // We have so far used _pos - pos bytes, including the reserved 8
    // bytes for byte length and number of subvalues. In the 1-byte number
    // case we would win back 6 bytes but would need one byte per subvalue
    // for the index table
    offsetSize = 1;
  } else {
    allowMemmove = ::isAllowedToMemmove(options, _start + pos, indexStart, indexEnd, 2);
    if (_pos - pos + 
        (needIndexTable ? 2 * n : 0) - 
        (allowMemmove ? (needNrSubs ? 4 : 6) : 0) <= 0xffff) {
      offsetSize = 2;
    } else {
      allowMemmove = false;
      if (_pos - pos + 
          (needIndexTable ? 4 * n : 0) <= 0xffffffffu) {
        offsetSize = 4;
      } else {
        offsetSize = 8;
      }
    }
  }

  VELOCYPACK_ASSERT(offsetSize == 1 || offsetSize == 2 || offsetSize == 4 || offsetSize == 8); 
  VELOCYPACK_ASSERT(!allowMemmove || offsetSize == 1 || offsetSize == 2);

  if (offsetSize < 8 &&
      !needIndexTable && 
      options->paddingBehavior == Options::PaddingBehavior::UsePadding) {
    // if we are allowed to use padding, we will pad to 8 bytes anyway. as we are not
    // using an index table, we can also use type 0x05 for all Arrays without making
    // things worse space-wise
    offsetSize = 8;
    allowMemmove = false;
  }

  // fix head byte
  _start[pos] = ::determineArrayType(needIndexTable, offsetSize);
  
  // Maybe we need to move down data:
  if (allowMemmove) {
    // check if one of the first entries in the array is ValueType::None 
    // (0x00). in this case, we could not distinguish between a None (0x00) 
    // and the optional padding. so we must prevent the memmove here
    ValueLength targetPos = 1 + 2 * offsetSize;
    if (!needIndexTable) {
      targetPos -= offsetSize;
    }
    if (_pos > (pos + 9)) {
      ValueLength len = _pos - (pos + 9);
      memmove(_start + pos + targetPos, _start + pos + 9, checkOverflow(len));
    }
    ValueLength const diff = 9 - targetPos;
    rollback(diff);
    if (needIndexTable) {
      for (std::size_t i = 0; i < n; i++) {
        indexStart[i] -= diff;
      }
    }  // Note: if !needIndexTable the index array is now wrong!
  }

  // Now build the table:
  if (needIndexTable) {
    reserve(offsetSize * n + (offsetSize == 8 ? 8 : 0));
    ValueLength tableBase = _pos;
    advance(offsetSize * n);
    for (std::size_t i = 0; i < n; i++) {
      uint64_t x = indexStart[i];
      for (std::size_t j = 0; j < offsetSize; j++) {
        _start[tableBase + offsetSize * i + j] = x & 0xff;
        x >>= 8;
      }
    }
  }

  // Finally fix the byte width at tthe end:
  if (offsetSize == 8 && needNrSubs) {
    reserve(8);
    appendLengthUnchecked<8>(n);
  }

  // Fix the byte length in the beginning:
  ValueLength x = _pos - pos;
  for (unsigned int i = 1; i <= offsetSize; i++) {
    _start[pos + i] = x & 0xff;
    x >>= 8;
  }

  if (offsetSize < 8 && needNrSubs) {
    x = n;
    for (unsigned int i = offsetSize + 1; i <= 2 * offsetSize; i++) {
      _start[pos + i] = x & 0xff;
      x >>= 8;
    }
  }

  // Now the array or object is complete, we pop a ValueLength
  // off the _stack:
  closeLevel();
  return *this;
}

Builder& Builder::close() {
  if (VELOCYPACK_UNLIKELY(isClosed())) {
    throw Exception(Exception::BuilderNeedOpenCompound);
  }
  VELOCYPACK_ASSERT(!_stack.empty());
  ValueLength const pos = _stack.back().startPos;
  ValueLength const indexStartPos = _stack.back().indexStartPos;
  uint8_t const head = _start[pos];

  VELOCYPACK_ASSERT(head == 0x06 || head == 0x0b || head == 0x13 ||
                    head == 0x14);

  bool const isArray = (head == 0x06 || head == 0x13);
  std::vector<ValueLength>::iterator indexStart = _indexes.begin() + indexStartPos; 
  std::vector<ValueLength>::iterator indexEnd = _indexes.end();
  ValueLength const n = std::distance(indexStart, indexEnd);

  if (n == 0) {
    closeEmptyArrayOrObject(pos, isArray);
    return *this;
  }

  // From now on index.size() > 0
  VELOCYPACK_ASSERT(n > 0);

  // check if we can use the compact Array / Object format
  if (head == 0x13 || head == 0x14 ||
      (head == 0x06 && options->buildUnindexedArrays) ||
      (head == 0x0b && (options->buildUnindexedObjects || n == 1))) {
    if (closeCompactArrayOrObject(pos, isArray, indexStart, indexEnd)) {
      // And, if desired, check attribute uniqueness:
      if (options->checkAttributeUniqueness && 
          n > 1 &&
          !checkAttributeUniqueness(Slice(_start + pos))) {
        // duplicate attribute name!
        throw Exception(Exception::DuplicateAttributeName);
      }
      return *this;
    }
    // This might fall through, if closeCompactArrayOrObject gave up!
  }

  if (isArray) {
    closeArray(pos, _indexes.begin() + indexStartPos, _indexes.end());
    return *this;
  }

  // from here on we are sure that we are dealing with Object types only.

  // fix head byte in case a compact Array / Object was originally requested
  _start[pos] = 0x0b;

  // First determine byte length and its format:
  unsigned int offsetSize = 8;
  // can be 1, 2, 4 or 8 for the byte width of the offsets,
  // the byte length and the number of subvalues:
  if (_pos - pos + n - 6 <= 0xff) {
    // We have so far used _pos - pos bytes, including the reserved 8
    // bytes for byte length and number of subvalues. In the 1-byte number
    // case we would win back 6 bytes but would need one byte per subvalue
    // for the index table
    offsetSize = 1;
    // One could move down things in the offsetSize == 2 case as well,
    // since we only need 4 bytes in the beginning. However, saving these
    // 4 bytes has been sacrificed on the Altar of Performance.
  } else if (_pos - pos + 2 * n <= 0xffff) {
    offsetSize = 2;
  } else if (_pos - pos + 4 * n <= 0xffffffffu) {
    offsetSize = 4;
  }
    
  if (offsetSize < 4 &&
      (options->paddingBehavior == Options::PaddingBehavior::NoPadding ||
       (offsetSize == 1 && options->paddingBehavior == Options::PaddingBehavior::Flexible))) {
    // Maybe we need to move down data:
    ValueLength targetPos = 1 + 2 * offsetSize;
    if (_pos > (pos + 9)) {
      ValueLength len = _pos - (pos + 9);
      memmove(_start + pos + targetPos, _start + pos + 9, checkOverflow(len));
    }
    ValueLength const diff = 9 - targetPos;
    rollback(diff);
    for (std::size_t i = 0; i < n; i++) {
      indexStart[i] -= diff;
    }
  }

  // Now build the table:
  reserve(offsetSize * n + (offsetSize == 8 ? 8 : 0));
  ValueLength tableBase = _pos;
  advance(offsetSize * n);
  // Object
  if (n >= 2) {
    sortObjectIndex(_start + pos, indexStart, indexEnd);
  }
  for (std::size_t i = 0; i < n; ++i) {
    uint64_t x = indexStart[i];
    for (std::size_t j = 0; j < offsetSize; ++j) {
      _start[tableBase + offsetSize * i + j] = x & 0xff;
      x >>= 8;
    }
  }
  // Finally fix the byte width in the type byte:
  if (offsetSize > 1) {
    if (offsetSize == 2) {
      _start[pos] += 1;
    } else if (offsetSize == 4) {
      _start[pos] += 2;
    } else {  // offsetSize == 8
      _start[pos] += 3;
      reserve(8);
      appendLengthUnchecked<8>(n);
    }
  }

  // Fix the byte length in the beginning:
  ValueLength x = _pos - pos;
  for (unsigned int i = 1; i <= offsetSize; i++) {
    _start[pos + i] = x & 0xff;
    x >>= 8;
  }

  if (offsetSize < 8) {
    x = n;
    for (unsigned int i = offsetSize + 1; i <= 2 * offsetSize; i++) {
      _start[pos + i] = x & 0xff;
      x >>= 8;
    }
  }

  // And, if desired, check attribute uniqueness:
  if (options->checkAttributeUniqueness && 
      n > 1 &&
      !checkAttributeUniqueness(Slice(_start + pos))) {
    // duplicate attribute name!
    throw Exception(Exception::DuplicateAttributeName);
  }

  // Now the array or object is complete, we pop a ValueLength
  // off the _stack:
  closeLevel();
      
  return *this;
}

// checks whether an Object value has a specific key attribute
bool Builder::hasKey(std::string_view key) const {
  return !getKey(key).isNone();
}

// return the value for a specific key of an Object value
Slice Builder::getKey(std::string_view key) const {
  if (VELOCYPACK_UNLIKELY(_stack.empty())) {
    throw Exception(Exception::BuilderNeedOpenObject);
  }
  VELOCYPACK_ASSERT(!_stack.empty());
  ValueLength const pos = _stack.back().startPos;
  ValueLength const indexStartPos = _stack.back().indexStartPos;
  if (VELOCYPACK_UNLIKELY(_start[pos] != 0x0b && _start[pos] != 0x14)) {
    throw Exception(Exception::BuilderNeedOpenObject);
  }
  std::vector<ValueLength>::const_iterator indexStart = _indexes.begin() + indexStartPos;
  std::vector<ValueLength>::const_iterator indexEnd = _indexes.end();
  while (indexStart != indexEnd) {
    Slice s(_start + pos + *indexStart);
    if (s.makeKey().isEqualString(key)) {
      return Slice(s.start() + s.byteSize());
    }
    ++indexStart;
  }
  return Slice();
}

void Builder::appendTag(uint64_t tag) {
  if (options->disallowTags) {
    // Tagged values explicitly disallowed
    throw Exception(Exception::BuilderTagsDisallowed);
  }
  if (tag <= 255) {
    reserve(1 + 1);
    appendByte(0xee);
    appendLengthUnchecked<1>(tag);
  } else {
    reserve(1 + 8);
    appendByte(0xef);
    appendLengthUnchecked<8>(tag);
  }
}

uint8_t* Builder::set(Value const& item) {
  auto const oldPos = _pos;
  auto ctype = item.cType();

  checkKeyIsString(item.valueType() == ValueType::String);

  // This method builds a single further VPack item at the current
  // append position. If this is an array or object, then an index
  // table is created and a new ValueLength is pushed onto the stack.
  switch (item.valueType()) {
    case ValueType::Null: {
      appendByte(0x18);
      break;
    }
    case ValueType::Bool: {
      if (VELOCYPACK_UNLIKELY(ctype != Value::CType::Bool)) {
        throw Exception(Exception::BuilderUnexpectedValue,
                        "Must give bool for ValueType::Bool");
      }
      appendByte(item.getBool() ? 0x1a : 0x19);
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
          throw Exception(Exception::BuilderUnexpectedValue,
                          "Must give number for ValueType::Double");
      }
      reserve(1 + sizeof(double));
      appendByteUnchecked(0x1b);
      std::memcpy(&x, &v, sizeof(double));
      appendLengthUnchecked<sizeof(double)>(x);
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
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue,
                          "Must give number for ValueType::SmallInt");
      }
      if (VELOCYPACK_UNLIKELY(vv < -6 || vv > 9)) {
        throw Exception(Exception::NumberOutOfRange,
                        "Number out of range of ValueType::SmallInt");
      }
      if (vv >= 0) {
        appendByte(static_cast<uint8_t>(vv + 0x30));
      } else {
        appendByte(static_cast<uint8_t>(vv + 0x40));
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
          v = toInt64(item.getUInt64());
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue,
                          "Must give number for ValueType::Int");
      }
      addInt(v);
      break;
    }
    case ValueType::UInt: {
      uint64_t v = 0;
      switch (ctype) {
        case Value::CType::Double:
          if (VELOCYPACK_UNLIKELY(item.getDouble() < 0.0)) {
            throw Exception(
                Exception::BuilderUnexpectedValue,
                "Must give non-negative number for ValueType::UInt");
          }
          v = static_cast<uint64_t>(item.getDouble());
          break;
        case Value::CType::Int64:
          if (VELOCYPACK_UNLIKELY(item.getInt64() < 0)) {
            throw Exception(
                Exception::BuilderUnexpectedValue,
                "Must give non-negative number for ValueType::UInt");
          }
          v = static_cast<uint64_t>(item.getInt64());
          break;
        case Value::CType::UInt64:
          v = item.getUInt64();
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue,
                          "Must give number for ValueType::UInt");
      }
      addUInt(v);
      break;
    }
    case ValueType::String: {
      char const* p;
      std::size_t size;
      if (ctype == Value::CType::String) {
        std::string const* s = item.getString();
        size = s->size();
        p = s->data();
      } else if (ctype == Value::CType::CharPtr) {
        p = item.getCharPtr();
        size = strlen(p);
      } else if (ctype == Value::CType::StringView) {
        std::string_view const* sv = item.getStringView();
        size = sv->size();
        p = sv->data();
      } else {
        throw Exception(
            Exception::BuilderUnexpectedValue,
            "Must give a string or char const* for ValueType::String");
      }
      if (size <= 126) {
        // short string
        reserve(1 + size);
        appendByteUnchecked(static_cast<uint8_t>(0x40 + size));
      } else {
        // long string
        reserve(1 + 8 + size);
        appendByteUnchecked(0xbf);
        appendLengthUnchecked<8>(size);
      }
      std::memcpy(_start + _pos, p, size);
      advance(size);
      break;
    }
    case ValueType::Array: {
      addArray(item.unindexed());
      break;
    }
    case ValueType::Object: {
      addObject(item.unindexed());
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
          v = toInt64(item.getUInt64());
          break;
        default:
          throw Exception(Exception::BuilderUnexpectedValue,
                          "Must give number for ValueType::UTCDate");
      }
      addUTCDate(v);
      break;
    }
    case ValueType::Binary: {
      if (VELOCYPACK_UNLIKELY(ctype != Value::CType::String && ctype != Value::CType::CharPtr &&
                              ctype != Value::CType::StringView)) {
        throw Exception(Exception::BuilderUnexpectedValue,
                        "Must provide std::string, std::string_view or char "
                        "const* for ValueType::Binary");
      }
      char const* p;
      ValueLength size;
      if (ctype == Value::CType::String) {
        p = item.getString()->data();
        size = item.getString()->size();
      } else if (ctype == Value::CType::StringView) {
        p = item.getStringView()->data();
        size = item.getStringView()->size();
      } else {
        p = item.getCharPtr();
        size = strlen(p);
      }
      appendUInt(size, 0xbf);
      reserve(size);
      std::memcpy(_start + _pos, p, checkOverflow(size));
      advance(size);
      break;
    }
    case ValueType::External: {
      if (options->disallowExternals) {
        // External values explicitly disallowed as a security
        // precaution
        throw Exception(Exception::BuilderExternalsDisallowed);
      }
      if (VELOCYPACK_UNLIKELY(ctype != Value::CType::VoidPtr)) {
        throw Exception(Exception::BuilderUnexpectedValue,
                        "Must give void pointer for ValueType::External");
      }
      reserve(1 + sizeof(void*));
      // store pointer. this doesn't need to be portable
      appendByteUnchecked(0x1d);
      void const* value = item.getExternal();
      std::memcpy(_start + _pos, &value, sizeof(void*));
      advance(sizeof(void*));
      break;
    }
    case ValueType::Illegal: {
      appendByte(0x17);
      break;
    }
    case ValueType::MinKey: {
      appendByte(0x1e);
      break;
    }
    case ValueType::MaxKey: {
      appendByte(0x1f);
      break;
    }
    case ValueType::BCD: {
      throw Exception(Exception::NotImplemented);
    }
    case ValueType::Tagged: {
      throw Exception(Exception::NotImplemented);
    }
    case ValueType::Custom: {
      if (options->disallowCustom) {
        // Custom values explicitly disallowed as a security precaution
        throw Exception(Exception::BuilderCustomDisallowed);
      }
      throw Exception(Exception::BuilderUnexpectedType,
                      "Cannot set a ValueType::Custom with this method");
    }
    case ValueType::None: {
      throw Exception(Exception::BuilderUnexpectedType,
                      "Cannot set a ValueType::None");
    }
  }
  return _start + oldPos;
}

uint8_t* Builder::set(Slice const& item) {
  checkKeyIsString(item);

  if (VELOCYPACK_UNLIKELY(options->disallowCustom && item.isCustom())) {
    // Custom values explicitly disallowed as a security precaution
    throw Exception(Exception::BuilderCustomDisallowed);
  }

  ValueLength const l = item.byteSize();
  reserve(l);
  std::memcpy(_start + _pos, item.start(), checkOverflow(l));
  advance(l);
  return _start + _pos - l;
}

uint8_t* Builder::set(ValuePair const& pair) {
  // This method builds a single further VPack item at the current
  // append position. This is the case for ValueType::String,
  // ValueType::Binary, or ValueType::Custom, which can be built
  // with two pieces of information

  auto const oldPos = _pos;

  checkKeyIsString(pair.valueType() == ValueType::String);

  if (pair.valueType() == ValueType::String) {
    uint64_t size = pair.getSize();
    if (size > 126) {
      // long string
      reserve(1 + 8 + size);
      appendByteUnchecked(0xbf);
      appendLengthUnchecked<8>(size);
    } else {
      // short string
      reserve(1 + size);
      appendByteUnchecked(static_cast<uint8_t>(0x40 + size));
    }
    VELOCYPACK_ASSERT(pair.getStart() != nullptr);
    std::memcpy(_start + _pos, pair.getStart(), checkOverflow(size));
    advance(size);
    return _start + oldPos;
  } else if (pair.valueType() == ValueType::Binary) {
    uint64_t v = pair.getSize();
    reserve(9 + v);
    appendUInt(v, 0xbf);
    VELOCYPACK_ASSERT(pair.getStart() != nullptr);
    std::memcpy(_start + _pos, pair.getStart(), checkOverflow(v));
    advance(v);
    return _start + oldPos;
  } else if (pair.valueType() == ValueType::Custom) {
    if (options->disallowCustom) {
      // Custom values explicitly disallowed as a security precaution
      throw Exception(Exception::BuilderCustomDisallowed);
    }
    // We only reserve space here, the caller has to fill in the custom type
    uint64_t size = pair.getSize();
    reserve(size);
    uint8_t const* p = pair.getStart();
    if (p != nullptr) {
      std::memcpy(_start + _pos, p, checkOverflow(size));
    }
    advance(size);
    return _start + _pos - size;
  }
  throw Exception(Exception::BuilderUnexpectedType,
                  "Only ValueType::Binary, ValueType::String and "
                  "ValueType::Custom are valid for ValuePair argument");
}

void Builder::cleanupAdd() noexcept {
  VELOCYPACK_ASSERT(!_stack.empty());
  VELOCYPACK_ASSERT(!_indexes.empty());
  _indexes.pop_back();
}

void Builder::reportAdd() {
  VELOCYPACK_ASSERT(!_stack.empty());
  if (_indexes.capacity() == 0) {
    // make an initial reservation for several items at
    // a time, in order to save frequent reallocations for
    // the first few attributes
    _indexes.reserve(8);
  }
  _indexes.push_back(_pos - _stack.back().startPos);
}


void Builder::closeLevel() noexcept {
  VELOCYPACK_ASSERT(!_stack.empty());
  ValueLength const indexStartPos = _stack.back().indexStartPos; 
  _stack.pop_back();
  _indexes.erase(_indexes.begin() + indexStartPos, _indexes.end());
}

bool Builder::checkAttributeUniqueness(Slice obj) const {
  VELOCYPACK_ASSERT(options->checkAttributeUniqueness == true);
  VELOCYPACK_ASSERT(obj.isObject());
  VELOCYPACK_ASSERT(obj.length() >= 2);
  
  if (obj.isSorted()) {
    // object attributes are sorted
    return checkAttributeUniquenessSorted(obj);
  }

  return checkAttributeUniquenessUnsorted(obj);
}

bool Builder::checkAttributeUniquenessSorted(Slice obj) const {
  ObjectIterator it(obj, false);

  // fetch initial key
  Slice previous = it.key(true);
  ValueLength len;
  char const* p = previous.getString(len);
  
  // advance to next key already  
  it.next();

  do {
    Slice const current = it.key(true);
    VELOCYPACK_ASSERT(current.isString());
    
    ValueLength len2;
    char const* q = current.getStringUnchecked(len2);

    if (len == len2 && std::memcmp(p, q, checkOverflow(len2)) == 0) {
      // identical key
      return false;
    }
    // re-use already calculated values for next round
    len = len2;
    p = q;
    it.next();
  } while (it.valid());

  // all keys unique
  return true;
}

bool Builder::checkAttributeUniquenessUnsorted(Slice obj) const {
  // cutoff value for linear attribute uniqueness scan
  // unsorted objects with this amount of attributes (or less) will
  // be validated using a non-allocating scan over the attributes
  // objects with more attributes will use a validation routine that
  // will use an std::unordered_set for O(1) lookups but with heap
  // allocations
  ObjectIterator it(obj, true);
    
  if (it.size() <= ::linearAttributeUniquenessCutoff) {
    return ::checkAttributeUniquenessUnsortedBrute(it);
  }
  return ::checkAttributeUniquenessUnsortedSet(it);
}

// Add all subkeys and subvalues into an object from an ObjectIterator
// and leaves open the object intentionally
uint8_t* Builder::add(ObjectIterator&& sub) {
  if (VELOCYPACK_UNLIKELY(_stack.empty())) {
    throw Exception(Exception::BuilderNeedOpenObject);
  }
  ValueLength const pos = _stack.back().startPos;
  if (VELOCYPACK_UNLIKELY(_start[pos] != 0x0b && _start[pos] != 0x14)) {
    throw Exception(Exception::BuilderNeedOpenObject);
  }
  if (VELOCYPACK_UNLIKELY(_keyWritten)) {
    throw Exception(Exception::BuilderKeyAlreadyWritten);
  }
  auto const oldPos = _pos;
  while (sub.valid()) {
    auto current = (*sub);
    add(current.key);
    add(current.value);
    sub.next();
  }
  return _start + oldPos;
}

// Add all subkeys and subvalues into an object from an ArrayIterator
// and leaves open the array intentionally
uint8_t* Builder::add(ArrayIterator&& sub) {
  if (VELOCYPACK_UNLIKELY(_stack.empty())) {
    throw Exception(Exception::BuilderNeedOpenArray);
  }
  ValueLength const pos = _stack.back().startPos;
  if (VELOCYPACK_UNLIKELY(_start[pos] != 0x06 && _start[pos] != 0x13)) {
    throw Exception(Exception::BuilderNeedOpenArray);
  }
  auto const oldPos = _pos;
  while (sub.valid()) {
    add(*sub);
    sub.next();
  }
  return _start + oldPos;
}

static_assert(sizeof(double) == 8, "double is not 8 bytes");
