////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "TemporaryStorageFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryOptions.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabasePathFeature.h"
#include "RocksDBEngine/RocksDBTempStorage.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
// normalizes a path, by making it absolute, unifying the directory separator
// characters, and making it end with a directory separator
std::string normalizePath(std::string const& currentDir,
                          std::string const& path) {
  std::string absolute = TRI_GetAbsolutePath(path, currentDir);
  TRI_NormalizePath(absolute);
  if (!absolute.empty() && absolute.back() != TRI_DIR_SEPARATOR_CHAR) {
    absolute.push_back(TRI_DIR_SEPARATOR_CHAR);
  }
  return absolute;
}

}  // namespace

StorageUsageTracker::StorageUsageTracker(std::uint64_t maxCapacity) noexcept
    : _maxCapacity(maxCapacity), _currentUsage(0) {}

// returns configured maximum disk capacity for intermediate results storage
// (0 = unlimited)
std::uint64_t StorageUsageTracker::maxCapacity() const noexcept {
  return _maxCapacity;
}

// returns current disk usage for intermediate results storage
std::uint64_t StorageUsageTracker::currentUsage() const noexcept {
  return _currentUsage.load(std::memory_order_relaxed);
}

// increases capacity usage by value bytes. throws an exception if
// that would move _currentUsage to a value > _maxCapacity
void StorageUsageTracker::increaseUsage(std::uint64_t value) {
  std::uint64_t old = _currentUsage.fetch_add(value, std::memory_order_relaxed);

  TRI_IF_FAILURE("lowTempStorageCapacity") {
    // simulate a low capacity value
    if (old + value >= 32 * 1024 * 1024) {
      decreaseUsage(value);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_RESOURCE_LIMIT,
          "disk capacity limit for intermediate results exceeded");
    }
  }

  if (_maxCapacity > 0 && old + value > _maxCapacity) {
    decreaseUsage(value);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "disk capacity limit for intermediate results exceeded");
  }
}

// decreases capacity usage by value bytes. assumes that _currentUsage >=
// value
void StorageUsageTracker::decreaseUsage(std::uint64_t value) noexcept {
  [[maybe_unused]] std::uint64_t old =
      _currentUsage.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(old >= value);
}

TemporaryStorageFeature::TemporaryStorageFeature(Server& server)
    : ArangodFeature{server, *this},
      _useEncryption(false),
      _allowHWAcceleration(true),
      _maxDiskCapacity(0),
      _spillOverThresholdNumRows(5000000),
      _spillOverThresholdMemoryUsage(128 * 1024 * 1024),
      _cleanedUpDirectory(false) {
  startsAfter<EngineSelectorFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<RocksDBEngine>();
}

TemporaryStorageFeature::~TemporaryStorageFeature() {
  if (canBeUsed() && !_cleanedUpDirectory) {
    try {
      cleanupDirectory();
    } catch (...) {
    }
    _cleanedUpDirectory = true;
  }
}

void TemporaryStorageFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options
      ->addOption(
          "--temp.intermediate-results-path",
          "path for ephemeral, intermediate results on disk (empty = not used)",
          new StringParameter(&_basePath),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  options
      ->addOption("--temp.intermediate-results-capacity",
                  "maximum capacity (in bytes) to use for ephemeral, "
                  "intermediate results on disk (0 = unlimited)",
                  new UInt64Parameter(&_maxDiskCapacity),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  options
      ->addOption(
          "--temp.intermediate-results-spillover-threshold-num-rows",
          "number of result rows after which a spillover to disk will "
          "happen for intermediate results (threshold per query executor)",
          new SizeTParameter(&_spillOverThresholdNumRows),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  options
      ->addOption(
          "--temp.intermediate-results-spillover-threshold-memory-usage",
          "memory usage threshold after which a spillover to disk will "
          "happen for intermediate results (threshold per query executor)",
          new SizeTParameter(&_spillOverThresholdMemoryUsage),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

#ifdef USE_ENTERPRISE
  options
      ->addOption("--temp.intermediate-results-encryption",
                  "encrypt ephemeral, intermediate results on disk",
                  new BooleanParameter(&_useEncryption),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Enterprise,
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  options
      ->addOption(
          "--temp.-intermediate-results-encryption-hardware-acceleration",
          "use Intel intrinsics-based encryption, requiring a CPU with "
          "the AES-NI instruction set. "
          "If turned off, then OpenSSL is used, which may use "
          "hardware-accelarated encryption too.",
          new BooleanParameter(&_allowHWAcceleration),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Enterprise,
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);
#endif
}

void TemporaryStorageFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (!canBeUsed()) {
    // feature not used. this is fine (TM)
    return;
  }

  // replace $PID in basepath with current process id
  _basePath = basics::StringUtils::replace(
      _basePath, "$PID", std::to_string(Thread::currentProcessId()));

  std::string currentDir = FileUtils::currentDirectory().result();

  // get regular database path
  std::string dbPath = normalizePath(
      currentDir, server().getFeature<DatabasePathFeature>().directory());
  std::string ourPath = normalizePath(currentDir, _basePath);

  if (dbPath == ourPath || ourPath.starts_with(dbPath)) {
    // if our path is the same as the database directory or inside it,
    // we refuse to start
    LOG_TOPIC("58b44", FATAL, Logger::STARTUP)
        << "path for intermediate results ('" << ourPath
        << "') must not be identical to or inside the database directory ('"
        << dbPath << "')";
    FATAL_ERROR_EXIT();
  }

  _basePath = ourPath;
  // configure defaults for query options
  aql::QueryOptions::defaultSpillOverThresholdNumRows =
      _spillOverThresholdNumRows;
  aql::QueryOptions::defaultSpillOverThresholdMemoryUsage =
      _spillOverThresholdMemoryUsage;
}

void TemporaryStorageFeature::prepare() {
  if (canBeUsed() && ServerState::instance()->isAgent()) {
    // we don't want any storage for intermediate results on agents, because
    // massive AQL queries will not be executed on them.
    LOG_TOPIC("97ac6", WARN, Logger::STARTUP)
        << "disabling storage for intermediate results on agent instance, "
           "because it is not useful here";
    _basePath.clear();
    TRI_ASSERT(!canBeUsed());
  }

  if (!canBeUsed()) {
    return;
  }

  if (basics::FileUtils::isDirectory(_basePath)) {
    // intentionally do not set _cleanedUpDirectory flag here
    cleanupDirectory();
  } else {
    std::string systemErrorStr;
    long errorNo;

    auto res = TRI_CreateRecursiveDirectory(_basePath.c_str(), errorNo,
                                            systemErrorStr);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("ed3ef", FATAL, Logger::FIXME)
          << "cannot create directory for intermediate results ('" << _basePath
          << "'): " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }
}

void TemporaryStorageFeature::start() {
  if (!canBeUsed()) {
    return;
  }

  _usageTracker = std::make_unique<StorageUsageTracker>(_maxDiskCapacity);

  auto backend = std::make_unique<RocksDBTempStorage>(
      _basePath, *_usageTracker, _useEncryption, _allowHWAcceleration);

  Result res = backend->init();
  if (res.fail()) {
    LOG_TOPIC("1c6f4", FATAL, Logger::FIXME)
        << "cannot initialize storage backend for intermediate results ('"
        << _basePath << "'): " << res.errorMessage();
    FATAL_ERROR_EXIT();
  }

  _backend = std::move(backend);
}

void TemporaryStorageFeature::stop() {
  if (!canBeUsed()) {
    return;
  }

  TRI_ASSERT(_backend != nullptr);
  _backend->close();
  _backend.reset();
}

void TemporaryStorageFeature::unprepare() {
  if (canBeUsed() && !_cleanedUpDirectory) {
    // clean up the directory with temporary files
    cleanupDirectory();
    // but only once
    _cleanedUpDirectory = true;
  }
}

bool TemporaryStorageFeature::canBeUsed() const noexcept {
  return !_basePath.empty();
}

void TemporaryStorageFeature::cleanupDirectory() {
  if (!canBeUsed()) {
    return;
  }

  // clean up our mess
  LOG_TOPIC("62215", DEBUG, Logger::FIXME)
      << "cleaning up directory for intermediate results '" << _basePath << "'";

  auto res = TRI_RemoveDirectory(_basePath.c_str());
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("97e4c", WARN, Logger::FIXME)
        << "error during removal of directory for intermediate results ('"
        << _basePath << "'): " << TRI_errno_string(res);
  }
}
