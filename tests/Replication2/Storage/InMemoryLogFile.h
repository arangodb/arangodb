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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include "Assertions/Assert.h"
#include "Basics/Result.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/IFileWriter.h"

namespace arangodb::replication2::storage::wal::test {

struct InMemoryFileReader : IFileReader {
  InMemoryFileReader(std::string& buffer) : _buffer(buffer) {}

  auto read(void* buffer, std::size_t n) -> std::size_t override {
    n = std::min(n, _buffer.size() - _position);
    memcpy(buffer, _buffer.data() + _position, n);
    _position += n;
    return n;
  }

  void seek(std::uint64_t pos) override {
    TRI_ASSERT(pos <= _buffer.size());
    _position = pos;
  }

  auto position() const -> std::uint64_t override { return _position; }

  auto size() const -> std::uint64_t override { return _buffer.size(); }

  std::string& _buffer;
  std::uint64_t _position = 0;
};

struct InMemoryFileWriter : IFileWriter {
  InMemoryFileWriter(std::string& buffer) : buffer(buffer) {}

  Result append(std::string_view data) override {
    buffer.append(data);
    return {};
  }

  void truncate(std::uint64_t size) override { buffer.resize(size); }

  void sync() override {}

  auto getReader() const -> std::unique_ptr<IFileReader> override {
    return std::make_unique<InMemoryFileReader>(buffer);
  }

  std::string& buffer;
};

}  // namespace arangodb::replication2::storage::wal::test
