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
#include "utils/file_utils.hpp"
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

TEST_F(utf8_path_tests, path) {
  #if defined(_MSC_VER)
    const char* native_path_sep("\\");
  #else
    const char* native_path_sep("/");
  #endif
  std::string data("data");
  std::string suffix(".other");
  std::string file1("deleteme");
  std::string file2(file1 + suffix);
  std::string dir1("deleteme.dir");
  auto pwd_native = irs::utf8_path(true).native();
  auto pwd_utf8 = irs::utf8_path(true).utf8();
  auto file1_abs_native = (irs::utf8_path(true) /= file1).native();
  auto file1f_abs_native = ((irs::utf8_path(true) += "/") += file1).native(); // abs file1 with forward slash
  auto file1n_abs_native = ((irs::utf8_path(true) += native_path_sep) += file1).native(); // abs file1 with native slash
  auto file1_abs_utf8 = (irs::utf8_path(true) /= file1).utf8();
  auto file1f_abs_utf8 = ((irs::utf8_path(true) += "/") += file1).utf8(); // abs file1 with forward slash
  auto file1n_abs_utf8 = ((irs::utf8_path(true) += native_path_sep) += file1).utf8(); // abs file1 with native slash
  auto file2_abs_native = (irs::utf8_path(true) /= file2).native();
  auto file2_abs_utf8 = (irs::utf8_path(true) /= file2).utf8();
  auto dir_abs_native = (irs::utf8_path(true) /= dir1).native();
  auto dir_abs_utf8 = (irs::utf8_path(true) /= dir1).utf8();

  // create file
  {
    std::ofstream out(file1.c_str());
    out << data;
    out.close();
  }

  // from native string
  {
    irs::utf8_path path1(file1_abs_native);
    irs::utf8_path path1f(file1f_abs_native);
    irs::utf8_path path1n(file1n_abs_native);
    irs::utf8_path path2(file2_abs_native);
    irs::utf8_path dir1(pwd_native);
    irs::utf8_path dir2(dir_abs_native);
    bool tmpBool;
    std::time_t tmpTime;
    uint64_t tmpUint;

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1f.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1f.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1f.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1n.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1n.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1n.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(dir1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_FALSE(dir1.file_size(tmpUint));

    ASSERT_TRUE(dir2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dir2.mtime(tmpTime));
    ASSERT_FALSE(dir2.file_size(tmpUint));
  }

  // from native string_ref
  {
    irs::utf8_path path1(file1_abs_native.c_str());
    irs::utf8_path path1f(file1f_abs_native.c_str());
    irs::utf8_path path1n(file1n_abs_native.c_str());
    irs::utf8_path path2(file2_abs_native.c_str());
    irs::utf8_path dir1(pwd_native.c_str());
    irs::utf8_path dir2(dir_abs_native.c_str());
    bool tmpBool;
    std::time_t tmpTime;
    uint64_t tmpUint;

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1f.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1f.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1f.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1n.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1n.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1n.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(dir1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_FALSE(dir1.file_size(tmpUint));

    ASSERT_TRUE(dir2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dir2.mtime(tmpTime));
    ASSERT_FALSE(dir2.file_size(tmpUint));
  }

  // from utf8 string
  {
    irs::utf8_path path1(file1_abs_utf8);
    irs::utf8_path path1f(file1f_abs_utf8);
    irs::utf8_path path1n(file1n_abs_utf8);
    irs::utf8_path path2(file2_abs_utf8);
    irs::utf8_path dir1(pwd_utf8);
    irs::utf8_path dir2(dir_abs_utf8);
    bool tmpBool;
    std::time_t tmpTime;
    uint64_t tmpUint;

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1f.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1f.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1f.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1n.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1n.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1n.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(dir1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_FALSE(dir1.file_size(tmpUint));

    ASSERT_TRUE(dir2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dir2.mtime(tmpTime));
    ASSERT_FALSE(dir2.file_size(tmpUint));
  }

  // from utf8 string_ref
  {
    irs::utf8_path path1(file1_abs_utf8.c_str());
    irs::utf8_path path1f(file1f_abs_utf8.c_str());
    irs::utf8_path path1n(file1n_abs_utf8.c_str());
    irs::utf8_path path2(file2_abs_utf8.c_str());
    irs::utf8_path dir1(pwd_utf8.c_str());
    irs::utf8_path dir2(dir_abs_utf8.c_str());
    bool tmpBool;
    std::time_t tmpTime;
    uint64_t tmpUint;

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1f.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1f.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1f.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1f.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path1n.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1n.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1n.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1n.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(dir1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dir1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_FALSE(dir1.file_size(tmpUint));

    ASSERT_TRUE(dir2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dir2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dir2.mtime(tmpTime));
    ASSERT_FALSE(dir2.file_size(tmpUint));
  }
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
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  path/=file1;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  std::string data("data");
  std::ofstream out1(file1.c_str());
  out1 << data;
  out1.close();
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_TRUE(path.file_size(tmpUint) && tmpUint == data.size());

  ASSERT_FALSE(path.mkdir());

  path+=suffix;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  std::ofstream out2(file2.c_str());
  out2 << data << data;
  out2.close();
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
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
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  path/=directory;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));

  ASSERT_TRUE(path.mkdir());
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));
}

TEST_F(utf8_path_tests, visit) {
  irs::utf8_path path;
  std::string data("data");
  std::string file1("deleteme.file1");
  std::string file2("deleteme.file2");
  std::string directory("deleteme.dir");
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;
  size_t actual_count = 0;
  auto visit_max = irs::integer_traits<size_t>::max();
  auto visitor = [&actual_count, &visit_max](const file_path_t name)->bool {
    ++actual_count;

    return --visit_max;
  };

  // create file
  {
    std::ofstream out1(file1.c_str());
    std::ofstream out2(file2.c_str());
    out1 << data;
    out2 << data << data;
    out1.close();
    out2.close();
  }

  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));
  ASSERT_FALSE(path.visit_directory(visitor));

  path = irs::utf8_path(true);
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));
  actual_count = 0;
  visit_max = irs::integer_traits<size_t>::max();
  ASSERT_TRUE(path.visit_directory(visitor) && actual_count > 1);
  actual_count = 0;
  visit_max = 1;
  ASSERT_TRUE(path.visit_directory(visitor) && actual_count == 1);

  path/=directory;
  ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_FALSE(path.mtime(tmpTime));
  ASSERT_FALSE(path.file_size(tmpUint));
  ASSERT_FALSE(path.visit_directory(visitor));

  ASSERT_TRUE(path.mkdir());
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));
  actual_count = 0;
  visit_max = irs::integer_traits<size_t>::max();
  ASSERT_TRUE(path.visit_directory(visitor, false) && actual_count == 0);

  // create file
  {
    irs::utf8_path filepath = path;
    filepath /= file1;
    std::ofstream out(filepath.utf8().c_str());
    out << data << data << data;
    out.close();
  }

  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_FALSE(path.file_size(tmpUint));
  actual_count = 0;
  visit_max = irs::integer_traits<size_t>::max();
  ASSERT_TRUE(path.visit_directory(visitor, false) && actual_count > 0);

  path /= file1;
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime));
  ASSERT_TRUE(path.file_size(tmpUint));
  ASSERT_FALSE(path.visit_directory(visitor));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------