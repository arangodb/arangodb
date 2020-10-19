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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/SharedSlice.h>

using namespace arangodb;
using namespace arangodb::velocypack;

namespace {
std::shared_ptr<uint8_t const> const staticSharedNoneBuffer{
    Slice::noneSliceData, [](auto) { /* don't delete the pointer */ }};
}

void SharedSlice::nullToNone() noexcept {
  if (VELOCYPACK_UNLIKELY(_start == nullptr)) {
    _start = staticSharedNoneBuffer;
  }
}

std::shared_ptr<uint8_t const> SharedSlice::copyBuffer(Buffer<uint8_t> const& buffer) {
  // template<class T> shared_ptr<T> make_shared( std::size_t N );
  // with T is U[] is only available since C++20 :(
  auto newBuffer = std::shared_ptr<uint8_t>(new uint8_t[buffer.byteSize()],
                                            [](uint8_t* ptr) { delete[] ptr; });
  memcpy(newBuffer.get(), buffer.data(), checkOverflow(buffer.byteSize()));
  return newBuffer;
}

std::shared_ptr<uint8_t const> SharedSlice::stealBuffer(Buffer<uint8_t>&& buffer) {
  // If the buffer doesn't use memory on the heap, we have to copy it.
  if (buffer.usesLocalMemory()) {
    return copyBuffer(buffer);
  }
  // Buffer uses velocypack_malloc/velocypack_free for memory management
  return std::shared_ptr<uint8_t const>(buffer.steal(), [](auto ptr) {
    return velocypack_free(ptr);
  });
}

Slice SharedSlice::slice() const noexcept { return Slice(_start.get()); }

SharedSlice::SharedSlice(std::shared_ptr<uint8_t const>&& data) noexcept
    : _start(std::move(data)) {
  nullToNone();
}

SharedSlice::SharedSlice(std::shared_ptr<uint8_t const> const& data) noexcept
    : _start(data) {
  nullToNone();
}

SharedSlice::SharedSlice(Buffer<uint8_t>&& buffer) noexcept
    : _start(stealBuffer(std::move(buffer))) {
  nullToNone();
}

SharedSlice::SharedSlice(Buffer<uint8_t> const& buffer) noexcept
    : _start(copyBuffer(buffer)) {
  nullToNone();
}

SharedSlice::SharedSlice(SharedSlice&& sharedPtr, Slice slice) noexcept
    : _start(std::move(sharedPtr._start), slice.start()) {
  sharedPtr._start = staticSharedNoneBuffer;
  nullToNone();
}

SharedSlice::SharedSlice(SharedSlice const& sharedPtr, Slice slice) noexcept
    : _start(sharedPtr._start, slice.start()) {
  nullToNone();
}

SharedSlice::SharedSlice() noexcept : _start(staticSharedNoneBuffer) {}

SharedSlice SharedSlice::value() const noexcept {
  return alias(slice().value());
}

uint64_t SharedSlice::getFirstTag() const { return slice().getFirstTag(); }

std::vector<uint64_t> SharedSlice::getTags() const { return slice().getTags(); }

bool SharedSlice::hasTag(uint64_t tagId) const { return slice().hasTag(tagId); }

std::shared_ptr<uint8_t const> SharedSlice::valueStart() const noexcept {
  return aliasPtr(slice().valueStart());
}

std::shared_ptr<uint8_t const> SharedSlice::start() const noexcept {
  return aliasPtr(slice().start());
}

uint8_t SharedSlice::head() const noexcept { return slice().head(); }

std::shared_ptr<uint8_t const> SharedSlice::begin() const noexcept {
  return aliasPtr(slice().begin());
}

std::shared_ptr<uint8_t const> SharedSlice::end() const {
  return aliasPtr(slice().end());
}

ValueType SharedSlice::type() const noexcept { return slice().type(); }

char const* SharedSlice::typeName() const { return slice().typeName(); }

uint64_t SharedSlice::hash(uint64_t seed) const { return slice().hash(seed); }

uint32_t SharedSlice::hash32(uint32_t seed) const {
  return slice().hash32(seed);
}

uint64_t SharedSlice::hashSlow(uint64_t seed) const {
  return slice().hashSlow(seed);
}

uint64_t SharedSlice::normalizedHash(uint64_t seed) const {
  return slice().normalizedHash(seed);
}

uint32_t SharedSlice::normalizedHash32(uint32_t seed) const {
  return slice().normalizedHash32(seed);
}

uint64_t SharedSlice::hashString(uint64_t seed) const noexcept {
  return slice().hashString(seed);
}

uint32_t SharedSlice::hashString32(uint32_t seed) const noexcept {
  return slice().hashString32(seed);
}

bool SharedSlice::isType(ValueType t) const { return slice().isType(t); }

bool SharedSlice::isNone() const noexcept { return slice().isNone(); }

bool SharedSlice::isIllegal() const noexcept { return slice().isIllegal(); }

bool SharedSlice::isNull() const noexcept { return slice().isNull(); }

bool SharedSlice::isBool() const noexcept { return slice().isBool(); }

bool SharedSlice::isBoolean() const noexcept { return slice().isBoolean(); }

bool SharedSlice::isTrue() const noexcept { return slice().isTrue(); }

bool SharedSlice::isFalse() const noexcept { return slice().isFalse(); }

bool SharedSlice::isArray() const noexcept { return slice().isArray(); }

bool SharedSlice::isObject() const noexcept { return slice().isObject(); }

bool SharedSlice::isDouble() const noexcept { return slice().isDouble(); }

bool SharedSlice::isUTCDate() const noexcept { return slice().isUTCDate(); }

bool SharedSlice::isExternal() const noexcept { return slice().isExternal(); }

bool SharedSlice::isMinKey() const noexcept { return slice().isMinKey(); }

bool SharedSlice::isMaxKey() const noexcept { return slice().isMaxKey(); }

bool SharedSlice::isInt() const noexcept { return slice().isInt(); }

bool SharedSlice::isUInt() const noexcept { return slice().isUInt(); }

bool SharedSlice::isSmallInt() const noexcept { return slice().isSmallInt(); }

bool SharedSlice::isString() const noexcept { return slice().isString(); }

bool SharedSlice::isBinary() const noexcept { return slice().isBinary(); }

bool SharedSlice::isBCD() const noexcept { return slice().isBCD(); }

bool SharedSlice::isCustom() const noexcept { return slice().isCustom(); }

bool SharedSlice::isTagged() const noexcept { return slice().isTagged(); }

bool SharedSlice::isInteger() const noexcept { return slice().isInteger(); }

bool SharedSlice::isNumber() const noexcept { return slice().isNumber(); }

bool SharedSlice::isSorted() const noexcept { return slice().isSorted(); }

bool SharedSlice::getBool() const { return slice().getBool(); }

bool SharedSlice::getBoolean() const { return slice().getBoolean(); }

double SharedSlice::getDouble() const { return slice().getDouble(); }

SharedSlice SharedSlice::at(ValueLength index) const {
  return alias(slice().at(index));
}

SharedSlice SharedSlice::operator[](ValueLength index) const {
  return alias(slice().operator[](index));
}

ValueLength SharedSlice::length() const { return slice().length(); }

SharedSlice SharedSlice::keyAt(ValueLength index, bool translate) const {
  return alias(slice().keyAt(index, translate));
}

SharedSlice SharedSlice::valueAt(ValueLength index) const {
  return alias(slice().valueAt(index));
}

SharedSlice SharedSlice::getNthValue(ValueLength index) const {
  return alias(slice().getNthValue(index));
}

SharedSlice SharedSlice::get(StringRef const& attribute) const {
  return alias(slice().get(attribute));
}

SharedSlice SharedSlice::get(std::string const& attribute) const {
  return alias(slice().get(attribute));
}

SharedSlice SharedSlice::get(char const* attribute) const {
  return alias(slice().get(attribute));
}

SharedSlice SharedSlice::get(char const* attribute, std::size_t length) const {
  return alias(slice().get(attribute, length));
}

SharedSlice SharedSlice::operator[](StringRef const& attribute) const {
  return alias(slice().operator[](attribute));
}

SharedSlice SharedSlice::operator[](std::string const& attribute) const {
  return alias(slice().operator[](attribute));
}

bool SharedSlice::hasKey(StringRef const& attribute) const {
  return slice().hasKey(attribute);
}

bool SharedSlice::hasKey(std::string const& attribute) const {
  return slice().hasKey(attribute);
}

bool SharedSlice::hasKey(char const* attribute) const {
  return slice().hasKey(attribute);
}

bool SharedSlice::hasKey(char const* attribute, std::size_t length) const {
  return slice().hasKey(attribute, length);
}

bool SharedSlice::hasKey(std::vector<std::string> const& attributes) const {
  return slice().hasKey(attributes);
}

std::shared_ptr<char const> SharedSlice::getExternal() const {
  return aliasPtr(slice().getExternal());
}

SharedSlice SharedSlice::resolveExternal() const {
  return alias(slice().resolveExternal());
}

SharedSlice SharedSlice::resolveExternals() const {
  return alias(slice().resolveExternals());
}

bool SharedSlice::isEmptyArray() const { return slice().isEmptyArray(); }

bool SharedSlice::isEmptyObject() const { return slice().isEmptyObject(); }

SharedSlice SharedSlice::translate() const {
  return alias(slice().translate());
}

int64_t SharedSlice::getInt() const { return slice().getInt(); }

uint64_t SharedSlice::getUInt() const { return slice().getUInt(); }

int64_t SharedSlice::getSmallInt() const { return slice().getSmallInt(); }

int64_t SharedSlice::getUTCDate() const { return slice().getUTCDate(); }

std::shared_ptr<char const> SharedSlice::getString(ValueLength& length) const {
  return aliasPtr(slice().getString(length));
}

std::shared_ptr<char const> SharedSlice::getStringUnchecked(ValueLength& length) const noexcept {
  return aliasPtr(slice().getStringUnchecked(length));
}

ValueLength SharedSlice::getStringLength() const {
  return slice().getStringLength();
}

std::string SharedSlice::copyString() const { return slice().copyString(); }

StringRef SharedSlice::stringRef() const { return slice().stringRef(); }

#ifdef VELOCYPACK_HAS_STRING_VIEW
std::string_view SharedSlice::stringView() const {
  return slice().stringView();
}

#endif

std::shared_ptr<uint8_t const> SharedSlice::getBinary(ValueLength& length) const {
  return aliasPtr(slice().getBinary(length));
}

ValueLength SharedSlice::getBinaryLength() const {
  return slice().getBinaryLength();
}

std::vector<uint8_t> SharedSlice::copyBinary() const {
  return slice().copyBinary();
}

ValueLength SharedSlice::byteSize() const { return slice().byteSize(); }

ValueLength SharedSlice::valueByteSize() const {
  return slice().valueByteSize();
}

ValueLength SharedSlice::findDataOffset(uint8_t head) const noexcept {
  return slice().findDataOffset(head);
}

ValueLength SharedSlice::getNthOffset(ValueLength index) const {
  return slice().getNthOffset(index);
}

SharedSlice SharedSlice::makeKey() const { return alias(slice().makeKey()); }

int SharedSlice::compareString(StringRef const& value) const {
  return slice().compareString(value);
}

int SharedSlice::compareString(std::string const& value) const {
  return slice().compareString(value);
}

int SharedSlice::compareString(char const* value, std::size_t length) const {
  return slice().compareString(value, length);
}

int SharedSlice::compareStringUnchecked(StringRef const& value) const noexcept {
  return slice().compareStringUnchecked(value);
}

int SharedSlice::compareStringUnchecked(std::string const& value) const noexcept {
  return slice().compareStringUnchecked(value);
}

int SharedSlice::compareStringUnchecked(char const* value, std::size_t length) const noexcept {
  return slice().compareStringUnchecked(value, length);
}

bool SharedSlice::isEqualString(StringRef const& attribute) const {
  return slice().isEqualString(attribute);
}

bool SharedSlice::isEqualString(std::string const& attribute) const {
  return slice().isEqualString(attribute);
}

bool SharedSlice::isEqualStringUnchecked(StringRef const& attribute) const noexcept {
  return slice().isEqualStringUnchecked(attribute);
}

bool SharedSlice::isEqualStringUnchecked(std::string const& attribute) const noexcept {
  return slice().isEqualStringUnchecked(attribute);
}

bool SharedSlice::binaryEquals(Slice const& other) const {
  return slice().binaryEquals(other);
}

bool SharedSlice::binaryEquals(SharedSlice const& other) const {
  return slice().binaryEquals(other.slice());
}

std::string SharedSlice::toHex() const { return slice().toHex(); }

std::string SharedSlice::toJson(Options const* options) const {
  return slice().toJson(options);
}

std::string SharedSlice::toString(Options const* options) const {
  return slice().toString(options);
}

std::string SharedSlice::hexType() const { return slice().hexType(); }

int64_t SharedSlice::getIntUnchecked() const noexcept {
  return slice().getIntUnchecked();
}

uint64_t SharedSlice::getUIntUnchecked() const noexcept {
  return slice().getUIntUnchecked();
}

int64_t SharedSlice::getSmallIntUnchecked() const noexcept {
  return slice().getSmallIntUnchecked();
}

std::shared_ptr<uint8_t const> SharedSlice::getBCD(int8_t& sign, int32_t& exponent,
                                                   ValueLength& mantissaLength) const {
  return aliasPtr(slice().getBCD(sign, exponent, mantissaLength));
}

SharedSlice SharedSlice::alias(Slice slice) const noexcept {
  return SharedSlice(*this, slice);
}

SharedSlice::SharedSlice(SharedSlice&& other) noexcept {
  _start = std::move(other._start);
  // Set other to point to None
  other._start = staticSharedNoneBuffer;
}

SharedSlice& SharedSlice::operator=(SharedSlice&& other) noexcept {
  _start = std::move(other._start);
  // Set other to point to None
  other._start = staticSharedNoneBuffer;
  return *this;
}
