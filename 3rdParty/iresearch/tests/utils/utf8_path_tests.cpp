////////////////////////////////////////////////////////////////////////////////
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <fstream>

#include "tests_shared.hpp"
#include "utils/utf8_path.hpp"

NS_LOCAL

class utf8_path_tests: public test_base {
  ::boost::filesystem::path cwd_;

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    test_base::SetUp();

    cwd_ = ::boost::filesystem::current_path();
    ::boost::filesystem::create_directories(test_dir()); // ensure path exists
    ::boost::filesystem::current_path(test_dir()); // ensure all files/directories created in a valid place
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    ::boost::filesystem::current_path(cwd_);

    test_base::TearDown();
  }
};

NS_END

#ifndef IRESEARCH_DLL

TEST_F(utf8_path_tests, current) {
  irs::utf8_path path(true);
  std::string directory("deleteme");
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;

  ASSERT_TRUE(test_dir().native() == path.native());
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));

  path/=directory;
  ASSERT_TRUE(path.mkdir());
  ASSERT_TRUE(path.chdir());
  ASSERT_TRUE(path.native() == irs::utf8_path(true).native());
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));
}

TEST_F(utf8_path_tests, empty) {
  irs::utf8_path path;
  std::string empty("");
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;

  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));
  path/=empty;
  ASSERT_FALSE(path.mkdir());
}

TEST_F(utf8_path_tests, file) {
  irs::utf8_path path;
  std::string suffix(".other");
  std::string file1("deleteme");
  std::string file2(file1 + suffix);
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;

  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  path/=file1;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  std::string data("data");
  std::ofstream out1(file1.c_str());
  out1 << data;
  out1.close();
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_TRUE(path.file_size(tmpUint) && tmpUint == data.size());

  ASSERT_FALSE(path.mkdir());

  path+=suffix;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  std::ofstream out2(file2.c_str());
  out2 << data << data;
  out2.close();
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_TRUE(path.file_size(tmpUint) && tmpUint == data.size() * 2);
}

TEST_F(utf8_path_tests, directory) {
  irs::utf8_path path;
  std::string directory("deleteme");
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;

  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  path/=directory;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  ASSERT_TRUE(path.mkdir());
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));
}

#endif