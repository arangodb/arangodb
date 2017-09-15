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

#include "store/fs_directory.hpp"
#include "utils/locale_utils.hpp"
#include "utils/file_utils.hpp"
#include "utils/process_utils.hpp"
#include "utils/network_utils.hpp"

#if defined (__GNUC__)
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <boost/locale.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <boost/locale/generator.hpp>
#include <boost/locale/conversion.hpp>

#include <fstream>

#ifndef _WIN32
#include <sys/file.h>
#endif

using namespace iresearch;

class fs_directory_test : public directory_test_case,
  public test_base {
  public:
  explicit fs_directory_test(const std::string& name = "directory") {
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

    dir_ = directory::make<fs_directory>(str);
    iresearch::fs_directory::remove_directory(str);
    iresearch::fs_directory::create_directory(str);
  }

  virtual void TearDown() {
    iresearch::fs_directory::remove_directory(path_.string(*codecvt_));
  }

  virtual void TestBody() {}

  const boost::filesystem::path& path() const {
    return path_;
  }

  private:
  const boost::filesystem::path::codecvt_type* codecvt_;
  boost::filesystem::path path_;
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
  auto path_utf8 = boost::locale::conv::utf_to_utf<char>(path_ucs2);
  string_ref path_utf8x((char*) path_ucs2.c_str(), path_ucs2.size() * 2);
  fs_directory_test dir(path_utf8);

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
      auto native_path = test_case_dir().native() + boost::filesystem::path::preferred_separator + path_ucs2;
      ASSERT_EQ(0, _wmkdir(native_path.c_str()));
    #else
      auto native_path = test_case_dir().native() + boost::filesystem::path::preferred_separator + path_utf8;
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
