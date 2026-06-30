////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

#include <unistd.h>

#include "RestServer/IDatabasePathProvider.h"

namespace arangodb::tests {

// Concrete IDatabasePathProvider backed by a unique temporary directory that is
// created on construction and recursively removed on destruction, keeping each
// test self-contained. The process id makes the path unique across concurrent
// test processes, the atomic counter unique within a single process.
class TempDatabasePathProvider final : public IDatabasePathProvider {
 public:
  TempDatabasePathProvider() {
    static std::atomic<uint64_t> counter{0};
    auto dir = std::filesystem::temp_directory_path() /
               ("arangotest-rocksdb-" + std::to_string(::getpid()) + "-" +
                std::to_string(counter.fetch_add(1)));
    std::filesystem::create_directories(dir);
    _directory = dir.string();
  }

  ~TempDatabasePathProvider() override {
    std::error_code ec;
    std::filesystem::remove_all(_directory, ec);
  }

  std::string const& directory() const override { return _directory; }

  std::string subdirectoryName(std::string const& subDirectory) const override {
    return (std::filesystem::path(_directory) / subDirectory).string();
  }

 private:
  std::string _directory;
};

}  // namespace arangodb::tests
