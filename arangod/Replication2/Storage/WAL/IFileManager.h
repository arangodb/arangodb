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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace arangodb::replication2::storage::wal {

struct IFileReader;
struct IFileWriter;

struct IFileManager {
  virtual ~IFileManager() = default;
  virtual auto listFiles() -> std::vector<std::string> = 0;

  virtual auto createReader(std::string const& filename)
      -> std::unique_ptr<IFileReader> = 0;
  virtual auto createWriter(std::string const& filename)
      -> std::unique_ptr<IFileWriter> = 0;

  virtual void removeAll() = 0;
};

}  // namespace arangodb::replication2::storage::wal