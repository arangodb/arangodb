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

#pragma once

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Mutex.h"

#include <openssl/evp.h>
#include <rocksdb/file_checksum.h>
#include <rocksdb/listener.h>

#include <string>

namespace arangodb {

class RocksDBSha256ChecksumFactory;
class RocksDBShaFileManager;

class RocksDBSha256Checksum : public rocksdb::FileChecksumGenerator {
 public:
  explicit RocksDBSha256Checksum(std::string const& filename, std::shared_ptr<RocksDBShaFileManager> shaFileManager);
  ~RocksDBSha256Checksum();

  void Update(char const* data, size_t n) override;
  void Finalize() override;

  std::string GetChecksum() const override;

  char const* Name() const override { return "ADBSha256"; }

 private:
  std::string const _fileName;
  std::shared_ptr<RocksDBShaFileManager> _shaFileManager;
  EVP_MD_CTX* _context;
  std::string _checksum;
};

class RocksDBShaFileManager : public rocksdb::EventListener, public std::enable_shared_from_this<RocksDBShaFileManager> {
 public:
  RocksDBShaFileManager(std::string const& path) : _rootPath{path} {}
  void checkMissingShaFiles();
  void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo& /*info*/) override;
  bool storeShaItems(std::string const& fileName, std::string const& checksum);
  bool writeShaFile(std::string const& fileName, std::string const& checksum);
  void deleteFile(std::string const& pathName);
 private:
  template <typename T>
  bool isSstFilename(T const& fileName) const;

  std::unordered_map<std::string, std::string> _calculatedHashes;
  Mutex _calculatedHashesMutex;
  std::string const _rootPath;
};

class RocksDBSha256ChecksumFactory : public rocksdb::FileChecksumGenFactory {
 public:
  RocksDBSha256ChecksumFactory(std::shared_ptr<RocksDBShaFileManager> shaFileManager)
      : _shaFileManager{shaFileManager} {};
  std::unique_ptr<rocksdb::FileChecksumGenerator> CreateFileChecksumGenerator(
      rocksdb::FileChecksumGenContext const& context) override;
  char const* Name() const override { return "RocksDBSha256ChecksumFactory"; }

 private:
  std::shared_ptr<RocksDBShaFileManager> _shaFileManager;
};

}
