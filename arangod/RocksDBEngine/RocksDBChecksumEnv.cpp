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
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Logger/LogMacros.h"

namespace arangodb::checksum {

ChecksumHelper::ChecksumHelper()
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

ChecksumHelper::~ChecksumHelper() {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  EVP_MD_CTX_free(_context);
#else
  EVP_MD_CTX_destroy(_context);
#endif
}

bool ChecksumHelper::isFileNameSst(std::string const& fileName) noexcept {
  return TRI_Basename(fileName).size() > 4 &&
         (fileName.compare(fileName.size() - 4, 4, ".sst") == 0);
}

bool ChecksumHelper::writeShaFile(std::string const& fileName,
                                  std::string const& checksum) {
  TRI_ASSERT(TRI_Basename(fileName).size() > 4);
  TRI_ASSERT(isFileNameSst(fileName));

  std::string shaFileName = fileName.substr(0, fileName.size() - 4);
  shaFileName += ".sha.";
  shaFileName += checksum;
  shaFileName += ".hash";
  LOG_TOPIC("80257", DEBUG, arangodb::Logger::ENGINES)
      << "shaCalcFile: done " << fileName << " result: " << shaFileName;
  auto res = TRI_WriteFile(shaFileName.c_str(), "", 0);
  if (res == TRI_ERROR_NO_ERROR) {
    _sstFileNamesToHashes.try_emplace(TRI_Basename(fileName), checksum);
    return true;
  }

  LOG_TOPIC("8f7ef", WARN, arangodb::Logger::ENGINES)
      << "shaCalcFile: TRI_WriteFile failed with " << res << " for "
      << shaFileName;
  return false;
}

std::string ChecksumHelper::computeChecksum() {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;
  if (EVP_DigestFinal_ex(_context, hash, &lengthOfHash) == 0) {
    TRI_ASSERT(false);
  }
  return basics::StringUtils::encodeHex(reinterpret_cast<char const*>(&hash[0]),
                                        lengthOfHash);
}

void ChecksumHelper::checkMissingShaFiles() {
  if (!_rootPath.empty()) {
    std::vector<std::string> fileList = TRI_FilesDirectory(_rootPath.c_str());
    std::sort(fileList.begin(), fileList.end());
    std::string sstFileName;
    for (auto it = fileList.begin(); fileList.end() != it; ++it) {
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
          _sstFileNamesToHashes.try_emplace(sstFileName, std::move(hash));
        } else {
          std::string tempPath =
              basics::FileUtils::buildFilename(_rootPath, *it);
          LOG_TOPIC("4eac9", DEBUG, arangodb::Logger::ENGINES)
              << "checkMissingShaFiles:"
                 " Deleting file "
              << tempPath;
          TRI_UnlinkFile(tempPath.c_str());
          _sstFileNamesToHashes.erase(sstFileName);
        }
      } else if (isFileNameSst(*it)) {
        auto hashIt = _sstFileNamesToHashes.find(*it);
        if (hashIt == _sstFileNamesToHashes.end()) {
          std::string tempPath =
              basics::FileUtils::buildFilename(_rootPath, *it);
          LOG_TOPIC("d6c86", DEBUG, arangodb::Logger::ENGINES)
              << "checkMissingShaFiles:"
                 " Computing checksum for "
              << tempPath;
          if (TRI_ProcessFile(tempPath.c_str(), [this](char const* buffer,
                                                       size_t n) {
                EVP_DigestUpdate(_context, static_cast<void const*>(buffer), n);
                return true;
              })) {
            writeShaFile(tempPath, computeChecksum());
          }
        }
      }
    }
  }
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
  result->reset(
      new ChecksumWritableFile(writableFile.release(), fileName, _helper));
  return s;
}

rocksdb::Status ChecksumWritableFile::Close() {
  TRI_ASSERT(_helper != nullptr);
  if (_helper->writeShaFile(_sstFileName, _helper->computeChecksum())) {
    return rocksdb::Status::OK();
  } else {
    return rocksdb::Status::Aborted("File writing was unsuccessful");
  }
}

rocksdb::Status ChecksumEnv::DeleteFile(const std::string& fileName) {
  if (!_helper->isFileNameSst(fileName) &&
      fileName.find(".sha") == std::string::npos) {
    // return Env::DeleteFile(fileName);

    if (TRI_UnlinkFile(fileName.data()) == TRI_ERROR_NO_ERROR) {
      return rocksdb::Status::OK();
    } else {
      return rocksdb::Status::Aborted("Could not unlink file " + fileName);
    }
  }
  return _helper->DeleteFile(fileName);
}

rocksdb::Status ChecksumHelper::DeleteFile(const std::string& fileName) {
  std::string shaFileName;
  if (auto it = _sstFileNamesToHashes.find(TRI_Basename(fileName));
      it != _sstFileNamesToHashes.end()) {
    TRI_ASSERT(fileName.size() > 4);
    shaFileName.append(fileName, 0,
                       fileName.size() - 4);  // append without .sst
    TRI_ASSERT(!isFileNameSst(shaFileName));
    shaFileName += ".sha" + (*it).second + ".hash";
    _sstFileNamesToHashes.erase(it);
  }
  if (!shaFileName.empty()) {
    auto res = TRI_UnlinkFile(shaFileName.c_str());
    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("e0a0d", DEBUG, arangodb::Logger::ENGINES)
          << "deleteCalcFile:  TRI_UnlinkFile succeeded for " << shaFileName;
    } else {
      LOG_TOPIC("acb34", WARN, arangodb::Logger::ENGINES)
          << "deleteCalcFile:  TRI_UnlinkFile failed with " << res << " for "
          << shaFileName;
    }
  }
  auto res = TRI_UnlinkFile(fileName.c_str());
  if (res == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("77a2a", DEBUG, arangodb::Logger::ENGINES)
        << "deleteCalcFile:  TRI_UnlinkFile succeeded for " << shaFileName;
    return rocksdb::Status::OK();
  } else {
    LOG_TOPIC("ce937", WARN, arangodb::Logger::ENGINES)
        << "deleteCalcFile:  TRI_UnlinkFile failed with " << res << " for "
        << shaFileName;
    return rocksdb::Status::Aborted("Could not delete file");
  }
}

}  // namespace arangodb::checksum