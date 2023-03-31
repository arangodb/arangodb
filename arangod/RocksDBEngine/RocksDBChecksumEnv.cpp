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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBChecksumEnv.h"

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"

#undef DeleteFile

namespace arangodb::checksum {

ChecksumCalculator::ChecksumCalculator() : _context(EVP_MD_CTX_new()) {
  if (_context == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  if (EVP_DigestInit_ex(_context, EVP_sha256(), nullptr) == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to initialize SHA256 processor");
  }
}

void ChecksumCalculator::computeFinalChecksum() {
  TRI_ASSERT(_context != nullptr);
  TRI_ASSERT(_checksum.empty());
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;
  if (EVP_DigestFinal_ex(_context, hash, &lengthOfHash) == 0) {
    TRI_ASSERT(false);
  }
  _checksum = basics::StringUtils::encodeHex(
      reinterpret_cast<char const*>(&hash[0]), lengthOfHash);
}

void ChecksumCalculator::updateIncrementalChecksum(char const* buffer,
                                                   size_t n) noexcept {
  TRI_ASSERT(_context != nullptr);
  updateEVPWithContent(buffer, n);
}

ChecksumCalculator::~ChecksumCalculator() {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  EVP_MD_CTX_free(_context);
#else
  EVP_MD_CTX_destroy(_context);
#endif
}

void ChecksumCalculator::updateEVPWithContent(const char* buffer,
                                              size_t n) noexcept {
  TRI_ASSERT(_context != nullptr);
  if (EVP_DigestUpdate(_context, static_cast<void const*>(buffer), n) == 0) {
    TRI_ASSERT(false);
  }
}

bool ChecksumHelper::isSstFile(std::string_view fileName) noexcept {
  return fileName.ends_with(".sst");
}

bool ChecksumHelper::isBlobFile(std::string_view fileName) noexcept {
  return fileName.ends_with(".blob");
}

bool ChecksumHelper::writeShaFile(std::string const& fileName,
                                  std::string const& checksum) {
  TRI_ASSERT(isSstFile(fileName) || isBlobFile(fileName));
  TRI_ASSERT(!checksum.empty());

  std::string shaFileName = buildShaFileNameFromSstOrBlob(fileName, checksum);
  TRI_ASSERT(!shaFileName.empty());

  LOG_TOPIC("80257", DEBUG, arangodb::Logger::ENGINES)
      << "shaCalcFile: done " << fileName << " result: " << shaFileName;
  auto res = TRI_WriteFile(shaFileName.c_str(), "", 0);
  if (res == TRI_ERROR_NO_ERROR) {
    std::string baseName = TRI_Basename(fileName);
    std::lock_guard mutexLock{_calculatedHashesMutex};
    _fileNamesToHashes.try_emplace(std::move(baseName), checksum);
    return true;
  }

  LOG_TOPIC("8f7ef", WARN, arangodb::Logger::ENGINES)
      << "shaCalcFile: writing file failed with " << res << " for "
      << shaFileName;
  return false;
}

void ChecksumHelper::checkMissingShaFiles() {
  if (_rootPath.empty()) {
    return;
  }

  std::vector<std::string> fileList = TRI_FilesDirectory(_rootPath.c_str());
  std::sort(fileList.begin(), fileList.end(),
            [](std::string const& lhs, std::string const& rhs) {
              // need to sort in a way that .sha files are returned before .sst
              // and .blob files. this is against the regular lexicographical
              // order
              //
              // the following file types are interesting for us:
              // - blob files: 000050.blob
              // - sst files:  000050.sst
              // - hash files:
              // 000050.sha.0d918fe7d474a0a872f1705f0c315e2a8cb8820d2592eafdcc0efbd3feb56a9b.hash

              if (lhs == rhs) {
                // thank you, some STL implementations!
                return false;
              }

              // find prefix (which is the number in front of the file)
              size_t lhsDot = lhs.find('.');
              size_t rhsDot = rhs.find('.');
              if (lhsDot == std::string::npos || rhsDot == std::string::npos) {
                // if we don't find a dot in the filename, we don't care and use
                // lexicographical ordering
                return lhs < rhs;
              }

              std::string_view lhsPrefix(lhs.data(), lhsDot);
              std::string_view rhsPrefix(rhs.data(), rhsDot);
              if (lhsPrefix != rhsPrefix) {
                // if the prefixes are not identical, we don't care.
                // we don't require numerical sorting for the prefixes.
                // for ex, it does not matter that "100" is sorted after "99"
                return lhs < rhs;
              }
              // prefixes are identical...

              // check file extension
              auto isInteresting = [](std::string_view name) noexcept -> bool {
                return name.ends_with(".sst") || name.ends_with(".blob") ||
                       name.ends_with(".hash");
              };

              if (!isInteresting(lhs) || !isInteresting(rhs)) {
                // we are dealing with a non-interesting file type
                return lhs < rhs;
              }

              if (lhs.ends_with(".hash")) {
                // cannot have 2 hash files for the same prefix
                TRI_ASSERT(!rhs.ends_with(".hash"));

                // prefixes of lhs and rhs are identical - .hash files should be
                // sorted first (before .sst or .blob files)
                return true;
              }
              if (rhs.ends_with(".hash")) {
                // cannot have 2 hash files for the same prefix
                TRI_ASSERT(!lhs.ends_with(".hash"));

                // prefixes of lhs and rhs are identical - .hash files should be
                // sorted first (before .sst or .blob files)
                return false;
              }

              // we only care about the order of .hash files relative to .blob
              // and .sst files. everything else does not matter
              return lhs < rhs;
            });

  for (auto it = fileList.begin(); it != fileList.end(); ++it) {
    if (it->size() < 5) {
      // filename is too short and does not matter
      continue;
    }
    TRI_ASSERT(*it == TRI_Basename(*it));
    std::string::size_type shaIndex = it->find(".sha.");

    if (shaIndex != std::string::npos) {
      // found .sha file
      std::string baseName = it->substr(0, shaIndex);
      auto nextIt = it + 1;
      if (nextIt != fileList.end() &&
          (*nextIt == baseName + ".sst" || *nextIt == baseName + ".blob")) {
        // .sha file is followed by either an .sst file or a .blob file...
        std::string full = baseName;
        if (isSstFile(*nextIt)) {
          full.append(".sst");
        } else if (isBlobFile(*nextIt)) {
          full.append(".blob");
        } else {
          TRI_ASSERT(false);
        }
        TRI_ASSERT(it->size() >= shaIndex + 64);
        std::string hash = it->substr(shaIndex + /*.sha.*/ 5, 64);
        // skip following .sst or .blob file
        it = nextIt;
        std::lock_guard mutexLock{_calculatedHashesMutex};
        _fileNamesToHashes.try_emplace(std::move(full), std::move(hash));
      } else {
        // .sha file is not followed by .sst or .blob file - remove it
        std::string tempPath = basics::FileUtils::buildFilename(_rootPath, *it);
        LOG_TOPIC("4eac9", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles: Deleting file " << tempPath;
        TRI_UnlinkFile(tempPath.data());

        // remove hash values from hash table
        std::lock_guard mutexLock{_calculatedHashesMutex};
        _fileNamesToHashes.erase(baseName + ".sst");
        _fileNamesToHashes.erase(baseName + ".blob");
      }
    } else if (isSstFile(*it) || isBlobFile(*it)) {
      // we have a .sst or .blob file which was not preceeded by a .hash file.
      // this means we need to recalculate the sha hash for it!
      std::string tempPath = basics::FileUtils::buildFilename(_rootPath, *it);
      LOG_TOPIC("d6c86", DEBUG, arangodb::Logger::ENGINES)
          << "checkMissingShaFiles: Computing checksum for " << tempPath;
      auto checksumCalc = ChecksumCalculator();
      if (TRI_ProcessFile(tempPath.c_str(), [&checksumCalc](char const* buffer,
                                                            size_t n) noexcept {
            checksumCalc.updateEVPWithContent(buffer, n);
            return true;
          })) {
        checksumCalc.computeFinalChecksum();
        writeShaFile(tempPath, checksumCalc.getChecksum());
      }
    }
  }
}

std::string ChecksumHelper::removeFromTable(std::string const& fileName) {
  TRI_ASSERT(!fileName.empty());

  std::string baseName = TRI_Basename(fileName);
  std::string checksum;
  {
    std::lock_guard mutexLock{_calculatedHashesMutex};
    if (auto it = _fileNamesToHashes.find(baseName);
        it != _fileNamesToHashes.end()) {
      checksum = it->second;
      _fileNamesToHashes.erase(it);
    }
  }
  return checksum;
}

std::string ChecksumHelper::buildShaFileNameFromSstOrBlob(
    std::string const& fileName, std::string const& checksum) {
  if (!fileName.empty() && !checksum.empty()) {
    TRI_ASSERT(isSstFile(fileName) || isBlobFile(fileName));
    TRI_ASSERT(fileName.size() > 4);
    size_t suffixLength;
    if (isSstFile(fileName)) {
      suffixLength = 4;  // ".sst"
    } else if (isBlobFile(fileName)) {
      suffixLength = 5;  // ".blob"
    } else {
      TRI_ASSERT(false);
      LOG_TOPIC("48357", ERR, Logger::ENGINES)
          << "invalid call to buildShaFileNameFromSstOrBlob with '" << fileName
          << "'";
      return {};
    }

    // file name without suffix
    std::string shaFileName =
        fileName.substr(0, fileName.size() - suffixLength);
    TRI_ASSERT(!isSstFile(shaFileName) && !isBlobFile(shaFileName));
    shaFileName += ".sha." + checksum + ".hash";
    return shaFileName;
  }

  return {};
}

rocksdb::Status ChecksumWritableFile::Append(rocksdb::Slice const& data) {
  _checksumCalc.updateIncrementalChecksum(data.data(), data.size());
  return rocksdb::WritableFileWrapper::Append(data);
}

rocksdb::Status ChecksumWritableFile::Close() {
  TRI_ASSERT(_helper != nullptr);
  _checksumCalc.computeFinalChecksum();
  if (!_helper->writeShaFile(_fileName, _checksumCalc.getChecksum())) {
    LOG_TOPIC("0b00e", WARN, arangodb::Logger::ENGINES)
        << "Writing sha file for " << _fileName << " was unsuccessful";
  }
  return rocksdb::WritableFileWrapper::Close();
}

rocksdb::Status ChecksumEnv::NewWritableFile(
    std::string const& fileName, std::unique_ptr<rocksdb::WritableFile>* result,
    rocksdb::EnvOptions const& options) {
  std::unique_ptr<rocksdb::WritableFile> writableFile;
  rocksdb::Status s =
      rocksdb::EnvWrapper::NewWritableFile(fileName, &writableFile, options);
  if (!s.ok()) {
    return s;
  }
  try {
    if (_helper->isSstFile(fileName) || _helper->isBlobFile(fileName)) {
      *result = std::make_unique<ChecksumWritableFile>(std::move(writableFile),
                                                       fileName, _helper);
    } else {
      *result = std::move(writableFile);
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("8b19e", ERR, arangodb::Logger::ENGINES)
        << "WritableFile: exception caught when allocating " << e.what();
    return rocksdb::Status::MemoryLimit(
        "WritableFile: exception caught when allocating");
  }
  return s;
}

rocksdb::Status ChecksumEnv::DeleteFile(std::string const& fileName) {
  if (_helper->isSstFile(fileName) || _helper->isBlobFile(fileName)) {
    std::string checksum = _helper->removeFromTable(fileName);
    std::string shaFileName =
        _helper->buildShaFileNameFromSstOrBlob(fileName, checksum);
    if (!shaFileName.empty()) {
      auto res = TRI_UnlinkFile(shaFileName.c_str());
      if (res == TRI_ERROR_NO_ERROR) {
        LOG_TOPIC("e0a0d", DEBUG, arangodb::Logger::ENGINES)
            << "deleteCalcFile: delete file succeeded for " << shaFileName;
      } else {
        LOG_TOPIC("acb34", WARN, arangodb::Logger::ENGINES)
            << "deleteCalcFile: delete file failed for " << shaFileName << ": "
            << TRI_errno_string(res);
      }
    }
  }
  rocksdb::Status s = rocksdb::EnvWrapper::DeleteFile(fileName);
  if (s.ok()) {
    LOG_TOPIC("77a2a", DEBUG, arangodb::Logger::ENGINES)
        << "deleteCalcFile: delete file succeeded for " << fileName;
  } else if (!s.IsPathNotFound()) {
    // background: RocksDB can call the DeleteFile() API also for
    // files it originally intends to create, but then changes its mind
    // about.
    // for example, when RocksDB flushes a memtable, it may turn out
    // that the to-be-created .sst file would be fully empty. in this
    // case, RocksDB does not create the .sst file, but it still calls
    // the DeleteFile() API To clean it up.
    LOG_TOPIC("ce937", WARN, arangodb::Logger::ENGINES)
        << "deleteCalcFile: delete file failed for " << fileName << ": "
        << rocksutils::convertStatus(s).errorMessage();
  }
  return s;
}

}  // namespace arangodb::checksum
