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

#include "FileDescriptorsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/BumpFileDescriptorsFeature.h"
#include "Basics/FileDescriptors.h"
#include "Basics/FileUtils.h"
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
      _countDescriptorsInterval(60 * 1000),
      _fileDescriptorsCurrent(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_file_descriptors_current{})),
      _fileDescriptorsLimit(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_file_descriptors_limit{})) {
  setOptional(false);
  startsAfter<BumpFileDescriptorsFeature>();
  startsAfter<GreetingsFeaturePhase>();
  startsAfter<EnvironmentFeature>();
}

void FileDescriptorsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options
      ->addOption(
          "--server.count-descriptors-interval",
          "Controls the interval (in milliseconds) in which the number of open "
          "file descriptors for the process is determined "
          "(0 = disable counting).",
          new UInt64Parameter(&_countDescriptorsInterval),
          arangodb::options::makeFlags())
      .setIntroducedIn(31100);
}

void FileDescriptorsFeature::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/) {
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
  FileDescriptors current;
  if (Result res = FileDescriptors::load(current); res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  _fileDescriptorsLimit.store(current.soft, std::memory_order_relaxed);
}

uint64_t FileDescriptorsFeature::current() const noexcept {
  return _fileDescriptorsCurrent.load(std::memory_order_relaxed);
}
uint64_t FileDescriptorsFeature::limit() const noexcept {
  return _fileDescriptorsLimit.load(std::memory_order_relaxed);
}

void FileDescriptorsFeature::countOpenFiles() {
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
