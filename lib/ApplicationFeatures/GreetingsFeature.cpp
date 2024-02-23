////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <string>

#include "GreetingsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/Version.h"

namespace arangodb {

char const* LGPLNotice =
    "This executable uses the GNU C library (glibc), which is licensed under "
    "the GNU Lesser General Public License (LGPL), see "
    "https://www.gnu.org/copyleft/lesser.html and "
    "https://www.gnu.org/licenses/gpl.html";

void logLGPLNotice(void) {
#ifdef __GLIBC__
  LOG_TOPIC("11111", INFO, arangodb::Logger::FIXME) << LGPLNotice;
#endif
}

void GreetingsFeature::prepare() {
  LOG_TOPIC("e52b0", INFO, arangodb::Logger::FIXME)
      << rest::Version::getVerboseVersionString();
  logLGPLNotice();

  // building in maintainer mode or enabling unit test code will incur runtime
  // overhead, so warn users about this
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // maintainer mode
  constexpr bool warn = true;
#else
  // unit tests on (enables TEST_VIRTUAL)
#ifdef ARANGODB_USE_GOOGLE_TESTS
  constexpr bool warn = true;
#else
  // neither maintainer mode nor unit tests
  constexpr bool warn = false;
#endif
#endif
  // cppcheck-suppress knownConditionTrueFalse
  if (warn) {
    LOG_TOPIC("0458b", WARN, arangodb::Logger::FIXME)
        << "🥑 This is a maintainer version intended for debugging. DO NOT "
           "USE IN PRODUCTION! 🔥";
    LOG_TOPIC("bd666", WARN, arangodb::Logger::FIXME)
        << "==================================================================="
           "================";
  }
}

void GreetingsFeature::unprepare() {
  LOG_TOPIC("4bcb9", INFO, arangodb::Logger::FIXME)
      << "ArangoDB has been shut down";
}

}  // namespace arangodb
