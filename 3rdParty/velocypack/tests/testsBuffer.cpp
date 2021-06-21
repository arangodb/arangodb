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

#include <ostream>
#include <string>
#include <iostream>

#include "tests-common.h"

TEST(BufferTest, CreateEmpty) {
  Buffer<uint8_t> buffer;

  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());
  ASSERT_NE(nullptr, buffer.data());
}

TEST(BufferTest, CreateEmptyWithSize) {
  Buffer<uint8_t> buffer(10);

  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());
  ASSERT_NE(nullptr, buffer.data());
}

TEST(BufferTest, CreateAndAppend) {
  std::string const value("this is a test string");
  Buffer<uint8_t> buffer;

  buffer.append(value.c_str(), value.size());
  ASSERT_EQ(value.size(), buffer.size());
  ASSERT_EQ(value.size(), buffer.length());
  ASSERT_EQ(value.size(), buffer.byteSize());
}

TEST(BufferTest, CreateAndAppendLong) {
  std::string const value("this is a test string");
  Buffer<uint8_t> buffer;

  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  ASSERT_EQ(1000 * value.size(), buffer.size());
  ASSERT_EQ(1000 * value.size(), buffer.length());
  ASSERT_EQ(1000 * value.size(), buffer.byteSize());
}

TEST(BufferTest, CopyConstruct) {
  std::string const value("this is a test string");
  Buffer<char> buffer;
  buffer.append(value.c_str(), value.size());

  Buffer<char> buffer2(buffer);
  ASSERT_EQ(value.size(), buffer2.size());
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_EQ(value, std::string(buffer2.data(), buffer2.size()));
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, CopyConstructLongValue) {
  std::string const value("this is a test string");
  
  Buffer<char> buffer;
  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  Buffer<char> buffer2(buffer);
  ASSERT_EQ(1000 * value.size(), buffer2.size());
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, CopyAssign) {
  std::string const value("this is a test string");
  Buffer<char> buffer;
  buffer.append(value.c_str(), value.size());

  Buffer<char> buffer2;
  buffer2 = buffer;
  ASSERT_EQ(value.size(), buffer2.size());
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_EQ(value, std::string(buffer2.data(), buffer2.size()));
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, CopyAssignLongValue) {
  std::string const value("this is a test string");

  Buffer<char> buffer;
  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  Buffer<char> buffer2;
  buffer2 = buffer;
  ASSERT_EQ(1000 * value.size(), buffer2.size());
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, CopyAssignDiscardOwnValue) {
  std::string const value("this is a test string");

  Buffer<char> buffer;
  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  Buffer<char> buffer2;
  for (std::size_t i = 0; i < 100; ++i) {
    buffer2.append(value.c_str(), value.size());
  }

  buffer2 = buffer;
  ASSERT_EQ(1000 * value.size(), buffer2.size());
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, MoveConstruct) {
  std::string const value("this is a test string");
  Buffer<char> buffer;
  buffer.append(value.c_str(), value.size());

  Buffer<char> buffer2(std::move(buffer));
  ASSERT_EQ(value.size(), buffer2.size());
  ASSERT_EQ(0UL, buffer.size()); // should be empty
  ASSERT_EQ(value, std::string(buffer2.data(), buffer2.size()));
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, MoveConstructLongValue) {
  std::string const value("this is a test string");
  
  Buffer<char> buffer;
  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  Buffer<char> buffer2(std::move(buffer));
  ASSERT_EQ(1000 * value.size(), buffer2.size());
  ASSERT_EQ(0UL, buffer.size()); // should be empty
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, MoveAssign) {
  std::string const value("this is a test string");
  Buffer<char> buffer;
  buffer.append(value.c_str(), value.size());

  Buffer<char> buffer2;
  buffer2 = std::move(buffer);
  ASSERT_EQ(value.size(), buffer2.size());
  ASSERT_EQ(0UL, buffer.size()); // should be empty
  ASSERT_EQ(value, std::string(buffer2.data(), buffer2.size()));
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, MoveAssignLongValue) {
  std::string const value("this is a test string");
  
  Buffer<char> buffer;
  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  Buffer<char> buffer2;
  buffer2 = std::move(buffer);
  ASSERT_EQ(1000 * value.size(), buffer2.size());
  ASSERT_EQ(0UL, buffer.size()); // should be empty
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, MoveAssignDiscardOwnValue) {
  std::string const value("this is a test string");
  
  Buffer<char> buffer;
  for (std::size_t i = 0; i < 1000; ++i) {
    buffer.append(value.c_str(), value.size());
  }

  Buffer<char> buffer2;
  for (std::size_t i = 0; i < 100; ++i) {
    buffer2.append(value.c_str(), value.size());
  }

  buffer2 = std::move(buffer);
  ASSERT_EQ(1000 * value.size(), buffer2.size());
  ASSERT_EQ(0UL, buffer.size()); // should be empty
  ASSERT_NE(buffer.data(), buffer2.data());
}

TEST(BufferTest, Clear) {
  Buffer<uint8_t> buffer;

  buffer.clear();
  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());

  buffer.append("foobar", 6);

  buffer.clear();
  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());

  for (std::size_t i = 0; i < 256; ++i) {
    buffer.push_back('x');
  }
  
  ASSERT_EQ(256UL, buffer.size());
  ASSERT_EQ(256UL, buffer.length());
  ASSERT_EQ(256UL, buffer.byteSize());
  ASSERT_FALSE(buffer.empty());
  
  buffer.clear();
  
  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());
}

TEST(BufferTest, SizeEmpty) {
  Buffer<uint8_t> buffer;

  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());
}

TEST(BufferTest, SizeNonEmpty) {
  Buffer<uint8_t> buffer;
  buffer.append("foobar", 6);

  ASSERT_EQ(6UL, buffer.size());
  ASSERT_EQ(6UL, buffer.length());
  ASSERT_EQ(6UL, buffer.byteSize());
  ASSERT_TRUE(!buffer.empty());
}
   
TEST(BufferTest, SizeAfterClear) {
  Buffer<uint8_t> buffer;
  buffer.append("foobar", 6);

  buffer.clear();
  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());
}

TEST(BufferTest, SizeAfterReset) {
  Buffer<uint8_t> buffer;
  buffer.append("foobar", 6);

  buffer.reset();
  ASSERT_EQ(0UL, buffer.size());
  ASSERT_EQ(0UL, buffer.length());
  ASSERT_EQ(0UL, buffer.byteSize());
  ASSERT_TRUE(buffer.empty());
}

TEST(BufferTest, VectorTest) {
  std::vector<Buffer<uint8_t>> buffers;

  Builder builder;
  builder.add(Value("der hund, der ist so bunt"));
  
  Slice s = builder.slice();
  ASSERT_TRUE(s.isString());
  Buffer<uint8_t> b;
  b.append(s.start(), s.byteSize());

  buffers.push_back(b);
  
  Buffer<uint8_t>& last = buffers.back();
  Slice copy(last.data());
  ASSERT_TRUE(copy.isString());
  ASSERT_TRUE(copy.binaryEquals(s));
  ASSERT_EQ("der hund, der ist so bunt", copy.copyString());
}

TEST(BufferTest, VectorMoveTest) {
  std::vector<Buffer<uint8_t>> buffers;

  Builder builder;
  builder.add(Value("der hund, der ist so bunt"));
  
  Slice s = builder.slice();
  ASSERT_TRUE(s.isString());
  Buffer<uint8_t> b;
  b.append(s.start(), s.byteSize());

  buffers.push_back(std::move(b));
  
  Buffer<uint8_t>& last = buffers.back();
  Slice copy(last.data());
  ASSERT_TRUE(copy.isString());
  ASSERT_TRUE(copy.binaryEquals(s));
  ASSERT_EQ(0UL, b.byteSize());
}

TEST(BufferTest, PushBackTest) {
  Buffer<uint8_t> buffer;
  
  buffer.push_back('x');
  ASSERT_EQ(1UL, buffer.size()); 
}

TEST(BufferTest, AppendUInt8Test) {
  Buffer<uint8_t> buffer;
  
  uint8_t const value[] = "der hund, der ist so bunt";
  char const* p = reinterpret_cast<char const*>(value);
  buffer.append(value, std::strlen(p));
  ASSERT_EQ(std::strlen(p), buffer.size()); 
}

TEST(BufferTest, AppendCharTest) {
  Buffer<uint8_t> buffer;
  
  char const* value = "der hund, der ist so bunt";
  buffer.append(value, std::strlen(value));
  ASSERT_EQ(std::strlen(value), buffer.size()); 
}

TEST(BufferTest, AppendStringTest) {
  Buffer<uint8_t> buffer;
  
  std::string const value("der hund, der ist so bunt");
  buffer.append(value);
  ASSERT_EQ(value.size(), buffer.size()); 
}

TEST(BufferTest, AppendBufferTest) {
  Buffer<uint8_t> buffer;
  
  Buffer<uint8_t> original;
  std::string const value("der hund, der ist so bunt");
  original.append(value);

  buffer.append(original);
  ASSERT_EQ(original.size(), buffer.size()); 
}

TEST(BufferTest, SubscriptTest) {
  Buffer<uint8_t> buffer;
 
  std::string const value("der hund, der ist so bunt");
  buffer.append(value); 

  ASSERT_EQ('d', buffer[0]);
  ASSERT_EQ('e', buffer[1]);
  ASSERT_EQ('r', buffer[2]);
  ASSERT_EQ(' ', buffer[3]);
  ASSERT_EQ('h', buffer[4]);
  ASSERT_EQ('u', buffer[5]);
  ASSERT_EQ('n', buffer[6]);
  ASSERT_EQ('d', buffer[7]);

  ASSERT_EQ('b', buffer[21]);
  ASSERT_EQ('u', buffer[22]);
  ASSERT_EQ('n', buffer[23]);
  ASSERT_EQ('t', buffer[24]);
}

TEST(BufferTest, SubscriptModifyTest) {
  Buffer<uint8_t> buffer;
 
  std::string const value("der hans");
  buffer.append(value); 

  buffer[0] = 'a';
  buffer[1] = 'b';
  buffer[2] = 'c';
  buffer[3] = 'd';
  //buffer[4] = 'e';
  buffer[5] = 'f';
  buffer[6] = 'g';
  buffer[7] = 'h';

  ASSERT_EQ('a', buffer[0]);
  ASSERT_EQ('b', buffer[1]);
  ASSERT_EQ('c', buffer[2]);
  ASSERT_EQ('d', buffer[3]);
  ASSERT_EQ('h', buffer[4]);
  ASSERT_EQ('f', buffer[5]);
  ASSERT_EQ('g', buffer[6]);
  ASSERT_EQ('h', buffer[7]);
}

TEST(BufferTest, SubscriptAtTest) {
  Buffer<uint8_t> buffer;
 
  std::string const value("der hund, der ist so bunt");
  buffer.append(value); 

  ASSERT_EQ('d', buffer.at(0));
  ASSERT_EQ('e', buffer.at(1));
  ASSERT_EQ('r', buffer.at(2));
  ASSERT_EQ(' ', buffer.at(3));
  ASSERT_EQ('h', buffer.at(4));
  ASSERT_EQ('u', buffer.at(5));
  ASSERT_EQ('n', buffer.at(6));
  ASSERT_EQ('d', buffer.at(7));

  ASSERT_EQ('b', buffer.at(21));
  ASSERT_EQ('u', buffer.at(22));
  ASSERT_EQ('n', buffer.at(23));
  ASSERT_EQ('t', buffer.at(24));

  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(25), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(26), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(1000), Exception::IndexOutOfBounds);

  buffer.reset();
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(0), Exception::IndexOutOfBounds);
}

TEST(BufferTest, SubscriptAtModifyTest) {
  Buffer<uint8_t> buffer;
 
  std::string const value("der hans");
  buffer.append(value); 

  buffer.at(0) = 'a';
  buffer.at(1) = 'b';
  buffer.at(2) = 'c';
  buffer.at(3) = 'd';
  //buffer.at(4) = 'e';
  buffer.at(5) = 'f';
  buffer.at(6) = 'g';
  buffer.at(7) = 'h';

  ASSERT_EQ('a', buffer.at(0));
  ASSERT_EQ('b', buffer.at(1));
  ASSERT_EQ('c', buffer.at(2));
  ASSERT_EQ('d', buffer.at(3));
  ASSERT_EQ('h', buffer.at(4));
  ASSERT_EQ('f', buffer.at(5));
  ASSERT_EQ('g', buffer.at(6));
  ASSERT_EQ('h', buffer.at(7));
  
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(8), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(9), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(1000), Exception::IndexOutOfBounds);

  buffer.reset();
  ASSERT_VELOCYPACK_EXCEPTION(buffer.at(0), Exception::IndexOutOfBounds);
}

TEST(BufferTest, ResetToTest) {
  Buffer<uint8_t> buffer;
 
  ASSERT_VELOCYPACK_EXCEPTION(buffer.resetTo(256), Exception::IndexOutOfBounds);
  buffer.resetTo(0);
  ASSERT_EQ(std::string(), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));

  buffer.push_back('a');
  ASSERT_EQ(std::string("a"), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));

  ASSERT_VELOCYPACK_EXCEPTION(buffer.resetTo(256), Exception::IndexOutOfBounds);
  
  buffer.resetTo(1);
  ASSERT_EQ(std::string("a"), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));
  buffer.resetTo(0);
  ASSERT_EQ(std::string(), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));
  
  buffer.append("foobar");
  ASSERT_EQ(std::string("foobar"), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));
  buffer.resetTo(3);

  ASSERT_EQ(std::string("foo"), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));
  buffer.resetTo(6);
  ASSERT_EQ(std::string("foobar"), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));
  
  buffer.resetTo(1);
  ASSERT_EQ(std::string("f"), std::string(reinterpret_cast<char const*>(buffer.data()), buffer.size()));
}

TEST(BufferTest, BufferNonDeleterTest) {
  Buffer<uint8_t> buffer;
  for (std::size_t i = 0; i < 256; ++i) {
    buffer.push_back('x');
  }

  {
    std::shared_ptr<Buffer<uint8_t>> ptr;
    ptr.reset(&buffer, BufferNonDeleter<uint8_t>());

    buffer.append("test");
    ptr.reset();
  }

  ASSERT_EQ(260, buffer.size());
  
  // make sure the Buffer is still usable
  for (std::size_t i = 0; i < 2048; ++i) {
    buffer.push_back('x');
  }
  
  ASSERT_EQ(2308, buffer.size());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
