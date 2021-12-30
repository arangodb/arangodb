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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <rocksdb/env_encryption.h>
#include "utils/encryption.hpp"

namespace arangodb {
namespace iresearch {

class RocksDBCipherStream final : public irs::encryption::stream {
 public:
  typedef std::unique_ptr<rocksdb::BlockAccessCipherStream> StreamPtr;

  explicit RocksDBCipherStream(StreamPtr&& stream) noexcept
      : _stream(std::move(stream)) {
    TRI_ASSERT(_stream);
  }

  virtual size_t block_size() const override { return _stream->BlockSize(); }

  virtual bool decrypt(uint64_t offset, irs::byte_type* data,
                       size_t size) override {
    return _stream->Decrypt(offset, reinterpret_cast<char*>(data), size).ok();
  }

  virtual bool encrypt(uint64_t offset, irs::byte_type* data,
                       size_t size) override {
    return _stream->Encrypt(offset, reinterpret_cast<char*>(data), size).ok();
  }

 private:
  StreamPtr _stream;
};  // RocksDBCipherStream

class RocksDBEncryptionProvider final : public irs::encryption {
 public:
  static std::shared_ptr<RocksDBEncryptionProvider> make(
      rocksdb::EncryptionProvider& encryption,
      rocksdb::Options const& options) {
    return std::make_shared<RocksDBEncryptionProvider>(encryption, options);
  }

  explicit RocksDBEncryptionProvider(rocksdb::EncryptionProvider& encryption,
                                     rocksdb::Options const& options)
      : _encryption(&encryption), _options(options) {}

  virtual size_t header_length() override {
    return _encryption->GetPrefixLength();
  }

  virtual bool create_header(std::string const& filename,
                             irs::byte_type* header) override {
    return _encryption
        ->CreateNewPrefix(filename, reinterpret_cast<char*>(header),
                          header_length())
        .ok();
  }

  virtual encryption::stream::ptr create_stream(
      std::string const& filename, irs::byte_type* header) override {
    rocksdb::Slice headerSlice(reinterpret_cast<char const*>(header),
                               header_length());

    std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
    if (!_encryption
             ->CreateCipherStream(filename, _options, headerSlice, &stream)
             .ok()) {
      return nullptr;
    }

    return std::make_unique<RocksDBCipherStream>(std::move(stream));
  }

 private:
  rocksdb::EncryptionProvider* _encryption;
  rocksdb::EnvOptions _options;
};  // RocksDBEncryptionProvider
}  // namespace iresearch
}  // namespace arangodb

namespace iresearch {
// use base irs::encryption type for ancestors
template<>
struct type<arangodb::iresearch::RocksDBEncryptionProvider>
    : type<irs::encryption> {};

}  // namespace iresearch
