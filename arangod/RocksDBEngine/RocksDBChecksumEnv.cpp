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
#include "Logger/LogMacros.h"

namespace arangodb {

ChecksumHelper::ChecksumHelper() {
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
}

ChecksumHelper::~ChecksumHelper() {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  EVP_MD_CTX_free(_context);
#else
  EVP_MD_CTX_destroy(_context);
#endif
}

bool ChecksumHelper::isSstFilename(std::string const& fileName) {
  return TRI_Basename(fileName).size() >= 4 &&
         (fileName.compare(fileName.size() - 4, 4, ".sst") == 0);
}

bool ChecksumHelper::writeShaFile(std::string const& fileName,
                                  std::string const& checksum) {
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
      << "shaCalcFile: TRI_WriteFile failed with " << res << " for "
      << newFileName;
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

rocksdb::Status ChecksumEnv::NewWritableFile(
    const std::string& fileName, std::unique_ptr<rocksdb::WritableFile>* result,
    const rocksdb::EnvOptions& options) {
  LOG_DEVEL << "NewWritableFile invoked";
  std::unique_ptr<rocksdb::WritableFile> wf;
  rocksdb::Status s =
      rocksdb::EnvWrapper::NewWritableFile(fileName, &wf, options);
  if (!s.ok()) {
    return s;
  }
  result->reset(new ChecksumWritableFile(wf.release()));
  return s;
}

rocksdb::Status ChecksumEnv::DeleteFile(const std::string& fileName) {
  size_t nameLength = fileName.length();
  TRI_ASSERT(nameLength > 4);
  if (::isSstFilename(fileName)) {
  }
}

}  // namespace arangodb