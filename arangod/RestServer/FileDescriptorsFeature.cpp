////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "FileDescriptorsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileDescriptors.h"
#include "Basics/FileUtils.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/EnvironmentFeature.h"

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <absl/strings/str_cat.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

#ifdef TRI_HAVE_GETRLIMIT
DECLARE_GAUGE(
    arangodb_file_descriptors_current, uint64_t,
    "Number of currently open file descriptors for the arangod process");
DECLARE_GAUGE(
    arangodb_file_descriptors_limit, uint64_t,
    "Limit for the number of open file descriptors for the arangod process");

namespace arangodb {

FileDescriptorsFeature::FileDescriptorsFeature(Server& server)
    : ArangodFeature{server, *this},
      _descriptorsMinimum(FileDescriptors::recommendedMinimum()),
#ifdef __linux__
      _countDescriptorsInterval(60 * 1000),
#else
      _countDescriptorsInterval(0),
#endif
      _fileDescriptorsCurrent(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_file_descriptors_current{})),
      _fileDescriptorsLimit(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_file_descriptors_limit{})) {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();
  startsAfter<EnvironmentFeature>();
}

void FileDescriptorsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--server.descriptors-minimum",
      "The minimum number of file descriptors needed to start (0 = no minimum)",
      new UInt64Parameter(&_descriptorsMinimum),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs,
                                   arangodb::options::Flags::OsLinux,
                                   arangodb::options::Flags::OsMac));

  options
      ->addOption(
          "--server.count-descriptors-interval",
          "Controls the interval (in milliseconds) in which the number of open "
          "file descriptors for the process is determined "
          "(0 = disable counting).",
          new UInt64Parameter(&_countDescriptorsInterval),
          arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs,
                                       arangodb::options::Flags::OsLinux))
      .setIntroducedIn(31100);
}

void FileDescriptorsFeature::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/) {
  if (_descriptorsMinimum > 0 &&
      (_descriptorsMinimum < FileDescriptors::requiredMinimum ||
       _descriptorsMinimum > FileDescriptors::maximumValue)) {
    LOG_TOPIC("7e15c", FATAL, Logger::STARTUP)
        << "invalid value for --server.descriptors-minimum. must be between "
        << FileDescriptors::requiredMinimum << " and "
        << FileDescriptors::maximumValue;
    FATAL_ERROR_EXIT();
  }

  constexpr uint64_t lowerBound = 10000;
  if (_countDescriptorsInterval > 0 && _countDescriptorsInterval < lowerBound) {
    LOG_TOPIC("c3011", WARN, Logger::SYSCALL)
        << "too low value for `--server.count-descriptors-interval`. Should be "
           "at least "
        << lowerBound;
    _countDescriptorsInterval = lowerBound;
  }
}

void FileDescriptorsFeature::prepare() {
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

  _fileDescriptorsLimit.store(current.soft, std::memory_order_relaxed);

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
        ") or adjust the value of the startup option "
        "--server.descriptors-minimum");
    if (_descriptorsMinimum == 0) {
      LOG_TOPIC("a33ba", WARN, Logger::SYSCALL) << message;
    } else {
      LOG_TOPIC("8c771", FATAL, Logger::SYSCALL) << message;
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_RESOURCES_TOO_LOW);
    }
  }
}

void FileDescriptorsFeature::countOpenFiles() {
#ifdef __linux__
  try {
    size_t numFiles = FileUtils::countFiles("/proc/self/fd");
    _fileDescriptorsCurrent.store(numFiles, std::memory_order_relaxed);
  } catch (std::exception const& ex) {
    LOG_TOPIC("bee41", DEBUG, Logger::SYSCALL)
        << "unable to count number of open files for arangod process: "
        << ex.what();
  } catch (...) {
    LOG_TOPIC("0a654", DEBUG, Logger::SYSCALL)
        << "unable to count number of open files for arangod process";
  }
#endif
}

void FileDescriptorsFeature::countOpenFilesIfNeeded() {
  if (_countDescriptorsInterval == 0) {
    return;
  }

  auto now = std::chrono::steady_clock::now();

  std::unique_lock guard{_lastCountMutex, std::try_to_lock};

  if (guard.owns_lock() &&
      (_lastCountStamp.time_since_epoch().count() == 0 ||
       now - _lastCountStamp >
           std::chrono::milliseconds(_countDescriptorsInterval))) {
    countOpenFiles();
    _lastCountStamp = now;
  }
}
}  // namespace arangodb

#endif
