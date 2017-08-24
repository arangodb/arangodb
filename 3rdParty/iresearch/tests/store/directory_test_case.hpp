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

#ifndef IRESEARCH_DIRECTORYTESTS_H
#define IRESEARCH_DIRECTORYTESTS_H

#include "tests_shared.hpp"
#include "store/directory.hpp"

class directory_test_case {
 public:
  void list();
  void visit();
  void smoke_index_io();
  void smoke_store();
  void string_read_write();
  void read_multiple_streams();
  void lock_obtain_release();
  void directory_size();

 protected:
  iresearch::directory::ptr dir_;
};

#endif
