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

#ifndef IRESEARCH_TESTS_SHARED_H
#define IRESEARCH_TESTS_SHARED_H

#include "gtest/gtest.h"

#include "../core/shared.hpp"
#include <memory>
#include <cstdio>

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  #include <boost/filesystem.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

NS_LOCAL
  namespace fs = boost::filesystem;
NS_END

NS_BEGIN(cmdline)
class parser;
NS_END // cmdline

inline void TODO_IMPLEMENT() {
  std::cerr << "\x1b[31mTODO: implement me" << std::endl;
}

class test_base : public ::testing::Test {
 public:
  static const std::string test_results;

  static int initialize( int argc, char* argv[] );
  static const fs::path& exec_path() { return exec_path_; }
  static const fs::path& exec_dir() { return exec_dir_; }
  static const fs::path& exec_file() { return exec_file_; }
  static const fs::path& resource_dir() { return resource_dir_; }
  static const fs::path& test_results_dir() { return res_dir_; }

  /* returns path to resource with the specified name */
  static std::string resource( const std::string& name );

  static std::string temp_file();

  static uint32_t iteration();
 
  const fs::path& test_dir() { return test_dir_; }
  const fs::path& test_case_dir() { return test_case_dir_; }

 protected:
  test_base() = default;
  virtual void SetUp() override;

 private:
  static void make_directories();
  static void parse_command_line(cmdline::parser& vm);
  static void prepare(const cmdline::parser& vm );

  static int argc_;
  static char** argv_;
  static std::string argv_ires_output_; /* argv_ for ires_output */
  static std::string test_name_; /* name of the current test */
  static fs::path exec_path_; /* path where executable resides */
  static fs::path exec_dir_; /* directory where executable resides */
  static fs::path exec_file_; /* executable file name */
  static fs::path out_dir_; /* output directory, default: exec_dir_ */

  //TODO: set path from CMAKE!!!
  static fs::path resource_dir_; /* resource directory */

  static fs::path res_dir_; /* output_dir_/test_name_YYYY_mm_dd_HH_mm_ss_XXXXXX */ 
  static fs::path res_path_; /* res_dir_/test_detail.xml */

  fs::path test_dir_; /* res_dir_/<test-name>*/
  fs::path test_case_dir_; /* test_dir/<test-case-name> */    
  bool artifacts_;
}; // test_base

// writes formatted report to the specified output stream
void flush_timers(std::ostream& out);

#endif