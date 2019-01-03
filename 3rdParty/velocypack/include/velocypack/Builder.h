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

#ifndef VELOCYPACK_BUILDER_H
#define VELOCYPACK_BUILDER_H

#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <memory>

#include "velocypack/velocypack-common.h"
#include "velocypack/AttributeTranslator.h"
#include "velocypack/Basics.h"
#include "velocypack/Buffer.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"
#include "velocypack/StringRef.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

namespace arangodb {
namespace velocypack {
class ArrayIterator;
class ObjectIterator;

class Builder {
  friend class Parser;  // The parser needs access to internals.

  // Here are the mechanics of how this building process works:
  // The whole VPack being built starts at where _start points to.
  // The variable _pos keeps the
  // current write position. The method "set" simply writes a new
  // VPack subobject at the current write position and advances
  // it. Whenever one makes an array or object, a ValueLength for
  // the beginning of the value is pushed onto the _stack, which
  // remembers that we are in the process of building an array or
  // object. The _index vectors are used to collect information
  // for the index tables of arrays and objects, which are written
  // behind the subvalues. The add methods are used to keep track
  // of the new subvalue in _index followed by a set, and are
  // what the user from the outside calls. The close method seals
  // the innermost array or object that is currently being built
  // and pops a ValueLength off the _stack. The vectors in _index
  // stay until the next clearTemporary() is called to minimize
  // allocations. In the beginning, the _stack is empty, which
  // allows to build a sequence of unrelated VPack objects in the
  // buffer. Whenever the stack is empty, one can use the start,
  // size and slice methods to get out the ready built VPack
  // object(s).
 
 private:
  // A struct for sorting index tables for objects:
  struct SortEntry {
    uint8_t const* nameStart;
    uint64_t nameSize;
    uint64_t offset;
  };

  std::shared_ptr<Buffer<uint8_t>> _buffer;  // Here we collect the result
  Buffer<uint8_t>* _bufferPtr;      // used for quicker access than shared_ptr
  uint8_t* _start;                  // Always points to the start of _buffer
  ValueLength _pos;                 // the append position
  std::vector<ValueLength> _stack;  // Start positions of
                                    // open objects/arrays
  std::vector<std::vector<ValueLength>> _index;  // Indices for starts
                                                 // of subindex
  // temporary buffer used for sorting medium to big objects
  std::vector<Builder::SortEntry> _sortEntries; 
  bool _keyWritten;  // indicates that in the current object the key
                     // has been written but the value not yet

 public:
  Options const* options;
 
  // create an empty Builder, using default Options 
  Builder()
      : _buffer(new Buffer<uint8_t>()),
        _bufferPtr(_buffer.get()),
        _start(_bufferPtr->data()),
        _pos(0),
        _keyWritten(false),
        options(&Options::Defaults) {}
 
  // create an empty Builder, with custom Options 
  explicit Builder(Options const* options)
      : _buffer(new Buffer<uint8_t>()),
        _bufferPtr(_buffer.get()),
        _start(_bufferPtr->data()),
        _pos(0),
        _keyWritten(false),
        options(options) {
    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
  }
  
  explicit Builder(std::shared_ptr<Buffer<uint8_t>> const& buffer,
                   Options const* options = &Options::Defaults)
      : _buffer(buffer), 
        _bufferPtr(_buffer.get()), 
        _pos(0), 
        _keyWritten(false), 
        options(options) {
    if (VELOCYPACK_UNLIKELY(_bufferPtr == nullptr)) {
      throw Exception(Exception::InternalError, "Buffer cannot be a nullptr");
    }
    _start = _bufferPtr->data();

    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
  }
  
  explicit Builder(Buffer<uint8_t>& buffer,
                   Options const* options = &Options::Defaults)
      : _bufferPtr(nullptr), 
        _pos(buffer.size()), 
        _keyWritten(false), 
        options(options) {
    _buffer.reset(&buffer, BufferNonDeleter<uint8_t>());
    _bufferPtr = _buffer.get();
    _start = _bufferPtr->data();

    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
  }
  
  explicit Builder(Slice slice, Options const* options = &Options::Defaults)
      : Builder(options) {
    add(slice);
  }

  ~Builder() {}

  Builder(Builder const& that)
      : _buffer(new Buffer<uint8_t>(*that._buffer)),
        _bufferPtr(_buffer.get()),
        _start(_bufferPtr->data()),
        _pos(that._pos),
        _stack(that._stack),
        _index(that._index),
        _keyWritten(that._keyWritten),
        options(that.options) {
    VELOCYPACK_ASSERT(options != nullptr);
  }

  Builder& operator=(Builder const& that) {
    if (this != &that) {
      _buffer.reset(new Buffer<uint8_t>(*that._buffer));
      _bufferPtr = _buffer.get();
      _start = _bufferPtr->data();
      _pos = that._pos;
      _stack = that._stack;
      _index = that._index;
      _keyWritten = that._keyWritten;
      options = that.options;
    }
    VELOCYPACK_ASSERT(options != nullptr);
    return *this;
  }

  Builder(Builder&& that) {
    if (!that.isClosed()) {
      throw Exception(Exception::InternalError, "Cannot move an open Builder");
    }
    _buffer = that._buffer;
    _bufferPtr = _buffer.get();
    _start = _bufferPtr->data();
    _pos = that._pos;
    _stack.clear();
    _stack.swap(that._stack);
    _index.clear();
    _index.swap(that._index);
    _keyWritten = that._keyWritten;
    options = that.options;
    that._pos = 0;
    that._keyWritten = false;
  }

  Builder& operator=(Builder&& that) {
    if (!that.isClosed()) {
      throw Exception(Exception::InternalError, "Cannot move an open Builder");
    }
    if (this != &that) {
      _buffer = that._buffer;
      _bufferPtr = _buffer.get();
      _start = _bufferPtr->data();
      _pos = that._pos;
      _stack.clear();
      _stack.swap(that._stack);
      _index.clear();
      _index.swap(that._index);
      _keyWritten = that._keyWritten;
      options = that.options;
      that._pos = 0;
      that._keyWritten = false;
    }
    return *this;
  }

  // get a const reference to the Builder's Buffer object
  std::shared_ptr<Buffer<uint8_t>> const& buffer() const { return _buffer; }

  // steal the Builder's Buffer object. afterwards the Builder
  // is unusable
  std::shared_ptr<Buffer<uint8_t>> steal() {
    // After a steal the Builder is broken!
    std::shared_ptr<Buffer<uint8_t>> res = _buffer;
    _buffer.reset();
    _bufferPtr = nullptr;
    _pos = 0;
    return res;
  }

  uint8_t const* data() const noexcept { return _bufferPtr->data(); }

  std::string toString() const;

  std::string toJson() const;

  static Builder clone(Slice const& slice,
                       Options const* options = &Options::Defaults) {
    if (options == nullptr) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }

    Builder b(options);
    b.add(slice);
    return b;  // Use return value optimization
  }

  void reserve(ValueLength len) {
    VELOCYPACK_ASSERT(_start == _bufferPtr->data());
    VELOCYPACK_ASSERT(_start + _pos >= _bufferPtr->data());
    VELOCYPACK_ASSERT(_start + _pos <= _bufferPtr->data() + _bufferPtr->size());

    // Reserves len bytes at pos of the current state (top of stack)
    // or throws an exception
    if (_pos + len < _bufferPtr->size()) {
      return;  // All OK, we can just increase tos->pos by len
    }

#ifndef VELOCYPACK_64BIT
    (void) checkOverflow(_pos + len);
#endif

    _bufferPtr->reserve(len);
    _start = _bufferPtr->data();
  }

  // Clear and start from scratch:
  void clear() noexcept {
    _pos = 0;
    _stack.clear();
    VELOCYPACK_ASSERT(_bufferPtr != nullptr);
    _bufferPtr->reset();
    _keyWritten = false;
  }

  // Return a pointer to the start of the result:
  uint8_t* start() const {
    if (isClosed()) {
      return _start;
    }
    throw Exception(Exception::BuilderNotSealed);
  }

  // Return a Slice of the result:
  inline Slice slice() const {
    if (isEmpty()) {
      return Slice();
    }
    return Slice(start());
  }

  // Compute the actual size here, but only when sealed
  ValueLength size() const {
    if (!isClosed()) {
      throw Exception(Exception::BuilderNotSealed);
    }
    return _pos;
  }

  inline bool isEmpty() const noexcept { return _pos == 0; }

  inline bool isClosed() const noexcept { return _stack.empty(); }

  bool isOpenArray() const noexcept {
    if (_stack.empty()) {
      return false;
    }
    ValueLength const tos = _stack.back();
    return _start[tos] == 0x06 || _start[tos] == 0x13;
  }

  bool isOpenObject() const noexcept {
    if (_stack.empty()) {
      return false;
    }
    ValueLength const tos = _stack.back();
    return _start[tos] == 0x0b || _start[tos] == 0x14;
  }
  
  inline void openArray(bool unindexed = false) {
    openCompoundValue(unindexed ? 0x13 : 0x06);
  }

  inline void openObject(bool unindexed = false) {
    openCompoundValue(unindexed ? 0x14 : 0x0b);
  }
  
  template <typename T>
  uint8_t* addUnchecked(char const* attrName, size_t attrLength, T const& sub) {
    bool haveReported = false;
    if (!_stack.empty()) {
      reportAdd();
      haveReported = true;
    }

    try {
      set(ValuePair(attrName, attrLength, ValueType::String));
      _keyWritten = true;
      return set(sub);
    } catch (...) {
      // clean up in case of an exception
      if (haveReported) {
        cleanupAdd();
      }
      throw;
    }
  }

  // Add a subvalue into an object from a Value:
  inline uint8_t* add(std::string const& attrName, Value const& sub) {
    return addInternal<Value>(attrName, sub);
  }
  inline uint8_t* add(StringRef const& attrName, Value const& sub) {
    return addInternal<Value>(attrName, sub);
  }
  inline uint8_t* add(char const* attrName, Value const& sub) {
    return addInternal<Value>(attrName, sub);
  }
  inline uint8_t* add(char const* attrName, size_t attrLength, Value const& sub) {
    return addInternal<Value>(attrName, attrLength, sub);
  }
 
  // Add a subvalue into an object from a Slice:
  inline uint8_t* add(std::string const& attrName, Slice const& sub) {
    return addInternal<Slice>(attrName, sub);
  }
  inline uint8_t* add(StringRef const& attrName, Slice const& sub) {
    return addInternal<Slice>(attrName, sub);
  }
  inline uint8_t* add(char const* attrName, Slice const& sub) {
    return addInternal<Slice>(attrName, sub);
  }
  inline uint8_t* add(char const* attrName, size_t attrLength, Slice const& sub) {
    return addInternal<Slice>(attrName, attrLength, sub);
  }
 
  // Add a subvalue into an object from a ValuePair:
  inline uint8_t* add(std::string const& attrName, ValuePair const& sub) {
    return addInternal<ValuePair>(attrName, sub);
  }
  inline uint8_t* add(StringRef const& attrName, ValuePair const& sub) {
    return addInternal<ValuePair>(attrName, sub);
  }
  inline uint8_t* add(char const* attrName, ValuePair const& sub) {
    return addInternal<ValuePair>(attrName, sub);
  }
  inline uint8_t* add(char const* attrName, size_t attrLength, ValuePair const& sub) {
    return addInternal<ValuePair>(attrName, attrLength, sub);
  }
 
  // Add a subvalue into an array from a Value:
  inline uint8_t* add(Value const& sub) {
    return addInternal<Value>(sub); 
  }
  
  // Add a slice to an array
  inline uint8_t* add(Slice const& sub) {
    return addInternal<Slice>(sub); 
  }

  // Add a subvalue into an array from a ValuePair:
  inline uint8_t* add(ValuePair const& sub) {
    return addInternal<ValuePair>(sub);
  }

  // Add an External slice to an array
  uint8_t* addExternal(uint8_t const* sub) {
    if (options->disallowExternals) {
      // External values explicitly disallowed as a security
      // precaution
      throw Exception(Exception::BuilderExternalsDisallowed);
    }

    bool haveReported = false;
    if (!_stack.empty()) {
      if (!_keyWritten) {
        reportAdd();
        haveReported = true;
      }
    }
    try {
      checkKeyIsString(Slice(sub).isString());
      auto oldPos = _pos;
      reserve(1 + sizeof(void*));
      // store pointer. this doesn't need to be portable
      appendByteUnchecked(0x1d);
      memcpy(_start + _pos, &sub, sizeof(void*));
      advance(sizeof(void*));
      return _start + oldPos;
    } catch (...) {
      // clean up in case of an exception
      if (haveReported) {
        cleanupAdd();
      }
      throw;
    }
  }
  
  // Add all subkeys and subvalues into an object from an ObjectIterator
  // and leaves open the object intentionally
  uint8_t* add(ObjectIterator& sub);
  uint8_t* add(ObjectIterator&& sub);

  // Add all subvalues into an array from an ArrayIterator
  // and leaves open the array intentionally
  uint8_t* add(ArrayIterator& sub);
  uint8_t* add(ArrayIterator&& sub);

  // Seal the innermost array or object:
  Builder& close();

  // Remove last subvalue written to an (unclosed) object or array:
  // Throws if an error occurs.
  void removeLast();

  // whether or not a specific key is present in an Object value
  bool hasKey(std::string const& key) const;

  // return an attribute from an Object value
  Slice getKey(std::string const& key) const;

  // Syntactic sugar for add:
  Builder& operator()(std::string const& attrName, Value const& sub) {
    add(attrName, sub);
    return *this;
  }

  // Syntactic sugar for add:
  Builder& operator()(std::string const& attrName, ValuePair const& sub) {
    add(attrName, sub);
    return *this;
  }

  // Syntactic sugar for add:
  Builder& operator()(std::string const& attrName, Slice const& sub) {
    add(attrName, sub);
    return *this;
  }

  // Syntactic sugar for add:
  Builder& operator()(Value const& sub) {
    add(sub);
    return *this;
  }

  // Syntactic sugar for add:
  Builder& operator()(ValuePair const& sub) {
    add(sub);
    return *this;
  }

  // Syntactic sugar for add:
  Builder& operator()(Slice const& sub) {
    add(sub);
    return *this;
  }

  // Syntactic sugar for close:
  Builder& operator()() {
    close();
    return *this;
  }

 private:
  void sortObjectIndexShort(uint8_t* objBase,
                            std::vector<ValueLength>& offsets) const;

  void sortObjectIndexLong(uint8_t* objBase,
                           std::vector<ValueLength>& offsets);

  void sortObjectIndex(uint8_t* objBase,
                       std::vector<ValueLength>& offsets) {
    if (offsets.size() > 32) {
      sortObjectIndexLong(objBase, offsets);
    } else {
      sortObjectIndexShort(objBase, offsets);
    }
  }

  // close for the empty case:
  Builder& closeEmptyArrayOrObject(ValueLength tos, bool isArray);

  // close for the compact case:
  bool closeCompactArrayOrObject(ValueLength tos, bool isArray,
                                 std::vector<ValueLength> const& index);

  // close for the array case:
  Builder& closeArray(ValueLength tos, std::vector<ValueLength>& index);

  void addNull() {
    appendByte(0x18);
  }

  void addFalse() {
    appendByte(0x19);
  }

  void addTrue() {
    appendByte(0x1a);
  }

  void addDouble(double v) {
    uint64_t dv;
    ValueLength vSize = sizeof(double);
    memcpy(&dv, &v, vSize);
    reserve(1 + vSize);
    appendByteUnchecked(0x1b);
    for (uint64_t x = dv; vSize > 0; vSize--) {
      appendByteUnchecked(x & 0xff);
      x >>= 8;
    }
  }

  void addInt(int64_t v) {
    if (v >= 0 && v <= 9) {
      appendByte(static_cast<uint8_t>(0x30 + v));
    } else if (v < 0 && v >= -6) {
      appendByte(static_cast<uint8_t>(0x40 + v));
    } else {
      appendInt(v, 0x1f);
    }
  }

  void addUInt(uint64_t v) {
    if (v <= 9) {
      appendByte(static_cast<uint8_t>(0x30 + v));
    } else {
      appendUInt(v, 0x27);
    }
  }

  void addUTCDate(int64_t v) {
    constexpr uint8_t vSize = sizeof(int64_t);  // is always 8
    reserve(1 + vSize);
    appendByteUnchecked(0x1c);
    appendLengthUnchecked<vSize>(toUInt64(v));
  }

  inline void checkKeyIsString(bool isString) {
    if (!_stack.empty()) {
      ValueLength const& tos = _stack.back();
      if (_start[tos] == 0x0b || _start[tos] == 0x14) {
        if (!_keyWritten && !isString) {
          throw Exception(Exception::BuilderKeyMustBeString);
        }
        _keyWritten = !_keyWritten;
      }
    }
  }

  inline void addArray(bool unindexed = false) {
    addCompoundValue(unindexed ? 0x13 : 0x06);
  }

  inline void addObject(bool unindexed = false) {
    addCompoundValue(unindexed ? 0x14 : 0x0b);
  }

  template <typename T>
  uint8_t* addInternal(T const& sub) {
    bool haveReported = false;
    if (!_stack.empty()) {
      if (!_keyWritten) {
        reportAdd();
        haveReported = true;
      }
    }
    try {
      return set(sub);
    } catch (...) {
      // clean up in case of an exception
      if (haveReported) {
        cleanupAdd();
      }
      throw;
    }
  }

  template <typename T>
  uint8_t* addInternal(std::string const& attrName, T const& sub) {
    return addInternal<T>(attrName.data(), attrName.size(), sub);
  }
  
  template <typename T>
  uint8_t* addInternal(StringRef const& attrName, T const& sub) {
    return addInternal<T>(attrName.data(), attrName.size(), sub);
  }

  template <typename T>
  uint8_t* addInternal(char const* attrName, T const& sub) {
    return addInternal<T>(attrName, strlen(attrName), sub);
  }

  template <typename T>
  uint8_t* addInternal(char const* attrName, size_t attrLength, T const& sub) {
    bool haveReported = false;
    if (!_stack.empty()) {
      ValueLength& tos = _stack.back();
      if (_start[tos] != 0x0b && _start[tos] != 0x14) {
        throw Exception(Exception::BuilderNeedOpenObject);
      }
      if (VELOCYPACK_UNLIKELY(_keyWritten)) {
        throw Exception(Exception::BuilderKeyAlreadyWritten);
      }
      reportAdd();
      haveReported = true;
    }

    try {
      if (options->attributeTranslator != nullptr) {
        // check if a translation for the attribute name exists
        uint8_t const* translated =
            options->attributeTranslator->translate(attrName, attrLength);

        if (translated != nullptr) {
          Slice item(translated);
          ValueLength const l = item.byteSize();
          reserve(l);
          memcpy(_start + _pos, translated, checkOverflow(l));
          advance(l);
          _keyWritten = true;
          return set(sub);
        }
        // otherwise fall through to regular behavior
      }

      set(ValuePair(attrName, attrLength, ValueType::String));
      _keyWritten = true;
      return set(sub);
    } catch (...) {
      // clean up in case of an exception
      if (haveReported) {
        cleanupAdd();
      }
      throw;
    }
  }
  
  void addCompoundValue(uint8_t type) {
    reserve(9);
    // an Array or Object is started:
    _stack.push_back(_pos);
    while (_stack.size() > _index.size()) {
      _index.emplace_back();
    }
    _index[_stack.size() - 1].clear();
    appendByteUnchecked(type);
    memset(_start + _pos, 0, 8);
    advance(8);  // Will be filled later with bytelength and nr subs
  }

  void openCompoundValue(uint8_t type) {
    bool haveReported = false;
    if (!_stack.empty()) {
      ValueLength& tos = _stack.back();
      if (!_keyWritten) {
        if (_start[tos] != 0x06 && _start[tos] != 0x13) {
          throw Exception(Exception::BuilderNeedOpenArray);
        }
        reportAdd();
        haveReported = true;
      } else {
        _keyWritten = false;
      }
    }
    try {
      addCompoundValue(type);
    } catch (...) {
      // clean up in case of an exception
      if (haveReported) {
        cleanupAdd();
      }
      throw;
    }
  }

  uint8_t* set(Value const& item);

  uint8_t* set(ValuePair const& pair);

  uint8_t* set(Slice const& item);

  void cleanupAdd() noexcept {
    size_t depth = _stack.size() - 1;
    VELOCYPACK_ASSERT(!_index[depth].empty());
    _index[depth].pop_back();
  }

  inline void reportAdd() {
    size_t depth = _stack.size() - 1;
    _index[depth].push_back(_pos - _stack[depth]);
  }

  template <uint64_t n>
  void appendLength(ValueLength v) {
    reserve(n);
    appendLengthUnchecked<n>(v);
  }
  
  template <uint64_t n>
  void appendLengthUnchecked(ValueLength v) {
    for (uint64_t i = 0; i < n; ++i) {
      appendByteUnchecked(v & 0xff);
      v >>= 8;
    }
  }

  void appendUInt(uint64_t v, uint8_t base) {
    reserve(9);
    ValueLength save = _pos;
    advance(1);
    uint8_t vSize = 0;
    do {
      vSize++;
      appendByteUnchecked(static_cast<uint8_t>(v & 0xff));
      v >>= 8;
    } while (v != 0);
    _start[save] = base + vSize;
  }

  // returns number of bytes required to store the value in 2s-complement
  static inline uint8_t intLength(int64_t value) {
    if (value >= -0x80 && value <= 0x7f) {
      // shortcut for the common case
      return 1;
    }
    uint64_t x = value >= 0 ? static_cast<uint64_t>(value)
                            : static_cast<uint64_t>(-(value + 1));
    uint8_t xSize = 0;
    do {
      xSize++;
      x >>= 8;
    } while (x >= 0x80);
    return xSize + 1;
  }

  void appendInt(int64_t v, uint8_t base) {
    uint8_t vSize = intLength(v);
    uint64_t x;
    if (vSize == 8) {
      x = toUInt64(v);
    } else {
      int64_t shift = 1LL << (vSize * 8 - 1);  // will never overflow!
      x = v >= 0 ? static_cast<uint64_t>(v)
                 : static_cast<uint64_t>(v + shift) + shift;
    }
    reserve(1 + vSize);
    appendByteUnchecked(base + vSize);
    while (vSize-- > 0) {
      appendByteUnchecked(x & 0xff);
      x >>= 8;
    }
  }
  
  inline void appendByte(uint8_t value) {
    reserve(1);
    appendByteUnchecked(value);
  }
  
  inline void appendByteUnchecked(uint8_t value) {
    _start[_pos++] = value;
    VELOCYPACK_ASSERT(_bufferPtr != nullptr);
    _bufferPtr->advance();
  }
  
  inline void resetTo(size_t value) {
    _pos = value;
    VELOCYPACK_ASSERT(_bufferPtr != nullptr);
    _bufferPtr->resetTo(value);
  }

  // move byte position x bytes ahead
  inline void advance(size_t value) noexcept {
    _pos += value;
    VELOCYPACK_ASSERT(_bufferPtr != nullptr);
    _bufferPtr->advance(value);
  }
              
  // move byte position x bytes back
  inline void rollback(size_t value) noexcept {
    _pos -= value;
    VELOCYPACK_ASSERT(_bufferPtr != nullptr);
    _bufferPtr->rollback(value);
  }

  bool checkAttributeUniqueness(Slice obj) const;
  bool checkAttributeUniquenessSorted(Slice obj) const;
  bool checkAttributeUniquenessUnsorted(Slice obj) const;
};

struct BuilderNonDeleter {
  void operator()(Builder*) {}
};

struct BuilderContainer {
  BuilderContainer(Builder* builder) : builder(builder) {}

  Builder& operator*() { return *builder; }
  Builder const& operator*() const { return *builder; }
  Builder* operator->() { return builder; }
  Builder const* operator->() const { return builder; }
  Builder* builder;
};

// convenience class scope guard for building objects
struct ObjectBuilder final : public BuilderContainer,
                             private NonHeapAllocatable,
                             NonCopyable {
  ObjectBuilder(Builder* builder, bool allowUnindexed = false)
      : BuilderContainer(builder) {
    builder->openObject(allowUnindexed);
  }
  ObjectBuilder(Builder* builder, std::string const& attributeName,
                bool allowUnindexed = false)
      : BuilderContainer(builder) {
    builder->add(attributeName, Value(ValueType::Object, allowUnindexed));
  }
  ObjectBuilder(Builder* builder, char const* attributeName,
                bool allowUnindexed = false)
      : BuilderContainer(builder) {
    builder->add(attributeName, Value(ValueType::Object, allowUnindexed));
  }
  ~ObjectBuilder() {
    try {
      builder->close();
    } catch (...) {
      // destructors must not throw
    }
  }
};

// convenience class scope guard for building arrays
struct ArrayBuilder final : public BuilderContainer,
                            private NonHeapAllocatable,
                            NonCopyable {
  ArrayBuilder(Builder* builder, bool allowUnindexed = false)
      : BuilderContainer(builder) {
    builder->openArray(allowUnindexed);
  }
  ArrayBuilder(Builder* builder, std::string const& attributeName,
               bool allowUnindexed = false)
      : BuilderContainer(builder) {
    builder->add(attributeName, Value(ValueType::Array, allowUnindexed));
  }
  ArrayBuilder(Builder* builder, char const* attributeName,
               bool allowUnindexed = false)
      : BuilderContainer(builder) {
    builder->add(attributeName, Value(ValueType::Array, allowUnindexed));
  }
  ~ArrayBuilder() {
    try {
      builder->close();
    } catch (...) {
      // destructors must not throw
    }
  }
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
