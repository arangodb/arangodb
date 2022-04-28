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

#include "RocksDBChecksumEnv.h"

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"

#undef DeleteFile

namespace arangodb::checksum {

ChecksumCalculator::ChecksumCalculator()
    :
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      _context(EVP_MD_CTX_new()) {
#else
      _context(EVP_MD_CTX_create()) {
#endif
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
                                                   size_t n) {
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

void ChecksumCalculator::updateEVPWithContent(const char* buffer, size_t n) {
  TRI_ASSERT(_context != nullptr);
  if (EVP_DigestUpdate(_context, static_cast<void const*>(buffer), n) == 0) {
    TRI_ASSERT(false);
  }
}

bool ChecksumHelper::isFileNameSst(std::string const& fileName) {
  return fileName.ends_with(".sst");
}

bool ChecksumHelper::writeShaFile(std::string const& fileName,
                                  std::string const& checksum) {
  TRI_ASSERT(isFileNameSst(fileName));

  std::string shaFileName = buildShaFileNameFromSst(fileName, checksum);

  LOG_TOPIC("80257", DEBUG, arangodb::Logger::ENGINES)
      << "shaCalcFile: done " << fileName << " result: " << shaFileName;
  auto res = TRI_WriteFile(shaFileName.c_str(), "", 0);
  if (res == TRI_ERROR_NO_ERROR) {
    std::string baseName = TRI_Basename(fileName);
    MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
    _fileNamesToHashes.try_emplace(std::move(baseName), checksum);
    return true;
  }

  LOG_TOPIC("8f7ef", WARN, arangodb::Logger::ENGINES)
      << "shaCalcFile: writing file failed with " << res << " for "
      << shaFileName;
  return false;
}

void ChecksumHelper::checkMissingShaFiles() {
  if (!_rootPath.empty()) {
    std::vector<std::string> fileList = TRI_FilesDirectory(_rootPath.c_str());
    std::sort(fileList.begin(), fileList.end());
    std::string sstFileName;
    for (auto it = fileList.begin(); it != fileList.end(); ++it) {
      if (it->size() < 5) {
        // filename is too short and does not matter
        continue;
      }
      TRI_ASSERT(*it == TRI_Basename(*it));
      std::string::size_type shaIndex = it->find(".sha.");
      if (shaIndex != std::string::npos) {
        sstFileName = it->substr(0, shaIndex) + ".sst";
        TRI_ASSERT(sstFileName == TRI_Basename(sstFileName));
        auto nextIt = it + 1;
        if (nextIt != fileList.end() && nextIt->compare(sstFileName) == 0) {
          TRI_ASSERT(it->size() >= shaIndex + 64);
          std::string hash = it->substr(shaIndex + /*.sha.*/ 5, 64);
          it = nextIt;
          MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
          _fileNamesToHashes.try_emplace(std::move(sstFileName),
                                         std::move(hash));
        } else {
          std::string tempPath =
              basics::FileUtils::buildFilename(_rootPath, *it);
          LOG_TOPIC("4eac9", DEBUG, arangodb::Logger::ENGINES)
              << "checkMissingShaFiles:"
                 " Deleting file "
              << tempPath;
          TRI_UnlinkFile(tempPath.data());
          MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
          _fileNamesToHashes.erase(sstFileName);
        }
      } else if (isFileNameSst(*it)) {
        std::unordered_map<std::string, std::string>::const_iterator hashIt;
        {
          MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
          hashIt = _fileNamesToHashes.find(*it);
        }
        if (hashIt == _fileNamesToHashes.end()) {
          std::string tempPath =
              basics::FileUtils::buildFilename(_rootPath, *it);
          LOG_TOPIC("d6c86", DEBUG, arangodb::Logger::ENGINES)
              << "checkMissingShaFiles:"
                 " Computing checksum for "
              << tempPath;
          auto checksumCalc = ChecksumCalculator();
          if (TRI_ProcessFile(tempPath.c_str(),
                              [&checksumCalc](char const* buffer, size_t n) {
                                checksumCalc.updateEVPWithContent(buffer, n);
                                return true;
                              })) {
            checksumCalc.computeFinalChecksum();
            writeShaFile(tempPath, checksumCalc.getChecksum());
          }
        }
      }
    }
  }
}

std::string ChecksumHelper::removeFromTable(std::string const& fileName) {
  std::string checksum;
  {
    std::string baseName = TRI_Basename(fileName);
    MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
    if (auto it = _fileNamesToHashes.find(baseName);
        it != _fileNamesToHashes.end()) {
      checksum = it->second;
      _fileNamesToHashes.erase(it);
    }
  }
  return checksum;
}

std::string ChecksumHelper::buildShaFileNameFromSst(
    std::string const& fileName, std::string const& checksum) {
  if (!fileName.empty() && !checksum.empty()) {
    TRI_ASSERT(fileName.size() > 4);
    std::string shaFileName = fileName.substr(0, fileName.size() - 4);
    TRI_ASSERT(!isFileNameSst(shaFileName));
    shaFileName += ".sha." + checksum + ".hash";
    return shaFileName;
  }
  return {};
}

rocksdb::Status ChecksumWritableFile::Append(const rocksdb::Slice& data) {
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
    const std::string& fileName, std::unique_ptr<rocksdb::WritableFile>* result,
    const rocksdb::EnvOptions& options) {
  std::unique_ptr<rocksdb::WritableFile> writableFile;
  rocksdb::Status s =
      rocksdb::EnvWrapper::NewWritableFile(fileName, &writableFile, options);
  if (!s.ok()) {
    return s;
  }
  try {
    if (_helper->isFileNameSst(fileName)) {
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

rocksdb::Status ChecksumEnv::DeleteFile(const std::string& fileName) {
  if (_helper->isFileNameSst(fileName)) {
    std::string checksum = _helper->removeFromTable(fileName);
    std::string shaFileName =
        _helper->buildShaFileNameFromSst(fileName, checksum);
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
  } else {
    LOG_TOPIC("ce937", WARN, arangodb::Logger::ENGINES)
        << "deleteCalcFile: delete file failed for " << fileName << ": "
        << rocksutils::convertStatus(s).errorMessage();
  }
  return s;
}

}  // namespace arangodb::checksum
