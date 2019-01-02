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
#include "utils/directory_utils.hpp"

using namespace iresearch;

class memory_directory_test : public directory_test_case,
                              public test_base {
 protected:
  virtual void SetUp() {
    test_base::SetUp();

    dir_ = directory::make<memory_directory>();
  }
}; // memory_directory_test

TEST_F(memory_directory_test, construct_check_allocator) {
  // default ctor
  {
    irs::memory_directory dir;
    ASSERT_FALSE(dir.attributes().get<irs::memory_allocator>());
    ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::get_allocator(dir));
  }

  // specify pool size
  {
    irs::memory_directory dir(42);
    auto* alloc_attr = dir.attributes().get<irs::memory_allocator>();
    ASSERT_NE(nullptr, alloc_attr);
    ASSERT_NE(nullptr, *alloc_attr);
    ASSERT_NE(alloc_attr->get(), &irs::memory_allocator::global()); // not a global allocator
    ASSERT_EQ(alloc_attr->get(), &irs::directory_utils::get_allocator(dir));
  }
}

TEST_F(memory_directory_test, file_reset_allocator) {
  memory_allocator alloc0(1);
  memory_allocator alloc1(1);
  memory_file file(alloc0);

  // get buffer from 'alloc0'
  auto buf0 = file.push_buffer();
  ASSERT_NE(nullptr, buf0.data);
  ASSERT_EQ(0, buf0.offset);
  ASSERT_EQ(256, buf0.size);

  // set length
  {
    auto mtime = file.mtime();
    ASSERT_EQ(0, file.length());
    file.length(1);
    ASSERT_EQ(1, file.length());
    ASSERT_LE(mtime, file.mtime());
  }

  // switch allocator
  file.reset(alloc1);
  ASSERT_EQ(0, file.length());

  // return back buffer to 'alloc0'
  file.pop_buffer();

  // get buffer from 'alloc1'
  auto buf1 = file.push_buffer();
  ASSERT_NE(nullptr, buf1.data);
  ASSERT_EQ(0, buf1.offset);
  ASSERT_EQ(256, buf1.size);

  ASSERT_NE(buf0.data, buf1.data);
}

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
