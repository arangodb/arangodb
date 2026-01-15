#include "CrashHandler/Dumper.h"

#include <format>
#include <queue>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/files.h"
#include "CrashHandler/DataSource.h"
#include "Rest/Version.h"

namespace arangodb::crash_handler {

Dumper::Dumper(std::string crashesDirectory)
    : _crashesDirectory(std::move(crashesDirectory)) {}

bool Dumper::ensureCrashDirectoryInitialized() {
  if (_crashDirectory.empty()) {
    _crashDirectory = ensureCrashDirectory(_crashesDirectory);
  }
  return !_crashDirectory.empty();
}

void Dumper::dumpDataSources() {
  if (!ensureCrashDirectoryInitialized()) {
    return;
  }

  for (auto const* dataSource : getDataSources()) {
    std::string filename = arangodb::basics::FileUtils::buildFilename(
        _crashDirectory,
        std::format("{}.json", dataSource->getDataSourceName()));
    auto data = dataSource->getCrashData();
    arangodb::basics::FileUtils::spit(filename, data.toJson());
  }
}

void Dumper::dumpSystemInfo() {
  if (!ensureCrashDirectoryInitialized()) {
    return;
  }

  auto const sysInfoFilename = arangodb::basics::FileUtils::buildFilename(
      _crashDirectory, "system_info.txt");

  std::string sysInfo = std::format(
      "ArangoDB Version: {}\n"
      "Platform: {}\n"
      "Number of Cores: {}\n"
      "Effective Number of Cores: {}\n"
      "Physical Memory: {} bytes\n"
      "Effective Physical Memory: {} bytes\n",
      arangodb::rest::Version::getServerVersion(),
      arangodb::rest::Version::getPlatform(),
      arangodb::NumberOfCores::getValue(),
      arangodb::NumberOfCores::getEffectiveValue(),
      arangodb::PhysicalMemory::getValue(),
      arangodb::PhysicalMemory::getEffectiveValue());

  arangodb::basics::FileUtils::spit(sysInfoFilename, sysInfo);
}

void Dumper::dumpBacktractInfo(std::string_view backtrace) {
  if (backtrace.empty() || !ensureCrashDirectoryInitialized()) {
    return;
  }
  auto const backtraceFilename = arangodb::basics::FileUtils::buildFilename(
      _crashDirectory, "backtrace.txt");
  arangodb::basics::FileUtils::spit(backtraceFilename, backtrace.data(),
                                    backtrace.size());
}

std::string Dumper::ensureCrashDirectory(
    std::string const& crashesDirectory) {
  if (crashesDirectory.empty()) {
    return {};
  }

  if (!arangodb::basics::FileUtils::exists(crashesDirectory)) {
    arangodb::basics::FileUtils::createDirectory(crashesDirectory);
  }

  auto const uuid = to_string(boost::uuids::random_generator()());
  auto const crashDirectory =
      arangodb::basics::FileUtils::buildFilename(crashesDirectory, uuid);
  arangodb::basics::FileUtils::createDirectory(crashDirectory);
  return crashDirectory;
}

void Dumper::cleanupOldCrashDirectories(
    std::string const& crashesDirectory, size_t maxCrashDirectories) {
  if (crashesDirectory.empty() ||
      !arangodb::basics::FileUtils::isDirectory(crashesDirectory)) {
    return;
  }

  auto const entries = arangodb::basics::FileUtils::listFiles(crashesDirectory);
  std::priority_queue<std::pair<int64_t, std::string>,
                      std::vector<std::pair<int64_t, std::string>>,
                      std::greater<>>
      crashDirs;

  for (auto const& entry : entries) {
    auto const fullPath =
        arangodb::basics::FileUtils::buildFilename(crashesDirectory, entry);
    if (arangodb::basics::FileUtils::isDirectory(fullPath)) {
      int64_t mtime{0};
      if (TRI_MTimeFile(fullPath.c_str(), &mtime) == TRI_ERROR_NO_ERROR) {
        crashDirs.emplace(mtime, entry);
      }
    }
  }

  // Remove the oldest directories until we're at the limit
  while (crashDirs.size() > maxCrashDirectories) {
    auto const& [mtime, dirName] = crashDirs.top();
    auto const crashDir =
        arangodb::basics::FileUtils::buildFilename(crashesDirectory, dirName);
    TRI_RemoveDirectory(crashDir.c_str());
    crashDirs.pop();
  }
}

}  // namespace arangodb::crash_handler