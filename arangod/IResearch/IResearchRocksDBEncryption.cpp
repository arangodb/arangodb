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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchRocksDBEncryption.h"

#include "Basics/debugging.h"
#include "utils/type_id.hpp"

namespace arangodb::iresearch {
RocksDBCipherStream::RocksDBCipherStream(StreamPtr&& stream) noexcept
    : _stream(std::move(stream)) {
  TRI_ASSERT(_stream);
}

bool RocksDBEncryptionProvider::create_header(std::string_view filename,
                                              irs::byte_type* header) {
  // TODO(MBkkt) remove std::string{...} when RocksDB update their interface
  return _encryption
      ->CreateNewPrefix(std::string{filename}, reinterpret_cast<char*>(header),
                        header_length())
      .ok();
}

irs::encryption::stream::ptr RocksDBEncryptionProvider::create_stream(
    std::string_view filename, irs::byte_type* header) {
  rocksdb::Slice headerSlice(reinterpret_cast<char const*>(header),
                             header_length());
  // TODO(MBkkt) remove std::string{...} when RocksDB update their interface
  std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
  if (!_encryption
           ->CreateCipherStream(std::string{filename}, _options, headerSlice,
                                &stream)
           .ok()) {
    return nullptr;
  }

  return std::make_unique<RocksDBCipherStream>(std::move(stream));
}
}  // namespace arangodb::iresearch

namespace iresearch {
// use base irs::encryption type for ancestors
template<>
struct type<arangodb::iresearch::RocksDBEncryptionProvider> : type<encryption> {
};

}  // namespace iresearch
