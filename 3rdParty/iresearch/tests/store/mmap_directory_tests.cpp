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

#include "tests_shared.hpp"

#ifdef _WIN32
  #include <boost/locale/encoding.hpp> // for boost::locale::conv::utf_to_utf()
#endif

#include "directory_test_case.hpp"
#include "store/mmap_directory.hpp"
#include "utils/locale_utils.hpp"

#include <fstream>

class mmap_directory_test : public directory_test_case,
  public test_base {
  public:
  explicit mmap_directory_test(const std::string& name = "directory") {
    static auto utf8_locale = iresearch::locale_utils::locale("", true);

    test_base::SetUp();
    codecvt_ = &std::use_facet<boost::filesystem::path::codecvt_type>(utf8_locale);
    path_ = test_case_dir();
#ifdef _WIN32
    // convert utf8->ucs2
    auto native_name = boost::locale::conv::utf_to_utf<wchar_t>(
      name.c_str(), name.c_str() + name.size()
    );
    path_.append(std::move(native_name));
#else
    path_.append(name);
#endif
  }

  void check_files() {
    const std::string file_name = "abcd";

    // create empty file
    {
      auto file = path_;
      file.append(file_name);
      std::ofstream f(file.native());
    }

    // read files from directory
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_TRUE(dir_->visit(list_files));
    ASSERT_EQ(1, files.size());
    ASSERT_EQ(file_name, files[0]);
  }

  virtual void SetUp() override {
    auto str = path_.string(*codecvt_);

    dir_ = irs::directory::make<irs::mmap_directory>(str);
    iresearch::mmap_directory::remove_directory(str);
    iresearch::mmap_directory::create_directory(str);
  }

  virtual void TearDown() {
    iresearch::mmap_directory::remove_directory(path_.string(*codecvt_));
  }

  virtual void TestBody() {}

  const boost::filesystem::path& path() const {
    return path_;
  }

  private:
  const boost::filesystem::path::codecvt_type* codecvt_;
  boost::filesystem::path path_;
};

TEST_F(mmap_directory_test, read_multiple_streams) {
  read_multiple_streams();
}

TEST_F(mmap_directory_test, string_read_write) {
  string_read_write();
}

TEST_F(mmap_directory_test, smoke_store) {
  smoke_store();
}

TEST_F(mmap_directory_test, list) {
  list();
}

TEST_F(mmap_directory_test, visit) {
  visit();
}

TEST_F(mmap_directory_test, index_io) {
  smoke_index_io();
}

TEST_F(mmap_directory_test, lock_obtain_release) {
  lock_obtain_release();
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------