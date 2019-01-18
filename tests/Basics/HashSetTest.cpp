////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for HashSet class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/HashSet.h"

#include "catch.hpp"

TEST_CASE("HashSetTest", "[HashSet]") {

/// @brief test size
SECTION("test_size") {
  arangodb::HashSet<size_t> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 1000; ++i) {
    CHECK(values.size() == i); 
    values.insert(i);
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  // insert same values again
  for (size_t i = 0; i < 1000; ++i) {
    CHECK(values.size() == 1000); 
    values.insert(i);
    CHECK(values.size() == 1000); 
    CHECK(!values.empty());
  }
  
  for (size_t i = 0; i < 1000; ++i) {
    CHECK(values.size() == 1000 - i); 
    CHECK(!values.empty());
    values.erase(i);
    CHECK(values.size() == 999 - i); 
  }
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 1000; ++i) {
    CHECK(values.size() == i); 
    values.insert(i);
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  values.clear();
  CHECK(values.size() == 0); 
  CHECK(values.empty());
}

/// @brief test with int
SECTION("test_int") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    CHECK(values.size() == i); 
    values.insert(i);
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) != values.end());
  }
    
  CHECK(values.find(123) == values.end());
  CHECK(values.find(999) == values.end());
  CHECK(values.find(100) == values.end());
  CHECK(values.find(-1) == values.end());
}

/// @brief test with std::string
SECTION("test_string") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 100; ++i) {
    CHECK(values.size() == i); 
    values.insert(std::string("test") + std::to_string(i));
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  for (size_t i = 0; i < 100; ++i) {
    std::string value = std::string("test") + std::to_string(i);
    CHECK(values.find(value) != values.end());
  }
    
  CHECK(values.find(std::string("test")) == values.end());
  CHECK(values.find(std::string("foo")) == values.end());
  CHECK(values.find(std::string("test100")) == values.end());
  CHECK(values.find(std::string("")) == values.end());
}

/// @brief test with std::string
SECTION("test_long_string") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 100; ++i) {
    CHECK(values.size() == i); 
    values.insert(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i));
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  for (size_t i = 0; i < 100; ++i) {
    std::string value = std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i);
    CHECK(values.find(value) != values.end());
  }
    
  CHECK(values.find(std::string("test")) == values.end());
  CHECK(values.find(std::string("foo")) == values.end());
  CHECK(values.find(std::string("test100")) == values.end());
  CHECK(values.find(std::string("")) == values.end());
}

/// @brief test with std::string
SECTION("test_string_duplicates") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 100; ++i) {
    CHECK(values.size() == i); 
    auto res = values.emplace(std::string("test") + std::to_string(i));
    CHECK(res.first != values.end());
    CHECK(res.second);
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  for (size_t i = 0; i < 100; ++i) {
    CHECK(values.size() == 100); 
    auto res = values.emplace(std::string("test") + std::to_string(i));
    CHECK(res.first != values.end());
    CHECK(!res.second);
    CHECK(values.size() == 100); 
    CHECK(!values.empty());
  }
  
  for (size_t i = 0; i < 100; ++i) {
    std::string value = std::string("test") + std::to_string(i);
    CHECK(values.find(value) != values.end());
  }
    
  CHECK(values.find(std::string("test")) == values.end());
  CHECK(values.find(std::string("foo")) == values.end());
  CHECK(values.find(std::string("test100")) == values.end());
  CHECK(values.find(std::string("")) == values.end());
}

/// @brief test erase
SECTION("test_erase") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());

  CHECK(values.erase(1234) == 0);
  CHECK(values.erase(0) == 0);
  
  for (int i = 0; i < 1000; ++i) {
    values.insert(i);
  }
  
  CHECK(values.erase(1234) == 0);
  CHECK(values.erase(0) == 1);

  CHECK(values.find(0) == values.end());
  for (int i = 1; i < 100; ++i) {
    CHECK(values.find(i) != values.end());
    CHECK(values.erase(i) == 1);
    CHECK(values.find(i) == values.end());
  }
  
  CHECK(values.size() == 900); 
  
  for (int i = 100; i < 1000; ++i) {
    CHECK(values.find(i) != values.end());
    CHECK(values.erase(i) == 1);
    CHECK(values.find(i) == values.end());
  }

  CHECK(values.size() == 0); 
  CHECK(values.empty()); 
}

/// @brief test reserve
SECTION("test_reserve") {
  arangodb::HashSet<size_t> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
 
  values.reserve(10000);
  CHECK(values.size() == 0); 
  CHECK(values.empty());
   
  for (size_t i = 0; i < 32; ++i) {
    values.insert(i);
  }

  CHECK(values.size() == 32);
  CHECK(!values.empty());

  values.reserve(10);
  CHECK(values.size() == 32); 
  CHECK(!values.empty());

  values.reserve(20000);
  CHECK(values.size() == 32); 
  CHECK(!values.empty());

  for (size_t i = 0; i < 32; ++i) {
    CHECK(values.find(i) != values.end());
  }
}

/// @brief test few values
SECTION("test_few") {
  arangodb::HashSet<size_t> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 32; ++i) {
    CHECK(values.size() == i); 
    values.insert(i);
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  CHECK(values.size() == 32); 
  CHECK(!values.empty());
  
  for (size_t i = 0; i < 32; ++i) {
    CHECK(values.find(i) != values.end());
  }
}

/// @brief test many values
SECTION("test_many") {
  arangodb::HashSet<size_t> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (size_t i = 0; i < 200000; ++i) {
    CHECK(values.size() == i); 
    values.insert(i);
    CHECK(values.size() == i + 1); 
    CHECK(!values.empty());
  }

  CHECK(values.size() == 200000); 
  CHECK(!values.empty());
  
  for (size_t i = 0; i < 200000; ++i) {
    CHECK(values.find(i) != values.end());
  }
}

/// @brief test copying
SECTION("test_copy_construct_local") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 2; ++i) {
    values.insert(i);
  }

  // copy 
  arangodb::HashSet<int> copy(values);

  CHECK(values.size() == 2); 
  CHECK(!values.empty());
  
  CHECK(copy.size() == 2); 
  CHECK(!copy.empty());

  for (int i = 0; i < 2; ++i) {
    CHECK(values.find(i) != values.end());
    CHECK(copy.find(i) != copy.end());
  }

  values.clear();
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  CHECK(copy.size() == 2); 
  CHECK(!copy.empty());
  
  for (int i = 0; i < 2; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test copying
SECTION("test_copy_construct_heap") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(i);
  }

  // copy 
  arangodb::HashSet<int> copy(values);

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) != values.end());
    CHECK(copy.find(i) != copy.end());
  }

  values.clear();
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());
  
  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test copying
SECTION("test_copy_construct_heap_huge") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i));
  }

  // copy 
  arangodb::HashSet<std::string> copy(values);

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != values.end());
    CHECK(copy.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != copy.end());
  }

  values.clear();
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());
  
  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) == values.end());
    CHECK(copy.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != copy.end());
  }
}

/// @brief test copying
SECTION("test_copy_assign_local") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 2; ++i) {
    values.insert(i);
  }

  // copy 
  arangodb::HashSet<int> copy = values;

  CHECK(values.size() == 2); 
  CHECK(!values.empty());
  
  CHECK(copy.size() == 2); 
  CHECK(!copy.empty());

  for (int i = 0; i < 2; ++i) {
    CHECK(values.find(i) != values.end());
    CHECK(copy.find(i) != copy.end());
  }

  values.clear();
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  CHECK(copy.size() == 2); 
  CHECK(!copy.empty());
  
  for (int i = 0; i < 2; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test copying
SECTION("test_copy_assign_heap") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(i);
  }

  // copy 
  arangodb::HashSet<int> copy = values;

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) != values.end());
    CHECK(copy.find(i) != copy.end());
  }

  values.clear();
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());
  
  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test copying
SECTION("test_copy_assign_heap_huge") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i));
  }

  // copy 
  arangodb::HashSet<std::string> copy = values;

  CHECK(values.size() == 100); 
  CHECK(!values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != values.end());
    CHECK(copy.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != copy.end());
  }

  values.clear();
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());
  
  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) == values.end());
    CHECK(copy.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != copy.end());
  }
}

/// @brief test moving
SECTION("test_move_construct_local") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 2; ++i) {
    values.insert(i);
  }

  // move
  arangodb::HashSet<int> copy(std::move(values));

  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  CHECK(copy.size() == 2); 
  CHECK(!copy.empty());

  for (int i = 0; i < 2; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test moving
SECTION("test_move_construct_heap") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(i);
  }

  // move
  arangodb::HashSet<int> copy(std::move(values));

  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test moving
SECTION("test_move_construct_heap_huge") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i));
  }

  // move
  arangodb::HashSet<std::string> copy(std::move(values));

  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) == values.end());
    CHECK(copy.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != copy.end());
  }
}

/// @brief test moving
SECTION("test_move_assign_local") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 2; ++i) {
    values.insert(i);
  }

  // move
  arangodb::HashSet<int> copy = std::move(values);

  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  CHECK(copy.size() == 2); 
  CHECK(!copy.empty());

  for (int i = 0; i < 2; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test moving
SECTION("test_move_assign_heap") {
  arangodb::HashSet<int> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(i);
  }

  // move
  arangodb::HashSet<int> copy = std::move(values);

  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(i) == values.end());
    CHECK(copy.find(i) != copy.end());
  }
}

/// @brief test moving
SECTION("test_move_assign_heap_huge") {
  arangodb::HashSet<std::string> values;
  
  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  for (int i = 0; i < 100; ++i) {
    values.insert(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i));
  }

  // move
  arangodb::HashSet<std::string> copy = std::move(values);

  CHECK(values.size() == 0); 
  CHECK(values.empty());
  
  CHECK(copy.size() == 100); 
  CHECK(!copy.empty());

  for (int i = 0; i < 100; ++i) {
    CHECK(values.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) == values.end());
    CHECK(copy.find(std::string("test-this-will-hopefully-disable-sso-everywhere") + std::to_string(i)) != copy.end());
  }
}

/// @brief test iterator
SECTION("test_iterator") {
  arangodb::HashSet<int> values;
  
  CHECK(values.begin() == values.end());

  for (int i = 0; i < 1000; ++i) {
    values.insert(i);
    CHECK(values.begin() != values.end());
    CHECK(values.find(i) != values.end());
    CHECK(values.find(i + 1000) == values.end());
  }

  size_t count;

  count = 0;
  for (auto const& it : values) {
    CHECK(it >= 0);
    CHECK(it < 1000);
    ++count;
  }
  CHECK(count == 1000);
  
  count = 0;
  for (auto it = values.begin(); it != values.end(); ++it) {
    CHECK(it != values.end());
    auto i = (*it);
    CHECK(i >= 0);
    CHECK(i < 1000);
    ++count;
  }
  CHECK(count == 1000);
  
  count = 0;
  for (auto it = values.cbegin(); it != values.cend(); ++it) {
    CHECK(it != values.end());
    auto i = (*it);
    CHECK(i >= 0);
    CHECK(i < 1000);
    ++count;
  }
  CHECK(count == 1000);
}

}
