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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RocksDBEngine/SortedRowsStorageBackendRocksDB.h"
#include "RestServer/arangod.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace arangodb {
class RocksDBTempStorage;

namespace application_features {
class ApplicationServer;
}

class TemporaryStorageFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "TemporaryStorage";
  }

  explicit TemporaryStorageFeature(Server& server);
  ~TemporaryStorageFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

  bool canBeUsed() const noexcept;

  template<typename... Args>
  std::unique_ptr<aql::SortedRowsStorageBackend> getSortedRowsStorage(
      Args&&... args) {
    return std::make_unique<SortedRowsStorageBackendRocksDB>(
        *_backend, std::forward<Args>(args)...);
  }

  // returns configured maximum disk capacity for intermediate results storage
  // (0 = unlimited)
  std::uint64_t maxCapacity() const noexcept;

  // returns current disk usage for intermediate results storage
  std::uint64_t currentUsage() const noexcept;

  // increases capacity usage by value bytes. throws an exception if
  // that would move _currentUsage to a value > _maxCapacity
  void increaseUsage(std::uint64_t value);

  // decreases capacity usage by value bytes. assumes that _currentUsage >=
  // value
  void decreaseUsage(std::uint64_t value) noexcept;

 private:
  void cleanupDirectory();

  std::string _basePath;
  std::string const _tempFilesPath;
  std::uint64_t _maxCapacity;

  std::atomic<std::uint64_t> _currentUsage;

  // populated only if !_path.empty()
  std::unique_ptr<RocksDBTempStorage> _backend;
};

}  // namespace arangodb
