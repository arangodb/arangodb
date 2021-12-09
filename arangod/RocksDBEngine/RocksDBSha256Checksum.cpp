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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBSha256Checksum.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"

namespace arangodb {

RocksDBSha256Checksum::RocksDBSha256Checksum(std::string const& filename, std::shared_ptr<RocksDBShaFileManager> shaFileManager)
    : _fileName(filename),
      _shaFileManager{std::move(shaFileManager)},
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      _context(EVP_MD_CTX_new()) {
#else
      _context(EVP_MD_CTX_create()) {
#endif
  if (_context == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  if (EVP_DigestInit_ex(_context, EVP_sha256(), nullptr) == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to initialize SHA256 processor");
  }
}

RocksDBSha256Checksum::~RocksDBSha256Checksum() {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  EVP_MD_CTX_free(_context);
#else
  EVP_MD_CTX_destroy(_context);
#endif
}

void RocksDBSha256Checksum::Update(char const* data, size_t n) {
  EVP_DigestUpdate(_context, static_cast<void const*>(data), n);
}

void RocksDBSha256Checksum::Finalize() {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;
  if (EVP_DigestFinal_ex(_context, hash, &lengthOfHash) == 0) {
    TRI_ASSERT(false);
  }
  _checksum = basics::StringUtils::encodeHex(reinterpret_cast<char const*>(&hash[0]), lengthOfHash);

  _shaFileManager->storeShaItems(_fileName, _checksum);
}

std::string RocksDBSha256Checksum::GetChecksum() const {
  TRI_ASSERT(!_checksum.empty());
  return _checksum;
}
  
std::unique_ptr<rocksdb::FileChecksumGenerator> RocksDBSha256ChecksumFactory::CreateFileChecksumGenerator(
    rocksdb::FileChecksumGenContext const& context) {
  if (context.requested_checksum_func_name == "RocksDBSha256Checksum") {
    return std::make_unique<RocksDBSha256Checksum>(context.file_name, _shaFileManager);
  }
  return std::make_unique<RocksDBSha256Checksum>(context.file_name, _shaFileManager);
  // for everything else fall back to CRC32 checksum
 // LOG_DEVEL << (void*) this <<  " GENERATING CRC";
 // return std::make_unique<rocksdb::FileChecksumGenCrc32c>(context);
}

bool RocksDBShaFileManager::storeShaItems(std::string const& fileName, std::string const& checksum) {
  if (writeShaFile(fileName, checksum)) {
    MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
    _calculatedHashes.try_emplace(TRI_Basename(fileName), checksum);
    return true;
  }
  return false;
}

bool RocksDBShaFileManager::writeShaFile(std::string const& fileName, std::string const& checksum) {
  TRI_ASSERT(TRI_Basename(fileName).size() > 4);
  TRI_ASSERT(isSstFilename(fileName));

  std::string newFileName = fileName.substr(0, fileName.size() - 4);
  newFileName += ".sha.";
  newFileName += checksum;
  newFileName += ".hash";
  LOG_TOPIC("80257", DEBUG, arangodb::Logger::ENGINES)
      << "shaCalcFile: done " << fileName << " result: " << newFileName;
  auto res = TRI_WriteFile(newFileName.c_str(), "", 0);
  if (res == TRI_ERROR_NO_ERROR) {
    return true;
  }

  LOG_TOPIC("8f7ef", WARN, arangodb::Logger::ENGINES)
      << "shaCalcFile: TRI_WriteFile failed with " << res << " for " << newFileName;
  return false;
}

template <typename T>
bool RocksDBShaFileManager::isSstFilename(T const& fileName) const {
  return TRI_Basename(fileName).size() >= 4 &&
         (fileName.compare(fileName.size() - 4, 4, ".sst") == 0);
}

void RocksDBShaFileManager::deleteFile(std::string const& pathName) {
  std::string fileNameBuilder;
  {
    MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
    auto it = _calculatedHashes.find(TRI_Basename(pathName));
    if (it != _calculatedHashes.end()) {
      fileNameBuilder.append(pathName, 0, pathName.size() - 4);  // append without .sst
      TRI_ASSERT(!isSstFilename(fileNameBuilder));
      fileNameBuilder.append(".sha.");
      fileNameBuilder.append((*it).second);
      fileNameBuilder.append(".hash");
      _calculatedHashes.erase(it);
    }
  }
  if (!fileNameBuilder.empty()) {
    auto res = TRI_UnlinkFile(fileNameBuilder.c_str());
    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("e0a0d", DEBUG, arangodb::Logger::ENGINES)
          << "deleteCalcFile:  TRI_UnlinkFile succeeded for " << fileNameBuilder;
    } else {
      LOG_TOPIC("acb34", WARN, arangodb::Logger::ENGINES)
          << "deleteCalcFile:  TRI_UnlinkFile failed with " << res << " for "
          << fileNameBuilder;
    }
  }
}

void RocksDBShaFileManager::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo& info) {
  deleteFile(info.file_path);
}

void RocksDBShaFileManager::checkMissingShaFiles() {
  std::vector<std::string> fileList = TRI_FilesDirectory(_rootPath.c_str());
  std::sort(fileList.begin(), fileList.end());

  std::string tempname;

  for (auto it = fileList.begin(); fileList.end() != it; ++it) {
    if (it->size() < 5) {
      // filename is too short and does not matter
      continue;
    }

    TRI_ASSERT(*it == TRI_Basename(*it));

    std::string::size_type shaIdx = it->find(".sha.");
    if (std::string::npos != shaIdx) {
      tempname = it->substr(0, shaIdx) + ".sst";
      TRI_ASSERT(tempname == TRI_Basename(tempname));
      auto next = it + 1;
      if (fileList.end() != next && 0 == next->compare(tempname)) {
        TRI_ASSERT(it->size() >= shaIdx + 64);
        std::string hash = it->substr(shaIdx + /*.sha.*/ 5, 64);
        it = next;
        MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
        _calculatedHashes.try_emplace(tempname, std::move(hash));
      } else {
        std::string tempPath = basics::FileUtils::buildFilename(_rootPath, *it);
        LOG_TOPIC("4eac9", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles:"
               " Deleting file "
            << tempPath;
        TRI_UnlinkFile(tempPath.c_str());
        MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
        _calculatedHashes.erase(tempname);
      }
    } else if (it->size() > 4 && it->compare(it->size() - 4, 4, ".sst") == 0) {
      MUTEX_LOCKER(mutexLock, _calculatedHashesMutex);
      auto hashIt = _calculatedHashes.find(*it);
      if (hashIt == _calculatedHashes.end()) {
        mutexLock.unlock();
        std::string tempPath = basics::FileUtils::buildFilename(_rootPath, *it);
        LOG_TOPIC("d6c86", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles:"
               " Computing checksum for "
            << tempPath;
        RocksDBSha256Checksum checksumGenerator(tempPath, shared_from_this());
        if (TRI_ProcessFile(tempPath.c_str(), [&checksumGenerator](char const* buffer, size_t n) {
              checksumGenerator.Update(buffer, n);
              return true;
            })) {
          checksumGenerator.Finalize();
        }
      }
    }
  }
}

} // namespace
