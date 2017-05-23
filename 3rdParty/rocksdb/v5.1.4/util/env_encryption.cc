//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors

#include <algorithm>
#include <cctype>
#include <iostream>

#include "rocksdb/env_encryption.h"
#include "util/aligned_buffer.h"

namespace rocksdb {

class EncryptedSequentialFile : public SequentialFile {
  private:
    std::unique_ptr<SequentialFile> file_;
    std::unique_ptr<BlockAccessCipherStream> stream_;
    uint64_t offset_;

     public:
  EncryptedSequentialFile(SequentialFile* f, BlockAccessCipherStream* s)
      : file_(f), stream_(s), offset_(0) {
  }

  // Read up to "n" bytes from the file.  "scratch[0..n-1]" may be
  // written by this routine.  Sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully read).
  // May set "*result" to point at data in "scratch[0..n-1]", so
  // "scratch[0..n-1]" must be live when "*result" is used.
  // If an error was encountered, returns a non-OK status.
  //
  // REQUIRES: External synchronization
  virtual Status Read(size_t n, Slice* result, char* scratch) override {
    assert(scratch);
    Status status = file_->Read(n, result, scratch);
    if (!status.ok()) {
      return status;
    }
    status = stream_->Decrypt(offset_, (char*)result->data(), result->size());
    offset_ += result->size(); // We've already ready data from disk, so update offset_ even if decryption fails.
    return status;
  }

  // Skip "n" bytes from the file. This is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // If end of file is reached, skipping will stop at the end of the
  // file, and Skip will return OK.
  //
  // REQUIRES: External synchronization
  virtual Status Skip(uint64_t n) override {
    auto status = file_->Skip(n);
    if (!status.ok()) {
      return status;
    }
    offset_ += n;
    return status;
  }

  // Indicates the upper layers if the current SequentialFile implementation
  // uses direct IO.
  virtual bool use_direct_io() const override { 
    return file_->use_direct_io(); 
  }

  // Use the returned alignment value to allocate
  // aligned buffer for Direct I/O
  virtual size_t GetRequiredBufferAlignment() const override { 
    return file_->GetRequiredBufferAlignment(); 
  }

  // Remove any kind of caching of data from the offset to offset+length
  // of this file. If the length is 0, then it refers to the end of file.
  // If the system is not caching the file contents, then this is a noop.
  virtual Status InvalidateCache(size_t offset, size_t length) override {
    return file_->InvalidateCache(offset, length);
  }

  // Positioned Read for direct I/O
  // If Direct I/O enabled, offset, n, and scratch should be properly aligned
  virtual Status PositionedRead(uint64_t offset, size_t n, Slice* result, char* scratch) override {
    assert(scratch);
    auto status = file_->PositionedRead(offset, n, result, scratch);
    if (!status.ok()) {
      return status;
    }
    offset_ = offset + result->size();
    status = stream_->Decrypt(offset, (char*)result->data(), result->size());
    return status;
  }

};

// A file abstraction for randomly reading the contents of a file.
class EncryptedRandomAccessFile : public RandomAccessFile {
  private:
    std::unique_ptr<RandomAccessFile> file_;
    std::unique_ptr<BlockAccessCipherStream> stream_;

 public:
  EncryptedRandomAccessFile(RandomAccessFile* f, BlockAccessCipherStream* s)
    : file_(f), stream_(s) { }

  // Read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine.  Sets "*result"
  // to the data that was read (including if fewer than "n" bytes were
  // successfully read).  May set "*result" to point at data in
  // "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
  // "*result" is used.  If an error was encountered, returns a non-OK
  // status.
  //
  // Safe for concurrent use by multiple threads.
  // If Direct I/O enabled, offset, n, and scratch should be aligned properly.
  virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override {
    assert(scratch);
    auto status = file_->Read(offset, n, result, scratch);
    if (!status.ok()) {
      return status;
    }
    status = stream_->Decrypt(offset, (char*)result->data(), result->size());
    return status;
  }

  // Readahead the file starting from offset by n bytes for caching.
  /*virtual Status Prefetch(uint64_t offset, size_t n) override {
    //return Status::OK();
    return file_->Prefetch(offset, n);
  }*/

  // Tries to get an unique ID for this file that will be the same each time
  // the file is opened (and will stay the same while the file is open).
  // Furthermore, it tries to make this ID at most "max_size" bytes. If such an
  // ID can be created this function returns the length of the ID and places it
  // in "id"; otherwise, this function returns 0, in which case "id"
  // may not have been modified.
  //
  // This function guarantees, for IDs from a given environment, two unique ids
  // cannot be made equal to eachother by adding arbitrary bytes to one of
  // them. That is, no unique ID is the prefix of another.
  //
  // This function guarantees that the returned ID will not be interpretable as
  // a single varint.
  //
  // Note: these IDs are only valid for the duration of the process.
  virtual size_t GetUniqueId(char* id, size_t max_size) const override {
    return file_->GetUniqueId(id, max_size);
  };

  virtual void Hint(AccessPattern pattern) override {
    file_->Hint(pattern);
  }

  // Indicates the upper layers if the current RandomAccessFile implementation
  // uses direct IO.
  virtual bool use_direct_io() const override {
     return file_->use_direct_io(); 
  }

  // Use the returned alignment value to allocate
  // aligned buffer for Direct I/O
  virtual size_t GetRequiredBufferAlignment() const override { 
    return file_->GetRequiredBufferAlignment(); 
  }

  // Remove any kind of caching of data from the offset to offset+length
  // of this file. If the length is 0, then it refers to the end of file.
  // If the system is not caching the file contents, then this is a noop.
  virtual Status InvalidateCache(size_t offset, size_t length) override {
    return file_->InvalidateCache(offset, length);
  }
};

// A file abstraction for sequential writing.  The implementation
// must provide buffering since callers may append small fragments
// at a time to the file.
class EncryptedWritableFile : public WritableFileWrapper {
  private:
    std::unique_ptr<WritableFile> file_;
    std::unique_ptr<BlockAccessCipherStream> stream_;

 public:
  EncryptedWritableFile(WritableFile* f, BlockAccessCipherStream* s)
    : WritableFileWrapper(f), file_(f), stream_(s) { }

  Status Append(const Slice& data) override { 
    AlignedBuffer buf;
    Status status;
    Slice dataToAppend(data); 
    if (data.size() > 0) {
      auto offset = GetFileSize();
      // Encrypt in cloned buffer
      buf.Alignment(GetRequiredBufferAlignment());
      buf.AllocateNewBuffer(data.size());
      memmove(buf.BufferStart(), data.data(), data.size());
      status = stream_->Encrypt(offset, buf.BufferStart(), data.size());
      if (!status.ok()) {
        return status;
      }
      dataToAppend = Slice(buf.BufferStart(), data.size());
    }
    status = file_->Append(dataToAppend); 
    if (!status.ok()) {
      return status;
    }
    return status;
  }

  Status PositionedAppend(const Slice& data, uint64_t offset) override {
    AlignedBuffer buf;
    Status status;
    Slice dataToAppend(data); 
    if (data.size() > 0) {
      // Encrypt in cloned buffer
      buf.Alignment(GetRequiredBufferAlignment());
      buf.AllocateNewBuffer(data.size());
      memmove(buf.BufferStart(), data.data(), data.size());
      status = stream_->Encrypt(offset, buf.BufferStart(), data.size());
      if (!status.ok()) {
        return status;
      }
      dataToAppend = Slice(buf.BufferStart(), data.size());
    }
    status = file_->PositionedAppend(dataToAppend, offset);
    if (!status.ok()) {
      return status;
    }
    return status;
  }

  // Indicates the upper layers if the current WritableFile implementation
  // uses direct IO.
  virtual bool use_direct_io() const override { return file_->use_direct_io(); }

  // Use the returned alignment value to allocate
  // aligned buffer for Direct I/O
  virtual size_t GetRequiredBufferAlignment() const override { return file_->GetRequiredBufferAlignment(); } 
};

// A file abstraction for random reading and writing.
class EncryptedRandomRWFile : public RandomRWFile {
  private:
    std::unique_ptr<RandomRWFile> file_;
    std::unique_ptr<BlockAccessCipherStream> stream_;

 public:
  EncryptedRandomRWFile(RandomRWFile* f, BlockAccessCipherStream* s)
    : file_(f), stream_(s) {}

  // Indicates if the class makes use of direct I/O
  // If false you must pass aligned buffer to Write()
  virtual bool use_direct_io() const override { return file_->use_direct_io(); }

  // Use the returned alignment value to allocate
  // aligned buffer for Direct I/O
  virtual size_t GetRequiredBufferAlignment() const override { 
    return file_->GetRequiredBufferAlignment(); 
  }

  // Write bytes in `data` at  offset `offset`, Returns Status::OK() on success.
  // Pass aligned buffer when use_direct_io() returns true.
  virtual Status Write(uint64_t offset, const Slice& data) override {
    AlignedBuffer buf;
    Status status;
    Slice dataToWrite(data); 
    if (data.size() > 0) {
      // Encrypt in cloned buffer
      buf.Alignment(GetRequiredBufferAlignment());
      buf.AllocateNewBuffer(data.size());
      memmove(buf.BufferStart(), data.data(), data.size());
      status = stream_->Encrypt(offset, buf.BufferStart(), data.size());
      if (!status.ok()) {
        return status;
      }
      dataToWrite = Slice(buf.BufferStart(), data.size());
    }
    status = file_->Write(offset, dataToWrite);
    return status;
  }

  // Read up to `n` bytes starting from offset `offset` and store them in
  // result, provided `scratch` size should be at least `n`.
  // Returns Status::OK() on success.
  virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override { 
    assert(scratch);
    auto status = file_->Read(offset, n, result, scratch);
    if (!status.ok()) {
      return status;
    }
    status = stream_->Decrypt(offset, (char*)result->data(), result->size());
    return status;
  }

  virtual Status Flush() override {
    return file_->Flush();
  }

  virtual Status Sync() override {
    return file_->Sync();
  }

  virtual Status Fsync() override { 
    return file_->Fsync();
  }

  virtual Status Close() override {
    return file_->Close();
  }
};

// EncryptedEnv implements an Env wrapper that adds encryption to files stored on disk.
class EncryptedEnv : public EnvWrapper {
 public:
  EncryptedEnv(Env* base_env, EncryptionProvider *provider)
      : EnvWrapper(base_env) {
    provider_ = provider;
  }

  virtual Status NewSequentialFile(const std::string& fname,
                                   std::unique_ptr<SequentialFile>* result,
                                   const EnvOptions& options) override {
    result->reset();
    if (options.use_mmap_reads) {
      return Status::InvalidArgument();
    }
    std::unique_ptr<SequentialFile> underlying;
    auto status = EnvWrapper::NewSequentialFile(fname, &underlying, options);
    if (!status.ok()) {
      return status;
    }
    std::unique_ptr<BlockAccessCipherStream> stream;
    status = provider_->CreateCipherStream(fname, options, &stream);
    if (!status.ok()) {
      return status;
    }
    (*result) = std::unique_ptr<SequentialFile>(new EncryptedSequentialFile(underlying.release(), stream.release()));
    return Status::OK();
  }

  virtual Status NewRandomAccessFile(const std::string& fname,
                                     unique_ptr<RandomAccessFile>* result,
                                     const EnvOptions& options) override {
    result->reset();
    if (options.use_mmap_reads) {
      return Status::InvalidArgument();
    }
    std::unique_ptr<RandomAccessFile> underlying;
    auto status = EnvWrapper::NewRandomAccessFile(fname, &underlying, options);
    if (!status.ok()) {
      return status;
    }
    std::unique_ptr<BlockAccessCipherStream> stream;
    status = provider_->CreateCipherStream(fname, options, &stream);
    if (!status.ok()) {
      return status;
    }
    (*result) = std::unique_ptr<RandomAccessFile>(new EncryptedRandomAccessFile(underlying.release(), stream.release()));
    return Status::OK();
  }

  virtual Status NewWritableFile(const std::string& fname,
                                 unique_ptr<WritableFile>* result,
                                 const EnvOptions& options) override {
    result->reset();
    if (options.use_mmap_writes) {
      return Status::InvalidArgument();
    }
    std::unique_ptr<WritableFile> underlying;
    Status status = EnvWrapper::NewWritableFile(fname, &underlying, options);
    if (!status.ok()) {
      return status;
    }
    std::unique_ptr<BlockAccessCipherStream> stream;
    status = provider_->CreateCipherStream(fname, options, &stream);
    if (!status.ok()) {
      return status;
    }
    (*result) = std::unique_ptr<WritableFile>(new EncryptedWritableFile(underlying.release(), stream.release()));
    return Status::OK();
  }

  /*virtual Status ReopenWritableFile(const std::string& fname,
                                   unique_ptr<WritableFile>* result,
                                   const EnvOptions& options) override {
    result->reset();
    if (options.use_mmap_writes) {
      return Status::InvalidArgument();
    }
    std::unique_ptr<WritableFile> underlying;
    Status status = EnvWrapper::ReopenWritableFile(fname, &underlying, options);
    if (!status.ok()) {
      return status;
    }
    std::unique_ptr<BlockAccessCipherStream> stream;
    status = provider_->CreateCipherStream(fname, options, &stream);
    if (!status.ok()) {
      return status;
    }
    (*result) = std::unique_ptr<WritableFile>(new EncryptedWritableFile(underlying.release(), stream.release()));
    return Status::OK();
  }*/

  virtual Status ReuseWritableFile(const std::string& fname,
                                   const std::string& old_fname,
                                   unique_ptr<WritableFile>* result,
                                   const EnvOptions& options) override {
    result->reset();
    if (options.use_mmap_writes) {
      return Status::InvalidArgument();
    }
    std::unique_ptr<WritableFile> underlying;
    Status status = EnvWrapper::ReuseWritableFile(fname, old_fname, &underlying, options);
    if (!status.ok()) {
      return status;
    }
    std::unique_ptr<BlockAccessCipherStream> stream;
    status = provider_->CreateCipherStream(fname, options, &stream);
    if (!status.ok()) {
      return status;
    }
    (*result) = std::unique_ptr<WritableFile>(new EncryptedWritableFile(underlying.release(), stream.release()));
    return Status::OK();
  }

  virtual Status NewRandomRWFile(const std::string& fname,
                                 unique_ptr<RandomRWFile>* result,
                                 const EnvOptions& options) override {
    result->reset();
    if (options.use_mmap_reads || options.use_mmap_writes) {
      return Status::InvalidArgument();
    }
    std::unique_ptr<RandomRWFile> underlying;
    Status status = EnvWrapper::NewRandomRWFile(fname, &underlying, options);
    if (!status.ok()) {
      return status;
    }
    std::unique_ptr<BlockAccessCipherStream> stream;
    status = provider_->CreateCipherStream(fname, options, &stream);
    if (!status.ok()) {
      return status;
    }
    (*result) = std::unique_ptr<RandomRWFile>(new EncryptedRandomRWFile(underlying.release(), stream.release()));
    return Status::OK();
  }

 private:
  EncryptionProvider *provider_;
};


// Returns an Env that encrypts data when stored on disk and decrypts data when 
// read from disk.
Env* NewEncryptedEnv(Env* base_env, EncryptionProvider* provider) {
  return new EncryptedEnv(base_env, provider);
}

// Encrypt one or more (partial) blocks of data at the file offset.
// Length of data is given in dataSize.
Status BlockAccessCipherStream::Encrypt(uint64_t fileOffset, char *data, size_t dataSize) {
  // Calculate block index
  auto blockSize = BlockSize();
  uint64_t blockIndex = fileOffset / blockSize;
  size_t blockOffset = fileOffset % blockSize;
  unique_ptr<char[]> blockBuffer;

  std::string scratch;
  AllocateScratch(scratch);

  // Encrypt individual blocks.
  while (1) {
    char *block = data;
    size_t n = std::min(dataSize, blockSize - blockOffset);
    if (n != blockSize) {
      // We're not encrypting a full block. 
      // Copy data to blockBuffer
      if (!blockBuffer.get()) {
        // Allocate buffer 
        blockBuffer = unique_ptr<char[]>(new char[blockSize]);
      }
      block = blockBuffer.get();
      // Copy plain data to block buffer 
      memmove(block + blockOffset, data, n);
    }
    auto status = EncryptBlock(blockIndex, block, (char*)scratch.data());
    if (!status.ok()) {
      return status;
    }
    if (block != data) {
      // Copy encrypted data back to `data`.
      memmove(data, block + blockOffset, n);
    }
    dataSize -= n;
    if (dataSize == 0) {
      return Status::OK();
    }
    data += n;
    blockOffset = 0;
    blockIndex++;
  }
}

// Decrypt one or more (partial) blocks of data at the file offset.
// Length of data is given in dataSize.
Status BlockAccessCipherStream::Decrypt(uint64_t fileOffset, char *data, size_t dataSize) {
  // Calculate block index
  auto blockSize = BlockSize();
  uint64_t blockIndex = fileOffset / blockSize;
  size_t blockOffset = fileOffset % blockSize;
  unique_ptr<char[]> blockBuffer;

  std::string scratch;
  AllocateScratch(scratch);

  // Decrypt individual blocks.
  while (1) {
    char *block = data;
    size_t n = std::min(dataSize, blockSize - blockOffset);
    if (n != blockSize) {
      // We're not decrypting a full block. 
      // Copy data to blockBuffer
      if (!blockBuffer.get()) {
        // Allocate buffer 
        blockBuffer = unique_ptr<char[]>(new char[blockSize]);
      }
      block = blockBuffer.get();
      // Copy encrypted data to block buffer 
      memmove(block + blockOffset, data, n);
    }
    auto status = DecryptBlock(blockIndex, block, (char*)scratch.data());
    if (!status.ok()) {
      return status;
    }
    if (block != data) {
      // Copy decrypted data back to `data`.
      memmove(data, block + blockOffset, n);
    }
    dataSize -= n;
    if (dataSize == 0) {
      return Status::OK();
    }
    data += n;
    blockOffset = 0;
    blockIndex++;
  }
}

// Encrypt a block of data.
// Length of data is equal to BlockSize().
Status ROT13BlockCipher::Encrypt(char *data) {
  for (size_t i = 0; i < blockSize_; ++i) {
      data[i] += 13;
  }
  return Status::OK();
}

// Decrypt a block of data.
// Length of data is equal to BlockSize().
Status ROT13BlockCipher::Decrypt(char *data) {
  return Encrypt(data);
}

// Allocate scratch space which is passed to EncryptBlock/DecryptBlock.
void CTRCipherStream::AllocateScratch(std::string& scratch) {
  auto blockSize = cipher_.BlockSize();
  scratch.reserve(blockSize);
}

// Encrypt a block of data at the given block index.
// Length of data is equal to BlockSize();
Status CTRCipherStream::EncryptBlock(uint64_t blockIndex, char *data, char* scratch) {

  // Create nonce + counter
  auto blockSize = cipher_.BlockSize();
  memmove(scratch, iv_, blockSize);
  *((uint64_t*)scratch) = blockIndex;

  // Encrypt nonce+counter 
  auto status = cipher_.Encrypt(scratch);
  if (!status.ok()) {
    return status;
  }

  // XOR data with ciphertext.
  for (size_t i = 0; i < blockSize; i++) {
    data[i] = data[i] ^ scratch[i];
  }
  return Status::OK();
}

// Decrypt a block of data at the given block index.
// Length of data is equal to BlockSize();
Status CTRCipherStream::DecryptBlock(uint64_t blockIndex, char *data, char* scratch) {
  // For CTR decryption & encryption are the same 
  return EncryptBlock(blockIndex, data, scratch);
}

Status CTREncryptionProvider::CreateCipherStream(const std::string& fname, const EnvOptions& options, unique_ptr<BlockAccessCipherStream>* result) {
  //std::cout << "CreateCipherStream for " << fname << "\n";
  (*result) = unique_ptr<BlockAccessCipherStream>(new CTRCipherStream(cipher_, iv_));
  return Status::OK();
}



}  // namespace rocksdb