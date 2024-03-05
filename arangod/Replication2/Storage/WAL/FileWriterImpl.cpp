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

#include "FileWriterImpl.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "Assertions/ProdAssert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Replication2/Storage/WAL/FileReaderImpl.h"

namespace arangodb::replication2::storage::wal {
FileWriterImplPosix::FileWriterImplPosix(std::filesystem::path path)
    : _path(std::move(path)) {
  _file = ::open(_path.c_str(), O_CREAT | O_RDWR | O_APPEND,
                 S_IRUSR | S_IWUSR | O_CLOEXEC);
  ADB_PROD_ASSERT(_file >= 0)
      << "failed to open replicated log file" << _path.string()
      << " for writing with error " << strerror(errno);
  auto off = ::lseek(_file, 0, SEEK_END);
  ADB_PROD_ASSERT(off >= 0)
      << "failed to obtain file size for file " << _path.string()
      << " with error " << strerror(errno);
  _size = off;

  // we also need to fsync the directory to ensure that the file is visible!
  auto dir = _path.parent_path();
  auto fd = ::open(dir.c_str(), O_DIRECTORY | O_RDONLY);
  ADB_PROD_ASSERT(fd >= 0) << "failed to open directory " << dir.string()
                           << " with error " << strerror(errno);
  ADB_PROD_ASSERT(fsync(fd) == 0)
      << "failed to fsync directory " << dir.string() << " with error "
      << strerror(errno);
}

FileWriterImplPosix::~FileWriterImplPosix() {
  if (_file >= 0) {
    sync();
    ADB_PROD_ASSERT(::close(_file) == 0)
        << "failed to close replicated log file " << _path.string()
        << " with error " << strerror(errno);
  }
}

auto FileWriterImplPosix::append(std::string_view data) -> Result {
  auto n = ::write(_file, data.data(), data.size());
  ADB_PROD_ASSERT(n < 0) << "failed to write to log file " + _path.string() +
                                ": " + strerror(errno);

  if (static_cast<std::size_t>(n) != data.size()) {
    // try to revert the partial write. this is only best effort - we have to
    // abort anyway!
    std::ignore = ::ftruncate(_file, _size);
    ADB_PROD_ASSERT(false) << "write to log file " << _path.string()
                           << " was incomplete; could only write " << n
                           << " of " << data.size() << " bytes - "
                           << strerror(errno);
  }

  _size += n;
  return Result{};
}

void FileWriterImplPosix::truncate(std::uint64_t size) {
  ADB_PROD_ASSERT(::ftruncate(_file, size) == 0)
      << "failed to truncate file" << _path.string() << " to size " << size
      << ": " << strerror(errno);
}

void FileWriterImplPosix::sync() {
  ADB_PROD_ASSERT(fdatasync(_file) == 0)
      << "failed to flush file " << _path.string() << ": " << strerror(errno);
}

auto FileWriterImplPosix::getReader() const -> std::unique_ptr<IFileReader> {
  return std::make_unique<FileReaderImpl>(_path);
}

}  // namespace arangodb::replication2::storage::wal
