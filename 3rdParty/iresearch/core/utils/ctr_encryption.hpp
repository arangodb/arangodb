////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_CTR_ENCRYPTION_H
#define IRESEARCH_CTR_ENCRYPTION_H

#include "encryption.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
///// @class cipher
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API cipher {
  virtual ~cipher() = default;

  virtual size_t block_size() const = 0;

  virtual bool encrypt(byte_type* data) const = 0;

  virtual bool decrypt(byte_type* data) const = 0;
}; // cipher

////////////////////////////////////////////////////////////////////////////////
///// @class ctr_encryption
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API ctr_encryption : public encryption {
 public:
  static const size_t DEFAULT_HEADER_LENGTH = 4096;
  static const size_t MIN_HEADER_LENGTH = sizeof(uint64_t);

  explicit ctr_encryption(const cipher& cipher) noexcept
   : cipher_(&cipher) {
  }

  virtual size_t header_length() noexcept override {
    return DEFAULT_HEADER_LENGTH;
  }

  virtual bool create_header(
    const std::string& filename,
    byte_type* header
  ) override;

  virtual stream::ptr create_stream(
    const std::string& filename,
    byte_type* header
  ) override;

 private:
  const cipher* cipher_;
}; // ctr_encryption

} // ROOT

#endif // IRESEARCH_CTR_ENCRYPTION_H

