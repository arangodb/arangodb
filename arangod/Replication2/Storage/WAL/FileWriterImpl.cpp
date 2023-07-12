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

#include "FileWriterImpl.h"

#include <fcntl.h>

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Replication2/Storage/WAL/FileReaderImpl.h"

namespace arangodb::replication2::storage::wal {

FileWriterImpl::FileWriterImpl(std::string path) : _path(std::move(path)) {
  _file = ::open(_path.c_str(), O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
  if (_file < 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "failed to open replicated log file");
  }
}

FileWriterImpl::~FileWriterImpl() {
  if (_file >= 0) {
    ::close(_file);
  }
}

auto FileWriterImpl::append(std::string_view data) -> Result {
  auto n = ::write(_file, data.data(), data.size());
  if (n < 0) {
    return Result(TRI_ERROR_INTERNAL, "failed to write to log file");
  }
  if (static_cast<std::size_t>(n) != data.size()) {
    return Result(TRI_ERROR_INTERNAL, "write to log file was incomplete");
  }

  return Result{};
}

void FileWriterImpl::truncate(std::uint64_t size) {
  if (::ftruncate(_file, size) != 0) {
    // TODO - error handling
  }
}

void FileWriterImpl::sync() {
  ADB_PROD_ASSERT(fdatasync(_file) == 0)
      << "failed to flush: " << strerror(errno);
}

auto FileWriterImpl::getReader() const -> std::unique_ptr<IFileReader> {
  return std::make_unique<FileReaderImpl>(_path);
}

}  // namespace arangodb::replication2::storage::wal
