////////////////////////////////////////////////////////////////////////////////
/// @brief test agency's key value node class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

using namespace arangodb;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace node_test {

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
  EXPECT_EQ(n(path).getDouble(), val);
   
}

}}} //namespace



