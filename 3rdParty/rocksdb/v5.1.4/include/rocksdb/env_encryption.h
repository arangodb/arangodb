//  Copyright (c) 2016-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.

#pragma once

#if !defined(ROCKSDB_LITE) 

#include <string>

#include "env.h"

namespace rocksdb {

class EncryptionProvider;

// Returns an Env that encrypts data when stored on disk and decrypts data when 
// read from disk.
Env* NewEncryptedEnv(Env* base_env, EncryptionProvider* provider);

// BlockAccessCipherStream is the base class for any cipher stream that 
// supports random access at block level (without requiring data from other blocks).
// E.g. CTR (Counter operation mode) supports this requirement.
class BlockAccessCipherStream {
    public:
      virtual ~BlockAccessCipherStream() {};

      // BlockSize returns the size of each block supported by this cipher stream.
      virtual size_t BlockSize() = 0;

      // Encrypt one or more (partial) blocks of data at the file offset.
      // Length of data is given in dataSize.
      virtual Status Encrypt(uint64_t fileOffset, char *data, size_t dataSize);

      // Decrypt one or more (partial) blocks of data at the file offset.
      // Length of data is given in dataSize.
      virtual Status Decrypt(uint64_t fileOffset, char *data, size_t dataSize);

    protected:
      // Allocate scratch space which is passed to EncryptBlock/DecryptBlock.
      virtual void AllocateScratch(std::string&) = 0;

      // Encrypt a block of data at the given block index.
      // Length of data is equal to BlockSize();
      virtual Status EncryptBlock(uint64_t blockIndex, char *data, char* scratch) = 0;

      // Decrypt a block of data at the given block index.
      // Length of data is equal to BlockSize();
      virtual Status DecryptBlock(uint64_t blockIndex, char *data, char* scratch) = 0;
};

// BlockCipher 
class BlockCipher {
    public:
      virtual ~BlockCipher() {};

      // BlockSize returns the size of each block supported by this cipher stream.
      virtual size_t BlockSize() = 0;

      // Encrypt a block of data.
      // Length of data is equal to BlockSize().
      virtual Status Encrypt(char *data) = 0;

      // Decrypt a block of data.
      // Length of data is equal to BlockSize().
      virtual Status Decrypt(char *data) = 0;
};

// Implements a BlockCipher using ROT13.
class ROT13BlockCipher : public BlockCipher {
    private: 
      size_t blockSize_;
    public:
      ROT13BlockCipher(size_t blockSize) 
        : blockSize_(blockSize) {}
      virtual ~ROT13BlockCipher() {};

      // BlockSize returns the size of each block supported by this cipher stream.
      virtual size_t BlockSize() override { return blockSize_; }

      // Encrypt a block of data.
      // Length of data is equal to BlockSize().
      virtual Status Encrypt(char *data) override;

      // Decrypt a block of data.
      // Length of data is equal to BlockSize().
      virtual Status Decrypt(char *data) override;
};

// CTRCipherStream implements BlockAccessCipherStream using an 
// Counter operations mode. 
// See https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation
class CTRCipherStream final : public BlockAccessCipherStream {
    private:
      BlockCipher& cipher_;
      const char* iv_;
    public:
      CTRCipherStream(BlockCipher& c, const char *iv) 
        : cipher_(c), iv_(iv) {};
      virtual ~CTRCipherStream() {};

      // BlockSize returns the size of each block supported by this cipher stream.
      virtual size_t BlockSize() override { return cipher_.BlockSize(); }

    protected:
      // Allocate scratch space which is passed to EncryptBlock/DecryptBlock.
      virtual void AllocateScratch(std::string&) override;

      // Encrypt a block of data at the given block index.
      // Length of data is equal to BlockSize();
      virtual Status EncryptBlock(uint64_t blockIndex, char *data, char *scratch) override;

      // Decrypt a block of data at the given block index.
      // Length of data is equal to BlockSize();
      virtual Status DecryptBlock(uint64_t blockIndex, char *data, char *scratch) override;
};

// The encryption provider is used to create a cipher stream for a specific file.
// The returned cipher stream will be used for actual encryption/decryption 
// actions.
class EncryptionProvider {
 public:
    virtual ~EncryptionProvider() {};

    // CreateCipherStream creates a block access cipher stream for a file given
    // given name and options.
    virtual Status CreateCipherStream(const std::string& fname, const EnvOptions& options,
                                 unique_ptr<BlockAccessCipherStream>* result) = 0;
};

// This encryption provider uses a CTR cipher stream, with a given block cipher 
// and IV.
class CTREncryptionProvider : public EncryptionProvider {
    private:
      BlockCipher& cipher_;
      const char* iv_;
 public:
      CTREncryptionProvider(BlockCipher& c, const char *iv) 
        : cipher_(c), iv_(iv) {};
    virtual ~CTREncryptionProvider() {}

    // CreateCipherStream creates a block access cipher stream for a file given
    // given name and options.
    virtual Status CreateCipherStream(const std::string& fname, const EnvOptions& options,
                                 unique_ptr<BlockAccessCipherStream>* result) override;
};

}  // namespace rocksdb

#endif  // !defined(ROCKSDB_LITE)