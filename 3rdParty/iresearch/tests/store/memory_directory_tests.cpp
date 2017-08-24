//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
