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
