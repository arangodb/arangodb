////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include <cstdlib>
#include <exception>

#include "utils/assert.hpp"
#include "utils/source_location.hpp"
#if defined(_MSC_VER)
#pragma warning(disable : 4101)
#pragma warning(disable : 4267)
#endif

#include <cmdline.h>

#if defined(_MSC_VER)
#pragma warning(default : 4267)
#pragma warning(default : 4101)
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4229)
#endif

#include <unicode/uclean.h>  // for u_cleanup

#if defined(_MSC_VER)
#pragma warning(default : 4229)
#endif

#include <functional>
#include <iostream>

#include "index-put.hpp"
#include "index-search.hpp"
#include "utils/log.hpp"
#include "utils/misc.hpp"
#include "utils/timer_utils.hpp"

#include <absl/container/flat_hash_map.h>

using handlers_t =
  absl::flat_hash_map<std::string, std::function<int(int argc, char* argv[])>>;

bool init_handlers(handlers_t&);

namespace {

[[noreturn]] void AssertCallback(irs::SourceLocation&& source,
                                 std::string_view message) {
  std::cerr << source.file << ":" << source.line << ": " << source.func
            << ": Assert failed: " << message << std::endl;
  std::abort();
}

void LogCallback(irs::SourceLocation&& source, std::string_view message) {
  std::cerr << source.file << ":" << source.line << ": " << source.func << ": "
            << message << std::endl;
}

const std::string HELP = "help";
const std::string MODE = "mode";

std::string get_modes_description(const handlers_t& handlers) {
  std::string message = "Select mode:";
  for (auto& entry : handlers) {
    message.append(" ");
    message += entry.first;
  }
  return message;
}

}  // namespace

int main(int argc, char* argv[]) {
  irs::assert::SetCallback(AssertCallback);
  handlers_t handlers;

  // initialize supported modes
  if (!init_handlers(handlers)) {
    return 1;
  }

  // init timers
  irs::timer_utils::init_stats(true);
  irs::Finally output_stats = []() noexcept {
    std::vector<std::tuple<std::string, size_t, size_t>> output;
    irs::timer_utils::visit(
      [&](const std::string& key, size_t count, size_t time_us) -> bool {
        if (count == 0) {
          return true;
        }
        output.emplace_back(key, count, time_us);
        return true;
      });
    std::sort(output.begin(), output.end());
    for (auto& [key, count, time_us] : output) {
      std::cout << key << " calls:" << count << ", time: " << time_us
                << " us, avg call: "
                << static_cast<double>(time_us) / static_cast<double>(count)
                << " us" << std::endl;
    }
  };

  // set error level
  irs::log::SetCallback(irs::log::Level::kFatal, &LogCallback);
  irs::log::SetCallback(irs::log::Level::kError, &LogCallback);

  // general description
  cmdline::parser cmdroot;
  cmdroot.add(HELP, '?', "Produce help message");
  cmdroot.add<std::string>(MODE, 'm', get_modes_description(handlers), true);
  cmdroot.parse(argc, argv);

  if (!cmdroot.exist(MODE) || cmdroot.exist(HELP)) {
    std::cout << cmdroot.usage() << std::endl;
  }

  const auto& mode = cmdroot.get<std::string>(MODE);
  const auto entry = handlers.find(mode);

  if (handlers.end() != entry) {
    return entry->second(argc, argv);
  }

  u_cleanup();  // cleanup ICU resources

  return 0;
}
