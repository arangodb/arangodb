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
#include <thread>
#include <condition_variable>

#include "tests_shared.hpp"
#include "utils/file_utils.hpp"
#include "utils/utf8_path.hpp"

using namespace std::chrono_literals;

namespace {

class utf8_path_tests: public test_base {
  irs::utf8_path cwd_;

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    test_base::SetUp();
    cwd_ = irs::utf8_path(true);
    test_dir().mkdir(); // ensure path exists
    test_dir().chdir(); // ensure all files/directories created in a valid place
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    cwd_.chdir();
    test_base::TearDown();
  }
};

}

TEST_F(utf8_path_tests, current) {
  // absolute path
  {
    irs::utf8_path path(true);
    std::string directory("deleteme");
    std::string directory2("deleteme2");
    bool tmpBool;
    std::time_t tmpTime;
    uint64_t tmpUint;

    #ifdef _WIN32
      wchar_t buf[_MAX_PATH];
      std::basic_string<wchar_t> current_dir(_wgetcwd(buf, _MAX_PATH));
      std::basic_string<wchar_t> prefix(L"\\\\?\\"); // prepended by chdir() and returned by win32
    #else
      char buf[PATH_MAX];
      std::basic_string<char> current_dir(getcwd(buf, PATH_MAX));
      std::basic_string<char> prefix;
    #endif

    ASSERT_TRUE(current_dir == (prefix + path.native()));
    ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path.file_size(tmpUint));

    path /= directory;
    ASSERT_TRUE(path.mkdir());
    ASSERT_TRUE(path.chdir());

    ASSERT_TRUE(path.native() == irs::utf8_path(true).native());
    ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path.file_size(tmpUint));

    path /= directory2;
    ASSERT_TRUE(path.mkdir());
    ASSERT_TRUE(path.chdir());

    ASSERT_TRUE(path.native() == irs::utf8_path(true).native());
    ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path.file_size(tmpUint));
  }

  // relative path
  {
    irs::utf8_path path;
    std::string directory("deleteme");
    std::string directory2("deleteme2");
    bool tmpBool;
    std::time_t tmpTime;
    uint64_t tmpUint;

    #ifdef _WIN32
      wchar_t buf[_MAX_PATH];
      std::basic_string<wchar_t> current_dir(_wgetcwd(buf, _MAX_PATH));
      std::basic_string<wchar_t> prefix(L"\\\\?\\"); // prepended by chdir() and returned by win32
    #else
      char buf[PATH_MAX];
      std::basic_string<char> current_dir(getcwd(buf, PATH_MAX));
      std::basic_string<char> prefix;
    #endif

    ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path.mtime(tmpTime));
    ASSERT_FALSE(path.file_size(tmpUint));

    path /= directory;
    ASSERT_TRUE(path.mkdir());

    ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path.file_size(tmpUint));

    path /= directory2;
    ASSERT_TRUE(path.mkdir());

    ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path.file_size(tmpUint));
  }
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

TEST_F(utf8_path_tests, absolute) {
  // empty
  {
    irs::utf8_path path;
    bool tmpBool;
    ASSERT_TRUE(path.absolute(tmpBool) && !tmpBool);
  }

  // cwd
  {
    irs::utf8_path path(true);
    bool tmpBool;
    ASSERT_TRUE(path.absolute(tmpBool) && tmpBool);
  }

  // relative
  {
    irs::utf8_path path;
    bool tmpBool;
    path += "deleteme";
    ASSERT_TRUE(path.absolute(tmpBool) && !tmpBool);
  }

  // absolute
  {
    irs::utf8_path cwd(true);
    irs::utf8_path path;
    bool tmpBool;
    path += cwd.native();
    ASSERT_TRUE(path.absolute(tmpBool) && tmpBool);
  }
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
    ASSERT_TRUE(dir1.file_size(tmpUint));

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
    ASSERT_TRUE(dir1.file_size(tmpUint));

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
    ASSERT_TRUE(dir1.file_size(tmpUint));

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
    ASSERT_TRUE(dir1.file_size(tmpUint));

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
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;

  // absolute path creation
  {
    irs::utf8_path path(true);
    std::string directory("deletemeA");

    ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path.file_size(tmpUint));

    path /= directory;
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
    ASSERT_TRUE(path.file_size(tmpUint));
  }

  // relative path creation
  {
    irs::utf8_path path;
    std::string directory("deletemeR");

    ASSERT_TRUE(path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path.mtime(tmpTime));
    ASSERT_FALSE(path.file_size(tmpUint));

    path /= directory;
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
    ASSERT_TRUE(path.file_size(tmpUint));
  }

  // recursive path creation (absolute)
  {
    std::string directory1("deleteme1");
    std::string directory2("deleteme2");
    irs::utf8_path path1(true);
    irs::utf8_path path2(true);

    path1 /= directory1;
    path2 /= directory1;
    path2 /= directory2;

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(path2.mkdir());

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path2.file_size(tmpUint));

    ASSERT_TRUE(path1.remove()); // recursive remove successful

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_FALSE(path2.remove()); // path already removed
  }

  // recursive path creation (relative)
  {
    std::string directory1("deleteme1");
    std::string directory2("deleteme2");
    irs::utf8_path path1;
    irs::utf8_path path2;

    path1 /= directory1;
    path2 /= directory1;
    path2 /= directory2;

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(path2.mkdir());

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path2.file_size(tmpUint));

    ASSERT_TRUE(path1.remove()); // recursive remove successful

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_FALSE(path2.remove()); // path already removed
  }

  // recursive path creation failure
  {
    std::string data("data");
    std::string directory("deleteme");
    std::string file("deleteme.file");
    irs::utf8_path path1;
    irs::utf8_path path2;

    path1 /= file;
    path2 /= file;
    path2 /= directory;

    // create file
    {
      std::ofstream out(file.c_str());
      out << data;
      out.close();
    }

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_FALSE(path2.mkdir());

    ASSERT_TRUE(path1.remove()); // file remove successful
    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));
  }

  // recursive multi-level path creation (absolute)
  {
    std::string directory1("deleteme1");
    std::string directory2("deleteme2/deleteme3"); // explicitly use '/' and not native
    irs::utf8_path path1(true);
    irs::utf8_path path2(true);

    path1 /= directory1;
    path2 /= directory1;
    path2 /= directory2;

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(path2.mkdir());

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path2.file_size(tmpUint));

    ASSERT_TRUE(path1.remove()); // recursive remove successful

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_FALSE(path2.remove()); // path already removed
  }

  // recursive multi-level path creation (relative)
  {
    std::string directory1("deleteme1");
    std::string directory2("deleteme2/deleteme3"); // explicitly use '/' and not native
    irs::utf8_path path1;
    irs::utf8_path path2;

    path1 /= directory1;
    path2 /= directory1;
    path2 /= directory2;

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_TRUE(path2.mkdir());

    ASSERT_TRUE(path1.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(path2.file_size(tmpUint));

    ASSERT_TRUE(path1.remove()); // recursive remove successful

    ASSERT_TRUE(path1.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path1.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path1.mtime(tmpTime));
    ASSERT_FALSE(path1.file_size(tmpUint));

    ASSERT_TRUE(path2.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(path2.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(path2.mtime(tmpTime));
    ASSERT_FALSE(path2.file_size(tmpUint));

    ASSERT_FALSE(path2.remove()); // path already removed
  }

  // recursive path creation with concurrency (full path exists)
  {
    std::string directory1("deleteme1/deleteme2/deleteme3");
    irs::utf8_path path1;
    irs::utf8_path path2;

    path1 /= directory1;
    path2 /= directory1;
   
    EXPECT_TRUE(path1.mkdir());
    EXPECT_FALSE(path2.mkdir()); // directory already exists 
    EXPECT_TRUE(path2.mkdir(false)); // directory exists, but creation is not mandatory

    ASSERT_TRUE(path1.remove());
    ASSERT_FALSE(path2.remove()); // path already removed
  }
  // recursive path creation with concurrency (only last sement added)
  {
    std::string directory1("deleteme1/deleteme2/deleteme3");
    std::string directory2("deleteme4");
    irs::utf8_path path1;
    irs::utf8_path path2;

    path1 /= directory1;
    path2 /= directory1;
    path2 /= directory2;

    ASSERT_TRUE(path1.mkdir());
    ASSERT_TRUE(path2.mkdir()); // last segment created

    ASSERT_TRUE(path1.remove());
    ASSERT_FALSE(path2.remove()); // path already removed
  }
  // race condition test inside path tree building
  {
    std::string directory1("deleteme1");
    std::string directory2("deleteme2/deleteme3/deleteme_thread");
    irs::utf8_path pathRoot;
    pathRoot /= directory1;

    // threads sync for start
    std::mutex mutex;
    std::condition_variable ready_cv;

    for (size_t j = 0; j < 3; ++j) {
      pathRoot.remove(); // make sure full path tree building always needed
      
      const auto thread_count = 20;
      std::vector<int> results(thread_count, false);
      std::vector<std::thread> pool;
      // We need all threads to be in same position to maximize test validity (not just ready to run!). 
      // So we count ready threads
      size_t readyCount = 0; 
      bool ready = false;  // flag to indicate all is ready (needed for spurious wakeup check)

      for (size_t i = 0; i < thread_count; ++i) {
        auto& result = results[i];
        pool.emplace_back(std::thread([ &result, &directory1, &directory2, i, &mutex, &ready_cv, &readyCount, &ready]() {
          irs::utf8_path path;
          path /= directory1;
          std::ostringstream ss;
          ss << directory2 << i;
          path /= ss.str();
          
          std::unique_lock<std::mutex> lk(mutex);
          ++readyCount;
          while (!ready) {
            ready_cv.wait(lk);
          }
          lk.unlock();
          result = path.mkdir(true) ? 1 : 0;
        }));
      }
     
      while(true) {
        {
          std::lock_guard<decltype(mutex)> lock(mutex);
          if (readyCount >= thread_count) { 
            // all threads on positions... go, go, go...
            ready = true;
            ready_cv.notify_all();
            break;
          }
        }
        std::this_thread::sleep_for(1000ms);
      }
      for (auto& thread : pool) {
        thread.join();
      }
     
      ASSERT_TRUE(std::all_of(
        results.begin(), results.end(), [](bool res) { return res != 0; }
      ));
      pathRoot.remove(); // cleanup
    }
  }


}

void validate_move(bool src_abs, bool dst_abs) {
  bool tmpBool;
  std::time_t tmpTime;
  uint64_t tmpUint;

  // non-existent -> non-existent/non-existent
  {
    std::string missing("deleteme");
    std::string src("deleteme.src");
    std::string dst("deleteme.dst0");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    dst_path/=missing;

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));
  }

  // non-existent -> directory/
  {
    std::string src("deleteme.src");
    std::string dst("deleteme.dst1");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/="";

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));
  }

  // non-existent -> directory/non-existent
  {
    std::string missing("deleteme");
    std::string src("deleteme.src");
    std::string dst("deleteme.dst2");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=missing;

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));
  }

  // non-existent -> directory/file
  {
    std::string file("deleteme.file");
    std::string src("deleteme.src");
    std::string dst("deleteme.dst3");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    dst_path/=file;

    // create file
    {
      std::string data("data");
      std::ofstream out(dst_path.c_str());
      out << data;
      out.close();
    }

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));
  }

  // non-existent -> directory/directory
  {
    std::string directory("deleteme");
    std::string src("deleteme.src");
    std::string dst("deleteme.dst4");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    dst_path/=directory;
    ASSERT_TRUE(dst_path.mkdir());

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));
  }

  // directory -> non-existent/non-existent
  {
    std::string missing("deleteme");
    std::string src("deleteme.src5");
    std::string dst("deleteme.dst5");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    dst_path/=missing;

    ASSERT_TRUE(src_path.mkdir());

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));
  }

  // directory -> directory/
  {
    std::string src("deleteme.src6");
    std::string dst("deleteme.dst6");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);
    irs::utf8_path dst_path_expected(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/="";
    dst_path_expected/=dst;
    dst_path_expected/=src;

    ASSERT_TRUE(src_path.mkdir());

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path_expected.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path_expected.mtime(tmpTime));
    ASSERT_FALSE(dst_path_expected.file_size(tmpUint));

#ifdef _WIN32
    // Boost fails to rename on win32
    ASSERT_FALSE(src_path.rename(dst_path));
#else
    ASSERT_TRUE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path_expected.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path_expected.mtime(tmpTime));
    ASSERT_FALSE(dst_path_expected.file_size(tmpUint));
#endif
  }

  // directory -> directory/non-existent
  {
    std::string missing("deleteme");
    std::string src("deleteme.src7");
    std::string dst("deleteme.dst7");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=missing;

    ASSERT_TRUE(src_path.mkdir());

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_TRUE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));
  }

  // directory -> directory/file
  {
    std::string file("deleteme");
    std::string src("deleteme.src8");
    std::string dst("deleteme.dst8");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);
    std::string dst_data("data");

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=file;

    ASSERT_TRUE(src_path.mkdir());

    // create file
    {
      std::ofstream out(dst_path.c_str());
      out << dst_data;
      out.close();
    }

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint) && tmpUint == dst_data.size());

#ifdef _WIN32
    // Boost forces overwrite on win32
    ASSERT_TRUE(src_path.rename(dst_path));
#else
    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint) && tmpUint == dst_data.size());
#endif
  }

  // directory -> directory/directory
  {
    std::string src_dir("deleteme.src");
    std::string dst_dir("deleteme.dst");
    std::string src("deleteme.src9");
    std::string dst("deleteme.dst9");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);
    irs::utf8_path src_path_expected(src_abs);
    irs::utf8_path dst_path_expected(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=dst_dir;
    src_path_expected/=src;
    src_path_expected/=src_dir;
    src_path_expected/=src_dir; // another nested directory
    dst_path_expected/=dst;
    dst_path_expected/=dst_dir;
    dst_path_expected/=src_dir; // expected another nested directory from src

    ASSERT_TRUE(src_path.mkdir());
    ASSERT_TRUE(dst_path.mkdir());
    ASSERT_TRUE(src_path_expected.mkdir());

    ASSERT_TRUE(src_path_expected.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path_expected.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path_expected.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path_expected.file_size(tmpUint));

    ASSERT_TRUE(dst_path_expected.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path_expected.mtime(tmpTime));
    ASSERT_FALSE(dst_path_expected.file_size(tmpUint));

#ifdef _WIN32
    ASSERT_FALSE(src_path.rename(dst_path));
#else
    // Boost on Posix merges directories
    ASSERT_TRUE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path_expected.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path_expected.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path_expected.mtime(tmpTime));
    ASSERT_FALSE(src_path_expected.file_size(tmpUint));

    ASSERT_TRUE(dst_path_expected.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path_expected.file_size(tmpUint));
#endif
  }

  // file -> non-existent/non-existent
  {
    std::string data("ABCdata123");
    std::string missing("deleteme");
    std::string src("deleteme.srcA");
    std::string dst("deleteme.dstA");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    dst_path/=missing;

    // create file
    {
      std::ofstream out(src_path.c_str());
      out << data;
      out.close();
    }

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));
  }

  // file -> directory/
  {
    std::string data("ABCdata123");
    std::string src("deleteme.srcB");
    std::string dst("deleteme.dstB");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);
    irs::utf8_path dst_path_expected(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/="";
    dst_path_expected/=dst;
    dst_path_expected/=src;

    // create file
    {
      std::ofstream out(src_path.c_str());
      out << data;
      out.close();
    }

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path_expected.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path_expected.mtime(tmpTime));
    ASSERT_FALSE(dst_path_expected.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path_expected.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path_expected.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path_expected.mtime(tmpTime));
    ASSERT_FALSE(dst_path_expected.file_size(tmpUint));
  }

  // file -> directory/non-existent
  {
    std::string data("ABCdata123");
    std::string missing("deleteme");
    std::string src("deleteme.srcC");
    std::string dst("deleteme.dstC");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=missing;

    // create file
    {
      std::ofstream out(src_path.c_str());
      out << data;
      out.close();
    }

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(dst_path.mtime(tmpTime));
    ASSERT_FALSE(dst_path.file_size(tmpUint));

    ASSERT_TRUE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint) && tmpUint == data.size());
  }

  // file -> directory/file
  {
    std::string src_data("ABCdata123");
    std::string dst_data("XyZ");
    std::string file("deleteme");
    std::string src("deleteme.srcD");
    std::string dst("deleteme.dstD");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=file;

    // create file
    {
      std::ofstream out(src_path.c_str());
      out << src_data;
      out.close();
    }

    // create file
    {
      std::ofstream out(dst_path.c_str());
      out << dst_data;
      out.close();
    }

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == src_data.size());

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint) && tmpUint == dst_data.size());

    ASSERT_TRUE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_FALSE(src_path.mtime(tmpTime));
    ASSERT_FALSE(src_path.file_size(tmpUint));

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint) && tmpUint == src_data.size());
  }

  // file -> directory/directory
  {
    std::string data("ABCdata123");
    std::string file("deleteme");
    std::string src("deleteme.srcE");
    std::string dst("deleteme.dstE");
    irs::utf8_path src_path(src_abs);
    irs::utf8_path dst_path(dst_abs);

    src_path/=src;
    dst_path/=dst;
    ASSERT_TRUE(dst_path.mkdir());
    dst_path/=file;

    // create file
    {
      std::ofstream out(src_path.c_str());
      out << data;
      out.close();
    }

    ASSERT_TRUE(dst_path.mkdir());

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));

    ASSERT_FALSE(src_path.rename(dst_path));

    ASSERT_TRUE(src_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.exists_directory(tmpBool) && !tmpBool);
    ASSERT_TRUE(src_path.exists_file(tmpBool) && tmpBool);
    ASSERT_TRUE(src_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(src_path.file_size(tmpUint) && tmpUint == data.size());

    ASSERT_TRUE(dst_path.exists(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_directory(tmpBool) && tmpBool);
    ASSERT_TRUE(dst_path.exists_file(tmpBool) && !tmpBool);
    ASSERT_TRUE(dst_path.mtime(tmpTime) && tmpTime > 0);
    ASSERT_TRUE(dst_path.file_size(tmpUint));
  }
}

TEST_F(utf8_path_tests, move_absolute_absolute) {
  validate_move(true, true);
}

TEST_F(utf8_path_tests, move_absolute_relative) {
  validate_move(true, false);
}

TEST_F(utf8_path_tests, move_relative_absolute) {
  validate_move(false, true);
}

TEST_F(utf8_path_tests, move_relative_relative) {
  validate_move(false, false);
}

TEST_F(utf8_path_tests, utf8_absolute) {
  // relative -> absolute
  {
    std::string directory("deleteme");
    irs::utf8_path expected(true);
    irs::utf8_path path;
    bool tmpBool;

    expected /= directory;
    path /= directory;

    ASSERT_TRUE(expected.absolute(tmpBool) && tmpBool); // tests below assume expected is absolute
    ASSERT_TRUE(path.absolute(tmpBool) && !tmpBool);
    ASSERT_TRUE(expected.utf8() == path.utf8_absolute());
    ASSERT_TRUE(irs::utf8_path(path.utf8_absolute()).absolute(tmpBool) && tmpBool);
  }

  // absolute -> absolute
  {
    std::basic_string<std::remove_pointer<file_path_t>::type> expected;
    irs::utf8_path path(true);
    bool tmpBool;

    ASSERT_TRUE(irs::file_utils::read_cwd(expected));
    ASSERT_TRUE(path.absolute(tmpBool) && tmpBool);
    ASSERT_TRUE(irs::utf8_path(expected).utf8() == path.utf8_absolute());
    ASSERT_TRUE(path.utf8() == path.utf8_absolute());
  }
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
  auto visit_max = std::numeric_limits<size_t>::max();
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
  ASSERT_TRUE(path.file_size(tmpUint));
  actual_count = 0;
  visit_max = std::numeric_limits<size_t>::max();
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
  ASSERT_TRUE(path.file_size(tmpUint));
  actual_count = 0;
  visit_max = std::numeric_limits<size_t>::max();
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
  ASSERT_TRUE(path.file_size(tmpUint));
  actual_count = 0;
  visit_max = std::numeric_limits<size_t>::max();
  ASSERT_TRUE(path.visit_directory(visitor, false) && actual_count > 0);

  path /= file1;
  ASSERT_TRUE(path.exists(tmpBool) && tmpBool);
  ASSERT_TRUE(path.exists_directory(tmpBool) && !tmpBool);
  ASSERT_TRUE(path.exists_file(tmpBool) && tmpBool);
  ASSERT_TRUE(path.mtime(tmpTime) && tmpTime > 0);
  ASSERT_TRUE(path.file_size(tmpUint));
  ASSERT_FALSE(path.visit_directory(visitor));
}
