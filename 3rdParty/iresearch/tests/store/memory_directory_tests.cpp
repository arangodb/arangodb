////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include "directory_test_case.hpp"

#include "store/memory_directory.hpp"

using namespace iresearch;

class memory_directory_test : public directory_test_case,
                              public test_base {
 protected:
  virtual void SetUp() {
    test_base::SetUp();

    dir_ = directory::make<memory_directory>();
  }
}; // memory_directory_test

TEST_F(memory_directory_test, read_multiple_streams) {
  read_multiple_streams();
}

TEST_F(memory_directory_test, string_read_write) {
  string_read_write();
}

TEST_F(memory_directory_test, smoke_store) {
  smoke_store();
}

TEST_F(memory_directory_test, list) {
  list();
}

TEST_F(memory_directory_test, visit) {
  visit();
}

TEST_F(memory_directory_test, index_io) {
  smoke_index_io();
}

TEST_F(memory_directory_test, lock_obtain_release) {
  lock_obtain_release();
}

TEST_F(memory_directory_test, directory_size) {
  directory_size();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
