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
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/StorageEngine.h"

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

TemporaryStorageFeature::TemporaryStorageFeature(Server& server)
    : ArangodFeature{server, *this}, _currentUsage(0) {
  startsAfter<EngineSelectorFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<RocksDBEngine>();
}

TemporaryStorageFeature::~TemporaryStorageFeature() {
  try {
    cleanupDirectory();
  } catch (...) {
  }
}

void TemporaryStorageFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption("--temp.intermediate-results-path",
                     "path for ephemeral, intermediate results",
                     new StringParameter(&_path));

  options->addOption("--temp.intermediate-results-capacity",
                     "maximum capacity (in bytes) to use for ephemeral, "
                     "intermediate results (0 = unlimited)",
                     new UInt64Parameter(&_maxCapacity));
}

void TemporaryStorageFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (_path.empty()) {
    // feature not used. this is fine (TM)
    return;
  }

  std::string currentDir = FileUtils::currentDirectory().result();

  // get regular database path
  std::string dbPath = normalizePath(
      currentDir, server().getFeature<DatabasePathFeature>().directory());
  std::string ourPath = normalizePath(currentDir, _path);

  if (dbPath == ourPath || ourPath.starts_with(dbPath)) {
    // if our path is the same as the database directory or inside it,
    // we refuse to start
    LOG_TOPIC("58b44", FATAL, arangodb::Logger::STARTUP)
        << "path for intermediate results ('" << ourPath
        << "') must not be identical to or inside the database directory ('"
        << dbPath << "')";
    FATAL_ERROR_EXIT();
  }

  _path = ourPath;
}

void TemporaryStorageFeature::prepare() {
  if (_path.empty()) {
    // feature not used
    return;
  }

  if (basics::FileUtils::isDirectory(_path)) {
    cleanupDirectory();
  } else {
    std::string systemErrorStr;
    long errorNo;

    auto res =
        TRI_CreateRecursiveDirectory(_path.c_str(), errorNo, systemErrorStr);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("ed3ef", FATAL, Logger::FIXME)
          << "cannot create directory for intermediate results ('" << _path
          << "'): " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }
}

void TemporaryStorageFeature::start() {
  if (_path.empty()) {
    // feature not used
    return;
  }
}

void TemporaryStorageFeature::stop() {}

void TemporaryStorageFeature::unprepare() { cleanupDirectory(); }

std::uint64_t TemporaryStorageFeature::maxCapacity() const noexcept {
  return _maxCapacity;
}

std::uint64_t TemporaryStorageFeature::currentUsage() const noexcept {
  return _currentUsage.load(std::memory_order_relaxed);
}

// increases capacity usage by value bytes. throws an exception if
// that would move _currentUsage to a value > _maxCapacity
void TemporaryStorageFeature::increaseUsage(std::uint64_t value) {
  std::uint64_t old = _currentUsage.fetch_add(value, std::memory_order_relaxed);

  if (_maxCapacity > 0 && old + value > _maxCapacity) {
    decreaseUsage(value);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "disk capacity limit for intermediate results exceeded");
  }
}

// decreases capacity usage by value bytes. assumes that _currentUsage >= value
void TemporaryStorageFeature::decreaseUsage(std::uint64_t value) noexcept {
  [[maybe_unused]] std::uint64_t old =
      _currentUsage.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(old >= value);
}

void TemporaryStorageFeature::cleanupDirectory() {
  if (_path.empty()) {
    // feature not used
    return;
  }

  // clean up our mess
  LOG_TOPIC("62215", INFO, Logger::FIXME)
      << "cleaning up directory for intermediate results '" << _path << "'";

  // clean up the entire temporary directory
  for (auto const& fileName : TRI_FullTreeDirectory(_path.c_str())) {
    TRI_UnlinkFile(basics::FileUtils::buildFilename(_path, fileName).data());
  }
}
