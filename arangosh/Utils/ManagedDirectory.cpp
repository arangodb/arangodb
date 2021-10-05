////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Cellar
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ManagedDirectory.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/voc-errors.h"

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
inline std::string filePath(std::string const& directory, std::string const& filename) {
  using arangodb::basics::FileUtils::buildFilename;
  return buildFilename(directory, filename);
}

/// @brief Opens a file given a path and flags
inline int openFile(std::string const& path, int flags) {
  return (::flagIsSet(flags, O_CREAT) ? TRI_CREATE(path.c_str(), flags, S_IRUSR | S_IWUSR)
                                      : TRI_OPEN(path.c_str(), flags));
}

/// @brief Closes an open file and sets the status
inline void closeFile(int& fd, arangodb::Result& status) {
  TRI_ASSERT(fd >= 0);
  if (0 != TRI_CLOSE(fd)) {
    status = TRI_set_errno(TRI_ERROR_SYS_ERROR);
  } else {
    status = arangodb::Result{};
  }
  fd = -1;
}

/// @brief determines if a file is writable
bool isWritable(int fd, int flags, std::string const& path, arangodb::Result& status) {
  if (::flagNotSet(flags, O_WRONLY)) {
    status = {TRI_ERROR_CANNOT_WRITE_FILE, "attempted to write to file " + path +
                                               " opened in read-only mode!"};
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
bool isReadable(int fd, int flags, std::string const& path, arangodb::Result& status) {
  if (::flagIsSet(flags, O_WRONLY)) {
    status = {TRI_ERROR_CANNOT_READ_FILE, "attempted to read from file " + path +
                                              " opened in write-only mode!"};
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
                     arangodb::Result& status, std::string const& path, int flags) {
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
inline TRI_read_return_t rawRead(int fd, char* buffer, size_t length, arangodb::Result& status,
                       std::string const& path, int flags) {
  TRI_read_return_t bytesRead = TRI_READ(fd, buffer, static_cast<TRI_read_t>(length));
  if (bytesRead < 0) {
    status = ::genericError(path, flags);
  }
  return bytesRead;
}

arangodb::Result readEncryptionFile(std::string const& directory, std::string& type,
                                    arangodb::EncryptionFeature* encryptionFeature) {
  using arangodb::basics::FileUtils::slurp;
  using arangodb::basics::StringUtils::trim;

  std::string newType = ::EncryptionTypeNone;
#ifdef USE_ENTERPRISE
  if (nullptr != encryptionFeature) {
    newType = encryptionFeature->encryptionType();
  }
#endif

  type = ::EncryptionTypeNone;
  auto filename = ::filePath(directory, ::EncryptionFilename);
  if (TRI_ExistsFile(filename.c_str())) {
    type = trim(slurp(filename));
  } else {
    type = newType;
  }

  if (type != newType) {
    return {TRI_ERROR_BAD_PARAMETER,
            std::string("encryption type in existing ENCRYPTION file '") + filename + "' (" + type +
              ") does not match requested encryption type (" + newType + ")"};
  }
  return {};
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

ManagedDirectory::ManagedDirectory(application_features::ApplicationServer& server,
                                   std::string const& path, bool requireEmpty,
                                   bool create, bool writeGzip)
    :
#ifdef USE_ENTERPRISE
      _encryptionFeature{&server.getFeature<EncryptionFeature>()},
#else
      _encryptionFeature(nullptr),
#endif
      _path{path},
      _encryptionType{::EncryptionTypeNone},
      _writeGzip(writeGzip),
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
                    std::string("the specified path '") + _path + "' already exists as a non-directory file");
      return;
    }

    std::vector<std::string> files(TRI_FilesDirectory(_path.c_str()));
    if (!files.empty()) {
      // directory exists, has files, and we aren't allowed to overwrite
      if (requireEmpty) {
        _status.reset(TRI_ERROR_CANNOT_OVERWRITE_FILE,
                      std::string("the specified path '") + _path + "' is a non-empty directory");
        return;
      }

      _status.reset(::readEncryptionFile(_path, _encryptionType, _encryptionFeature));
      if (::EncryptionTypeNone != _encryptionType) {
        _writeGzip = false;
      }
      return;
    }
    // fall through to write encryption file
  } else {
    // create directory if it doesn't exist
    if (create) {
      long systemError;
      std::string errorMessage;
      auto res = TRI_CreateDirectory(_path.c_str(), systemError, errorMessage);
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

  // currently gzip and encryption are mutually exclusive, encryption wins
  if (::EncryptionTypeNone != _encryptionType) {
    _writeGzip = false;
  }
}

ManagedDirectory::~ManagedDirectory() = default;

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

std::unique_ptr<ManagedDirectory::File> ManagedDirectory::readableFile(std::string const& filename,
                                                                       int flags) {
  std::unique_ptr<File> file;

  if (!_status.fail()) {  // directory is in a bad state?
    try {
      bool gzFlag = filename.size() > 3 && 
                    (0 == filename.substr(filename.size() - 3).compare(".gz"));
      file = std::make_unique<File>(*this, filename,
                                    (ManagedDirectory::DefaultReadFlags ^ flags), gzFlag);
    } catch (...) {
      _status.reset(TRI_ERROR_CANNOT_READ_FILE, "error opening file " +
                                                    ::filePath(*this, filename) +
                                                    " for reading");
      file.reset();
    }
  }

  return file;
}

std::unique_ptr<ManagedDirectory::File> ManagedDirectory::readableFile(int fileDescriptor) {

  std::unique_ptr<File> file{nullptr};

  if (_status.fail()) {  // directory is in a bad state
    return file;
  }

  try {
    file = std::make_unique<File>(*this, fileDescriptor, false);
  } catch (...) {
    _status.reset(TRI_ERROR_CANNOT_READ_FILE, "error opening console pipe"
                                                  " for reading");
    return {nullptr};
  }

  return file;
}


std::unique_ptr<ManagedDirectory::File> ManagedDirectory::writableFile(
  std::string const& filename, bool overwrite, int flags, bool gzipOk) {
  std::unique_ptr<File> file;

  if (_status.fail()) {  // directory is in a bad state
    return file;
  }

  try {
    std::string filenameCopy = filename;
    if (_writeGzip && gzipOk) {
      filenameCopy.append(".gz");
    } // if

    // deal with existing file first if it exists
    auto path = ::filePath(*this, filenameCopy);
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

    file = std::make_unique<File>(*this, filenameCopy,
                                  (ManagedDirectory::DefaultWriteFlags ^ flags), _writeGzip && gzipOk);
  } catch (...) {
    return {nullptr};
  }

  return file;
}

void ManagedDirectory::spitFile(std::string const& filename, std::string const& content) {
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
      parser.parse(reinterpret_cast<uint8_t const*>(content.data()), content.size());
    } catch (...) {
      throw;  // TODO determine what to actually do here?
    }
  }
  return builder;
}

ManagedDirectory::File::File(ManagedDirectory const& directory,
                             std::string const& filename, int flags,
                             bool isGzip)
    : _directory{directory},
      _path{::filePath(_directory, filename)},
      _flags{flags},
      _fd{::openFile(_path, _flags)},
      _gzfd(-1),
      _gzFile(nullptr),
#ifdef USE_ENTERPRISE
      _context{::getContext(_directory, _fd, _flags)},
      _status {::initialStatus(_fd, _path, _flags, _context.get())}
#else
      _status {::initialStatus(_fd, _path, _flags)}
#endif
{
  TRI_ASSERT(::flagNotSet(_flags, O_RDWR));  // disallow read/write (encryption)

  if (isGzip) {
    prepareGzip((O_WRONLY & _flags) ? "wb" : "rb");
  }
}

ManagedDirectory::File::File(ManagedDirectory const& directory,
                             int fd,
                             bool isGzip)
    : _directory{directory},
      _path{"stdin"},
      _flags{0},
      _fd{fd},
      _gzfd(-1),
      _gzFile(nullptr),
#ifdef USE_ENTERPRISE
      _context{::getContext(_directory, _fd, _flags)},
      _status {::initialStatus(_fd, _path, _flags, _context.get())}
#else
      _status {::initialStatus(_fd, _path, _flags)}
#endif
{
  TRI_ASSERT(::flagNotSet(_flags, O_RDWR));  // disallow read/write (encryption)

  if (isGzip) {
    prepareGzip("rb");
  }
}

ManagedDirectory::File::~File() {
  MUTEX_LOCKER(lock, _mutex);
  try {
    if (_gzfd >= 0) {
      gzclose(_gzFile);
      _gzfd = -1;
      _gzFile = nullptr;
    } // if

    if (_fd >= 0) {
      ::closeFile(_fd, _status);
    }
  } catch (...) {
  }
}

void ManagedDirectory::File::prepareGzip(char const* gzFlags) {
  TRI_ASSERT(_gzFile == nullptr);

  if (_fd >= 0) {
    // gzip is going to perform a redundant close,
    //  simpler code to give it redundant handle
    _gzfd = TRI_DUP(_fd);
  }
    
  _gzFile = gzdopen(_gzfd, gzFlags);

  if (_gzFile != nullptr) {
    // allocate a larger buffer for decompression.
    // 128kb seems to be enough here... larger buffers didn't
    // really improve speed during testing
    int r = gzbuffer(_gzFile, 128 * 1024); 
    if (r != 0) {
      _status.reset(TRI_ERROR_OUT_OF_MEMORY, "unable to allocate gzip buffer");
    }
  }
}

Result const& ManagedDirectory::File::status() const { return _status; }

std::string const& ManagedDirectory::File::path() const { return _path; }

void ManagedDirectory::File::write(char const* data, size_t length) {
  MUTEX_LOCKER(lock, _mutex);
  writeNoLock(data, length);
}

void ManagedDirectory::File::writeNoLock(char const* data, size_t length) {
  if (!::isWritable(_fd, _flags, _path, _status)) {
    return;
  }

#ifdef USE_ENTERPRISE
  if (_context && _directory.isEncrypted()) {
    bool written = _directory.encryptionFeature()->writeData(*_context, data, length);
    if (!written) {
      _status = _context->status();
    }
    return;
  }
#endif
  if (isGzip()) {
    int const written = gzwrite(_gzFile, data, static_cast<unsigned int>(length));
    if (written < (int)length) {
      _status = ::genericError(_path, _flags);
    }
  } else {
    ::rawWrite(_fd, data, length, _status, _path, _flags);
  }
}

TRI_read_return_t ManagedDirectory::File::read(char* buffer, size_t length) {
  MUTEX_LOCKER(lock, _mutex);
  return readNoLock(buffer, length);
}

TRI_read_return_t ManagedDirectory::File::readNoLock(char* buffer, size_t length) {
  TRI_read_return_t bytesRead = -1;
  if (!::isReadable(_fd, _flags, _path, _status)) {
    return bytesRead;
  }

#ifdef USE_ENTERPRISE
  if (_context && _directory.isEncrypted()) {
    bytesRead = _directory.encryptionFeature()->readData(*_context, buffer, length);
    if (bytesRead < 0) {
      _status = _context->status();
    }
    return bytesRead;
  }
#endif
  if (isGzip()) {
    bytesRead = gzread(_gzFile, buffer, static_cast<unsigned int>(length));
  } else {
    bytesRead = ::rawRead(_fd, buffer, length, _status, _path, _flags);
  } // else
  return bytesRead;
}

std::string ManagedDirectory::File::slurp() {
  MUTEX_LOCKER(lock, _mutex);

  std::string content;
  if (::isReadable(_fd, _flags, _path, _status)) {
    char buffer[::DefaultIOChunkSize];
    while (true) {
      TRI_read_return_t bytesRead = readNoLock(buffer, ::DefaultIOChunkSize);
      if (_status.ok()) {
        content.append(buffer, bytesRead);
      }
      if (bytesRead <= 0) {
        break;
      }
    }
  }
  return content;
}

void ManagedDirectory::File::spit(std::string const& content) {
  MUTEX_LOCKER(lock, _mutex);

  if (!::isWritable(_fd, _flags, _path, _status)) {
    return;
  }

  size_t leftToWrite = content.size();
  auto data = content.data();
  while (true) {
    size_t n = std::min(leftToWrite, ::DefaultIOChunkSize);
    writeNoLock(data, n);
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
  MUTEX_LOCKER(lock, _mutex);

  if (_gzfd >= 0) {
    gzclose(_gzFile);
    _gzfd = -1;
    _gzFile = nullptr;
  } // if

  if (_fd >= 0) {
    ::closeFile(_fd, _status);
  }
  return _status;
}


std::int64_t ManagedDirectory::File::offset() const {
  MUTEX_LOCKER(lock, _mutex);

  std::int64_t fileBytesRead = -1;

  if (isGzip()) {
    fileBytesRead = gzoffset(_gzFile);
  } else {
    fileBytesRead = TRI_LSEEK(_fd, 0L, SEEK_CUR);
  } // else

  return fileBytesRead;
}

void ManagedDirectory::File::skip(size_t count) {
  MUTEX_LOCKER(lock, _mutex);

  // TODO is there a better implementation than just read count bytes?
  // how does this work with gzip?
  size_t const bufferSize = 4 * 1024;
  char buffer[bufferSize];

  while (count > 0) {
    TRI_read_return_t bytesRead = readNoLock(buffer, std::min(bufferSize, count));
    if (bytesRead <= 0) {
      break; // eof or error (_status will be set)
    }

    count -= bytesRead;
  }
}

}  // namespace arangodb
