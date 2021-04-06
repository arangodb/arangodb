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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <iostream>

#include "Mocks/LogLevels.h"

#include "Agency/Node.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace node_test {

TEST(SmallBufferTest, defaultConstructor) {
  SmallBuffer sb;
  ASSERT_EQ(nullptr, sb.data());
  ASSERT_EQ(0, sb.size());
  ASSERT_TRUE(sb.empty());
}

TEST(SmallBufferTest, sizeConstructor) {
  for (size_t i : std::vector<size_t>({10, 123, 2049, 65536, 1237235})) {
    SmallBuffer sb(i);
    ASSERT_NE(sb.data(), nullptr);
    ASSERT_EQ(i, sb.size());
    ASSERT_FALSE(sb.empty());
  }
}

TEST(SmallBufferTest, memcpyConstructor) {
  char const* x = "The quick brown fox jumps over the lazy dog.";
  size_t s = strlen(x);
  SmallBuffer sb((uint8_t const*) x, s);
  ASSERT_NE(sb.data(), nullptr);
  ASSERT_EQ(s, sb.size());
  ASSERT_FALSE(sb.empty());
  ASSERT_EQ(0, strncmp(x, (char const*) sb.data(), s));
}

TEST(SmallBufferTest, copyConstructor) {
  SmallBuffer sb1(123);
  for (size_t i = 0; i < 123; ++i) {
    sb1.data()[i] = (uint8_t)i;
  }
  SmallBuffer sb2(sb1);
  ASSERT_NE(sb2.data(), nullptr);
  ASSERT_NE(sb2.data(), sb1.data());
  ASSERT_FALSE(sb2.empty());
  ASSERT_EQ(123, sb2.size());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb2.data()[i]);
  }
}

TEST(SmallBufferTest, moveConstructor) {
  SmallBuffer sb1(123);
  for (size_t i = 0; i < 123; ++i) {
    sb1.data()[i] = (uint8_t)i;
  }
  uint8_t* p = sb1.data();
  SmallBuffer sb2(std::move(sb1));

  ASSERT_EQ(nullptr, sb1.data());
  ASSERT_EQ(0, sb1.size());
  ASSERT_TRUE(sb1.empty());

  ASSERT_EQ(p, sb2.data());
  ASSERT_EQ(123, sb2.size());
  ASSERT_FALSE(sb2.empty());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb2.data()[i]);
  }
}

TEST(SmallBufferTest, copyAssignment) {
  SmallBuffer sb1(123);
  for (size_t i = 0; i < 123; ++i) {
    sb1.data()[i] = (uint8_t)i;
  }
  SmallBuffer sb2;
  sb2 = sb1;
  ASSERT_NE(sb2.data(), nullptr);
  ASSERT_NE(sb2.data(), sb1.data());
  ASSERT_FALSE(sb2.empty());
  ASSERT_EQ(123, sb2.size());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb2.data()[i]);
  }
  SmallBuffer sb3(147);
  sb3 = sb1;
  ASSERT_NE(sb3.data(), nullptr);
  ASSERT_NE(sb3.data(), sb1.data());
  ASSERT_FALSE(sb3.empty());
  ASSERT_EQ(123, sb3.size());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb3.data()[i]);
  }
}

TEST(SmallBufferTest, copySelfAssignment) {
  SmallBuffer sb1(123);
  for (size_t i = 0; i < 123; ++i) {
    sb1.data()[i] = (uint8_t)i;
  }

  // we want a self-assignment here, but we have
  // to make it look complicated so compilers don't 
  // detect it too easily
  SmallBuffer* target = &sb1;
  *target = sb1;
  
  ASSERT_NE(sb1.data(), nullptr);
  ASSERT_FALSE(sb1.empty());
  ASSERT_EQ(123, sb1.size());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb1.data()[i]);
  }
}

TEST(SmallBufferTest, moveAssignment) {
  SmallBuffer sb1(123);
  for (size_t i = 0; i < 123; ++i) {
    sb1.data()[i] = (uint8_t)i;
  }
  uint8_t* p = sb1.data();
  SmallBuffer sb2;
  sb2 = std::move(sb1);

  ASSERT_EQ(nullptr, sb1.data());
  ASSERT_EQ(0, sb1.size());
  ASSERT_TRUE(sb1.empty());

  ASSERT_EQ(p, sb2.data());
  ASSERT_EQ(123, sb2.size());
  ASSERT_FALSE(sb2.empty());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb2.data()[i]);
  }

  SmallBuffer sb3(147);
  sb3 = std::move(sb2);

  ASSERT_EQ(nullptr, sb2.data());
  ASSERT_EQ(0, sb2.size());
  ASSERT_TRUE(sb2.empty());

  ASSERT_EQ(p, sb3.data());
  ASSERT_EQ(123, sb3.size());
  ASSERT_FALSE(sb3.empty());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb3.data()[i]);
  }
}

TEST(SmallBufferTest, moveSelfAssignment) {
  SmallBuffer sb1(123);
  for (size_t i = 0; i < 123; ++i) {
    sb1.data()[i] = (uint8_t)i;
  }

  // we want a self-move assignment here, but we have
  // to make it look complicated so compilers don't 
  // detect it too easily
  SmallBuffer* target = &sb1;
  *target = std::move(sb1);

  ASSERT_NE(nullptr, sb1.data());
  ASSERT_EQ(123, sb1.size());
  ASSERT_FALSE(sb1.empty());
  for (size_t i = 0; i < 123; ++i) {
    ASSERT_EQ((uint8_t) i, sb1.data()[i]);
  }
}

class NodeTest
  : public ::testing::Test,
    public arangodb::tests::LogSuppressor<arangodb::Logger::SUPERVISION, arangodb::LogLevel::ERR> {
 protected:

  NodeTest() {}
};


TEST_F(NodeTest, node_name) {

  std::string name("node");
  Node n(name);

  EXPECT_EQ(n.name(), name);
}


TEST_F(NodeTest, node_assign_string_slice) {

  std::string path("/a/b/c"), name("node"), val("test");
  Node n(name);
  auto b = std::make_shared<VPackBuilder>();
  
  b->add(VPackValue(val));
  n(path) = b->slice();
  EXPECT_EQ(n(path).getString(), val);
   
}


TEST_F(NodeTest, node_assign_double_slice) {

  std::string path("/a/b/c"), name("node");
  double val(8.1);
  Node n(name);
  auto b = std::make_shared<VPackBuilder>();
  
  b->add(VPackValue(val));
  n(path) = b->slice();
  EXPECT_DOUBLE_EQ(n(path).getDouble(), val);
   
}

TEST_F(NodeTest, node_assign_int_slice) {

  std::string path("/a/b/c"), name("node");
  int64_t val(8);
  Node n(name);
  auto b = std::make_shared<VPackBuilder>();
  
  b->add(VPackValue(val));
  n(path) = b->slice();
  EXPECT_EQ(n(path).getInt(), val);
   
}

TEST_F(NodeTest, node_assign_array_slice) {

  std::string path("/a/b/c"), name("node");;
  Node n(name);
  auto b = std::make_shared<VPackBuilder>();
  { VPackArrayBuilder a(b.get());
    b->add(VPackValue("Hello world"));
    b->add(VPackValue(3.14159265359));
    b->add(VPackValue(64)); }

  n(path) = b->slice();
  EXPECT_EQ(n(path).getArray().binaryEquals(b->slice()), true);
   
}

TEST_F(NodeTest, node_applyOp_set) {

  std::string path("/a/pi"), name("node");;
  Node n(name);
  double pi = 3.14159265359;
  int eleven = 11;

  auto b = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder a(b.get());
    b->add("op", VPackValue("set"));
    b->add("new", VPackValue(pi)); }

  auto ret = n(path).applyOp(b->slice());
  EXPECT_EQ(ret.ok(), true);
  EXPECT_EQ(ret.get(), nullptr);
  EXPECT_DOUBLE_EQ(n(path).getDouble(), pi);

  b = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder a(b.get());
    b->add("op", VPackValue("set"));
    b->add("new", VPackValue(eleven)); }

  ret = n(path).applyOp(b->slice());
  EXPECT_EQ(ret.ok(), true);
  EXPECT_EQ(n(path).getInt(), eleven);

  b = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder a(b.get());
    b->add("op", VPackValue("set"));
    b->add("val", VPackValue(eleven)); }

  ret = n(path).applyOp(b->slice());
  EXPECT_EQ(ret.ok(), false);
  // std::cout << ret.errorMessage() << std::endl;

  b = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder a(b.get());
    b->add("op", VPackValue("set")); }

  ret = n(path).applyOp(b->slice());
  EXPECT_EQ(ret.ok(), false);
  // std::cout << ret.errorMessage() << std::endl;
}

TEST_F(NodeTest, node_applyOp_delete) {

  std::string path("/a/pi"), name("node");;
  Node n(name);
  double pi = 3.14159265359;

  Builder b;
  { VPackObjectBuilder a(&b);
    b.add("op", VPackValue("set"));
    b.add("new", VPackValue(pi)); }

  auto ret = n(path).applyOp(b.slice());
  EXPECT_EQ(ret.ok(), true);

  b.clear();
  { VPackObjectBuilder a(&b);
    b.add("op", VPackValue("delete")); }

  ret = n(path).applyOp(b.slice());
  EXPECT_EQ(ret.ok(), true);
  EXPECT_NE(ret.get(), nullptr);
  EXPECT_EQ(ret.get()->getDouble(), pi);
  EXPECT_NE(ret.get()->has(path), true);
  
}

TEST_F(NodeTest, node_applyOp_bs) {

  std::string path("/a/pi"), name("node");;
  Node n(name);
  std::string oper = "bs", error = std::string("Unknown operation '") + oper + "'";

  Builder b;
  { VPackObjectBuilder a(&b);
    b.add("op", VPackValue(oper)); }

  auto ret = n(path).applyOp(b.slice());
  EXPECT_EQ(ret.ok(), false);
  EXPECT_EQ(ret.errorMessage(), error);

}

TEST_F(NodeTest, node_applyOp_lock) {

  std::string pathpi("/a/pi"), path("/a"), name("node");
  Node n(name);
  std::string lock = "read-lock", unlock = "read-unlock", wlock = "write-lock",
    wulock = "write-unlock", caller1 = "this",  caller2 = "that", caller3 = "them";
  auto ret = arangodb::ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_FAILED);
  
  Builder lck1;
  { VPackObjectBuilder a(&lck1);
    lck1.add("op", VPackValue(lock));
    lck1.add("by", VPackValue(caller1)); }
  Builder ulck1;
  { VPackObjectBuilder a(&ulck1);
    ulck1.add("op", VPackValue(unlock));
    ulck1.add("by", VPackValue(caller1)); }
  Builder wlck1;
  { VPackObjectBuilder a(&wlck1);
    wlck1.add("op", VPackValue(wlock));
    wlck1.add("by", VPackValue(caller1)); }
  Builder wulck1;
  { VPackObjectBuilder a(&wulck1);
    wulck1.add("op", VPackValue(wulock));
    wulck1.add("by", VPackValue(caller1)); }
  Builder lck2;
  { VPackObjectBuilder a(&lck2);
    lck2.add("op", VPackValue(lock));
    lck2.add("by", VPackValue(caller2)); }
  Builder ulck2;
  { VPackObjectBuilder a(&ulck2);
    ulck2.add("op", VPackValue(unlock));
    ulck2.add("by", VPackValue(caller2)); }
  Builder wlck2;
  { VPackObjectBuilder a(&wlck2);
    wlck2.add("op", VPackValue(wlock));
    wlck2.add("by", VPackValue(caller2)); }
  Builder ulck3;
  { VPackObjectBuilder a(&ulck3);
    ulck3.add("op", VPackValue(unlock));
    ulck3.add("by", VPackValue(caller3)); }

  // caller1 unlock -> reject (no locks yet)
  ret = n(path).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), false);
  
  // caller1 lock -> accept
  ret = n(path).applyOp(lck1.slice());
  EXPECT_EQ(ret.ok(), true);

  // caller1 lock -> reject (same locker)
  ret = n(path).applyOp(lck1.slice());
  EXPECT_EQ(ret.ok(), false);
  
  // caller2 lock -> accept
  ret = n(path).applyOp(lck2.slice());
  EXPECT_EQ(ret.ok(), true);
  
  // caller2 lock -> reject (same locker)
  ret = n(path).applyOp(lck2.slice());
  EXPECT_EQ(ret.ok(), false);
  
  // caller1 unlock -> accept
  ret = n(path).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), true);
  
  // caller1 lock -> accept
  ret = n(path).applyOp(lck1.slice());
  EXPECT_EQ(ret.ok(), true);
  
  // caller1 unlock -> accept
  ret = n(path).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), true);
  
  // caller1 unlock -> reject (not a locker)
  ret = n(path).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), false);
  
  // caller1 write lock -> reject (cannot write lock while still locked by caller2) 
  ret = n(path).applyOp(wlck1.slice());
  EXPECT_EQ(ret.ok(), false);

  // caller2 unlock -> accept
  ret = n(path).applyOp(ulck2.slice());
  EXPECT_EQ(ret.ok(), true);

  // Node should be gone
  EXPECT_NE(ret.get(), nullptr);
  EXPECT_EQ(n.has(path), false);
  
  // caller1 write lock -> accept
  ret = n(path).applyOp(wlck1.slice());
  EXPECT_EQ(ret.ok(), true);

  // caller1 write lock -> reject (exclusive)
  ret = n(path).applyOp(wlck1.slice());
  EXPECT_EQ(ret.ok(), false);

  // caller1 write lock -> reject (exclusive)
  ret = n(path).applyOp(wlck2.slice());
  EXPECT_EQ(ret.ok(), false);

  // caller1 write unlock -> accept
  ret = n(path).applyOp(wulck1.slice());
  EXPECT_EQ(ret.ok(), true);

  // Node should be gone
  EXPECT_NE(ret.get(), nullptr);
  EXPECT_EQ(n.has(path), false);

  double pi = 3.14159265359;
  Builder b;
  { VPackObjectBuilder a(&b);
    b.add("op", VPackValue("set"));
    b.add("new", VPackValue(pi)); }
  ret = n(pathpi).applyOp(b.slice());

  //////////////// Only lockable I
  
  // caller1 unlock -> reject (no locks yet)
  ret = n(path).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), false);
  
  // caller1 lock -> accept
  ret = n(path).applyOp(lck1.slice());
  EXPECT_EQ(ret.ok(), false);

  // caller1 unlock -> reject (no locks yet)
  ret = n(path).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), false);

  // Node should not be gone (pathpi is beneath)
  EXPECT_EQ(n.has(path), true);

  //////////////// Only lockable caller1
    
  // II unlock -> reject (no locks yet)
  ret = n(pathpi).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), false);
  
  // caller1 lock -> accept
  ret = n(pathpi).applyOp(lck1.slice());
  EXPECT_EQ(ret.ok(), false);

  // caller1 unlock -> reject (no locks yet)
  ret = n(pathpi).applyOp(ulck1.slice());
  EXPECT_EQ(ret.ok(), false);

  // Node should not be gone (pathpipi holds pi)
  EXPECT_EQ(n.has(pathpi), true);

}

}}} //namespace


