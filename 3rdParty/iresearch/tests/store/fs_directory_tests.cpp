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

#include "store/fs_directory.hpp"
#include "utils/process_utils.hpp"
#include "utils/network_utils.hpp"

#include <fstream>

#ifndef _WIN32
#include <sys/file.h>
#endif

using namespace iresearch;

class fs_directory_test : public directory_test_case,
  public test_base {
  public:
  explicit fs_directory_test(const std::string& name = "directory") {
    test_base::SetUp();
    path_ = test_case_dir();
    path_ /= name;
  }

  void check_files() {
    const std::string file_name = "abcd";

    // create empty file
    {
      auto file = path_;

      file /= file_name;

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
    dir_ = directory::make<fs_directory>(path_.utf8());
    path_.remove();
    path_.mkdir();
  }

  virtual void TearDown() override {
    path_.remove();
  }

  virtual void TestBody() override {}

  const irs::utf8_path& path() const {
    return path_;
  }

  private:
  irs::utf8_path path_;
};

TEST_F(fs_directory_test, read_multiple_streams) {
  read_multiple_streams();
}

TEST_F(fs_directory_test, string_read_write) {
  string_read_write();
}

TEST_F(fs_directory_test, smoke_store) {
  smoke_store();
}

TEST_F(fs_directory_test, list) {
  list();
}

TEST_F(fs_directory_test, visit) {
  visit();
}

TEST_F(fs_directory_test, index_io) {
  smoke_index_io();
}

TEST_F(fs_directory_test, lock_obtain_release) {
  lock_obtain_release();
}

TEST_F(fs_directory_test, orphaned_lock) {
  // orhpaned lock file with orphaned pid, same hostname
  {
    // create lock file
    {
      char hostname[256] = {};
      ASSERT_EQ(0, get_host_name(hostname, sizeof hostname));

      const std::string pid = std::to_string(integer_traits<int>::const_max);
      auto out = dir_->create("lock");
      ASSERT_FALSE(!out);
      out->write_bytes(reinterpret_cast<const byte_type*>(hostname), strlen(hostname));
      out->write_byte(0);
      out->write_bytes(reinterpret_cast<const byte_type*>(pid.c_str()), pid.size());
    }

    // try to obtain lock
    {
      auto lock = dir_->make_lock("lock");
      ASSERT_FALSE(!lock);
      ASSERT_TRUE(lock->lock());
    }

    bool exists;
    ASSERT_TRUE(dir_->exists(exists, "lock") && !exists);
  }

  // orphaned lock file with invalid pid (not a number), same hostname
  {
    // create lock file
    {
      char hostname[256] = {};
      ASSERT_EQ(0, get_host_name(hostname, sizeof hostname));

      const std::string pid = "invalid_pid";
      auto out = dir_->create("lock");
      ASSERT_FALSE(!out);
      out->write_bytes(reinterpret_cast<const byte_type*>(hostname), strlen(hostname));
      out->write_byte(0);
      out->write_bytes(reinterpret_cast<const byte_type*>(pid.c_str()), pid.size());
    }

    // try to obtain lock
    {
      auto lock = dir_->make_lock("lock");
      ASSERT_FALSE(!lock);
      ASSERT_TRUE(lock->lock());
      ASSERT_TRUE(lock->unlock());
      bool exists;
      ASSERT_TRUE(dir_->exists(exists, "lock") && !exists);
    }
  }

  // orphaned empty lock file 
  {
    // create lock file
    {
      auto out = dir_->create("lock");
      ASSERT_FALSE(!out);
      out->flush();
    }

    // try to obtain lock
    {
      auto lock = dir_->make_lock("lock");
      ASSERT_FALSE(!lock);
      ASSERT_TRUE(lock->lock());
      ASSERT_TRUE(lock->unlock());
      bool exists;
      ASSERT_TRUE(dir_->exists(exists, "lock") && !exists);
    }
  }

  // orphaned lock file with hostname only
  {
    // create lock file
    {
      char hostname[256] = {};
      ASSERT_EQ(0, get_host_name(hostname, sizeof hostname));
      auto out = dir_->create("lock");
      ASSERT_FALSE(!out);
      out->write_bytes(reinterpret_cast<const byte_type*>(hostname), strlen(hostname));
    }

    // try to obtain lock
    {
      auto lock = dir_->make_lock("lock");
      ASSERT_FALSE(!lock);
      ASSERT_TRUE(lock->lock());
      ASSERT_TRUE(lock->unlock());
      bool exists;
      ASSERT_TRUE(dir_->exists(exists, "lock") && !exists);
    }
  }

  // orphaned lock file with valid pid, same hostname
  {
    // create lock file
    {
      char hostname[256] = {};
      ASSERT_EQ(0, get_host_name(hostname, sizeof hostname));
      const std::string pid = std::to_string(iresearch::get_pid());
      auto out = dir_->create("lock");
      ASSERT_FALSE(!out);
      out->write_bytes(reinterpret_cast<const byte_type*>(hostname), strlen(hostname));
      out->write_byte(0);
      out->write_bytes(reinterpret_cast<const byte_type*>(pid.c_str()), pid.size());
    }

    bool exists;
    ASSERT_TRUE(dir_->exists(exists, "lock") && exists);

    // try to obtain lock, after closing stream
    auto lock = dir_->make_lock("lock");
    ASSERT_FALSE(!lock);
    ASSERT_FALSE(lock->lock()); // still have different hostname
  }

  // orphaned lock file with valid pid, different hostname
  {
    // create lock file
    {
      char hostname[] = "not_a_valid_hostname_//+&*(%$#@! }";

      const std::string pid = std::to_string(iresearch::get_pid());
      auto out = dir_->create("lock");
      ASSERT_FALSE(!out);
      out->write_bytes(reinterpret_cast<const byte_type*>(hostname), strlen(hostname));
      out->write_byte(0);
      out->write_bytes(reinterpret_cast<const byte_type*>(pid.c_str()), pid.size());
    }

    bool exists;
    ASSERT_TRUE(dir_->exists(exists, "lock") && exists);

    // try to obtain lock, after closing stream
    auto lock = dir_->make_lock("lock");
    ASSERT_FALSE(!lock);
    ASSERT_FALSE(lock->lock()); // still have different hostname
  }
}

TEST_F(fs_directory_test, utf8_chars) {
  std::wstring path_ucs2 = L"\u0442\u0435\u0441\u0442\u043E\u0432\u0430\u044F_\u0434\u0438\u0440\u0435\u043A\u0442\u043E\u0440\u0438\u044F";
  irs::utf8_path path(path_ucs2);
  fs_directory_test dir(path.utf8());

  // create directory via iResearch functions
  {
    dir.SetUp();
    dir.smoke_store();
    // Read files from directory
    dir.check_files();
    dir.TearDown();
  }

  // create directory via native functions
  {
    #ifdef _WIN32
      auto native_path = test_case_dir().native() + L'\\' + path.native();
      ASSERT_EQ(0, _wmkdir(native_path.c_str()));
    #else
      auto native_path = test_case_dir().native() + '/' + path.utf8();
      ASSERT_EQ(0, mkdir(native_path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO));
    #endif

    dir.smoke_store();
    // Read files from directory
    dir.check_files();
    dir.TearDown();
  }
}

TEST_F(fs_directory_test, directory_size) {
  directory_size();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------