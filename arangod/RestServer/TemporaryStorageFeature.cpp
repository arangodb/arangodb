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
////////////////////////////////////////////////////////////////////////////////

#include "TemporaryStorageFeature.h"

#include <filesystem>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryOptions.h"
#include "RestServer/IDatabasePathProvider.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
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
  std::string absolute =
      std::filesystem::absolute(std::filesystem::path(currentDir) / path)
          .string();
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

TemporaryStorageFeature::TemporaryStorageFeature(
    application_features::ApplicationServer& server,
    IDatabasePathProvider& databasePathProvider)
    : TemporaryStorageFeature(server, databasePathProvider,
                              TemporaryStorageFeatureOptions{}) {}

TemporaryStorageFeature::TemporaryStorageFeature(
    application_features::ApplicationServer& server,
    IDatabasePathProvider& databasePathProvider,
    TemporaryStorageFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _cleanedUpDirectory(false),
      _databasePathProvider(databasePathProvider) {
  startsAfter<RocksDBEngine>();
  startsAfter<DatabasePathFeature>();

  if (!canBeUsed() || ServerState::instance()->isAgent()) {
    return;  // no basePath configured, or on agent (prepare() will clear it)
  }

  // replace $PID with current process id
  _options.basePath = basics::StringUtils::replace(
      _options.basePath, "$PID", std::to_string(Thread::currentProcessId()));

  // configure defaults for query options
  aql::QueryOptions::defaultSpillOverThresholdNumRows =
      _options.spillOverThresholdNumRows;
  aql::QueryOptions::defaultSpillOverThresholdMemoryUsage =
      _options.spillOverThresholdMemoryUsage;

  // Verify that the intermediate-results path is not identical to or inside
  // the database directory.
  auto const currentDir = std::filesystem::current_path();
  _options.basePath = normalizePath(currentDir, _options.basePath);
  std::string dbPath =
      normalizePath(currentDir, _databasePathProvider.directory());
  if (dbPath == _options.basePath || _options.basePath.starts_with(dbPath)) {
    LOG_TOPIC("58b44", FATAL, Logger::STARTUP)
        << "path for intermediate results ('" << _options.basePath
        << "') must not be identical to or inside the database directory ('"
        << dbPath << "')";
    FATAL_ERROR_EXIT();
  }
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

void TemporaryStorageFeature::prepare() {
  if (canBeUsed() && ServerState::instance()->isAgent()) {
    // we don't want any storage for intermediate results on agents, because
    // massive AQL queries will not be executed on them.
    LOG_TOPIC("97ac6", WARN, Logger::STARTUP)
        << "disabling storage for intermediate results on agent instance, "
           "because it is not useful here";
    _options.basePath.clear();
    TRI_ASSERT(!canBeUsed());
  }

  if (!canBeUsed()) {
    return;
  }

  if (std::filesystem::is_directory(_options.basePath)) {
    // intentionally do not set _cleanedUpDirectory flag here
    cleanupDirectory();
  } else {
    std::string systemErrorStr;
    long errorNo;

    auto res = TRI_CreateRecursiveDirectory(_options.basePath.c_str(), errorNo,
                                            systemErrorStr);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("ed3ef", FATAL, Logger::FIXME)
          << "cannot create directory for intermediate results ('"
          << _options.basePath << "'): " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }
}

void TemporaryStorageFeature::start() {
  if (!canBeUsed()) {
    return;
  }

  _usageTracker =
      std::make_unique<StorageUsageTracker>(_options.maxDiskCapacity);

  auto backend = std::make_unique<RocksDBTempStorage>(
      _options.basePath, *_usageTracker, _options.useEncryption,
      _options.allowHWAcceleration);

  Result res = backend->init();
  if (res.fail()) {
    LOG_TOPIC("1c6f4", FATAL, Logger::FIXME)
        << "cannot initialize storage backend for intermediate results ('"
        << _options.basePath << "'): " << res.errorMessage();
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
  return !_options.basePath.empty();
}

void TemporaryStorageFeature::cleanupDirectory() {
  if (!canBeUsed()) {
    return;
  }

  // clean up our mess
  LOG_TOPIC("62215", DEBUG, Logger::FIXME)
      << "cleaning up directory for intermediate results '" << _options.basePath
      << "'";

  auto res = TRI_RemoveDirectory(_options.basePath.c_str());
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("97e4c", WARN, Logger::FIXME)
        << "error during removal of directory for intermediate results ('"
        << _options.basePath << "'): " << TRI_errno_string(res);
  }
}
