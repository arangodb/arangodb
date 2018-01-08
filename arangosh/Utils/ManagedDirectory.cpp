////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Cellar
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "ManagedDirectory.hpp"

#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/files.h"
#include "Logger/Logger.h"

using namespace arangodb;

namespace {
/// @brief Generates a generic I/O error based on the path and flags
inline Result genericError(std::string const& path, int flags) noexcept {
  if (flags & O_RDONLY) {
    return {TRI_ERROR_CANNOT_READ_FILE, "error while reading file " + path};
  }
  return {TRI_ERROR_CANNOT_WRITE_FILE, "error while writing file " + path};
}
}  // namespace

namespace {
/// @brief Assembles the file path from the directory and filename
inline std::string filePath(ManagedDirectory const& directory,
                            std::string const& filename) noexcept {
  return basics::FileUtils::buildFilename(directory.path(), filename);
}
}  // namespace

namespace {
/// @brief Opens a file given a path and flags
inline int openFile(std::string const& path, int flags) noexcept {
  return ((flags & O_CREAT)
              ? TRI_TRACKED_CREATE_FILE(path.c_str(), flags, S_IRUSR | S_IWUSR)
              : TRI_TRACKED_OPEN_FILE(path.c_str(), flags));
}
}  // namespace

namespace {
/// @brief Closes an open file and sets the status
inline void closeFile(int fd, Result& status) noexcept {
  status = Result{TRI_TRACKED_CLOSE_FILE(fd)};
}
}  // namespace

#ifdef USE_ENTERPRISE
namespace {
/// @brief Begins encryption for the file and returns the encryption context
inline std::unique_ptr<EncryptionFeature::Context> getContext(
    ManagedDirectory const& directory, int fd, int flags) noexcept {
  if (fd < 0 || directory.encryptionFeature() == nullptr) {
    return {nullptr};
  }

  return ((flags & O_RDONLY)
              ? directory.encryptionFeature()->beginDecryption(fd)
              : directory.encryptionFeature()->beginEncryption(fd));
}
}  // namespace
#endif

namespace {
/// @brief Generates the initial status for the directory
#ifdef USE_ENTERPRISE
Result initialStatus(int fd, std::string const& path, int flags,
                     EncryptionFeature::Context* context) noexcept
#else
Result initialStatus(int fd, std::string const& path, int flags) noexcept
#endif
{
  if (fd < 0) {
    return ::genericError(path, flags);
  }

#ifdef USE_ENTERPRISE
  return (context ? context->status() : Result{TRI_ERROR_NO_ERROR});
#else
  return {TRI_ERROR_NO_ERROR};
#endif
}
}  // namespace

namespace {
/// @brief Performs a raw (non-encrypted) write
inline void rawWrite(int fd, char const* data, size_t length, Result& status,
                     std::string const& path, int flags) noexcept {
  bool written = TRI_WritePointer(fd, data, length);
  if (!written) {
    status = ::genericError(path, flags);
  }
}
}  // namespace

namespace {
/// @brief Performs a raw (non-decrypted) read
inline ssize_t rawRead(int fd, char* buffer, size_t length, Result& status,
                       std::string const& path, int flags) noexcept {
  ssize_t bytesRead = TRI_ReadPointer(fd, buffer, length);
  if (bytesRead < 0) {
    status = ::genericError(path, flags);
  }
  return bytesRead;
}
}  // namespace

ManagedDirectory::ManagedDirectory(std::string const& path, bool requireEmpty,
                                   bool create)
    :
#ifdef USE_ENTERPRISE
      _encryptionFeature{application_features::ApplicationServer::getFeature<
          EncryptionFeature>("Encryption")},
#endif
      _path{path},
      _status{TRI_ERROR_NO_ERROR} {
  if (_path.empty()) {
    _status = {TRI_ERROR_BAD_PARAMETER, "must specify a path"};
    return;
  }

  bool exists = TRI_ExistsFile(_path.c_str());
  bool isDirectory = TRI_IsDirectory(_path.c_str());
  bool isEmptyDirectory = false;
  if (isDirectory) {
    // if it's a directory, check if it has files
    std::vector<std::string> files(TRI_FullTreeDirectory(_path.c_str()));
    isEmptyDirectory = (files.size() <= 1);
    // TODO: TRI_FullTreeDirectory always returns at least one element ("")
    // even if directory is empty?
  }

  // path exists, but is a file, not a directory
  if (exists && !isDirectory) {
    _status = {TRI_ERROR_FILE_EXISTS,
               "path specified already exists as a non-directory file"};
    return;
  }

  // directory exists, has files, and we aren't allowed to overwrite
  if (requireEmpty && isDirectory && !isEmptyDirectory) {
    _status = {TRI_ERROR_CANNOT_OVERWRITE_FILE,
               "path specified is a non-empty directory"};
    return;
  }

  // create directory if it doesn't exist
  if (!isDirectory) {
    if (create) {
      long systemError;
      std::string errorMessage;
      int res = TRI_CreateDirectory(_path.c_str(), systemError, errorMessage);
      if (res != TRI_ERROR_NO_ERROR) {
        if (res == TRI_ERROR_SYS_ERROR) {
          res = TRI_ERROR_CANNOT_CREATE_DIRECTORY;
        }
        _status = {res, "unable to create output directory '" + _path +
                            "': " + errorMessage};
        return;
      }
    } else {
      _status = {TRI_ERROR_FILE_NOT_FOUND};
    }
  }

#ifdef USE_ENTERPRISE
  if (_encryptionFeature) {
    _encryptionFeature->writeEncryptionFile(_path);
  }
#endif
}

ManagedDirectory::~ManagedDirectory() {}

#ifdef USE_ENTERPRISE
EncryptionFeature const* ManagedDirectory::encryptionFeature() const noexcept {
  return _encryptionFeature;
}
#endif

Result const& ManagedDirectory::status() const noexcept { return _status; }

void ManagedDirectory::resetStatus() noexcept {
  _status = {TRI_ERROR_NO_ERROR};
}

bool ManagedDirectory::encryptionEnabled() const noexcept {
#if USE_ENTERPRISE
  if (!_encryptionFeature) {
    return false;
  }
  return _encryptionFeature->enabled();
#else
  return false;
#endif
}

std::string const& ManagedDirectory::path() const noexcept { return _path; }

std::unique_ptr<ManagedDirectory::File> ManagedDirectory::readableFile(
    std::string const& filename, int flags) noexcept {
  std::unique_ptr<File> file{nullptr};

  if (_status.fail()) {
    return std::move(file);
  }

  try {
    file = std::make_unique<File>(*this, filename,
                                  (ManagedDirectory::DefaultReadFlags ^ flags));
  } catch (...) {
    _status = {
        TRI_ERROR_CANNOT_READ_FILE,
        "error opening file " + ::filePath(*this, filename) + " for reading"};
    return {nullptr};
  }

  return std::move(file);
}

std::unique_ptr<ManagedDirectory::File> ManagedDirectory::writableFile(
    std::string const& filename, bool overwrite, int flags) noexcept {
  std::unique_ptr<File> file{nullptr};

  if (_status.fail()) {
    return std::move(file);
  }

  try {
    // deal with existing file first if it exists
    auto path = ::filePath(*this, filename);
    bool fileExists = TRI_ExistsFile(path.c_str());
    if (fileExists) {
      if (overwrite) {
        TRI_UnlinkFile(path.c_str());
      } else {
        _status = {TRI_ERROR_CANNOT_WRITE_FILE,
                   "file " + path + " already exists"};
        return {nullptr};
      }
    }

    file = std::make_unique<File>(
        *this, filename, (ManagedDirectory::DefaultWriteFlags ^ flags));
  } catch (...) {
    return {nullptr};
  }

  return std::move(file);
}

ManagedDirectory::File::File(ManagedDirectory const& directory,
                             std::string const& filename, int flags)
    : _directory{directory},
      _path{::filePath(_directory, filename)},
      _flags{flags},
      _fd{::openFile(_path, _flags)},
#ifdef USE_ENTERPRISE
      _context{::getContext(_directory, _fd, _flags)},
      _status {
  ::initialStatus(_fd, _path, _flags, _context.get())
}
#else
      _status {
  ::initialStatus(_fd, _path, _flags)
}
#endif
{
  TRI_ASSERT(O_RDWR != (_flags & O_RDWR));
  TRI_ASSERT(O_RDONLY == (_flags & O_RDONLY) ||
             O_WRONLY == (_flags & O_WRONLY));
}

ManagedDirectory::File::~File() {
  if (_fd > 0) {
    ::closeFile(_fd, _status);
  }
}

Result const& ManagedDirectory::File::status() const noexcept {
  return _status;
}

std::string const& ManagedDirectory::File::path() const noexcept {
  return _path;
}

Result const& ManagedDirectory::File::write(char const* data,
                                            size_t length) noexcept {
  if (O_WRONLY != (_flags & O_WRONLY)) {
    _status = {
        TRI_ERROR_CANNOT_WRITE_FILE,
        "attempted to write to file " + _path + " opened in read-only mode!"};
    return _status;
  }
#ifdef USE_ENTERPRISE
  if (_context) {
    bool written =
        _directory.encryptionFeature()->writeData(*_context, data, length);
    if (!written) {
      _status = _context->status();
    }
  } else {
    ::rawWrite(_fd, data, length, _status, _path, _flags);
  }
#else
  ::rawWrite(_fd, data, length, _status, _path, _flags);
#endif
  return _status;
}

std::pair<Result const&, ssize_t> ManagedDirectory::File::read(
    char* buffer, size_t length) noexcept {
  ssize_t bytesRead = -1;
  if (O_RDONLY != (_flags & O_RDONLY)) {
    _status = {
        TRI_ERROR_CANNOT_READ_FILE,
        "attempted to read from file " + _path + " opened in write-only mode!"};
    return std::make_pair(_status, bytesRead);
  }
#ifdef USE_ENTERPRISE
  if (_context) {
    bytesRead =
        _directory.encryptionFeature()->readData(*_context, buffer, length);
    if (bytesRead < 0) {
      _status = _context->status();
    }
  } else {
    bytesRead = ::rawRead(_fd, buffer, length, _status, _path, _flags);
  }
#else
  bytesRead = ::rawRead(_fd, buffer, length, _status, _path, _flags);
#endif
  return std::make_pair(_status, bytesRead);
}

Result const& ManagedDirectory::File::close() noexcept {
  ::closeFile(_fd, _status);
  return _status;
}
