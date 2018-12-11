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

#include "ManagedDirectory.h"

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Logger/Logger.h"

namespace {

/// @brief size of char buffer to use for file slurping
constexpr size_t DefaultIOChunkSize = 8192;

/// @brief the filename for the encryption file
constexpr auto EncryptionFilename = "ENCRYPTION";

/// @brief encryption type specification for no encryption
constexpr auto EncryptionTypeNone = "none";

/// @brief determines whether a given bit flag is set
inline bool flagIsSet(int value, int flagToCheck) {
  TRI_ASSERT(0 != flagToCheck);  // does not work correctly if flag is 0
  return (flagToCheck == (value & flagToCheck));
}

/// @brief determines whether a given bit flag is not set
inline bool flagNotSet(int value, int flagToCheck) {
  TRI_ASSERT(0 != flagToCheck);  // does not work correctly if flag is 0
  return (flagToCheck != (value & flagToCheck));
}

/// @brief Generates a generic I/O error based on the path and flags
inline arangodb::Result genericError(std::string const& path, int flags) {
  if (::flagIsSet(flags, O_WRONLY)) {
    return {TRI_ERROR_CANNOT_WRITE_FILE, "error while writing file " + path};
  }
  return {TRI_ERROR_CANNOT_READ_FILE, "error while reading file " + path};
}

/// @brief Assembles the file path from the directory and filename
inline std::string filePath(arangodb::ManagedDirectory const& directory,
                            std::string const& filename) {
  using arangodb::basics::FileUtils::buildFilename;
  return buildFilename(directory.path(), filename);
}

/// @brief Assembles the file path from the directory path and filename
inline std::string filePath(std::string const& directory,
                            std::string const& filename) {
  using arangodb::basics::FileUtils::buildFilename;
  return buildFilename(directory, filename);
}

/// @brief Opens a file given a path and flags
inline int openFile(std::string const& path, int flags) {
  return (::flagIsSet(flags, O_CREAT)
              ? TRI_CREATE(path.c_str(), flags, S_IRUSR | S_IWUSR)
              : TRI_OPEN(path.c_str(), flags));
}

/// @brief Closes an open file and sets the status
inline void closeFile(int& fd, arangodb::Result& status) {
  TRI_ASSERT(fd >= 0);
  status = arangodb::Result{TRI_CLOSE(fd)};
  fd = -1;
}

/// @brief determines if a file is writable
bool isWritable(int fd, int flags, std::string const& path,
                arangodb::Result& status) {
  if (::flagNotSet(flags, O_WRONLY)) {
    status = {
        TRI_ERROR_CANNOT_WRITE_FILE,
        "attempted to write to file " + path + " opened in read-only mode!"};
    return false;
  }
  if (fd < 0) {
    status = {TRI_ERROR_CANNOT_WRITE_FILE,
              "attempted to write to file " + path + " which is not open"};
    return false;
  }
  return status.ok();
}

/// @brief determines if a file is readable
bool isReadable(int fd, int flags, std::string const& path,
                arangodb::Result& status) {
  if (::flagIsSet(flags, O_WRONLY)) {
    status = {
        TRI_ERROR_CANNOT_READ_FILE,
        "attempted to read from file " + path + " opened in write-only mode!"};
    return false;
  }
  if (fd < 0) {
    status = {TRI_ERROR_CANNOT_READ_FILE,
              "attempted to read from file " + path + " which is not open"};
    return false;
  }
  return status.ok();
}

#ifdef USE_ENTERPRISE
/// @brief Begins encryption for the file and returns the encryption context
inline std::unique_ptr<arangodb::EncryptionFeature::Context> getContext(
    arangodb::ManagedDirectory const& directory, int fd, int flags) {
  if (fd < 0 || directory.encryptionFeature() == nullptr) {
    return {nullptr};
  }

  return (::flagIsSet(flags, O_WRONLY)
              ? directory.encryptionFeature()->beginEncryption(fd)
              : directory.encryptionFeature()->beginDecryption(fd));
}
#endif

/// @brief Generates the initial status for the directory
#ifdef USE_ENTERPRISE
arangodb::Result initialStatus(int fd, std::string const& path, int flags,
                               arangodb::EncryptionFeature::Context* context)
#else
arangodb::Result initialStatus(int fd, std::string const& path, int flags)
#endif
{
  if (fd < 0) {
    return ::genericError(path, flags);
  }

#ifdef USE_ENTERPRISE
  return (context ? context->status() : arangodb::Result{TRI_ERROR_NO_ERROR});
#else
  return {TRI_ERROR_NO_ERROR};
#endif
}

/// @brief Performs a raw (non-encrypted) write
inline void rawWrite(int fd, char const* data, size_t length,
                     arangodb::Result& status, std::string const& path,
                     int flags) {
  while (length > 0) {
    ssize_t written = TRI_WRITE(fd, data, static_cast<TRI_write_t>(length));
    if (written < 0) {
      status = ::genericError(path, flags);
      break;
    }
    length -= written;
    data += written;
  }
}

/// @brief Performs a raw (non-decrypted) read
inline ssize_t rawRead(int fd, char* buffer, size_t length,
                       arangodb::Result& status, std::string const& path,
                       int flags) {
  ssize_t bytesRead = TRI_READ(fd, buffer, static_cast<TRI_read_t>(length));
  if (bytesRead < 0) {
    status = ::genericError(path, flags);
  }
  return bytesRead;
}

void readEncryptionFile(std::string const& directory, std::string& type) {
  using arangodb::basics::FileUtils::slurp;
  using arangodb::basics::StringUtils::trim;
  type = ::EncryptionTypeNone;
  auto filename = ::filePath(directory, ::EncryptionFilename);
  if (TRI_ExistsFile(filename.c_str())) {
    type = trim(slurp(filename));
  }
}

#ifdef USE_ENTERPRISE
void writeEncryptionFile(std::string const& directory, std::string& type,
                         arangodb::EncryptionFeature* encryptionFeature) {
#else
void writeEncryptionFile(std::string const& directory, std::string& type) {
#endif
  using arangodb::basics::FileUtils::spit;
  type = ::EncryptionTypeNone;
  auto filename = ::filePath(directory, ::EncryptionFilename);
#ifdef USE_ENTERPRISE
  if (nullptr != encryptionFeature) {
    type = encryptionFeature->encryptionType();
  }
#endif
  spit(filename, type);
}

}  // namespace

namespace arangodb {

ManagedDirectory::ManagedDirectory(std::string const& path, bool requireEmpty,
                                   bool create)
    :
#ifdef USE_ENTERPRISE
      _encryptionFeature{application_features::ApplicationServer::getFeature<
          EncryptionFeature>("Encryption")},
#endif
      _path{path},
      _encryptionType{::EncryptionTypeNone},
      _status{TRI_ERROR_NO_ERROR} {
  if (_path.empty()) {
    _status.reset(TRI_ERROR_BAD_PARAMETER, "must specify a path");
    return;
  }

  bool exists = TRI_ExistsFile(_path.c_str());
  if (exists) {
    bool isDirectory = TRI_IsDirectory(_path.c_str());
    // path exists, but is a file, not a directory
    if (!isDirectory) {
      _status.reset(TRI_ERROR_FILE_EXISTS,
                    "path specified already exists as a non-directory file");
      return;
    }

    std::vector<std::string> files(TRI_FullTreeDirectory(_path.c_str()));
    bool isEmpty = (files.size() <= 1);
    // TODO: TRI_FullTreeDirectory always returns at least one element ("")
    // even if directory is empty?

    if (!isEmpty) {
      // directory exists, has files, and we aren't allowed to overwrite
      if (requireEmpty) {
        _status.reset(TRI_ERROR_CANNOT_OVERWRITE_FILE,
                      "path specified is a non-empty directory");
        return;
      }
      ::readEncryptionFile(_path, _encryptionType);
      return;
    }
    // fall through to write encryption file
  } else {
    // create directory if it doesn't exist
    if (create) {
      long systemError;
      std::string errorMessage;
      int res = TRI_CreateDirectory(_path.c_str(), systemError, errorMessage);
      if (res != TRI_ERROR_NO_ERROR) {
        if (res == TRI_ERROR_SYS_ERROR) {
          res = TRI_ERROR_CANNOT_CREATE_DIRECTORY;
        }
        _status.reset(res, "unable to create output directory '" + _path +
                               "': " + errorMessage);
        return;
      }
      // fall through to write encryption file
    } else {
      _status.reset(TRI_ERROR_FILE_NOT_FOUND);
      return;
    }
  }

#ifdef USE_ENTERPRISE
  ::writeEncryptionFile(_path, _encryptionType, _encryptionFeature);
#else
  ::writeEncryptionFile(_path, _encryptionType);
#endif
}

ManagedDirectory::~ManagedDirectory() {}

Result const& ManagedDirectory::status() const { return _status; }

void ManagedDirectory::resetStatus() { _status.reset(TRI_ERROR_NO_ERROR); }

std::string const& ManagedDirectory::path() const { return _path; }

std::string ManagedDirectory::pathToFile(std::string const& filename) const {
  return ::filePath(*this, filename);
}

bool ManagedDirectory::isEncrypted() const {
  return (::EncryptionTypeNone != _encryptionType);
}

std::string const& ManagedDirectory::encryptionType() const {
  return _encryptionType;
}

#ifdef USE_ENTERPRISE
EncryptionFeature const* ManagedDirectory::encryptionFeature() const {
  return _encryptionFeature;
}
#endif

std::unique_ptr<ManagedDirectory::File> ManagedDirectory::readableFile(
    std::string const& filename, int flags) {
  std::unique_ptr<File> file{nullptr};

  if (_status.fail()) {  // directory is in a bad state
    return file;
  }

  try {
    file = std::make_unique<File>(*this, filename,
                                  (ManagedDirectory::DefaultReadFlags ^ flags));
  } catch (...) {
    _status.reset(
        TRI_ERROR_CANNOT_READ_FILE,
        "error opening file " + ::filePath(*this, filename) + " for reading");
    return {nullptr};
  }

  return file;
}

std::unique_ptr<ManagedDirectory::File> ManagedDirectory::writableFile(
    std::string const& filename, bool overwrite, int flags) {
  std::unique_ptr<File> file{nullptr};

  if (_status.fail()) {  // directory is in a bad state
    return file;
  }

  try {
    // deal with existing file first if it exists
    auto path = ::filePath(*this, filename);
    bool fileExists = TRI_ExistsFile(path.c_str());
    if (fileExists) {
      if (overwrite) {
        TRI_UnlinkFile(path.c_str());
      } else {
        _status.reset(TRI_ERROR_CANNOT_WRITE_FILE,
                      "file " + path + " already exists");
        return {nullptr};
      }
    }

    file = std::make_unique<File>(
        *this, filename, (ManagedDirectory::DefaultWriteFlags ^ flags));
  } catch (...) {
    return {nullptr};
  }

  return file;
}

void ManagedDirectory::spitFile(std::string const& filename,
                                std::string const& content) {
  auto file = writableFile(filename, true);
  if (!file) {
    _status = ::genericError(filename, O_WRONLY);
  } else if (file->status().fail()) {
    _status = file->status();
  }
  file->spit(content);
}

std::string ManagedDirectory::slurpFile(std::string const& filename) {
  std::string content;
  auto file = readableFile(filename);
  if (!file || file->status().fail()) {
    return content;
  }
  content = file->slurp();
  return content;
}

VPackBuilder ManagedDirectory::vpackFromJsonFile(std::string const& filename) {
  VPackBuilder builder;
  auto content = slurpFile(filename);
  if (!content.empty()) {
    // The Parser might throw;
    try {
      VPackParser parser(builder);
      parser.parse(reinterpret_cast<uint8_t const*>(content.data()),
                   content.size());
    } catch (...) {
      throw;  // TODO determine what to actually do here?
    }
  }
  return builder;
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
  TRI_ASSERT(::flagNotSet(_flags, O_RDWR));  // disallow read/write (encryption)
}

ManagedDirectory::File::~File() {
  try {
    if (_fd >= 0) {
      ::closeFile(_fd, _status);
    }
  } catch (...) {
  }
}

Result const& ManagedDirectory::File::status() const { return _status; }

std::string const& ManagedDirectory::File::path() const { return _path; }

void ManagedDirectory::File::write(char const* data, size_t length) {
  if (!::isWritable(_fd, _flags, _path, _status)) {
    return;
  }

#ifdef USE_ENTERPRISE
  if (_context && _directory.isEncrypted()) {
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
}

ssize_t ManagedDirectory::File::read(char* buffer, size_t length) {
  ssize_t bytesRead = -1;
  if (!::isReadable(_fd, _flags, _path, _status)) {
    return bytesRead;
  }

#ifdef USE_ENTERPRISE
  if (_context && _directory.isEncrypted()) {
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
  return bytesRead;
}

std::string ManagedDirectory::File::slurp() {
  std::string content;
  if (!::isReadable(_fd, _flags, _path, _status)) {
    return content;
  }

  char buffer[::DefaultIOChunkSize];
  while (true) {
    ssize_t bytesRead = read(buffer, ::DefaultIOChunkSize);
    if (_status.ok()) {
      content.append(buffer, bytesRead);
    }
    if (bytesRead <= 0) {
      break;
    }
  }

  return content;
}

void ManagedDirectory::File::spit(std::string const& content) {
  if (!::isWritable(_fd, _flags, _path, _status)) {
    return;
  }

  size_t leftToWrite = content.size();
  auto data = content.data();
  while (true) {
    size_t n = std::min(leftToWrite, ::DefaultIOChunkSize);
    write(data, n);
    if (_status.fail()) {
      break;
    }
    data += n;
    leftToWrite -= n;
    if (0 == leftToWrite) {
      break;
    }
  }
}

Result const& ManagedDirectory::File::close() {
  if (_fd >= 0) {
    ::closeFile(_fd, _status);
  }
  return _status;
}

}  // namespace arangodb
