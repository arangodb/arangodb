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

#include "BumpFileDescriptorsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileDescriptors.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <absl/strings/str_cat.h>

#include <cstdlib>
#include <cstring>
#include <string>

using namespace arangodb::application_features;
using namespace arangodb::options;

#ifdef TRI_HAVE_GETRLIMIT
namespace arangodb {

void BumpFileDescriptorsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  // we do this initialization here so we don't need to include
  // FileDescriptors.h in the header file.
  _descriptorsMinimum = FileDescriptors::recommendedMinimum();

  options
      ->addOption(
          _optionName,
          "The minimum number of file descriptors needed to start (0 = no "
          "minimum)",
          new UInt64Parameter(&_descriptorsMinimum),
          arangodb::options::makeFlags())
      .setIntroducedIn(31200);
}

void BumpFileDescriptorsFeature::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/) {
  if (_descriptorsMinimum > 0 &&
      (_descriptorsMinimum < FileDescriptors::requiredMinimum ||
       _descriptorsMinimum > FileDescriptors::maximumValue)) {
    LOG_TOPIC("7e15c", FATAL, Logger::STARTUP)
        << "invalid value for " << _optionName << ". must be between "
        << FileDescriptors::requiredMinimum << " and "
        << FileDescriptors::maximumValue;
    FATAL_ERROR_EXIT();
  }
}

void BumpFileDescriptorsFeature::prepare() {
  if (Result res = FileDescriptors::adjustTo(
          static_cast<FileDescriptors::ValueType>(_descriptorsMinimum));
      res.fail()) {
    LOG_TOPIC("97831", FATAL, Logger::SYSCALL) << res;
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_RESOURCES_TOO_LOW);
  }

  FileDescriptors current;
  if (Result res = FileDescriptors::load(current); res.fail()) {
    LOG_TOPIC("17d7b", FATAL, Logger::SYSCALL)
        << "cannot get the file descriptors limit value: " << res;
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_RESOURCES_TOO_LOW);
  }

  LOG_TOPIC("a1c60", INFO, Logger::SYSCALL)
      << "file-descriptors (nofiles) hard limit is "
      << FileDescriptors::stringify(current.hard) << ", soft limit is "
      << FileDescriptors::stringify(current.soft);

  auto required =
      std::max(static_cast<FileDescriptors::ValueType>(_descriptorsMinimum),
               FileDescriptors::requiredMinimum);

  if (current.soft < required) {
    auto message = absl::StrCat(
        "file-descriptors (nofiles) soft limit is too low, currently ",
        FileDescriptors::stringify(current.soft), ". please raise to at least ",
        required, " (e.g. via ulimit -n ", required,
        ") or adjust the value of the startup option ", _optionName);
    if (_descriptorsMinimum == 0) {
      LOG_TOPIC("a33ba", WARN, Logger::SYSCALL) << message;
    } else {
      LOG_TOPIC("8c771", FATAL, Logger::SYSCALL) << message;
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_RESOURCES_TOO_LOW);
    }
  }
}

}  // namespace arangodb
#endif
