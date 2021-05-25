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

#include <array>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

#include "tests-common.h"

TEST(BuilderTest, ConstructWithBufferRef) {
  Builder b1;
  uint32_t u = 1;
  b1.openObject();
  b1.add("test",Value(u));
  b1.close();
  Buffer<uint8_t> buf = *b1.steal();
  Builder b2(buf);
  Slice s(buf.data());

  ASSERT_EQ(ValueType::Object, s.type());
  ASSERT_EQ(s.get("test").getUInt(), u);
  ASSERT_EQ(ValueType::SmallInt, s.get("test").type());
  ASSERT_EQ(u, s.get("test").getUInt());
}

TEST(BuilderTest, AddObjectInArray) {
  Builder b;
  b.openArray();
  b.openObject();
  b.close();
  b.close();
  Slice s(b.slice());
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(1UL, s.length());
  Slice ss(s[0]);
  ASSERT_TRUE(ss.isObject());
  ASSERT_EQ(0UL, ss.length());
}

TEST(BuilderTest, AddObjectIteratorEmpty) {
  Builder obj;
  obj.openObject();
  obj.add("1-one", Value(1));
  obj.add("2-two", Value(2));
  obj.add("3-three", Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  ASSERT_TRUE(b.isClosed());
  ASSERT_VELOCYPACK_EXCEPTION(b.add(ObjectIterator(objSlice)), Exception::BuilderNeedOpenObject);
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, AddObjectIteratorKeyAlreadyWritten) {
  Builder obj;
  obj.openObject();
  obj.add("1-one", Value(1));
  obj.add("2-two", Value(2));
  obj.add("3-three", Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  ASSERT_TRUE(b.isClosed());
  b.openObject();
  b.add(Value("foo"));
  ASSERT_FALSE(b.isClosed());
  ASSERT_VELOCYPACK_EXCEPTION(b.add(ObjectIterator(objSlice)), Exception::BuilderKeyAlreadyWritten);
  ASSERT_FALSE(b.isClosed());
}

TEST(BuilderTest, AddObjectIteratorNonObject) {
  Builder obj;
  obj.openObject();
  obj.add("1-one", Value(1));
  obj.add("2-two", Value(2));
  obj.add("3-three", Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openArray();
  ASSERT_FALSE(b.isClosed());
  ASSERT_VELOCYPACK_EXCEPTION(b.add(ObjectIterator(objSlice)), Exception::BuilderNeedOpenObject);
  ASSERT_FALSE(b.isClosed());
}

TEST(BuilderTest, AddObjectIteratorTop) {
  Builder obj;
  obj.openObject();
  obj.add("1-one", Value(1));
  obj.add("2-two", Value(2));
  obj.add("3-three", Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openObject();
  ASSERT_FALSE(b.isClosed());
  b.add(ObjectIterator(objSlice));
  ASSERT_FALSE(b.isClosed());
  Slice result = b.close().slice();
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\"1-one\":1,\"2-two\":2,\"3-three\":3}", result.toJson());
}

TEST(BuilderTest, AddObjectIteratorReference) {
  Builder obj;
  obj.openObject();
  obj.add("1-one", Value(1));
  obj.add("2-two", Value(2));
  obj.add("3-three", Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openObject();
  ASSERT_FALSE(b.isClosed());
  auto it = ObjectIterator(objSlice);
  b.add(it);
  ASSERT_FALSE(b.isClosed());
  Slice result = b.close().slice();
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\"1-one\":1,\"2-two\":2,\"3-three\":3}", result.toJson());
}

TEST(BuilderTest, AddObjectIteratorSub) {
  Builder obj;
  obj.openObject();
  obj.add("1-one", Value(1));
  obj.add("2-two", Value(2));
  obj.add("3-three", Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openObject();
  b.add("1-something", Value("tennis"));
  b.add(Value("2-values"));
  b.openObject();
  b.add(ObjectIterator(objSlice));
  ASSERT_FALSE(b.isClosed());
  b.close(); // close one level
  b.add("3-bark", Value("qux"));
  ASSERT_FALSE(b.isClosed());
  Slice result = b.close().slice();
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\"1-something\":\"tennis\",\"2-values\":{\"1-one\":1,\"2-two\":2,\"3-three\":3},\"3-bark\":\"qux\"}", result.toJson());
}

TEST(BuilderTest, AddArrayIteratorEmpty) {
  Builder obj;
  obj.openArray();
  obj.add(Value(1));
  obj.add(Value(2));
  obj.add(Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  ASSERT_TRUE(b.isClosed());
  ASSERT_VELOCYPACK_EXCEPTION(b.add(ArrayIterator(objSlice)), Exception::BuilderNeedOpenArray);
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, AddArrayIteratorNonArray) {
  Builder obj;
  obj.openArray();
  obj.add(Value(1));
  obj.add(Value(2));
  obj.add(Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openObject();
  ASSERT_FALSE(b.isClosed());
  ASSERT_VELOCYPACK_EXCEPTION(b.add(ArrayIterator(objSlice)), Exception::BuilderNeedOpenArray);
  ASSERT_FALSE(b.isClosed());
}

TEST(BuilderTest, AddArrayIteratorTop) {
  Builder obj;
  obj.openArray();
  obj.add(Value(1));
  obj.add(Value(2));
  obj.add(Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openArray();
  ASSERT_FALSE(b.isClosed());
  b.add(ArrayIterator(objSlice));
  ASSERT_FALSE(b.isClosed());
  Slice result = b.close().slice();
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("[1,2,3]", result.toJson());
}

TEST(BuilderTest, AddArrayIteratorReference) {
  Builder obj;
  obj.openArray();
  obj.add(Value(1));
  obj.add(Value(2));
  obj.add(Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openArray();
  ASSERT_FALSE(b.isClosed());
  auto it = ArrayIterator(objSlice);
  b.add(it);
  ASSERT_FALSE(b.isClosed());
  Slice result = b.close().slice();
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("[1,2,3]", result.toJson());
}

TEST(BuilderTest, AddArrayIteratorSub) {
  Builder obj;
  obj.openArray();
  obj.add(Value(1));
  obj.add(Value(2));
  obj.add(Value(3));
  Slice objSlice = obj.close().slice();

  Builder b;
  b.openArray();
  b.add(Value("tennis"));
  b.openArray();
  b.add(ArrayIterator(objSlice));
  ASSERT_FALSE(b.isClosed());
  b.close(); // close one level
  b.add(Value("qux"));
  ASSERT_FALSE(b.isClosed());
  Slice result = b.close().slice();
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("[\"tennis\",[1,2,3],\"qux\"]", result.toJson());
}

TEST(BuilderTest, CreateWithoutBufferOrOptions) {
  ASSERT_VELOCYPACK_EXCEPTION(new Builder(nullptr), Exception::InternalError);

  std::shared_ptr<Buffer<uint8_t>> buffer;
  ASSERT_VELOCYPACK_EXCEPTION(new Builder(buffer, nullptr),
                              Exception::InternalError);

  buffer.reset(new Buffer<uint8_t>());
  ASSERT_VELOCYPACK_EXCEPTION(new Builder(buffer, nullptr),
                              Exception::InternalError);

  Builder b;
  ASSERT_TRUE(b.isEmpty());
  b.add(Value(123));
  ASSERT_FALSE(b.isEmpty());
  Slice s = b.slice();

  ASSERT_VELOCYPACK_EXCEPTION(b.clone(s, nullptr), Exception::InternalError);
}

TEST(BuilderTest, Copy) {
  Builder b;
  ASSERT_TRUE(b.isEmpty());
  b.openArray();
  for (int i = 0; i < 10; i++) {
    b.add(Value("abcdefghijklmnopqrstuvwxyz"));
  }
  b.close();
  ASSERT_FALSE(b.isEmpty());

  Builder a(b);
  ASSERT_FALSE(a.isEmpty());
  ASSERT_NE(a.buffer().get(), b.buffer().get());
  ASSERT_TRUE(a.buffer().get() != nullptr);
  ASSERT_TRUE(b.buffer().get() != nullptr);
}

TEST(BuilderTest, CopyAssign) {
  Builder b;
  ASSERT_TRUE(b.isEmpty());

  Builder a;
  ASSERT_TRUE(a.isEmpty());
  a = b;
  ASSERT_TRUE(a.isEmpty());
  ASSERT_TRUE(b.isEmpty());

  ASSERT_NE(a.buffer().get(), b.buffer().get());
  ASSERT_TRUE(a.buffer().get() != nullptr);
  ASSERT_TRUE(b.buffer().get() != nullptr);
}

TEST(BuilderTest, MoveConstructOpenObject) {
  Builder b;
  b.openObject();
  ASSERT_FALSE(b.isClosed());

  Builder a(std::move(b));
  ASSERT_FALSE(a.isClosed());
}

TEST(BuilderTest, MoveConstructOpenArray) {
  Builder b;
  b.openArray();
  ASSERT_FALSE(b.isClosed());

  Builder a(std::move(b));
  ASSERT_FALSE(a.isClosed());
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, MoveAssignOpenObject) {
  Builder b;
  b.openObject();
  ASSERT_FALSE(b.isClosed());

  Builder a;
  a = std::move(b);
  ASSERT_FALSE(a.isClosed());
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, MoveAssignOpenArray) {
  Builder b;
  b.openArray();
  ASSERT_FALSE(b.isClosed());

  Builder a;
  a = std::move(b);
  ASSERT_FALSE(a.isClosed());
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, Move) {
  Builder b;
  ASSERT_TRUE(b.isEmpty());

  auto shptrb = b.buffer();
  Builder a(std::move(b));
  ASSERT_TRUE(a.isEmpty());
  ASSERT_TRUE(b.isEmpty());
  auto shptra = a.buffer();
  ASSERT_EQ(shptrb.get(), shptra.get());
  ASSERT_NE(a.buffer().get(), nullptr);
  ASSERT_EQ(b.buffer().get(),  nullptr);
}

TEST(BuilderTest, MoveNonEmpty) {
  Builder b;
  b.add(Value("foobar"));
  ASSERT_FALSE(b.isEmpty());

  auto shptrb = b.buffer();
  Builder a(std::move(b));
  ASSERT_FALSE(a.isEmpty());
  ASSERT_TRUE(b.isEmpty());

  auto shptra = a.buffer();
  ASSERT_EQ(shptrb.get(), shptra.get());
  ASSERT_NE(a.buffer().get(), nullptr);
  ASSERT_EQ(b.buffer().get(), nullptr);
}

TEST(BuilderTest, MoveAssign) {
  Builder b;
  ASSERT_TRUE(b.isEmpty());

  auto shptrb = b.buffer();
  Builder a = std::move(b);
  ASSERT_TRUE(a.isEmpty());
  ASSERT_TRUE(b.isEmpty());
  auto shptra = a.buffer();
  ASSERT_EQ(shptrb.get(), shptra.get());
  ASSERT_NE(a.buffer().get(), nullptr);
  ASSERT_EQ(b.buffer().get(), nullptr);
}

TEST(BuilderTest, MoveAssignNonEmpty) {
  Builder b;
  b.add(Value("foobar"));
  ASSERT_FALSE(b.isEmpty());

  auto shptrb = b.buffer();
  Builder a = std::move(b);
  ASSERT_FALSE(a.isEmpty());
  ASSERT_TRUE(b.isEmpty());
  auto shptra = a.buffer();
  ASSERT_EQ(shptrb.get(), shptra.get());
  ASSERT_NE(a.buffer().get(), nullptr);
  ASSERT_EQ(b.buffer().get(), nullptr);
}

TEST(BuilderTest, ConstructFromSlice) {
  Builder b1;
  b1.openObject();
  b1.add("foo", Value("bar"));
  b1.add("bar", Value("baz"));
  b1.close();

  Builder b2(b1.slice());
  ASSERT_FALSE(b2.isEmpty());
  ASSERT_TRUE(b2.isClosed());

  ASSERT_TRUE(b2.slice().isObject());
  ASSERT_TRUE(b2.slice().hasKey("foo"));
  ASSERT_TRUE(b2.slice().hasKey("bar"));

  b1.clear();
  ASSERT_TRUE(b1.isEmpty());

  ASSERT_FALSE(b2.isEmpty());
  ASSERT_TRUE(b2.slice().isObject());
  ASSERT_TRUE(b2.slice().hasKey("foo"));
  ASSERT_TRUE(b2.slice().hasKey("bar"));
}

TEST(BuilderTest, UsingEmptySharedPtr) {
  std::shared_ptr<Buffer<uint8_t>> buffer;

  ASSERT_VELOCYPACK_EXCEPTION(Builder(buffer), Exception::InternalError);
}

TEST(BuilderTest, SizeUsingSharedPtr) {
  auto buffer = std::make_shared<Buffer<uint8_t>>();
  buffer->append("\x45testi", 6);

  {
    Builder b(buffer);
    ASSERT_FALSE(b.isEmpty());
    ASSERT_TRUE(b.slice().isString());
    ASSERT_EQ("testi", b.slice().copyString());
    ASSERT_EQ(6, b.size());
  }

  buffer->clear();
  {
    Builder b(buffer);
    ASSERT_TRUE(b.isEmpty());
    ASSERT_TRUE(b.slice().isNone());
  }
}

TEST(BuilderTest, SizeUsingBufferReference) {
  Buffer<uint8_t> buffer;
  buffer.append("\x45testi", 6);

  {
    Builder b(buffer);
    ASSERT_FALSE(b.isEmpty());
    ASSERT_TRUE(b.slice().isString());
    ASSERT_EQ("testi", b.slice().copyString());
    ASSERT_EQ(6, b.size());
  }

  buffer.clear();
  {
    Builder b(buffer);
    ASSERT_TRUE(b.isEmpty());
    ASSERT_TRUE(b.slice().isNone());
  }
}

TEST(BuilderTest, UsingExistingBuffer) {
  Buffer<uint8_t> buffer;
  Builder b1(buffer);
  b1.add(Value("the-quick-brown-fox-jumped-over-the-lazy-dog"));

  // copy-construct
  Builder b2(b1);
  ASSERT_TRUE(b2.slice().isString());
  ASSERT_EQ("the-quick-brown-fox-jumped-over-the-lazy-dog", b2.slice().copyString());

  // copy-assign
  Builder b3;
  b3 = b2;
  ASSERT_TRUE(b3.slice().isString());
  ASSERT_EQ("the-quick-brown-fox-jumped-over-the-lazy-dog", b3.slice().copyString());

  // move-construct
  Builder b4(std::move(b3));
  ASSERT_TRUE(b4.slice().isString());
  ASSERT_EQ("the-quick-brown-fox-jumped-over-the-lazy-dog", b4.slice().copyString());

  // move-assign
  Builder b5;
  b5 = std::move(b4);
  ASSERT_TRUE(b5.slice().isString());
  ASSERT_EQ("the-quick-brown-fox-jumped-over-the-lazy-dog", b5.slice().copyString());

  b5.clear();
  b5.add(Value("the-foxx"));
  ASSERT_TRUE(b5.slice().isString());
  ASSERT_EQ("the-foxx", b5.slice().copyString());
}

TEST(BuilderTest, BufferRef) {
  Builder b;
  b.add(Value("the-foxx"));

  auto& ref = b.bufferRef();
  ASSERT_TRUE(Slice(ref.data()).isEqualString("the-foxx"));

  auto& ref2 = b.bufferRef();
  ASSERT_TRUE(Slice(ref2.data()).isEqualString("the-foxx"));
}

TEST(BuilderTest, BufferRefAfterStolen) {
  Builder b;
  b.add(Value("the-foxx"));

  b.steal();
  ASSERT_VELOCYPACK_EXCEPTION(b.bufferRef(), Exception::InternalError);
}

TEST(BuilderTest, AfterStolen) {
  Builder b1;
  b1.add(Value("the-quick-brown-fox-jumped-over-the-lazy-dog"));

  // sets the shared_ptr of the Builder's Buffer to nullptr
  ASSERT_NE(nullptr, b1.steal());

  // copy-construct
  Builder b2(b1);
  ASSERT_EQ(nullptr, b2.steal());

  // copy-assign
  Builder b3;
  b3 = b2;
  ASSERT_EQ(nullptr, b3.steal());

  // move-construct
  Builder b4(std::move(b3));
  ASSERT_EQ(nullptr, b4.steal());

  // move-assign
  Builder b5;
  b5 = std::move(b4);
  ASSERT_EQ(nullptr, b5.steal());
}

TEST(BuilderTest, StealBuffer) {
  Builder b;
  b.openArray();
  for (int i = 0; i < 10; i++) {
    b.add(Value("abcdefghijklmnopqrstuvwxyz"));
  }
  b.close();
  ASSERT_FALSE(b.isEmpty());

  auto ptr1 = b.buffer().get();
  std::shared_ptr<Buffer<uint8_t>> buf(b.steal());
  ASSERT_TRUE(b.isEmpty());
  auto ptr2 = b.buffer().get();
  auto ptr3 = buf.get();
  ASSERT_EQ(ptr1, ptr3);
  ASSERT_NE(ptr2, ptr1);
}

TEST(BuilderTest, SizeWithOpenObject) {
  Builder b;
  ASSERT_EQ(0UL, b.size());

  b.add(Value(ValueType::Object));
  ASSERT_VELOCYPACK_EXCEPTION(b.size(), Exception::BuilderNotSealed);

  b.close();
  ASSERT_EQ(1UL, b.size());
}

TEST(BuilderTest, BufferSharedPointerNoSharing) {
  Builder b;
  b.add(Value(ValueType::Array));
  // construct a long string that will exceed the Builder's initial buffer
  b.add(Value(
      "skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddddddd"
      "dddddddjfkdfffffffffffffffffffffffff,"
      "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmmmm"
      "mmmmmmmmmmmmmmmmdddddddfjf"));
  b.close();

  std::shared_ptr<Buffer<uint8_t>> const& builderBuffer = b.buffer();

  // only the Builder itself is using the Buffer
  ASSERT_EQ(1, builderBuffer.use_count());
}

TEST(BuilderTest, BufferSharedPointerStealFromParser) {
  Parser parser;
  parser.parse(
      "\"skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddddd"
      "dddddddddjfkdfffffffffffffffffffffffff,"
      "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmmmm"
      "mmmmmmmmmmmmmmmmdddddddfjf\"");

  std::shared_ptr<Builder> b = parser.steal();
  // only the Builder itself is using its Buffer
  std::shared_ptr<Buffer<uint8_t>> const& builderBuffer = b->buffer();
  ASSERT_EQ(1, builderBuffer.use_count());
}

TEST(BuilderTest, BufferSharedPointerCopy) {
  Builder b;
  b.add(Value(ValueType::Array));
  // construct a long string that will exceed the Builder's initial buffer
  b.add(Value(
      "skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddddddd"
      "dddddddjfkdfffffffffffffffffffffffff,"
      "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmmmm"
      "mmmmmmmmmmmmmmmmdddddddfjf"));
  b.close();

  std::shared_ptr<Buffer<uint8_t>> const& builderBuffer = b.buffer();
  auto ptr = builderBuffer.get();

  // only the Builder itself is using its Buffer
  ASSERT_EQ(1, builderBuffer.use_count());

  std::shared_ptr<Buffer<uint8_t>> copy = b.buffer();
  ASSERT_EQ(2, copy.use_count());
  ASSERT_EQ(2, builderBuffer.use_count());

  copy.reset();
  ASSERT_EQ(1, builderBuffer.use_count());
  ASSERT_EQ(ptr, builderBuffer.get());
}

TEST(BuilderTest, BufferSharedPointerStealFromParserExitScope) {
  std::shared_ptr<Builder> b(new Builder());
  std::shared_ptr<Buffer<uint8_t>> builderBuffer = b->buffer();
  ASSERT_EQ(2, builderBuffer.use_count());
  auto ptr = builderBuffer.get();

  {
    Parser parser;
    parser.parse(
        "\"skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddd"
        "dddddddddddjfkdfffffffffffffffffffffffff,"
        "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmm"
        "mmmmmmmmmmmmmmmmmmdddddddfjf\"");

    ASSERT_EQ(2, builderBuffer.use_count());

    b = parser.steal();
    ASSERT_EQ(1, builderBuffer.use_count());
    std::shared_ptr<Buffer<uint8_t>> const& builderBuffer2 = b->buffer();
    ASSERT_NE(ptr, builderBuffer2.get());
    ASSERT_EQ(1, builderBuffer2.use_count());
  }

  ASSERT_EQ(1, builderBuffer.use_count());
  ASSERT_EQ(ptr, builderBuffer.get());
}

TEST(BuilderTest, BufferSharedPointerStealAndReturn) {
  auto func = []() -> std::shared_ptr<Builder> {
    Parser parser;
    parser.parse(
        "\"skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddd"
        "dddddddddddjfkdfffffffffffffffffffffffff,"
        "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmm"
        "mmmmmmmmmmmmmmmmmmdddddddfjf\"");

    return parser.steal();
  };

  std::shared_ptr<Builder> b = func();
  ASSERT_EQ(0xbf, *(b->buffer()->data()));  // long UTF-8 string...
  ASSERT_EQ(216UL, b->size());
}

TEST(BuilderTest, BufferSharedPointerStealMultiple) {
  Parser parser;
  parser.parse(
      "\"skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddddd"
      "dddddddddjfkdfffffffffffffffffffffffff,"
      "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmmmm"
      "mmmmmmmmmmmmmmmmdddddddfjf\"");

  std::shared_ptr<Builder> b = parser.steal();
  ASSERT_EQ(0xbf, *(b->buffer()->data()));  // long UTF-8 string...
  ASSERT_EQ(216UL, b->size());
  ASSERT_EQ(1, b->buffer().use_count());

  // steal again, should work, but Builder should be empty:
  // CHANGED: parser is broken after steal()
  // std::shared_ptr<Builder> b2 = parser.steal();
  // ASSERT_EQ(b2->buffer()->size(), 0UL);
}

TEST(BuilderTest, BufferSharedPointerInject) {
  std::shared_ptr<Buffer<uint8_t>> buffer(new Buffer<uint8_t>);
  auto ptr = buffer.get();

  Builder b(buffer);
  std::shared_ptr<Buffer<uint8_t>> const& builderBuffer = b.buffer();

  ASSERT_EQ(2, buffer.use_count());
  ASSERT_EQ(2, builderBuffer.use_count());
  ASSERT_EQ(ptr, builderBuffer.get());

  b.add(Value(ValueType::Array));
  // construct a long string that will exceed the Builder's initial buffer
  b.add(Value(
      "skjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjddddddddddddddddddddddddddddddddddd"
      "dddddddjfkdfffffffffffffffffffffffff,"
      "mmmmmmmmmmmmmmmmmmmmmmmmddddddddddddddddddddddddddddddddddddmmmmmmmmmmmm"
      "mmmmmmmmmmmmmmmmdddddddfjf"));
  b.close();

  std::shared_ptr<Buffer<uint8_t>> copy = b.buffer();
  ASSERT_EQ(3, buffer.use_count());
  ASSERT_EQ(3, copy.use_count());
  ASSERT_EQ(3, builderBuffer.use_count());
  ASSERT_EQ(ptr, copy.get());

  copy.reset();
  ASSERT_EQ(2, buffer.use_count());
  ASSERT_EQ(2, builderBuffer.use_count());

  b.steal();   // steals the buffer, resulting shared_ptr is forgotten
  ASSERT_EQ(1, buffer.use_count());
  ASSERT_EQ(ptr, buffer.get());
}

TEST(BuilderTest, AddNonCompoundTypeAllowUnindexed) {
  Builder b;
  b.add(Value(ValueType::Array, true));
  b.add(Value(ValueType::Object, true));

  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::None, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Null, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Bool, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Double, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::UTCDate, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::External, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::MinKey, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::MaxKey, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Int, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::UInt, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::SmallInt, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::String, true)),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Binary, true)),
                              Exception::InvalidValueType);
}

TEST(BuilderTest, BoolWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(1.5, ValueType::Bool)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(-100, ValueType::Bool)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(static_cast<uint64_t>(100UL), ValueType::Bool)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::Bool)),
                              Exception::BuilderUnexpectedValue);
}

TEST(BuilderTest, StringWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(1.5, ValueType::String)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(-100, ValueType::String)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(static_cast<uint64_t>(100UL), ValueType::String)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::String)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(false, ValueType::String)),
                              Exception::BuilderUnexpectedValue);
}

TEST(BuilderTest, SmallIntWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1.2, ValueType::SmallInt));
  b.add(Value(-1, ValueType::SmallInt));
  b.add(Value(static_cast<uint64_t>(1UL), ValueType::SmallInt));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(-100, ValueType::SmallInt)),
                              Exception::NumberOutOfRange);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::SmallInt)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::SmallInt)),
                              Exception::BuilderUnexpectedValue);
  b.close();

  Slice s = b.slice();
  ASSERT_EQ("[1,-1,1]", s.toJson());
}

TEST(BuilderTest, IntWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1.2, ValueType::Int));
  b.add(Value(-1, ValueType::Int));
  b.add(Value(static_cast<uint64_t>(1UL), ValueType::Int));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::Int)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::Int)),
                              Exception::BuilderUnexpectedValue);
  b.close();

  Slice s = b.slice();
  ASSERT_EQ("[1,-1,1]", s.toJson());
}

TEST(BuilderTest, UIntWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1.2, ValueType::UInt));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(-1.2, ValueType::UInt)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(-1, ValueType::UInt)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::UInt)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::UInt)),
                              Exception::BuilderUnexpectedValue);
  b.add(Value(static_cast<uint64_t>(42UL), ValueType::UInt));
  b.add(Value(static_cast<int64_t>(23), ValueType::UInt));
  b.close();

  Slice s = b.slice();
  ASSERT_EQ("[1,42,23]", s.toJson());
}

TEST(BuilderTest, DoubleWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1.2, ValueType::Double));
  b.add(Value(-1.2, ValueType::Double));
  b.add(Value(-1, ValueType::Double));
  b.add(Value(static_cast<uint64_t>(1UL), ValueType::Double));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::Double)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::Double)),
                              Exception::BuilderUnexpectedValue);
  b.close();

  Slice s = b.slice();
  ASSERT_EQ("[1.2,-1.2,-1,1]", s.toJson());
}

TEST(BuilderTest, UTCDateWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1.2, ValueType::UTCDate));
  b.add(Value(static_cast<int64_t>(1), ValueType::UTCDate));
  b.add(Value(static_cast<uint64_t>(1UL), ValueType::UTCDate));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::UTCDate)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::UTCDate)),
                              Exception::BuilderUnexpectedValue);
}

TEST(BuilderTest, BinaryWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value("foobar", ValueType::Binary));
  b.add(Value(std::string("foobar"), ValueType::Binary));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(1.5, ValueType::Binary)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(static_cast<int64_t>(1), ValueType::Binary)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(static_cast<uint64_t>(1), ValueType::Binary)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::Binary)),
                              Exception::BuilderUnexpectedValue);
}

TEST(BuilderTest, ExternalWithOtherTypes) {
  Builder b;
  b.add(Value(ValueType::Array));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value("foobar", ValueType::External)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(std::string("foobar"), ValueType::External)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(1.5, ValueType::External)),
                              Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(static_cast<int64_t>(1), ValueType::External)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(static_cast<uint64_t>(1), ValueType::External)),
      Exception::BuilderUnexpectedValue);
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true, ValueType::External)),
                              Exception::BuilderUnexpectedValue);
}

TEST(BuilderTest, AddAndOpenArray) {
  Builder b1;
  ASSERT_TRUE(b1.isClosed());
  b1.openArray();
  ASSERT_FALSE(b1.isClosed());
  b1.add(Value("bar"));
  b1.close();
  ASSERT_TRUE(b1.isClosed());
  ASSERT_EQ(0x02, b1.slice().head());

  Builder b2;
  ASSERT_TRUE(b2.isClosed());
  b2.openArray();
  ASSERT_FALSE(b2.isClosed());
  b2.add(Value("bar"));
  b2.close();
  ASSERT_TRUE(b2.isClosed());
  ASSERT_EQ(0x02, b2.slice().head());
}

TEST(BuilderTest, AddAndOpenObject) {
  Builder b1;
  ASSERT_TRUE(b1.isClosed());
  b1.openObject();
  ASSERT_FALSE(b1.isClosed());
  b1.add("foo", Value("bar"));
  b1.close();
  ASSERT_TRUE(b1.isClosed());
  ASSERT_EQ(0x14, b1.slice().head());
  ASSERT_EQ("{\n  \"foo\" : \"bar\"\n}", b1.toString());
  ASSERT_EQ(1UL, b1.slice().length());

  Builder b2;
  ASSERT_TRUE(b2.isClosed());
  b2.openObject();
  ASSERT_FALSE(b2.isClosed());
  b2.add("foo", Value("bar"));
  b2.close();
  ASSERT_TRUE(b2.isClosed());
  ASSERT_EQ(0x14, b2.slice().head());
  ASSERT_EQ("{\n  \"foo\" : \"bar\"\n}", b2.toString());
  ASSERT_EQ(1UL, b2.slice().length());
}

TEST(BuilderTest, MinKey) {
  Builder b;
  b.add(Value(ValueType::MinKey));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t const correctResult[] = {0x1e};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, MaxKey) {
  Builder b;
  b.add(Value(ValueType::MaxKey));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t const correctResult[] = {0x1f};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, Custom) {
  Builder b;
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Custom)),
                              Exception::BuilderUnexpectedType);
}

TEST(BuilderTest, None) {
  Builder b;
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::None)),
                              Exception::BuilderUnexpectedType);
}

TEST(BuilderTest, Null) {
  Builder b;
  b.add(Value(ValueType::Null));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t const correctResult[] = {0x18};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, False) {
  Builder b;
  b.add(Value(false));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t const correctResult[] = {0x19};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, True) {
  Builder b;
  b.add(Value(true));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t const correctResult[] = {0x1a};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, Int64) {
  static int64_t value = INT64_MAX;
  Builder b;
  b.add(Value(value));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[9] = {0x27, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0x7f};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, UInt64) {
  static uint64_t value = 1234;
  Builder b;
  b.add(Value(value));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[3] = {0x29, 0xd2, 0x04};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, Double) {
  static double value = 123.456;
  Builder b;
  b.add(Value(value));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[9] = {0x1b, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};
  ASSERT_EQ(8ULL, sizeof(double));
  dumpDouble(value, correctResult + 1);

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, String) {
  Builder b;
  b.add(Value("abcdefghijklmnopqrstuvwxyz"));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
                                    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
                                    0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
                                    0x75, 0x76, 0x77, 0x78, 0x79, 0x7a};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArrayEmpty) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.close();
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x01};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArraySingleEntry) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(uint64_t(1)));
  b.close();
  uint8_t* result = b.start();
  ASSERT_EQ(0x02U, *result);
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x02, 0x03, 0x31};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArraySingleEntryLong) {
  std::string const value(
      "ngdddddljjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjsdddffffffffffffmmmmmmmmmmmmmmmsf"
      "dlllllllllllllllllllllllllllllllllllllllllllllllllrjjjjjjsdddddddddddddd"
      "ddddhhhhhhkkkkkkkksssssssssssssssssssssssssssssssssdddddddddddddddddkkkk"
      "kkkkkkkkksddddddddddddssssssssssfvvvvvvvvvvvvvvvvvvvvvvvvvvvfvgfff");
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(value));
  b.close();
  uint8_t* result = b.start();
  ASSERT_EQ(0x03U, *result);
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x03, 0x2c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbf, 0x1a, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0x67, 0x64, 0x64, 0x64, 0x64,
      0x64, 0x6c, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a,
      0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a,
      0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x73, 0x64, 0x64,
      0x64, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0x66, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d, 0x6d,
      0x6d, 0x6d, 0x6d, 0x6d, 0x73, 0x66, 0x64, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
      0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
      0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
      0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
      0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x72, 0x6a, 0x6a, 0x6a,
      0x6a, 0x6a, 0x6a, 0x73, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
      0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x68, 0x68,
      0x68, 0x68, 0x68, 0x68, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b,
      0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
      0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
      0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x64, 0x64, 0x64,
      0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
      0x64, 0x64, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b,
      0x6b, 0x6b, 0x6b, 0x73, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
      0x64, 0x64, 0x64, 0x64, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
      0x73, 0x73, 0x66, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76,
      0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76,
      0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x66, 0x76, 0x67, 0x66, 0x66, 0x66};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArraySameSizeEntries) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(uint64_t(1)));
  b.add(Value(uint64_t(2)));
  b.add(Value(uint64_t(3)));
  b.close();
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x02, 0x05, 0x31, 0x32, 0x33};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArraySomeValues) {
  double value = 2.3;
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(uint64_t(1200)));
  b.add(Value(value));
  b.add(Value("abc"));
  b.add(Value(true));
  b.close();

  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x06, 0x18, 0x04, 0x29, 0xb0, 0x04,  // uint(1200) = 0x4b0
      0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // double(2.3)
      0x43, 0x61, 0x62, 0x63, 0x1a, 0x03, 0x06, 0x0f, 0x13};
  dumpDouble(value, correctResult + 7);

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArrayCompact) {
  double value = 2.3;
  Builder b;
  b.add(Value(ValueType::Array, true));
  b.add(Value(uint64_t(1200)));
  b.add(Value(value));
  b.add(Value("abc"));
  b.add(Value(true));
  b.close();

  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x13, 0x14, 0x29, 0xb0, 0x04, 0x1b,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00,  // double
                                    0x43, 0x61, 0x62, 0x63, 0x1a, 0x04};
  dumpDouble(value, correctResult + 6);

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ObjectEmpty) {
  Builder b;
  b.add(Value(ValueType::Object));
  b.close();
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x0a};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ObjectEmptyCompact) {
  Builder b;
  b.add(Value(ValueType::Object, true));
  b.close();
  uint8_t* result = b.start();
  ValueLength len = b.size();

  // should still build the compact variant
  static uint8_t correctResult[] = {0x0a};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ObjectSorted) {
  double value = 2.3;
  Builder b;
  b.add(Value(ValueType::Object));
  b.add("d", Value(uint64_t(1200)));
  b.add("c", Value(value));
  b.add("b", Value("abc"));
  b.add("a", Value(true));
  b.close();

  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x0b, 0x20, 0x04, 0x41, 0x64, 0x29, 0xb0, 0x04,  // "d": uint(1200) =
                                                       // 0x4b0
      0x41, 0x63, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // "c": double(2.3)
      0x41, 0x62, 0x43, 0x61, 0x62, 0x63,  // "b": "abc"
      0x41, 0x61, 0x1a,                    // "a": true
      0x19, 0x13, 0x08, 0x03};
  dumpDouble(value, correctResult + 11);

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ObjectCompact) {
  double value = 2.3;
  Builder b;
  b.add(Value(ValueType::Object, true));
  b.add("d", Value(uint64_t(1200)));
  b.add("c", Value(value));
  b.add("b", Value("abc"));
  b.add("a", Value(true));
  b.close();

  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x14, 0x1c, 0x41, 0x64, 0x29, 0xb0, 0x04, 0x41, 0x63, 0x1b,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // double
      0x41, 0x62, 0x43, 0x61, 0x62, 0x63, 0x41, 0x61, 0x1a, 0x04};

  dumpDouble(value, correctResult + 10);

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ArrayCompactBytesizeBelowThreshold) {
  Builder b;
  b.add(Value(ValueType::Array, true));
  for (std::size_t i = 0; i < 124; ++i) {
    b.add(Value(uint64_t(i % 10)));
  }
  b.close();

  uint8_t* result = b.start();
  Slice s(result);

  ASSERT_EQ(127UL, s.byteSize());

  ASSERT_EQ(0x13, result[0]);
  ASSERT_EQ(0x7f, result[1]);
  for (std::size_t i = 0; i < 124; ++i) {
    ASSERT_EQ(0x30 + (i % 10), result[2 + i]);
  }
  ASSERT_EQ(0x7c, result[126]);
}

TEST(BuilderTest, ArrayCompactBytesizeAboveThreshold) {
  Builder b;
  b.add(Value(ValueType::Array, true));
  for (std::size_t i = 0; i < 125; ++i) {
    b.add(Value(uint64_t(i % 10)));
  }
  b.close();

  uint8_t* result = b.start();
  Slice s(result);

  ASSERT_EQ(129UL, s.byteSize());

  ASSERT_EQ(0x13, result[0]);
  ASSERT_EQ(0x81, result[1]);
  ASSERT_EQ(0x01, result[2]);
  for (std::size_t i = 0; i < 125; ++i) {
    ASSERT_EQ(0x30 + (i % 10), result[3 + i]);
  }
  ASSERT_EQ(0x7d, result[128]);
}

TEST(BuilderTest, ArrayCompactLengthBelowThreshold) {
  Builder b;
  b.add(Value(ValueType::Array, true));
  for (std::size_t i = 0; i < 127; ++i) {
    b.add(Value("aaa"));
  }
  b.close();

  uint8_t* result = b.start();
  Slice s(result);

  ASSERT_EQ(512UL, s.byteSize());

  ASSERT_EQ(0x13, result[0]);
  ASSERT_EQ(0x80, result[1]);
  ASSERT_EQ(0x04, result[2]);
  for (std::size_t i = 0; i < 127; ++i) {
    ASSERT_EQ(0x43, result[3 + i * 4]);
  }
  ASSERT_EQ(0x7f, result[511]);
}

TEST(BuilderTest, ArrayCompactLengthAboveThreshold) {
  Builder b;
  b.add(Value(ValueType::Array, true));
  for (std::size_t i = 0; i < 128; ++i) {
    b.add(Value("aaa"));
  }
  b.close();

  uint8_t* result = b.start();
  Slice s(result);

  ASSERT_EQ(517UL, s.byteSize());

  ASSERT_EQ(0x13, result[0]);
  ASSERT_EQ(0x85, result[1]);
  ASSERT_EQ(0x04, result[2]);
  for (std::size_t i = 0; i < 128; ++i) {
    ASSERT_EQ(0x43, result[3 + i * 4]);
  }
  ASSERT_EQ(0x01, result[515]);
  ASSERT_EQ(0x80, result[516]);
}

TEST(BuilderTest, ExternalDisallowed) {
  uint8_t externalStuff[] = {0x01};
  Options options;
  options.disallowExternals = true;

  Builder b(&options);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(const_cast<void const*>(static_cast<void*>(externalStuff)),
                  ValueType::External)),
      Exception::BuilderExternalsDisallowed);
  
  ASSERT_VELOCYPACK_EXCEPTION(
      b.addExternal(externalStuff), Exception::BuilderExternalsDisallowed);
}

TEST(BuilderTest, External) {
  uint8_t externalStuff[] = {0x01};
  Builder b;
  b.add(Value(const_cast<void const*>(static_cast<void*>(externalStuff)),
              ValueType::External));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[1 + sizeof(char*)] = {0x00};
  correctResult[0] = 0x1d;
  uint8_t* p = externalStuff;
  memcpy(correctResult + 1, &p, sizeof(uint8_t*));

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ExternalUTCDate) {
  int64_t const v = -24549959465;
  Builder bExternal;
  bExternal.add(Value(v, ValueType::UTCDate));

  Builder b;
  b.add(Value(const_cast<void const*>(static_cast<void*>(bExternal.start()))));

  Slice s(b.start());
  ASSERT_EQ(ValueType::External, s.type());
#ifdef VELOCYPACK_64BIT
  ASSERT_EQ(9ULL, s.byteSize());
#else
  ASSERT_EQ(5ULL, s.byteSize());
#endif
  Slice sExternal(reinterpret_cast<uint8_t const*>(s.getExternal()));
  ASSERT_EQ(9ULL, sExternal.byteSize());
  ASSERT_EQ(ValueType::UTCDate, sExternal.type());
  ASSERT_EQ(v, sExternal.getUTCDate());
}

TEST(BuilderTest, ExternalDouble) {
  double const v = -134.494401;
  Builder bExternal;
  bExternal.add(Value(v));

  Builder b;
  b.add(Value(const_cast<void const*>(static_cast<void*>(bExternal.start()))));

  Slice s(b.start());
  ASSERT_EQ(ValueType::External, s.type());
#ifdef VELOCYPACK_64BIT
  ASSERT_EQ(9ULL, s.byteSize());
#else
  ASSERT_EQ(5ULL, s.byteSize());
#endif

  Slice sExternal(reinterpret_cast<uint8_t const*>(s.getExternal()));
  ASSERT_EQ(9ULL, sExternal.byteSize());
  ASSERT_EQ(ValueType::Double, sExternal.type());
  ASSERT_DOUBLE_EQ(v, sExternal.getDouble());
}

TEST(BuilderTest, ExternalBinary) {
  char const* p = "the quick brown FOX jumped over the lazy dog";
  Builder bExternal;
  bExternal.add(Value(std::string(p), ValueType::Binary));

  Builder b;
  b.add(Value(const_cast<void const*>(static_cast<void*>(bExternal.start()))));

  Slice s(b.start());
  ASSERT_EQ(ValueType::External, s.type());
#ifdef VELOCYPACK_64BIT
  ASSERT_EQ(9ULL, s.byteSize());
#else
  ASSERT_EQ(5ULL, s.byteSize());
#endif

  Slice sExternal(reinterpret_cast<uint8_t const*>(s.getExternal()));
  ASSERT_EQ(2 + strlen(p), sExternal.byteSize());
  ASSERT_EQ(ValueType::Binary, sExternal.type());
  ValueLength len;
  uint8_t const* str = sExternal.getBinary(len);
  ASSERT_EQ(strlen(p), len);
  ASSERT_EQ(0, memcmp(str, p, len));
}

TEST(BuilderTest, ExternalString) {
  char const* p = "the quick brown FOX jumped over the lazy dog";
  Builder bExternal;
  bExternal.add(Value(std::string(p)));

  Builder b;
  b.add(Value(const_cast<void const*>(static_cast<void*>(bExternal.start()))));

  Slice s(b.start());
  ASSERT_EQ(ValueType::External, s.type());
#ifdef VELOCYPACK_64BIT
  ASSERT_EQ(9ULL, s.byteSize());
#else
  ASSERT_EQ(5ULL, s.byteSize());
#endif

  Slice sExternal(reinterpret_cast<uint8_t const*>(s.getExternal()));
  ASSERT_EQ(1 + strlen(p), sExternal.byteSize());
  ASSERT_EQ(ValueType::String, sExternal.type());
  ValueLength len;
  char const* str = sExternal.getString(len);
  ASSERT_EQ(strlen(p), len);
  ASSERT_EQ(0, strncmp(str, p, len));
}

TEST(BuilderTest, ExternalExternal) {
  char const* p = "the quick brown FOX jumped over the lazy dog";
  Builder bExternal;
  bExternal.add(Value(std::string(p)));

  Builder bExExternal;
  bExExternal.add(
      Value(const_cast<void const*>(static_cast<void*>(bExternal.start()))));
  bExExternal.add(Value(std::string(p)));

  Builder b;
  b.add(
      Value(const_cast<void const*>(static_cast<void*>(bExExternal.start()))));

  Slice s(b.start());
  ASSERT_EQ(ValueType::External, s.type());
#ifdef VELOCYPACK_64BIT
  ASSERT_EQ(9ULL, s.byteSize());
#else
  ASSERT_EQ(5ULL, s.byteSize());
#endif

  Slice sExternal(reinterpret_cast<uint8_t const*>(s.getExternal()));
  ASSERT_EQ(ValueType::External, sExternal.type());
#ifdef VELOCYPACK_64BIT
  ASSERT_EQ(9ULL, sExternal.byteSize());
#else
  ASSERT_EQ(5ULL, sExternal.byteSize());
#endif

  Slice sExExternal(reinterpret_cast<uint8_t const*>(sExternal.getExternal()));
  ASSERT_EQ(1 + strlen(p), sExExternal.byteSize());
  ASSERT_EQ(ValueType::String, sExExternal.type());
  ValueLength len;
  char const* str = sExExternal.getString(len);
  ASSERT_EQ(strlen(p), len);
  ASSERT_EQ(0, strncmp(str, p, len));
}

TEST(BuilderTest, ExternalAsObjectKey) {
  uint8_t externalStuff[] = {0x18};
  /*
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(
        b.add(Value(const_cast<void const*>(static_cast<void*>(externalStuff)),
                ValueType::External)), Exception::BuilderKeyMustBeString);
  }
  */

  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(
        b.addExternal(externalStuff), Exception::BuilderKeyMustBeString);
  }
}

TEST(BuilderTest, UInt) {
  uint64_t value = 0x12345678abcdef;
  Builder b;
  b.add(Value(value));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x2e, 0xef, 0xcd, 0xab,
                                    0x78, 0x56, 0x34, 0x12};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, IntPos) {
  int64_t value = 0x12345678abcdef;
  Builder b;
  b.add(Value(value));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x26, 0xef, 0xcd, 0xab,
                                    0x78, 0x56, 0x34, 0x12};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, IntNeg) {
  int64_t value = -0x12345678abcdef;
  Builder b;
  b.add(Value(value));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0x26, 0x11, 0x32, 0x54,
                                    0x87, 0xa9, 0xcb, 0xed};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, Int1Limits) {
  int64_t values[] = {-0x80LL,
                      0x7fLL,
                      -0x81LL,
                      0x80LL,
                      -0x8000LL,
                      0x7fffLL,
                      -0x8001LL,
                      0x8000LL,
                      -0x800000LL,
                      0x7fffffLL,
                      -0x800001LL,
                      0x800000LL,
                      -0x80000000LL,
                      0x7fffffffLL,
                      -0x80000001LL,
                      0x80000000LL,
                      -0x8000000000LL,
                      0x7fffffffffLL,
                      -0x8000000001LL,
                      0x8000000000LL,
                      -0x800000000000LL,
                      0x7fffffffffffLL,
                      -0x800000000001LL,
                      0x800000000000LL,
                      -0x80000000000000LL,
                      0x7fffffffffffffLL,
                      -0x80000000000001LL,
                      0x80000000000000LL,
                      arangodb::velocypack::toInt64(0x8000000000000000ULL),
                      0x7fffffffffffffffLL};
  for (std::size_t i = 0; i < sizeof(values) / sizeof(int64_t); i++) {
    int64_t v = values[i];
    Builder b;
    b.add(Value(v));
    uint8_t* result = b.start();
    Slice s(result);
    ASSERT_TRUE(s.isInt());
    ASSERT_EQ(v, s.getInt());
  }
}

TEST(BuilderTest, StringChar) {
  char const* value = "der fuxx ging in den wald und aß pilze";
  std::size_t const valueLen = strlen(value);
  Builder b;
  b.add(Value(value));

  Slice slice = Slice(b.start());
  ASSERT_TRUE(slice.isString());

  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(valueLen, len);
  ASSERT_EQ(0, strncmp(s, value, valueLen));

  std::string c = slice.copyString();
  ASSERT_EQ(valueLen, c.size());
  ASSERT_EQ(0, strncmp(value, c.c_str(), valueLen));
}

TEST(BuilderTest, StringString) {
  std::string const value("der fuxx ging in den wald und aß pilze");
  Builder b;
  b.add(Value(value));

  Slice slice = Slice(b.start());
  ASSERT_TRUE(slice.isString());

  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(value.size(), len);
  ASSERT_EQ(0, strncmp(s, value.c_str(), value.size()));

  std::string c = slice.copyString();
  ASSERT_EQ(value.size(), c.size());
  ASSERT_EQ(value, c);
}

TEST(BuilderTest, StringView) {
  std::string_view const value("der fuxx ging in den wald und aß pilze");
  Builder b;
  b.add(Value(value));

  Slice slice = Slice(b.start());
  ASSERT_TRUE(slice.isString());

  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(value.size(), len);
  ASSERT_EQ(0, strncmp(s, value.data(), value.size()));

  std::string_view c = slice.stringView();
  ASSERT_EQ(value.size(), c.size());
  ASSERT_EQ(value, c);
}

TEST(BuilderTest, BinaryViaValuePair) {
  uint8_t binaryStuff[] = {0x02, 0x03, 0x05, 0x08, 0x0d};

  Builder b;
  b.add(ValuePair(binaryStuff, sizeof(binaryStuff)));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {0xc0, 0x05, 0x02, 0x03, 0x05, 0x08, 0x0d};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
}

TEST(BuilderTest, ShortStringViaValuePair) {
  char const* p = "the quick brown fox jumped over the lazy dog";

  Builder b;
  b.add(ValuePair(p, strlen(p), ValueType::String));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x6c, 0x74, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x20, 0x62,
      0x72, 0x6f, 0x77, 0x6e, 0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a, 0x75, 0x6d,
      0x70, 0x65, 0x64, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65,
      0x20, 0x6c, 0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
  
  ASSERT_EQ(p, b.slice().copyString());
  ASSERT_TRUE(StringRef(p).equals(b.slice().stringRef()));
}

TEST(BuilderTest, LongStringViaValuePair) {
  char const* p =
      "the quick brown fox jumped over the lazy dog, and it jumped and jumped "
      "and jumped and went on. But then, the String needed to get even longer "
      "and longer until the test finally worked.";

  Builder b;
  b.add(ValuePair(p, strlen(p), ValueType::String));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0xbf, 0xb7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x68, 0x65,
      0x20, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x20, 0x62, 0x72, 0x6f, 0x77, 0x6e,
      0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a, 0x75, 0x6d, 0x70, 0x65, 0x64, 0x20,
      0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6c, 0x61, 0x7a,
      0x79, 0x20, 0x64, 0x6f, 0x67, 0x2c, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x69,
      0x74, 0x20, 0x6a, 0x75, 0x6d, 0x70, 0x65, 0x64, 0x20, 0x61, 0x6e, 0x64,
      0x20, 0x6a, 0x75, 0x6d, 0x70, 0x65, 0x64, 0x20, 0x61, 0x6e, 0x64, 0x20,
      0x6a, 0x75, 0x6d, 0x70, 0x65, 0x64, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x77,
      0x65, 0x6e, 0x74, 0x20, 0x6f, 0x6e, 0x2e, 0x20, 0x42, 0x75, 0x74, 0x20,
      0x74, 0x68, 0x65, 0x6e, 0x2c, 0x20, 0x74, 0x68, 0x65, 0x20, 0x53, 0x74,
      0x72, 0x69, 0x6e, 0x67, 0x20, 0x6e, 0x65, 0x65, 0x64, 0x65, 0x64, 0x20,
      0x74, 0x6f, 0x20, 0x67, 0x65, 0x74, 0x20, 0x65, 0x76, 0x65, 0x6e, 0x20,
      0x6c, 0x6f, 0x6e, 0x67, 0x65, 0x72, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x6c,
      0x6f, 0x6e, 0x67, 0x65, 0x72, 0x20, 0x75, 0x6e, 0x74, 0x69, 0x6c, 0x20,
      0x74, 0x68, 0x65, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x66, 0x69, 0x6e,
      0x61, 0x6c, 0x6c, 0x79, 0x20, 0x77, 0x6f, 0x72, 0x6b, 0x65, 0x64, 0x2e};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));

  ASSERT_EQ(p, b.slice().copyString());
  ASSERT_TRUE(StringRef(p).equals(b.slice().stringRef()));
}

TEST(BuilderTest, CustomViaValuePair) {
  char const* p = "\xf4\x2cthe quick brown fox jumped over the lazy dog";

  Builder b;
  b.add(ValuePair(p, strlen(p), ValueType::Custom));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0xf4, 0x2c, 0x74, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x20,
      0x62, 0x72, 0x6f, 0x77, 0x6e, 0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a, 0x75,
      0x6d, 0x70, 0x65, 0x64, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68,
      0x65, 0x20, 0x6c, 0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));
  auto s = b.slice();
  ASSERT_EQ(sizeof(correctResult), s.byteSize());
}

TEST(BuilderTest, CustomValueDisallowed) {
  char const* p = "\xf4\x2cthe quick brown fox jumped over the lazy dog";

  Options options;
  options.disallowCustom = true;
  Builder b(&options);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Slice(reinterpret_cast<uint8_t const*>(p))),
      Exception::BuilderCustomDisallowed);
}

TEST(BuilderTest, CustomPairDisallowed) {
  char const* p = "\xf4\x2cthe quick brown fox jumped over the lazy dog";

  Options options;
  options.disallowCustom = true;
  Builder b(&options);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(ValuePair(p, strlen(p), ValueType::Custom)),
      Exception::BuilderCustomDisallowed);
}

TEST(BuilderTest, InvalidTypeViaValuePair) {
  char const* p = "fail";

  Builder b;
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(ValuePair(p, strlen(p), ValueType::UTCDate)),
      Exception::BuilderUnexpectedType);
}

TEST(BuilderTest, CustomValueType) {
  Options options;
  options.disallowCustom = true;
  Builder b(&options);
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(ValueType::Custom)),
      Exception::BuilderCustomDisallowed);

  options.disallowCustom = false;
  ASSERT_VELOCYPACK_EXCEPTION(
      b.add(Value(ValueType::Custom)),
      Exception::BuilderUnexpectedType);
}

TEST(BuilderTest, IllegalValueType) {
  Builder b;
  b.add(Value(ValueType::Illegal));

  ASSERT_EQ(ValueType::Illegal, b.slice().type());
}

TEST(BuilderTest, UTCDate) {
  int64_t const value = 12345678;
  Builder b;
  b.add(Value(value, ValueType::UTCDate));

  Slice s(b.start());
  ASSERT_EQ(0x1cU, s.head());
  ASSERT_TRUE(s.isUTCDate());
  ASSERT_EQ(9UL, s.byteSize());
  ASSERT_EQ(value, s.getUTCDate());
}

TEST(BuilderTest, UTCDateZero) {
  int64_t const value = 0;
  Builder b;
  b.add(Value(value, ValueType::UTCDate));

  Slice s(b.start());
  ASSERT_EQ(0x1cU, s.head());
  ASSERT_TRUE(s.isUTCDate());
  ASSERT_EQ(9UL, s.byteSize());
  ASSERT_EQ(value, s.getUTCDate());
}

TEST(BuilderTest, UTCDateMin) {
  int64_t const value = INT64_MIN;
  Builder b;
  b.add(Value(value, ValueType::UTCDate));

  Slice s(b.start());
  ASSERT_EQ(0x1cU, s.head());
  ASSERT_TRUE(s.isUTCDate());
  ASSERT_EQ(9UL, s.byteSize());
  ASSERT_EQ(value, s.getUTCDate());
}

TEST(BuilderTest, UTCDateMax) {
  int64_t const value = INT64_MAX;
  Builder b;
  b.add(Value(value, ValueType::UTCDate));

  Slice s(b.start());
  ASSERT_EQ(0x1cU, s.head());
  ASSERT_TRUE(s.isUTCDate());
  ASSERT_EQ(9UL, s.byteSize());
  ASSERT_EQ(value, s.getUTCDate());
}

TEST(BuilderTest, CustomTypeID) {
  // This is somewhat tautological, nevertheless...
  static uint8_t const correctResult[]
      = {0xf5, 0x0b, 0x2b, 0x78, 0x56, 0x34, 0x12,
         0x45, 0x02, 0x03, 0x05, 0x08, 0x0d};

  Builder b;
  uint8_t* p = b.add(ValuePair(sizeof(correctResult), ValueType::Custom));
  memcpy(p, correctResult, sizeof(correctResult));
  uint8_t* result = b.start();
  ValueLength len = b.size();

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));

  auto s = b.slice();
  ASSERT_EQ(sizeof(correctResult), s.byteSize());
}

TEST(BuilderTest, AddBCD) {
  Builder b;
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::BCD)),
                              Exception::NotImplemented);
}

TEST(BuilderTest, AddOnNonArray) {
  Builder b;
  b.add(Value(ValueType::Object));
  ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(true)),
                              Exception::BuilderKeyMustBeString);
}

TEST(BuilderTest, AddOnNonObject) {
  Builder b;
  b.add(Value(ValueType::Array));
  ASSERT_VELOCYPACK_EXCEPTION(b.add("foo", Value(true)),
                              Exception::BuilderNeedOpenObject);
}

TEST(BuilderTest, StartCalledOnOpenObject) {
  Builder b;
  b.add(Value(ValueType::Object));
  ASSERT_VELOCYPACK_EXCEPTION(b.start(), Exception::BuilderNotSealed);
}

TEST(BuilderTest, StartCalledOnOpenObjectWithSubs) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(2));
  b.close();
  ASSERT_VELOCYPACK_EXCEPTION(b.start(), Exception::BuilderNotSealed);
}

TEST(BuilderTest, HasKeyNonObject) {
  Builder b;
  b.add(Value(1));
  ASSERT_VELOCYPACK_EXCEPTION(b.hasKey("foo"),
                              Exception::BuilderNeedOpenObject);
}

TEST(BuilderTest, HasKeyArray) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1));
  ASSERT_VELOCYPACK_EXCEPTION(b.hasKey("foo"),
                              Exception::BuilderNeedOpenObject);
}

TEST(BuilderTest, HasKeyEmptyObject) {
  Builder b;
  b.add(Value(ValueType::Object));
  ASSERT_FALSE(b.hasKey("foo"));
  ASSERT_FALSE(b.hasKey("bar"));
  ASSERT_FALSE(b.hasKey("baz"));
  ASSERT_FALSE(b.hasKey("quetzalcoatl"));
  b.close();
}

TEST(BuilderTest, HasKeySubObject) {
  Builder b;
  b.add(Value(ValueType::Object));
  b.add("foo", Value(1));
  b.add("bar", Value(true));
  ASSERT_TRUE(b.hasKey("foo"));
  ASSERT_TRUE(b.hasKey("bar"));
  ASSERT_FALSE(b.hasKey("baz"));

  b.add("bark", Value(ValueType::Object));
  ASSERT_FALSE(b.hasKey("bark"));
  ASSERT_FALSE(b.hasKey("foo"));
  ASSERT_FALSE(b.hasKey("bar"));
  ASSERT_FALSE(b.hasKey("baz"));
  b.close();

  ASSERT_TRUE(b.hasKey("foo"));
  ASSERT_TRUE(b.hasKey("bar"));
  ASSERT_TRUE(b.hasKey("bark"));
  ASSERT_FALSE(b.hasKey("baz"));

  b.add("baz", Value(42));
  ASSERT_TRUE(b.hasKey("foo"));
  ASSERT_TRUE(b.hasKey("bar"));
  ASSERT_TRUE(b.hasKey("bark"));
  ASSERT_TRUE(b.hasKey("baz"));
  b.close();
}

TEST(BuilderTest, HasKeyCompact) {
  Builder b;
  b.add(Value(ValueType::Object, true));
  b.add("foo", Value(1));
  b.add("bar", Value(true));
  ASSERT_TRUE(b.hasKey("foo"));
  ASSERT_TRUE(b.hasKey("bar"));
  ASSERT_FALSE(b.hasKey("baz"));

  b.add("bark", Value(ValueType::Object, true));
  ASSERT_FALSE(b.hasKey("bark"));
  ASSERT_FALSE(b.hasKey("foo"));
  ASSERT_FALSE(b.hasKey("bar"));
  ASSERT_FALSE(b.hasKey("baz"));
  b.close();

  ASSERT_TRUE(b.hasKey("foo"));
  ASSERT_TRUE(b.hasKey("bar"));
  ASSERT_TRUE(b.hasKey("bark"));
  ASSERT_FALSE(b.hasKey("baz"));

  b.add("baz", Value(42));
  ASSERT_TRUE(b.hasKey("foo"));
  ASSERT_TRUE(b.hasKey("bar"));
  ASSERT_TRUE(b.hasKey("bark"));
  ASSERT_TRUE(b.hasKey("baz"));
  b.close();
}

TEST(BuilderTest, GetKeyNonObject) {
  Builder b;
  b.add(Value(1));
  ASSERT_VELOCYPACK_EXCEPTION(b.getKey("foo"),
                              Exception::BuilderNeedOpenObject);
}

TEST(BuilderTest, GetKeyArray) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1));
  ASSERT_VELOCYPACK_EXCEPTION(b.getKey("foo"),
                              Exception::BuilderNeedOpenObject);
}

TEST(BuilderTest, GetKeyEmptyObject) {
  Builder b;
  b.add(Value(ValueType::Object));
  ASSERT_TRUE(b.getKey("foo").isNone());
  ASSERT_TRUE(b.getKey("bar").isNone());
  ASSERT_TRUE(b.getKey("baz").isNone());
  ASSERT_TRUE(b.getKey("quetzalcoatl").isNone());
  b.close();
}

TEST(BuilderTest, GetKeySubObject) {
  Builder b;
  b.add(Value(ValueType::Object));
  b.add("foo", Value(1));
  b.add("bar", Value(true));
  ASSERT_EQ(1UL, b.getKey("foo").getUInt());
  ASSERT_TRUE(b.getKey("bar").getBool());
  ASSERT_TRUE(b.getKey("baz").isNone());

  b.add("bark", Value(ValueType::Object));
  ASSERT_TRUE(b.getKey("bark").isNone());
  ASSERT_TRUE(b.getKey("foo").isNone());
  ASSERT_TRUE(b.getKey("bar").isNone());
  ASSERT_TRUE(b.getKey("baz").isNone());
  b.close();

  ASSERT_EQ(1UL, b.getKey("foo").getUInt());
  ASSERT_TRUE(b.getKey("bar").getBool());
  ASSERT_TRUE(b.getKey("baz").isNone());
  ASSERT_TRUE(b.getKey("bark").isObject());

  b.add("baz", Value(42));
  ASSERT_EQ(1UL, b.getKey("foo").getUInt());
  ASSERT_TRUE(b.getKey("bar").getBool());
  ASSERT_EQ(42UL, b.getKey("baz").getUInt());
  ASSERT_TRUE(b.getKey("bark").isObject());
  b.close();
}

TEST(BuilderTest, GetKeyCompact) {
  Builder b;
  b.add(Value(ValueType::Object, true));
  b.add("foo", Value(1));
  b.add("bar", Value(true));
  ASSERT_EQ(1UL, b.getKey("foo").getUInt());
  ASSERT_TRUE(b.getKey("bar").getBool());
  ASSERT_TRUE(b.getKey("baz").isNone());

  b.add("bark", Value(ValueType::Object, true));
  ASSERT_FALSE(b.getKey("bark").isObject());
  ASSERT_TRUE(b.getKey("foo").isNone());
  ASSERT_TRUE(b.getKey("bar").isNone());
  ASSERT_TRUE(b.getKey("baz").isNone());
  ASSERT_TRUE(b.getKey("bark").isNone());
  b.close();

  ASSERT_EQ(1UL, b.getKey("foo").getUInt());
  ASSERT_TRUE(b.getKey("bar").getBool());
  ASSERT_TRUE(b.getKey("bark").isObject());
  ASSERT_TRUE(b.getKey("baz").isNone());

  b.add("baz", Value(42));
  ASSERT_EQ(1UL, b.getKey("foo").getUInt());
  ASSERT_TRUE(b.getKey("bar").getBool());
  ASSERT_TRUE(b.getKey("bark").isObject());
  ASSERT_EQ(42UL, b.getKey("baz").getUInt());
  b.close();
}

TEST(BuilderTest, IsClosedMixed) {
  Builder b;
  ASSERT_TRUE(b.isClosed());
  b.add(Value(ValueType::Null));
  ASSERT_TRUE(b.isClosed());
  b.add(Value(true));
  ASSERT_TRUE(b.isClosed());

  b.add(Value(ValueType::Array));
  ASSERT_FALSE(b.isClosed());

  b.add(Value(true));
  ASSERT_FALSE(b.isClosed());
  b.add(Value(true));
  ASSERT_FALSE(b.isClosed());

  b.close();
  ASSERT_TRUE(b.isClosed());

  b.add(Value(ValueType::Object));
  ASSERT_FALSE(b.isClosed());

  b.add("foo", Value(true));
  ASSERT_FALSE(b.isClosed());

  b.add("bar", Value(true));
  ASSERT_FALSE(b.isClosed());

  b.add("baz", Value(ValueType::Array));
  ASSERT_FALSE(b.isClosed());

  b.close();
  ASSERT_FALSE(b.isClosed());

  b.close();
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, IsClosedObject) {
  Builder b;
  ASSERT_TRUE(b.isClosed());
  b.add(Value(ValueType::Object));
  ASSERT_FALSE(b.isClosed());

  b.add("foo", Value(true));
  ASSERT_FALSE(b.isClosed());

  b.add("bar", Value(true));
  ASSERT_FALSE(b.isClosed());

  b.add("baz", Value(ValueType::Object));
  ASSERT_FALSE(b.isClosed());

  b.close();
  ASSERT_FALSE(b.isClosed());

  b.close();
  ASSERT_TRUE(b.isClosed());
}

TEST(BuilderTest, CloseClosed) {
  Builder b;
  ASSERT_TRUE(b.isClosed());
  b.add(Value(ValueType::Object));
  ASSERT_FALSE(b.isClosed());
  b.close();

  ASSERT_VELOCYPACK_EXCEPTION(b.close(), Exception::BuilderNeedOpenCompound);
}

TEST(BuilderTest, Clone) {
  Builder b;
  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.add("bar", Value(false));
  b.add("baz", Value("foobarbaz"));
  b.close();

  Slice s1(b.start());
  Builder clone = b.clone(s1);
  ASSERT_NE(s1.start(), clone.start());

  Slice s2(clone.start());

  ASSERT_TRUE(s1.isObject());
  ASSERT_TRUE(s2.isObject());
  ASSERT_EQ(3UL, s1.length());
  ASSERT_EQ(3UL, s2.length());

  ASSERT_TRUE(s1.hasKey("foo"));
  ASSERT_TRUE(s2.hasKey("foo"));
  ASSERT_NE(s1.get("foo").start(), s2.get("foo").start());
  ASSERT_TRUE(s1.hasKey("bar"));
  ASSERT_TRUE(s2.hasKey("bar"));
  ASSERT_NE(s1.get("bar").start(), s2.get("bar").start());
  ASSERT_TRUE(s1.hasKey("baz"));
  ASSERT_TRUE(s2.hasKey("baz"));
  ASSERT_NE(s1.get("baz").start(), s2.get("baz").start());
}

TEST(BuilderTest, CloneDestroyOriginal) {
  Builder clone;  // empty
  {
    Builder b;
    b.add(Value(ValueType::Object));
    b.add("foo", Value(true));
    b.add("bar", Value(false));
    b.add("baz", Value("foobarbaz"));
    b.close();

    Slice s(b.start());
    clone = b.clone(s);
    ASSERT_NE(b.start(), clone.start());
    // now b goes out of scope. clone should survive!
  }

  Slice s(clone.start());
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(3UL, s.length());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.get("foo").getBoolean());
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_FALSE(s.get("bar").getBoolean());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ("foobarbaz", s.get("baz").copyString());
}

TEST(BuilderTest, AttributeTranslations) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("mötör", 5);
  translator->add("quetzalcoatl", 6);
  translator->seal();

  ASSERT_EQ(6, translator->count());
  ASSERT_NE(nullptr, translator->builder());

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  Builder b(&options);
  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.add("bar", Value(false));
  b.add("baz", Value(1));
  b.add("bart", Value(2));
  b.add("bark", Value(42));
  b.add("mötör", Value(19));
  b.add("mötörhead", Value(20));
  b.add("quetzal", Value(21));
  b.close();

  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x0b, 0x35, 0x08, 0x31, 0x1a, 0x32, 0x19, 0x33, 0x31, 0x44, 0x62,
      0x61, 0x72, 0x74, 0x32, 0x34, 0x20, 0x2a, 0x35, 0x20, 0x13, 0x4b,
      0x6d, 0xc3, 0xb6, 0x74, 0xc3, 0xb6, 0x72, 0x68, 0x65, 0x61, 0x64,
      0x20, 0x14, 0x47, 0x71, 0x75, 0x65, 0x74, 0x7a, 0x61, 0x6c, 0x20,
      0x15, 0x05, 0x0f, 0x09, 0x07, 0x03, 0x12, 0x15, 0x23};

  ASSERT_EQ(sizeof(correctResult), len);
  ASSERT_EQ(0, memcmp(result, correctResult, len));

  Slice s = b.slice();

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_TRUE(s.hasKey("bart"));
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_TRUE(s.hasKey("mötör"));
  ASSERT_TRUE(s.hasKey("mötörhead"));
  ASSERT_TRUE(s.hasKey("quetzal"));
}

TEST(BuilderTest, AttributeTranslationsSorted) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("mötör", 5);
  translator->add("quetzalcoatl", 6);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  Builder b(&options);
  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.add("bar", Value(false));
  b.add("baz", Value(1));
  b.add("bart", Value(2));
  b.add("bark", Value(42));
  b.add("mötör", Value(19));
  b.add("mötörhead", Value(20));
  b.add("quetzal", Value(21));
  b.close();

  uint8_t* result = b.start();
  ValueLength len = b.size();

  static uint8_t correctResult[] = {
      0x0b, 0x35, 0x08, 0x31, 0x1a, 0x32, 0x19, 0x33, 0x31, 0x44, 0x62,
      0x61, 0x72, 0x74, 0x32, 0x34, 0x20, 0x2a, 0x35, 0x20, 0x13, 0x4b,
      0x6d, 0xc3, 0xb6, 0x74, 0xc3, 0xb6, 0x72, 0x68, 0x65, 0x61, 0x64,
      0x20, 0x14, 0x47, 0x71, 0x75, 0x65, 0x74, 0x7a, 0x61, 0x6c, 0x20,
      0x15, 0x05, 0x0f, 0x09, 0x07, 0x03, 0x12, 0x15, 0x23};

  ASSERT_EQ(sizeof(correctResult), len);

  ASSERT_EQ(0, memcmp(result, correctResult, len));
  Slice s = b.slice();

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_TRUE(s.hasKey("bart"));
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_TRUE(s.hasKey("mötör"));
  ASSERT_TRUE(s.hasKey("mötörhead"));
  ASSERT_TRUE(s.hasKey("quetzal"));
}

TEST(BuilderTest, AttributeTranslationsRollbackSet) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->seal();

  Options options;
  options.attributeTranslator = translator.get();

  Builder b(&options);
  b.add(Value(ValueType::Object));
  ASSERT_VELOCYPACK_EXCEPTION(b.add("foo", Value(ValueType::Custom)),
                              Exception::BuilderUnexpectedType);
  ASSERT_VELOCYPACK_EXCEPTION(b.add("FOOBAR", Value(ValueType::Custom)),
                              Exception::BuilderUnexpectedType);
}

TEST(BuilderTest, ToString) {
  Builder b;
  b.add(Value(ValueType::Object));
  b.add("test1", Value(123));
  b.add("test2", Value("foobar"));
  b.add("test3", Value(true));
  b.close();

  ASSERT_EQ("{\n  \"test1\" : 123,\n  \"test2\" : \"foobar\",\n  \"test3\" : true\n}",
            b.toString());
}

TEST(BuilderTest, ObjectBuilder) {
  Builder b;
  {
    ASSERT_TRUE(b.isClosed());

    ObjectBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add("foo", Value("aha"));
    ob->add("bar", Value("qux"));
    ASSERT_FALSE(ob->isClosed());
    ASSERT_FALSE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\n  \"bar\" : \"qux\",\n  \"foo\" : \"aha\"\n}", b.toString());
}

TEST(BuilderTest, ObjectBuilderNested) {
  Builder b;
  {
    ASSERT_TRUE(b.isClosed());

    ObjectBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add("foo", Value("aha"));
    ob->add("bar", Value("qux"));
    {
      ObjectBuilder ob2(&b, "hans");
      ASSERT_EQ(&*ob2, &b);
      ASSERT_FALSE(ob2->isClosed());
      ASSERT_FALSE(ob->isClosed());
      ASSERT_FALSE(b.isClosed());

      ob2->add("bart", Value("a"));
      ob2->add("zoo", Value("b"));
    }
    {
      ObjectBuilder ob2(&b, std::string("foobar"));
      ASSERT_EQ(&*ob2, &b);
      ASSERT_FALSE(ob2->isClosed());
      ASSERT_FALSE(ob->isClosed());
      ASSERT_FALSE(b.isClosed());

      ob2->add("bark", Value(1));
      ob2->add("bonk", Value(2));
    }

    ASSERT_FALSE(ob->isClosed());
    ASSERT_FALSE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\n  \"bar\" : \"qux\",\n  \"foo\" : \"aha\",\n  \"foobar\" : {\n    \"bark\" : 1,\n    \"bonk\" : 2\n  },\n  \"hans\" : {\n    \"bart\" : \"a\",\n    \"zoo\" : \"b\"\n  }\n}", b.toString());
}

TEST(BuilderTest, ObjectBuilderNestedArrayInner) {
  Builder b;
  {
    ASSERT_TRUE(b.isClosed());

    ObjectBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add("foo", Value("aha"));
    ob->add("bar", Value("qux"));
    {
      ArrayBuilder ab2(&b, "hans");
      ASSERT_EQ(&*ab2, &b);
      ASSERT_FALSE(ab2->isClosed());
      ASSERT_FALSE(ob->isClosed());
      ASSERT_FALSE(b.isClosed());

      ab2->add(Value("a"));
      ab2->add(Value("b"));
    }
    {
      ArrayBuilder ab2(&b, std::string("foobar"));
      ASSERT_EQ(&*ab2, &b);
      ASSERT_FALSE(ab2->isClosed());
      ASSERT_FALSE(ob->isClosed());
      ASSERT_FALSE(b.isClosed());

      ab2->add(Value(1));
      ab2->add(Value(2));
    }

    ASSERT_FALSE(ob->isClosed());
    ASSERT_FALSE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\n  \"bar\" : \"qux\",\n  \"foo\" : \"aha\",\n  \"foobar\" : [\n    1,\n    2\n  ],\n  \"hans\" : [\n    \"a\",\n    \"b\"\n  ]\n}", b.toString());
}

TEST(BuilderTest, ObjectBuilderClosed) {
  Builder b;
  ASSERT_TRUE(b.isClosed());

  // when the object builder goes out of scope, it will close
  // the object. if we manually close it ourselves in addition,
  // this should not fail as we can't allow throwing destructors
  {
    ObjectBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add("foo", Value("aha"));
    ob->add("bar", Value("qux"));
    b.close(); // manually close the builder
    ASSERT_TRUE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("{\n  \"bar\" : \"qux\",\n  \"foo\" : \"aha\"\n}", b.toString());
}

TEST(BuilderTest, ArrayBuilder) {
  Options options;
  Builder b(&options);
  {
    ASSERT_TRUE(b.isClosed());

    ArrayBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add(Value("foo"));
    ob->add(Value("bar"));
    ASSERT_FALSE(ob->isClosed());
    ASSERT_FALSE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("[\n  \"foo\",\n  \"bar\"\n]", b.toString());
}

TEST(BuilderTest, ArrayBuilderNested) {
  Options options;
  Builder b(&options);
  {
    ASSERT_TRUE(b.isClosed());

    ArrayBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add(Value("foo"));
    ob->add(Value("bar"));
    {
      ArrayBuilder ob2(&b);
      ASSERT_EQ(&*ob2, &b);
      ASSERT_FALSE(ob2->isClosed());
      ASSERT_FALSE(ob->isClosed());
      ASSERT_FALSE(b.isClosed());

      ob2->add(Value("bart"));
      ob2->add(Value("qux"));
    }
    {
      ArrayBuilder ob2(&b);
      ASSERT_EQ(&*ob2, &b);
      ASSERT_FALSE(ob2->isClosed());
      ASSERT_FALSE(ob->isClosed());
      ASSERT_FALSE(b.isClosed());

      ob2->add(Value(1));
      ob2->add(Value(2));
    }
    ASSERT_FALSE(ob->isClosed());
    ASSERT_FALSE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("[\n  \"foo\",\n  \"bar\",\n  [\n    \"bart\",\n    \"qux\"\n  ],\n  [\n    1,\n    2\n  ]\n]", b.toString());
}

TEST(BuilderTest, ArrayBuilderClosed) {
  Options options;
  Builder b(&options);
  ASSERT_TRUE(b.isClosed());

  // when the array builder goes out of scope, it will close
  // the object. if we manually close it ourselves in addition,
  // this should not fail as we can't allow throwing destructors
  {
    ArrayBuilder ob(&b);
    ASSERT_EQ(&*ob, &b);
    ASSERT_FALSE(b.isClosed());
    ASSERT_FALSE(ob->isClosed());
    ob->add(Value("foo"));
    ob->add(Value("bar"));
    b.close(); // manually close the builder
    ASSERT_TRUE(ob->isClosed());
    ASSERT_TRUE(b.isClosed());
  }
  ASSERT_TRUE(b.isClosed());

  ASSERT_EQ("[\n  \"foo\",\n  \"bar\"\n]", b.toString());
}

TEST(BuilderTest, AddKeysSeparately1) {
  Builder b;
  b.openObject();
  b.add(Value("name"));
  b.add(Value("Neunhoeffer"));
  b.add(Value("firstName"));
  b.add(Value("Max"));
  b.close();
  ASSERT_EQ(R"({"firstName":"Max","name":"Neunhoeffer"})", b.toJson());
}

TEST(BuilderTest, AddKeysSeparately2) {
  Builder b;

  b.openObject();
  b.add(Value("foo"));
  b.openArray();
  b.close();

  b.add(Value("bar"));
  b.openObject();
  b.close();

  b.add(Value("baz"));
  uint8_t buf[] = { 0x31 };
  Slice s(buf);
  b.add(s);

  b.add(Value("bumm"));
  Options options;
  options.clearBuilderBeforeParse = false;
  Parser p(b, &options);
  p.parse("[13]");
  b.close();
  ASSERT_EQ(R"({"bar":{},"baz":1,"bumm":[13],"foo":[]})", b.toJson());
}

TEST(BuilderTest, AddKeysSeparatelyFail) {
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(false)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Null)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Array)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::Object)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(1.0)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(12, ValueType::UTCDate)),
                                Exception::BuilderKeyMustBeString);
  }
  uint8_t buf[] = { 0x31 };
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(&buf, ValueType::External)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::MinKey)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(ValueType::MaxKey)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(1)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(-112)),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(Value(113)),
                                Exception::BuilderKeyMustBeString);
  }
  Slice s(buf);
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.add(s),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.openObject(),
                                Exception::BuilderNeedOpenArray);
  }
  {
    Builder b;
    b.openObject();
    ASSERT_VELOCYPACK_EXCEPTION(b.openArray(),
                                Exception::BuilderNeedOpenArray);
  }
  {
    Builder b;
    b.openObject();
    Options opt;
    opt.clearBuilderBeforeParse = false;
    Parser p(b, &opt);
    ASSERT_VELOCYPACK_EXCEPTION(p.parse("[13]"),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    Options opt;
    opt.clearBuilderBeforeParse = false;
    Parser p(b, &opt);
    ASSERT_VELOCYPACK_EXCEPTION(p.parse("\"max\""),
                                Exception::BuilderKeyMustBeString);
  }
  {
    Builder b;
    b.openObject();
    b.add(Value("abc"));
    ASSERT_VELOCYPACK_EXCEPTION(b.add("abc", Value(1)),
                                Exception::BuilderKeyAlreadyWritten);
  }
}

TEST(BuilderTest, HandInBuffer) {
  Buffer<uint8_t> buf;
  {
    Builder b(buf);
    ASSERT_EQ(&Options::Defaults, b.options);
    b.openObject();
    b.add("a",Value(123));
    b.add("b",Value("abc"));
    b.close();
    Slice s(buf.data());
    ASSERT_TRUE(s.isObject());
    ASSERT_EQ(2UL, s.length());
    Slice ss = s.get("a");
    ASSERT_TRUE(ss.isInteger());
    ASSERT_EQ(123L, ss.getInt());
    ss = s.get("b");
    ASSERT_TRUE(ss.isString());
    ASSERT_EQ(std::string("abc"), ss.copyString());
  }
  // check that everthing is still valid
  Slice s(buf.data());
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(2UL, s.length());
  Slice ss = s.get("a");
  ASSERT_TRUE(ss.isInteger());
  ASSERT_EQ(123L, ss.getInt());
  ss = s.get("b");
  ASSERT_TRUE(ss.isString());
  ASSERT_EQ(std::string("abc"), ss.copyString());
}


TEST(BuilderTest, HandInBufferNoOptions) {
  Buffer<uint8_t> buf;
  ASSERT_VELOCYPACK_EXCEPTION(new Builder(buf, nullptr),
                              Exception::InternalError);
}

TEST(BuilderTest, HandInBufferCustomOptions) {
  Buffer<uint8_t> buf;

  {
    Options options;
    Builder b(buf, &options);
    ASSERT_EQ(&options, b.options);

    b.openObject();
    b.add("a",Value(123));
    b.add("b",Value("abc"));
    b.close();
  }
  Slice s(buf.data());
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(2UL, s.length());
  Slice ss = s.get("a");
  ASSERT_TRUE(ss.isInteger());
  ASSERT_EQ(123L, ss.getInt());
  ss = s.get("b");
  ASSERT_TRUE(ss.isString());
  ASSERT_EQ(std::string("abc"), ss.copyString());
}

TEST(BuilderTest, HandInSharedBufferNoOptions) {
  auto buf = std::make_shared<Buffer<uint8_t>>();
  ASSERT_VELOCYPACK_EXCEPTION(new Builder(buf, nullptr),
                              Exception::InternalError);
}

TEST(BuilderTest, HandInSharedBufferCustomOptions) {
  auto buf = std::make_shared<Buffer<uint8_t>>();

  {
    Options options;
    Builder b(buf, &options);
    ASSERT_EQ(&options, b.options);

    b.openObject();
    b.add("a",Value(123));
    b.add("b",Value("abc"));
    b.close();
  }
  Slice s(buf->data());
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(2UL, s.length());
  Slice ss = s.get("a");
  ASSERT_TRUE(ss.isInteger());
  ASSERT_EQ(123L, ss.getInt());
  ss = s.get("b");
  ASSERT_TRUE(ss.isString());
  ASSERT_EQ(std::string("abc"), ss.copyString());
}

TEST(BuilderTest, SliceEmpty) {
  Builder b;
  ASSERT_EQ(ValueType::None, b.slice().type());
  ASSERT_EQ(1ULL, b.slice().byteSize());
}

TEST(BuilderTest, AddKeyToNonObject) {
  Builder b;
  b.openArray();

  ASSERT_VELOCYPACK_EXCEPTION(b.add(std::string("bar"), Value("foobar")), Exception::BuilderNeedOpenObject);
}

TEST(BuilderTest, KeyWritten) {
  Builder b;
  b.openObject();
  b.add(Value("foo"));

  ASSERT_VELOCYPACK_EXCEPTION(b.add(std::string("bar"), Value("foobar")), Exception::BuilderKeyAlreadyWritten);
}

TEST(BuilderTest, AddWithTranslator) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("mötör", 5);
  translator->add("quetzalcoatl", 6);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  Builder b(&options);
  b.openObject();
  b.add(std::string("foo"), Value("bar"));
  b.add(std::string("bar"), Value("baz"));
  b.add(std::string("bark"), Value("bank"));
  b.add(std::string("bonk"), Value("b0rk"));
  b.add(std::string("mötör"), Value("köter"));
  b.close();

  Slice s = b.slice();
  ASSERT_EQ(5UL, s.length());
  ASSERT_EQ("bar", s.keyAt(0).copyString());
  ASSERT_EQ(2UL, s.keyAt(0, false).getUInt());
  ASSERT_EQ("baz", s.valueAt(0).copyString());

  ASSERT_EQ("bark", s.keyAt(1).copyString());
  ASSERT_EQ(4UL, s.keyAt(1, false).getUInt());
  ASSERT_EQ("bank", s.valueAt(1).copyString());

  ASSERT_EQ("bonk", s.keyAt(2).copyString());
  ASSERT_EQ("b0rk", s.valueAt(2).copyString());

  ASSERT_EQ("foo", s.keyAt(3).copyString());
  ASSERT_EQ(1UL, s.keyAt(3, false).getUInt());
  ASSERT_EQ("bar", s.valueAt(3).copyString());

  ASSERT_EQ("mötör", s.keyAt(4).copyString());
  ASSERT_EQ(5UL, s.keyAt(4, false).getUInt());
  ASSERT_EQ("köter", s.valueAt(4).copyString());
}

TEST(BuilderTest, EmptyAttributeNames) {
  Builder b;
  {
    ObjectBuilder guard(&b);
    b.add("", Value(123));
    b.add("a", Value("abc"));
  }
  Slice s = b.slice();

  ASSERT_EQ(2UL, s.length());

  Slice ss = s.get("");
  ASSERT_FALSE(ss.isNone());
  ASSERT_TRUE(ss.isInteger());
  ASSERT_EQ(123L, ss.getInt());

  ss = s.get("a");
  ASSERT_FALSE(ss.isNone());
  ASSERT_TRUE(ss.isString());
  ASSERT_EQ(std::string("abc"), ss.copyString());

  ss = s.get("b");
  ASSERT_TRUE(ss.isNone());
}

TEST(BuilderTest, EmptyAttributeNamesNotThere) {
  Builder b;
  {
    ObjectBuilder guard(&b);
    b.add("b", Value(123));
    b.add("a", Value("abc"));
  }
  Slice s = b.slice();

  ASSERT_EQ(2UL, s.length());

  Slice ss = s.get("b");
  ASSERT_FALSE(ss.isNone());
  ASSERT_TRUE(ss.isInteger());
  ASSERT_EQ(123L, ss.getInt());

  ss = s.get("a");
  ASSERT_FALSE(ss.isNone());
  ASSERT_TRUE(ss.isString());
  ASSERT_EQ(std::string("abc"), ss.copyString());

  ss = s.get("");
  ASSERT_TRUE(ss.isNone());
}

TEST(BuilderTest, AddHundredNones) {
  Builder b;
  b.openArray(false);
  for (std::size_t i = 0; i < 100; ++i) {
    b.add(Slice::noneSlice());
  }
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(100UL, s.length());
  for (std::size_t i = 0; i < 100; ++i) {
    ASSERT_EQ(ValueType::None, s.at(i).type());
  }
}

TEST(BuilderTest, AddHundredNonesCompact) {
  Builder b;
  b.openArray(true);
  for (std::size_t i = 0; i < 100; ++i) {
    b.add(Slice::noneSlice());
  }
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(100UL, s.length());
  for (std::size_t i = 0; i < 100; ++i) {
    ASSERT_EQ(ValueType::None, s.at(i).type());
  }
}

TEST(BuilderTest, AddThousandNones) {
  Builder b;
  b.openArray(false);
  for (std::size_t i = 0; i < 1000; ++i) {
    b.add(Slice::noneSlice());
  }
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(1000UL, s.length());
  for (std::size_t i = 0; i < 1000; ++i) {
    ASSERT_EQ(ValueType::None, s.at(i).type());
  }
}

TEST(BuilderTest, AddThousandNonesCompact) {
  Builder b;
  b.openArray(true);
  for (std::size_t i = 0; i < 1000; ++i) {
    b.add(Slice::noneSlice());
  }
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(1000UL, s.length());
  for (std::size_t i = 0; i < 1000; ++i) {
    ASSERT_EQ(ValueType::None, s.at(i).type());
  }
}

TEST(BuilderTest, UsePaddingForOneByteArray) {
  Options options;
  Builder b(&options);

  auto build = [&b]() {
    b.clear();
    b.openArray();

    for (size_t i = 0; i < 20; ++i) {
      b.add(Value(i));
    }

    b.close();
    return b.slice().start();
  };

  auto test = [&b]() {
    for (uint64_t i = 0; i < 20; ++i) {
      ASSERT_EQ(i, b.slice().at(i).getUInt());
    }
  };

  options.paddingBehavior = Options::PaddingBehavior::NoPadding;
  uint8_t const* data = build();

  ASSERT_EQ(0x06, data[0]);
  ASSERT_EQ(0x35, data[1]);
  ASSERT_EQ(0x14, data[2]);
  ASSERT_EQ(0x30, data[3]);
  ASSERT_EQ(0x31, data[4]);
  ASSERT_EQ(0x32, data[5]);
  ASSERT_EQ(0x33, data[6]);
  ASSERT_EQ(0x34, data[7]);
  ASSERT_EQ(0x35, data[8]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::Flexible;
  data = build();

  ASSERT_EQ(0x06, data[0]);
  ASSERT_EQ(0x35, data[1]);
  ASSERT_EQ(0x14, data[2]);
  ASSERT_EQ(0x30, data[3]);
  ASSERT_EQ(0x31, data[4]);
  ASSERT_EQ(0x32, data[5]);
  ASSERT_EQ(0x33, data[6]);
  ASSERT_EQ(0x34, data[7]);
  ASSERT_EQ(0x35, data[8]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::UsePadding;
  data = build();

  ASSERT_EQ(0x06, data[0]);
  ASSERT_EQ(0x3b, data[1]);
  ASSERT_EQ(0x14, data[2]);
  ASSERT_EQ(0x00, data[3]);
  ASSERT_EQ(0x00, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x30, data[9]);
  ASSERT_EQ(0x31, data[10]);

  test();
}

TEST(BuilderTest, UsePaddingForTwoByteArray) {
  Options options;
  Builder b(&options);

  auto build = [&b]() {
    b.clear();
    b.openArray();

    for (std::uint64_t i = 0; i < 260; ++i) {
      b.add(Value(i));
    }

    b.close();
    return b.slice().start();
  };

  auto test = [&b]() {
    for (uint64_t i = 0; i < 260; ++i) {
      ASSERT_EQ(i, b.slice().at(i).getUInt());
    }
  };

  options.paddingBehavior = Options::PaddingBehavior::NoPadding;
  uint8_t const* data = build();

  ASSERT_EQ(0x07, data[0]);
  ASSERT_EQ(0x0f, data[1]);
  ASSERT_EQ(0x04, data[2]);
  ASSERT_EQ(0x04, data[3]);
  ASSERT_EQ(0x01, data[4]);
  ASSERT_EQ(0x30, data[5]);
  ASSERT_EQ(0x31, data[6]);
  ASSERT_EQ(0x32, data[7]);
  ASSERT_EQ(0x33, data[8]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::Flexible;
  data = build();

  ASSERT_EQ(0x07, data[0]);
  ASSERT_EQ(0x13, data[1]);
  ASSERT_EQ(0x04, data[2]);
  ASSERT_EQ(0x04, data[3]);
  ASSERT_EQ(0x01, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x30, data[9]);
  ASSERT_EQ(0x31, data[10]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::UsePadding;
  data = build();

  ASSERT_EQ(0x07, data[0]);
  ASSERT_EQ(0x13, data[1]);
  ASSERT_EQ(0x04, data[2]);
  ASSERT_EQ(0x04, data[3]);
  ASSERT_EQ(0x01, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x30, data[9]);
  ASSERT_EQ(0x31, data[10]);

  test();
}

TEST(BuilderTest, UsePaddingForEquallySizedArray) {
  Options options;
  Builder b(&options);

  auto build = [&b]() {
    b.clear();
    b.openArray();

    for (size_t i = 0; i < 3; ++i) {
      b.add(Value(i));
    }

    b.close();
    return b.slice().start();
  };

  auto test = [&b]() {
    for (uint64_t i = 0; i < 3; ++i) {
      ASSERT_EQ(i, b.slice().at(i).getUInt());
    }
  };

  options.paddingBehavior = Options::PaddingBehavior::NoPadding;
  uint8_t const* data = build();

  ASSERT_EQ(0x02, data[0]);
  ASSERT_EQ(0x05, data[1]);
  ASSERT_EQ(0x30, data[2]);
  ASSERT_EQ(0x31, data[3]);
  ASSERT_EQ(0x32, data[4]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::Flexible;
  data = build();

  ASSERT_EQ(0x02, data[0]);
  ASSERT_EQ(0x05, data[1]);
  ASSERT_EQ(0x30, data[2]);
  ASSERT_EQ(0x31, data[3]);
  ASSERT_EQ(0x32, data[4]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::UsePadding;
  data = build();

  ASSERT_EQ(0x05, data[0]);
  ASSERT_EQ(0x0c, data[1]);
  ASSERT_EQ(0x00, data[2]);
  ASSERT_EQ(0x00, data[3]);
  ASSERT_EQ(0x00, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x30, data[9]);
  ASSERT_EQ(0x31, data[10]);
  ASSERT_EQ(0x32, data[11]);

  test();
}

TEST(BuilderTest, UsePaddingForOneByteObject) {
  Options options;
  Builder b(&options);

  auto build = [&b]() {
    b.clear();
    b.openObject();

    for (size_t i = 0; i < 10; ++i) {
      b.add(Value(std::string("test") + std::to_string(i)));
      b.add(Value(i));
    }

    b.close();
    return b.slice().start();
  };

  auto test = [&b]() {
    for (uint64_t i = 0; i < 10; ++i) {
      std::string key = std::string("test") + std::to_string(i);
      ASSERT_TRUE(b.slice().hasKey(key));
      ASSERT_EQ(i, b.slice().get(key).getUInt());
    }
  };

  options.paddingBehavior = Options::PaddingBehavior::NoPadding;
  uint8_t const* data = build();

  ASSERT_EQ(0x0b, data[0]);
  ASSERT_EQ(0x53, data[1]);
  ASSERT_EQ(0x0a, data[2]);
  ASSERT_EQ(0x45, data[3]);
  ASSERT_EQ(0x74, data[4]);
  ASSERT_EQ(0x65, data[5]);
  ASSERT_EQ(0x73, data[6]);
  ASSERT_EQ(0x74, data[7]);
  ASSERT_EQ(0x30, data[8]);
  ASSERT_EQ(0x30, data[9]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::Flexible;
  data = build();

  ASSERT_EQ(0x0b, data[0]);
  ASSERT_EQ(0x53, data[1]);
  ASSERT_EQ(0x0a, data[2]);
  ASSERT_EQ(0x45, data[3]);
  ASSERT_EQ(0x74, data[4]);
  ASSERT_EQ(0x65, data[5]);
  ASSERT_EQ(0x73, data[6]);
  ASSERT_EQ(0x74, data[7]);
  ASSERT_EQ(0x30, data[8]);
  ASSERT_EQ(0x30, data[9]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::UsePadding;
  data = build();

  ASSERT_EQ(0x0b, data[0]);
  ASSERT_EQ(0x59, data[1]);
  ASSERT_EQ(0x0a, data[2]);
  ASSERT_EQ(0x00, data[3]);
  ASSERT_EQ(0x00, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x45, data[9]);
  ASSERT_EQ(0x74, data[10]);

  test();
}

TEST(BuilderTest, UsePaddingForTwoByteObject) {
  Options options;
  Builder b(&options);

  auto build = [&b]() {
    b.clear();
    b.openObject();

    for (std::uint64_t i = 0; i < 260; ++i) {
      b.add(Value(std::string("test") + std::to_string(i)));
      b.add(Value(i));
    }

    b.close();
    return b.slice().start();
  };

  auto test = [&b]() {
    for (uint64_t i = 0; i < 260; ++i) {
      std::string key = std::string("test") + std::to_string(i);
      ASSERT_TRUE(b.slice().hasKey(key));
      ASSERT_EQ(i, b.slice().get(key).getUInt());
    }
  };

  options.paddingBehavior = Options::PaddingBehavior::NoPadding;
  uint8_t const* data = build();

  ASSERT_EQ(0x0c, data[0]);
  ASSERT_EQ(0xc1, data[1]);
  ASSERT_EQ(0x0b, data[2]);
  ASSERT_EQ(0x04, data[3]);
  ASSERT_EQ(0x01, data[4]);
  ASSERT_EQ(0x45, data[5]);
  ASSERT_EQ(0x74, data[6]);
  ASSERT_EQ(0x65, data[7]);
  ASSERT_EQ(0x73, data[8]);
  ASSERT_EQ(0x74, data[9]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::Flexible;
  data = build();

  ASSERT_EQ(0x0c, data[0]);
  ASSERT_EQ(0xc5, data[1]);
  ASSERT_EQ(0x0b, data[2]);
  ASSERT_EQ(0x04, data[3]);
  ASSERT_EQ(0x01, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x45, data[9]);
  ASSERT_EQ(0x74, data[10]);

  test();

  options.paddingBehavior = Options::PaddingBehavior::UsePadding;
  data = build();

  ASSERT_EQ(0x0c, data[0]);
  ASSERT_EQ(0xc5, data[1]);
  ASSERT_EQ(0x0b, data[2]);
  ASSERT_EQ(0x04, data[3]);
  ASSERT_EQ(0x01, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x45, data[9]);
  ASSERT_EQ(0x74, data[10]);

  test();
}

TEST(BuilderTest, Tags) {
  Builder b;
  b.openObject();
  b.add("a", Value(0));
  b.addTagged("b", 0, Value(0));
  b.addTagged("c", 1, Value(1));
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(3UL, s.length());

  ASSERT_FALSE(s.get("a").isTagged());
  ASSERT_FALSE(s.get("b").isTagged());
  ASSERT_TRUE(s.get("c").isTagged());
  ASSERT_FALSE(s.get("c").value().isTagged());
  ASSERT_EQ(s.get("c").value().getInt(), 1);
}

TEST(BuilderTest, TagsDisallowed) {
  Options options;
  options.disallowTags = true;

  Builder b(&options);
  b.openObject();
  b.add("a", Value(0));
  ASSERT_VELOCYPACK_EXCEPTION(b.addTagged("b", 1, Value(0)), Exception::BuilderTagsDisallowed);
}

TEST(BuilderTest, Tags8Byte) {
  Builder b;
  b.openObject();
  b.add("a", Value(0));
  b.addTagged("b", 257, Value(0));
  b.addTagged("c", 123456789, Value(1));
  b.addTagged("d", 1, Value(2));
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(4UL, s.length());

  ASSERT_FALSE(s.get("a").isTagged());
  ASSERT_TRUE(s.get("b").isTagged());
  ASSERT_TRUE(s.get("c").isTagged());
  ASSERT_TRUE(s.get("d").isTagged());
  ASSERT_FALSE(s.get("b").value().isTagged());
  ASSERT_FALSE(s.get("c").value().isTagged());
  ASSERT_FALSE(s.get("d").value().isTagged());
  ASSERT_EQ(s.get("b").value().getInt(), 0);
  ASSERT_EQ(s.get("c").value().getInt(), 1);
  ASSERT_EQ(s.get("d").value().getInt(), 2);
}

TEST(BuilderTest, Tags8ByteDisallowed) {
  Options options;
  options.disallowTags = true;

  Builder b(&options);
  b.openObject();
  b.add("a", Value(0));
  ASSERT_VELOCYPACK_EXCEPTION(b.addTagged("b", 99999999, Value(0)), Exception::BuilderTagsDisallowed);
}

TEST(BuilderTest, TagsArray) {
  Builder b;
  b.openArray();
  b.add(Value(0));
  b.addTagged(0, Value(0));
  b.addTagged(1, Value(1));
  b.close();

  Slice s = b.slice();

  ASSERT_EQ(3UL, s.length());

  ASSERT_FALSE(s.at(0).isTagged());
  ASSERT_FALSE(s.at(1).isTagged());
  ASSERT_TRUE(s.at(2).isTagged());
  ASSERT_FALSE(s.at(2).value().isTagged());
  ASSERT_EQ(s.at(2).value().getInt(), 1);
}

TEST(BuilderTest, TestBoundariesWithPaddingButContainingNones) {
  Options options;
  Builder b(&options);

  auto fill = [&b](std::size_t n) {
    b.clear();
    b.openArray();
    b.add(Slice::noneSlice());
    for (std::size_t i = 0; i < n; ++i) {
      b.add(Value(1));
    }
    b.add(Value("the-quick-brown-foxx"));
    b.close();
  };

  std::array<Options::PaddingBehavior, 3> behaviors = {
    Options::PaddingBehavior::Flexible,
    Options::PaddingBehavior::NoPadding,
    Options::PaddingBehavior::UsePadding
  };
    
  for (auto const& behavior : behaviors) {
    options.paddingBehavior = behavior;
    {
      fill(110);
      uint8_t const* data = b.slice().start();
      ASSERT_EQ(6U, data[0]);
      ASSERT_EQ(253U, data[1]);
      ASSERT_EQ(112U, data[2]);
      // padding
      ASSERT_EQ(0U, data[3]);
      ASSERT_EQ(0U, data[4]);
      ASSERT_EQ(0U, data[5]);
      ASSERT_EQ(0U, data[6]);
      ASSERT_EQ(0U, data[7]);
      ASSERT_EQ(0U, data[8]);
    }
    
    {
      fill(111);
      uint8_t const* data = b.slice().start();
      ASSERT_EQ(6U, data[0]);
      ASSERT_EQ(255U, data[1]);
      ASSERT_EQ(113U, data[2]);
      // padding
      ASSERT_EQ(0U, data[3]);
      ASSERT_EQ(0U, data[4]);
      ASSERT_EQ(0U, data[5]);
      ASSERT_EQ(0U, data[6]);
      ASSERT_EQ(0U, data[7]);
      ASSERT_EQ(0U, data[8]);
    }

    // switch to 2-byte offset sizes
    {
      fill(112);
      uint8_t const* data = b.slice().start();
      ASSERT_EQ(7U, data[0]);
      ASSERT_EQ(115U, data[1]);
      ASSERT_EQ(1U, data[2]);
      ASSERT_EQ(114U, data[3]);
      ASSERT_EQ(0U, data[4]);
      // padding, because of contained None slice
      ASSERT_EQ(0U, data[5]);
      ASSERT_EQ(0U, data[6]);
      ASSERT_EQ(0U, data[7]);
      ASSERT_EQ(0U, data[8]);
    }
  }
}

TEST(BuilderTest, getSharedSliceEmpty) {
  SharedSlice ss;
  Builder b;
  ASSERT_EQ(1, b.buffer().use_count());
  auto const sharedSlice = b.sharedSlice();
  ASSERT_EQ(1, b.buffer().use_count());
  ASSERT_EQ(3, sharedSlice.buffer().use_count());
}

TEST(BuilderTest, getSharedSlice) {
  auto const check = [](Builder& b, bool const /* isSmall */) {
    auto const slice = b.slice();
    ASSERT_EQ(1, b.buffer().use_count());
    auto const sharedSlice = b.sharedSlice();
    ASSERT_EQ(1, b.buffer().use_count());
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    ASSERT_FALSE(haveSameOwnership(b.buffer(), sharedSlice.buffer()));
    ASSERT_EQ(slice.byteSize(), sharedSlice.byteSize());
    ASSERT_EQ(0, memcmp(slice.start(), sharedSlice.start().get(), slice.byteSize()));
  };

  auto smallBuilder = Builder{};
  auto largeBuilder = Builder{};

  // A buffer can take slices up to 192 bytes without allocating memory.
  // This will fit:
  smallBuilder.add(Value(0));
  // This will not:
  largeBuilder.add(Value(std::string(size_t{256}, 'x')));

  check(smallBuilder, true);
  check(largeBuilder, false);
}
  
TEST(BuilderTest, getSharedSliceOpen) {
  SharedSlice ss;
  Builder b;
  b.openObject();
  ASSERT_VELOCYPACK_EXCEPTION(ss = b.sharedSlice(), Exception::BuilderNotSealed);
}

TEST(BuilderTest, stealSharedSlice) {
  auto const check = [](Builder& b, bool const isSmall) {
    ASSERT_EQ(isSmall, b.buffer()->usesLocalMemory());
    auto const bufferCopy = *b.buffer();
    auto const slice = Slice{bufferCopy.data()};
    ASSERT_EQ(1, b.buffer().use_count());
    auto const sharedSlice = std::move(b).sharedSlice();
    ASSERT_EQ(nullptr, b.buffer());
    ASSERT_EQ(0, b.buffer().use_count());
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    ASSERT_EQ(slice.byteSize() , sharedSlice.byteSize());
    ASSERT_EQ(0, memcmp(slice.start() , sharedSlice.start().get(), slice.byteSize()));
  };

  auto smallBuilder = Builder{};
  auto largeBuilder = Builder{};

  // A buffer can take slices up to 192 bytes without allocating memory.
  // This will fit:
  smallBuilder.add(Value(0));
  // This will not:
  largeBuilder.add(Value(std::string(size_t{256}, 'x')));

  check(smallBuilder, true);
  check(largeBuilder, false);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
