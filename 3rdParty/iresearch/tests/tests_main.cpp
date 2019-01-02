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
  #pragma warning(disable: 4101)
  #pragma warning(disable: 4267)
#endif

  #include <cmdline.h>

#if defined(_MSC_VER)
  #pragma warning(default: 4267)
  #pragma warning(default: 4101)
#endif

#if !defined(_MSC_VER)
  #include <dlfcn.h> // for RTLD_NEXT
#endif

#include <signal.h> // for signal(...)/raise(...)

#if defined(_MSC_VER)
  #pragma warning(disable: 4229)
#endif

  #include <unicode/uclean.h> // for u_cleanup

#if defined(_MSC_VER)
  #pragma warning(default: 4229)
#endif

#include "tests_shared.hpp"
#include "tests_config.hpp"

#include <ctime>
#include <formats/formats.hpp>

#include <utils/attributes.hpp>

#include "index/doc_generator.hpp"
#include "utils/file_utils.hpp"
#include "utils/log.hpp"
#include "utils/network_utils.hpp"
#include "utils/bitset.hpp"
#include "utils/runtime_utils.hpp"

#ifdef _MSC_VER
  // +1 for \0 at end of string
  #define mkdtemp(templ) 0 == _mkdir(0 == _mktemp_s(templ, strlen(templ) + 1) ? templ : nullptr) ? templ : nullptr
#endif

/* -------------------------------------------------------------------
* iteration_tracker
* ------------------------------------------------------------------*/

struct iteration_tracker : ::testing::Environment {
  virtual void SetUp() {
    ++iteration;
  }

  static uint32_t iteration;
};

uint32_t iteration_tracker::iteration = (std::numeric_limits<uint32_t>::max)();

/* -------------------------------------------------------------------
* test_base
* ------------------------------------------------------------------*/

const std::string IRES_HELP("help");
const std::string IRES_LOG_LEVEL("ires_log_level");
const std::string IRES_LOG_STACK("ires_log_stack");
const std::string IRES_OUTPUT("ires_output");
const std::string IRES_OUTPUT_PATH("ires_output_path");
const std::string IRES_RESOURCE_DIR("ires_resource_dir");

const std::string test_base::test_results("test_detail.xml");

irs::utf8_path test_base::exec_path_;
irs::utf8_path test_base::exec_dir_;
irs::utf8_path test_base::exec_file_;
irs::utf8_path test_base::out_dir_;
irs::utf8_path test_base::resource_dir_;
irs::utf8_path test_base::res_dir_;
irs::utf8_path test_base::res_path_;
std::string test_base::test_name_;
int test_base::argc_;
char** test_base::argv_;
decltype(test_base::argv_ires_output_) test_base::argv_ires_output_;

uint32_t test_base::iteration() {
  return iteration_tracker::iteration;
}

std::string test_base::resource( const std::string& name ) {
  auto path = resource_dir_;

  path /= name;

  return path.utf8();
}

void test_base::SetUp() {
  namespace tst = ::testing;
  const tst::TestInfo* ti = tst::UnitTest::GetInstance()->current_test_info();

  test_case_dir_ = res_dir_;

  if (::testing::FLAGS_gtest_repeat > 1 || ::testing::FLAGS_gtest_repeat < 0) {
    test_case_dir_ /=
      std::string("iteration ").append(std::to_string(iteration()));
  }

  test_case_dir_ /= ti->test_case_name();
  (test_dir_ = test_case_dir_) /= ti->name();
  test_dir_.mkdir();
}

void test_base::prepare(const cmdline::parser& parser) {
  if (parser.exist(IRES_HELP)) {
    return;
  }

  make_directories();

  {
    auto log_level = parser.get<irs::logger::level_t>(IRES_LOG_LEVEL);

    irs::logger::output_le(log_level, stderr); // set desired log level

    if (parser.get<bool>(IRES_LOG_STACK)) {
      irs::logger::stack_trace_level(log_level); // force enable stack trace
    }
  }

  if (parser.exist(IRES_OUTPUT)) {
    std::unique_ptr<char*[]> argv(new char*[2 + argc_]);
    std::memcpy(argv.get(), argv_, sizeof(char*)*(argc_));    
    argv_ires_output_.append("--gtest_output=xml:").append(res_path_.utf8());
    argv[argc_++] = &argv_ires_output_[0];

    /* let last argv equals to nullptr */
    argv[argc_] = nullptr;
    argv_ = argv.release();
  }
}

void test_base::make_directories() {
  irs::utf8_path exec_path(argv_[0]);
  auto exec_native = exec_path.native();
  auto path_parts = irs::file_utils::path_parts(&exec_native[0]);

  exec_path_ = exec_path;
  exec_file_ = path_parts.basename;
  exec_dir_ = path_parts.dirname;
  test_name_ = irs::utf8_path(path_parts.stem).utf8();

  if (out_dir_.native().empty()) {
    out_dir_ = exec_dir_;
  }

  std::cout << "launching: " << exec_path_.utf8() << std::endl;
  std::cout << "options:" << std::endl;
  std::cout << "\t" << IRES_OUTPUT_PATH << ": " << out_dir_.utf8() << std::endl;
  std::cout << "\t" << IRES_RESOURCE_DIR << ": " << resource_dir_.utf8() << std::endl;

  out_dir_ = out_dir_.utf8_absolute();
  (res_dir_ = out_dir_) /= test_name_;

  // add timestamp to res_dir_
  {
    std::tm tinfo;

    if (iresearch::localtime(tinfo, std::time(nullptr))) {
      char buf[21]{};

      strftime(buf, sizeof buf, "_%Y_%m_%d_%H_%M_%S", &tinfo);
      res_dir_ += irs::string_ref(buf, sizeof buf - 1);
    } else {
      res_dir_ += "_unknown";
    }
  }

  // add temporary string to res_dir_
  {
    char templ[] = "_XXXXXX";

    res_dir_ += irs::string_ref(templ, sizeof templ - 1);
  }

  auto res_dir_templ = res_dir_.utf8();

  res_dir_ = mkdtemp(&(res_dir_templ[0]));
  (res_path_ = res_dir_) /= test_results;
}

void test_base::parse_command_line(cmdline::parser& cmd) {
  static const auto log_level_reader = [](const std::string &s)->irs::logger::level_t {
    static const std::map<std::string, irs::logger::level_t> log_levels = {
      { "FATAL", irs::logger::level_t::IRL_FATAL },
      { "ERROR", irs::logger::level_t::IRL_ERROR },
      { "WARN", irs::logger::level_t::IRL_WARN },
      { "INFO", irs::logger::level_t::IRL_INFO },
      { "DEBUG", irs::logger::level_t::IRL_DEBUG },
      { "TRACE", irs::logger::level_t::IRL_TRACE },
    };
    auto itr = log_levels.find(s);

    if (itr == log_levels.end()) {
      throw std::invalid_argument(s);
    }

    return itr->second;
  };

  cmd.add(IRES_HELP, '?', "print this message");
  cmd.add(IRES_LOG_LEVEL, 0, "threshold log level <FATAL|ERROR|WARN|INFO|DEBUG|TRACE>", false, irs::logger::level_t::IRL_FATAL, log_level_reader);
  cmd.add(IRES_LOG_STACK, 0, "always log stack trace", false, false);
  cmd.add(IRES_OUTPUT, 0, "generate an XML report");
  cmd.add(IRES_OUTPUT_PATH, 0, "output directory", false, out_dir_.utf8());
  cmd.add(IRES_RESOURCE_DIR, 0, "resource directory", false, irs::utf8_path(IResearch_test_resource_dir).utf8());

  if (!cmd.parse(argc_, argv_)) {
    std::cout << cmd.error_full() << std::endl;
    std::cout << cmd.usage() << std::endl;
    exit(1);
  }

  if (cmd.exist(IRES_HELP)) {
    std::cout << cmd.usage() << std::endl;
    return;
  }

  resource_dir_ = cmd.get<std::string>(IRES_RESOURCE_DIR);
  out_dir_ = cmd.get<std::string>(IRES_OUTPUT_PATH);
}

int test_base::initialize(int argc, char* argv[]) {
  argc_ = argc;
  argv_ = argv;
  ::testing::AddGlobalTestEnvironment(new iteration_tracker());
  ::testing::InitGoogleTest(&argc_, argv_);

  cmdline::parser cmd;

  parse_command_line(cmd);
  prepare(cmd);

  return RUN_ALL_TESTS();
}

void stack_trace_handler(int sig) {
  // reset to default handler
  signal(sig, SIG_DFL);
  // print stack trace
  iresearch::logger::stack_trace(iresearch::logger::IRL_FATAL); // IRL_FATAL is logged to stderr above
  // re-signal to default handler (so we still get core dump if needed...)
  raise(sig);
}

void install_stack_trace_handler() {
  signal(SIGILL, stack_trace_handler);
  signal(SIGSEGV, stack_trace_handler);
  signal(SIGABRT, stack_trace_handler);

  #ifndef _MSC_VER
    signal(SIGBUS, stack_trace_handler);
  #endif
}

#ifndef _MSC_VER
  // override GCC 'throw' handler to print stack trace before throw
  extern "C" {
    #if defined(__APPLE__)
      void __cxa_throw(void* ex, struct std::type_info* info, void(*dest)(void *)) {
        static void(*rethrow)(void*,struct std::type_info*,void(*)(void*)) =
          (void(*)(void*,struct std::type_info*,void(*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");

        IR_FRMT_DEBUG("exception type: %s", reinterpret_cast<const std::type_info*>(info)->name());
        IR_LOG_STACK_TRACE();
        rethrow(ex, info, dest);
        abort(); // otherwise MacOS reports "function declared 'noreturn' should not return"
      }
    #else
      void __cxa_throw(void* ex, void* info, void(*dest)(void*)) {
        static void(*rethrow)(void*,void*,void(*)(void*)) __attribute__ ((noreturn)) =
          (void(*)(void*,void*,void(*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");

        IR_FRMT_DEBUG("exception type: %s", reinterpret_cast<const std::type_info*>(info)->name());
        IR_LOG_STACK_TRACE();
        rethrow(ex, info, dest);
      }
    #endif
  }
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

int main( int argc, char* argv[] ) {
  install_stack_trace_handler();

  const int code = test_base::initialize( argc, argv );

  std::cout << "Path to test result directory: " 
            << test_base::test_results_dir().utf8()
            << std::endl;

  u_cleanup(); // cleanup ICU resources

  return code;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
