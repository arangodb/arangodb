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
#include <rocksdb/status.h>

#include "Basics/Common.h"
#include "Basics/Mutex.h"

#undef DeleteFile

namespace arangodb::checksum {

class ChecksumCalculator {
 public:
  ChecksumCalculator();
  void computeFinalChecksum();
  void updateIncrementalChecksum(char const* buffer, size_t n);
  void updateEVPWithContent(char const* buffer, size_t n);
  [[nodiscard]] std::string getChecksum() const { return _checksum; }
  ~ChecksumCalculator();

 private:
  EVP_MD_CTX* _context;
  std::string _checksum;
};

class ChecksumHelper {
 public:
  explicit ChecksumHelper(std::string const& rootPath) : _rootPath{rootPath} {}
  [[nodiscard]] static bool isFileNameSst(std::string const& fileName);
  // writeShaFile() also inserts the .sst file name and the checksum in the
  // _fileNamesToHashes table
  bool writeShaFile(std::string const& fileName, std::string const& checksum);
  [[nodiscard]] static std::string buildShaFileNameFromSst(
      std::string const& fileName, std::string const& checksum);
  [[nodiscard]] std::string removeFromTable(std::string const& fileName);
  void checkMissingShaFiles();

 private:
  std::string _rootPath;

  Mutex _calculatedHashesMutex;
  std::unordered_map<std::string, std::string> _fileNamesToHashes;
};

class ChecksumWritableFile : public rocksdb::WritableFileWrapper {
 public:
  explicit ChecksumWritableFile(
      std::unique_ptr<rocksdb::WritableFile> writableFile,
      std::string const& fileName, std::shared_ptr<ChecksumHelper> helper)
      : rocksdb::WritableFileWrapper(writableFile.get()),
        _writableFile(std::move(writableFile)),
        _fileName(fileName),
        _helper(std::move(helper)) {}
  rocksdb::Status Append(const rocksdb::Slice& data) override;
  rocksdb::Status Close() override;

 private:
  std::unique_ptr<rocksdb::WritableFile> _writableFile;
  std::string const _fileName;
  std::shared_ptr<ChecksumHelper> _helper;
  ChecksumCalculator _checksumCalc;
};

class ChecksumEnv : public rocksdb::EnvWrapper {
 public:
  explicit ChecksumEnv(Env* t, std::string const& path)
      : EnvWrapper(t), _helper{std::make_shared<ChecksumHelper>(path)} {}

  rocksdb::Status NewWritableFile(
      const std::string& fileName,
      std::unique_ptr<rocksdb::WritableFile>* result,
      const rocksdb::EnvOptions& options) override;

  rocksdb::Status DeleteFile(const std::string& fileName) override;

  std::shared_ptr<ChecksumHelper> getHelper() const { return _helper; }

 private:
  std::shared_ptr<ChecksumHelper> _helper;
};

}  // namespace arangodb::checksum
