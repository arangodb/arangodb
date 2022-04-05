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

#pragma once

#include <openssl/evp.h>
#include <rocksdb/env.h>

namespace arangodb::checksum {

class ChecksumHelper {
 public:
  ChecksumHelper();
  ChecksumHelper(std::string const& rootPath) : _rootPath(rootPath){};
  static bool isFileNameSst(std::string const& fileName) noexcept;
  bool writeShaFile(std::string const& fileName, std::string const& checksum);
  rocksdb::Status DeleteFile(std::string const& fileName);
  std::string computeChecksum();
  void checkMissingShaFiles();
  ~ChecksumHelper();

 private:
  std::string _rootPath;
  EVP_MD_CTX* _context;
  std::unordered_map<std::string, std::string> _sstFileNamesToHashes;
};

class ChecksumWritableFile : public rocksdb::WritableFileWrapper {
 public:
  explicit ChecksumWritableFile(rocksdb::WritableFile* t,
                                std::string const& fileName,
                                std::shared_ptr<ChecksumHelper> helper)
      : rocksdb::WritableFileWrapper(t),
        _sstFileName(fileName),
        _helper(helper) {}
  rocksdb::Status Append(const rocksdb::Slice& data) override {
    LOG_DEVEL << "Appending " << data.size()
              << " bytes to ChecksummingWritableFile.";
    return rocksdb::WritableFileWrapper::Append(data);
  }
  rocksdb::Status Close() override;

 private:
  std::string const _sstFileName;
  std::shared_ptr<ChecksumHelper> _helper;
};

class ChecksumEnv
    : public rocksdb::EnvWrapper {  // must mix it with Env::Default() for the
                                    // moment
 public:
  explicit ChecksumEnv(Env* t, std::string const& path) : EnvWrapper(t) {
    _helper = std::make_shared<ChecksumHelper>(path);
  }
  /*
  explicit ChecksumEnv(Env* t) : EnvWrapper(t) {}
  explicit ChecksumEnv(std::unique_ptr<Env>&& t) : EnvWrapper(std::move(t)) {}
  explicit ChecksumEnv(const std::shared_ptr<Env>& t) : EnvWrapper(t) {}
   */

  rocksdb::Status NewWritableFile(
      const std::string& fileName,
      std::unique_ptr<rocksdb::WritableFile>* result,
      const rocksdb::EnvOptions& options) override;

  rocksdb::Status DeleteFile(const std::string& fileName) override;

  std::shared_ptr<ChecksumHelper>& getHelper() { return _helper; }

 private:
  std::shared_ptr<ChecksumHelper> _helper;
};

}  // namespace arangodb::checksum