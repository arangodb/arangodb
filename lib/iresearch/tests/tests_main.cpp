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

#if defined(_MSC_VER)
#pragma warning(disable : 4101)
#pragma warning(disable : 4267)
#endif

#include <cmdline.h>

#if defined(_MSC_VER)
#pragma warning(default : 4267)
#pragma warning(default : 4101)
#endif

#if !defined(_MSC_VER)
#include <dlfcn.h>  // for RTLD_NEXT
#endif

#include <signal.h>  // for signal(...)/raise(...)

#if defined(_MSC_VER)
#pragma warning(disable : 4229)
#endif

#include <unicode/uclean.h>  // for u_cleanup

#if defined(_MSC_VER)
#pragma warning(default : 4229)
#endif

#include <unicode/udata.h>

#include <analysis/minhash_token_stream.hpp>
#include <ctime>
#include <filesystem>
#include <formats/formats.hpp>
#include <utils/attributes.hpp>
#include <vector>

#include "index/doc_generator.hpp"
#include "tests_config.hpp"
#include "tests_shared.hpp"
#include "utils/bitset.hpp"
#include "utils/file_utils.hpp"
#include "utils/log.hpp"
#include "utils/mmap_utils.hpp"
#include "utils/network_utils.hpp"
#include "utils/runtime_utils.hpp"

#include <absl/strings/str_cat.h>

#ifdef _MSC_VER
// +1 for \0 at end of string
#define mkdtemp(templ)                                                    \
  0 == _mkdir(0 == _mktemp_s(templ, strlen(templ) + 1) ? templ : nullptr) \
    ? templ                                                               \
    : nullptr
#endif

namespace {

void AssertCallback(irs::SourceLocation&& source, std::string_view message) {
  FAIL() << source.file << ":" << source.line << ": " << source.func
         << ": Assert failed: " << message;
}

void LogCallback(irs::SourceLocation&& source, std::string_view message) {
  std::cerr << source.file << ":" << source.line << ": " << source.func << ": "
            << message << std::endl;
}

// Custom ICU data
irs::mmap_utils::mmap_handle icu_data{irs::IResourceManager::kNoop};

struct IterationTracker final : ::testing::Environment {
  static uint32_t sIteration;

  void SetUp() final { ++sIteration; }
};

uint32_t IterationTracker::sIteration = (std::numeric_limits<uint32_t>::max)();

const std::string IRES_HELP("help");
const std::string IRES_LOG_LEVEL("ires_log_level");
const std::string IRES_LOG_STACK("ires_log_stack");
const std::string IRES_OUTPUT("ires_output");
const std::string IRES_OUTPUT_PATH("ires_output_path");
const std::string IRES_RESOURCE_DIR("ires_resource_dir");
const std::string IRES_ICU_DATA("icu-data");

}  // namespace

const std::string test_env::test_results("test_detail.xml");

std::filesystem::path test_env::exec_path_;
std::filesystem::path test_env::exec_dir_;
std::filesystem::path test_env::exec_file_;
std::filesystem::path test_env::out_dir_;
std::filesystem::path test_env::resource_dir_;
std::filesystem::path test_env::res_dir_;
std::filesystem::path test_env::res_path_;
std::string test_env::test_name_;
int test_env::argc_;
char** test_env::argv_;
decltype(test_env::argv_ires_output_) test_env::argv_ires_output_;

uint32_t test_env::iteration() { return IterationTracker::sIteration; }

std::filesystem::path test_env::resource(const std::string& name) {
  return resource_dir_ / name;
}

bool test_env::prepare(const cmdline::parser& parser) {
  if (parser.exist(IRES_HELP)) {
    return true;
  }

  if (parser.exist(IRES_ICU_DATA)) {
    // icu initialize for data file
    std::filesystem::path icu_data_file_path{
      parser.get<std::string>(IRES_ICU_DATA)};
    IRS_LOG_INFO(absl::StrCat("Loading custom ICU data file: ",
                              icu_data_file_path.string()));
    bool data_exists{false};
    if (irs::file_utils::exists(data_exists, icu_data_file_path.c_str()) &&
        data_exists) {
      if (icu_data.open(icu_data_file_path.c_str())) {
        UErrorCode status{U_ZERO_ERROR};
        udata_setCommonData(icu_data.addr(), &status);
        if (!U_SUCCESS(status)) {
          IRS_LOG_FATAL(absl::StrCat(
            "Failed to set custom ICU data file with status: ", status));
          return false;
        }
      }
    } else {
      IRS_LOG_FATAL("Custom data file not found.");
      return false;
    }
  }

  make_directories();

  {
    auto log_level = parser.get<int>(IRES_LOG_LEVEL);

    for (auto level = static_cast<int>(irs::log::Level::kFatal);
         level <= log_level; ++level) {
      irs::log::SetCallback(static_cast<irs::log::Level>(level), LogCallback);
    }
  }

  if (parser.exist(IRES_OUTPUT)) {
    std::unique_ptr<char*[]> argv(new char*[2 + argc_]);
    std::memcpy(argv.get(), argv_, sizeof(char*) * (argc_));
    argv_ires_output_.append("--gtest_output=xml:").append(res_path_.string());
    argv[argc_++] = &argv_ires_output_[0];

    // let last argv equals to nullptr
    argv[argc_] = nullptr;
    argv_ = argv.release();
  }
  return true;
}

void test_env::make_directories() {
  std::filesystem::path exec_path(argv_[0]);
  auto path_parts = irs::file_utils::path_parts(exec_path.c_str());

  exec_path_ = exec_path;
  exec_file_ = std::filesystem::path{
    irs::basic_string_view<std::filesystem::path::value_type>(
      path_parts.basename)};
  exec_dir_ = std::filesystem::path{
    irs::basic_string_view<std::filesystem::path::value_type>(
      path_parts.dirname)};
  test_name_ =
    std::filesystem::path{
      irs::basic_string_view<std::filesystem::path::value_type>(
        path_parts.stem)}
      .string();

  if (out_dir_.native().empty()) {
    out_dir_ = exec_dir_;
  }

  std::cout << "launching: " << exec_path_.string() << std::endl;
  std::cout << "options:" << std::endl;
  std::cout << "\t" << IRES_OUTPUT_PATH << ": " << out_dir_.string()
            << std::endl;
  std::cout << "\t" << IRES_RESOURCE_DIR << ": " << resource_dir_.string()
            << std::endl;

  irs::file_utils::ensure_absolute(out_dir_);
  (res_dir_ = out_dir_) /= test_name_;

  // add timestamp to res_dir_
  {
    std::tm tinfo;

    if (irs::localtime(tinfo, std::time(nullptr))) {
      char buf[21]{};

      strftime(buf, sizeof buf, "_%Y_%m_%d_%H_%M_%S", &tinfo);
      res_dir_ += std::string_view{buf, sizeof buf - 1};
    } else {
      res_dir_ += "_unknown";
    }
  }

  // add temporary string to res_dir_
  {
    char templ[] = "_XXXXXX";

    res_dir_ += std::string_view{templ, sizeof templ - 1};
  }

  auto res_dir_templ = res_dir_.string();

  res_dir_ = mkdtemp(res_dir_templ.data());
  (res_path_ = res_dir_) /= test_results;
}

void test_env::parse_command_line(cmdline::parser& cmd) {
  static const auto log_level_reader = [](const std::string& s) -> int {
    static const std::map<std::string, irs::log::Level> log_levels = {
      {"FATAL", irs::log::Level::kFatal},  //
      {"ERROR", irs::log::Level::kError},  //
      {"WARN", irs::log::Level::kWarn},    //
      {"INFO", irs::log::Level::kInfo},    //
      {"DEBUG", irs::log::Level::kDebug},  //
      {"TRACE", irs::log::Level::kTrace},  //
    };
    auto itr = log_levels.find(s);

    if (itr == log_levels.end()) {
      throw std::invalid_argument(s);
    }

    return static_cast<int>(itr->second);
  };

  cmd.add(IRES_HELP, '?', "print this message");
  cmd.add(IRES_LOG_LEVEL, 0,
          "threshold log level <FATAL|ERROR|WARN|INFO|DEBUG|TRACE>", false,
          static_cast<int>(irs::log::Level::kFatal), log_level_reader);
  cmd.add(IRES_LOG_STACK, 0, "always log stack trace", false, false);
  cmd.add(IRES_OUTPUT, 0, "generate an XML report");
  cmd.add(IRES_OUTPUT_PATH, 0, "output directory", false, out_dir_.string());
  cmd.add(IRES_RESOURCE_DIR, 0, "resource directory", false,
          std::filesystem::path(IResearch_test_resource_dir).string());
  cmd.add(IRES_ICU_DATA, 0, "custom icu data file", false, std::string());
  cmd.parse(argc_, argv_);

  if (cmd.exist(IRES_HELP)) {
    std::cout << cmd.usage() << std::endl;
    return;
  }

  resource_dir_ =
    std::filesystem::path{cmd.get<std::string>(IRES_RESOURCE_DIR)};
  out_dir_ = std::filesystem::path{cmd.get<std::string>(IRES_OUTPUT_PATH)};
}

int test_env::initialize(int argc, char* argv[]) {
  argc_ = argc;
  argv_ = argv;

  cmdline::parser cmd;
  parse_command_line(cmd);
  if (!prepare(cmd)) {
    return -1;
  }

  ::testing::AddGlobalTestEnvironment(new IterationTracker());
  ::testing::InitGoogleTest(&argc_, argv_);

  irs::analysis::MinHashTokenStream::init();

  return RUN_ALL_TESTS();
}

void test_base::SetUp() {
  const auto* ti = ::testing::UnitTest::GetInstance()->current_test_info();

  if (!ti) {
    throw std::runtime_error("not a test suite");
  }

  test_case_dir_ = test_results_dir();

  if (::testing::FLAGS_gtest_repeat < 0 || 1 < ::testing::FLAGS_gtest_repeat) {
    test_case_dir_ /= absl::StrCat("iteration ", iteration());
  }

  test_case_dir_ /= ti->test_case_name();
  (test_dir_ = test_case_dir_) /= ti->name();
  std::filesystem::create_directories(test_dir_);
}

void test_base::TearDown() {
  if (!test_dir_.empty()) {
    auto path = test_dir_;
    if (!HasFailure()) {
      std::filesystem::remove_all(path);
    } else if (std::filesystem::is_empty(path)) {
      std::filesystem::remove(path);
    }
  }
}

int main(int argc, char* argv[]) {
  irs::assert::SetCallback(AssertCallback);

  const int code = test_env::initialize(argc, argv);

  std::cout << "Path to test result directory: "
            << test_env::test_results_dir().string() << std::endl;

  u_cleanup();  // cleanup ICU resources

  std::vector<std::filesystem::path> paths;
  for (const auto& entry : std::filesystem::recursive_directory_iterator{
         test_env::test_results_dir()}) {
    if (std::filesystem::is_empty(entry.path())) {
      paths.emplace_back(entry.path());
    }
  }
  for (auto& path : paths) {
    do {
      std::filesystem::remove(path);
      path = path.parent_path();
    } while (std::filesystem::is_empty(path));
  }

  return code;
}
