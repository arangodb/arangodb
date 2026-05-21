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

#pragma once

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <memory>

#include "resource_manager.hpp"
#include "shared.hpp"

#define SOURCE_LOCATION (__FILE__ ":" IRS_TO_STRING(__LINE__))

namespace cmdline {
class parser;
}  // namespace cmdline

class test_env {
 public:
  static const std::string test_results;

  static int initialize(int argc, char* argv[]);
  static const std::filesystem::path& exec_path() { return exec_path_; }
  static const std::filesystem::path& exec_dir() { return exec_dir_; }
  static const std::filesystem::path& exec_file() { return exec_file_; }
  static const std::filesystem::path& resource_dir() { return resource_dir_; }
  static const std::filesystem::path& test_results_dir() { return res_dir_; }

  // returns path to resource with the specified name
  static std::filesystem::path resource(const std::string& name);

  static uint32_t iteration();

 private:
  static void make_directories();
  static void parse_command_line(cmdline::parser& vm);
  static bool prepare(const cmdline::parser& vm);

  static int argc_;
  static char** argv_;
  static std::string argv_ires_output_;     // argv_ for ires_output
  static std::string test_name_;            // name of the current test //
  static std::filesystem::path exec_path_;  // path where executable resides
  static std::filesystem::path exec_dir_;  // directory where executable resides
  static std::filesystem::path exec_file_;  // executable file name
  // output directory, default: exec_dir_
  static std::filesystem::path out_dir_;

  // TODO: set path from CMAKE!!!
  static std::filesystem::path resource_dir_;  // resource directory

  static std::filesystem::path
    res_dir_;  // output_dir_/test_name_YYYY_mm_dd_HH_mm_ss_XXXXXX
  static std::filesystem::path res_path_;  // res_dir_/test_detail.xml
};

struct SimpleMemoryAccounter : public irs::IResourceManager {
  void Increase(size_t value) override {
    counter_ += value;
    if (!result_) {
      throw std::runtime_error{"SimpleMemoryAccounter"};
    }
  }

  void Decrease(size_t value) noexcept override { counter_ -= value; }

  size_t counter_{0};
  bool result_{true};
};

struct MaxMemoryCounter final : irs::IResourceManager {
  void Reset() noexcept {
    current = 0;
    max = 0;
  }

  void Increase(size_t value) final {
    current += value;
    max = std::max(max, current);
  }

  void Decrease(size_t value) noexcept final { current -= value; }

  size_t current{0};
  size_t max{0};
};

struct TestResourceManager {
  SimpleMemoryAccounter cached_columns;
  SimpleMemoryAccounter consolidations;
  SimpleMemoryAccounter file_descriptors;
  SimpleMemoryAccounter readers;
  SimpleMemoryAccounter transactions;

  irs::ResourceManagementOptions options{.transactions = &transactions,
                                         .readers = &readers,
                                         .consolidations = &consolidations,
                                         .file_descriptors = &file_descriptors,
                                         .cached_columns = &cached_columns};
};

class test_base : public test_env, public ::testing::Test {
 public:
  const std::filesystem::path& test_dir() const { return test_dir_; }
  const std::filesystem::path& test_case_dir() const { return test_case_dir_; }
  TestResourceManager& GetResourceManager() const noexcept {
    return resource_manager_;
  }

 protected:
  test_base() = default;
  void SetUp() override;
  void TearDown() override;

 private:
  std::filesystem::path test_dir_;       // res_dir_/<test-name>
  std::filesystem::path test_case_dir_;  // test_dir/<test-case-name>
  mutable TestResourceManager resource_manager_;
};

template<typename T>
class test_param_base : public test_base,
                        public ::testing::WithParamInterface<T> {};
