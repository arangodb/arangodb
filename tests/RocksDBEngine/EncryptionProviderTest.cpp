////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Random/RandomGenerator.h"

#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include <velocypack/velocypack-aliases.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/EncryptionProvider.h"
#endif

using namespace arangodb;

#ifdef USE_ENTERPRISE

namespace {
static const char* SAMPLE_KEY = "01234567890123456789012345678901";
}

TEST(EncryptionProviderTest, simple) {
  
  // hand rolled AES-256-CTR mode
  arangodb::enterprise::EncryptionProvider hwprovider(rocksdb::Slice(SAMPLE_KEY, 32),
                                                      /*allowAcceleration*/true);
  
  // openssl EVP variant
  arangodb::enterprise::EncryptionProvider evpprovider(rocksdb::Slice(SAMPLE_KEY, 32), /*allow*/false);

  // hand-rolled CTR mode with the openssl software-only AES_encrypt
  arangodb::enterprise::AES256BlockCipher cipher(rocksdb::Slice(SAMPLE_KEY, 32));
  rocksdb::CTREncryptionProvider softprovider(cipher);
  
  ASSERT_EQ(hwprovider.GetPrefixLength(), softprovider.GetPrefixLength());
  
  std::string prefix;
  prefix.resize(softprovider.GetPrefixLength());
  
  rocksdb::EnvOptions opts;
  
  constexpr size_t kBuffSize = 1<<14;
    
  std::array<char, kBuffSize> buffer1, buffer2, buffer3, buffer4;
  
  for (int x = 0; x < 64; x++) {
    for (size_t i = 0; i < kBuffSize; i++) {
      buffer1[i] = buffer2[i] = buffer3[i] = buffer4[i] = RandomGenerator::interval(0, 255);
    }
    
    size_t offset = RandomGenerator::interval(0, kBuffSize / 2);
    size_t len = RandomGenerator::interval(0, static_cast<std::int32_t>(kBuffSize - offset));
    
    softprovider.CreateNewPrefix("", prefix.data(), prefix.size());
    rocksdb::Slice prefixSlice(prefix);
    
    std::unique_ptr<rocksdb::BlockAccessCipherStream> streamIntel, streamEVP, streamSW;
    ASSERT_TRUE(hwprovider.CreateCipherStream("", opts, prefixSlice, &streamIntel).ok());
    ASSERT_TRUE(evpprovider.CreateCipherStream("", opts, prefixSlice, &streamEVP).ok());
    ASSERT_TRUE(softprovider.CreateCipherStream("", opts, prefixSlice, &streamSW).ok());

    ASSERT_NE(streamIntel, nullptr);
    ASSERT_NE(streamEVP, nullptr);
    ASSERT_NE(streamSW, nullptr);
    
    // encryption sanity check
    ASSERT_TRUE(streamIntel->Encrypt(0, buffer1.data(), 16).ok());
    ASSERT_TRUE(streamEVP->Encrypt(0, buffer2.data(), 16).ok());
    ASSERT_TRUE(streamSW->Encrypt(0, buffer3.data(), 16).ok());
    ASSERT_EQ(memcmp(buffer1.data(), buffer2.data(), 16), 0);
    ASSERT_EQ(memcmp(buffer2.data(), buffer3.data(), 16), 0);
    // decryption sanity check
    ASSERT_TRUE(streamIntel->Decrypt(0, buffer1.data(), 16).ok());
    ASSERT_TRUE(streamEVP->Decrypt(0, buffer2.data(), 16).ok());
    ASSERT_TRUE(streamSW->Decrypt(0, buffer3.data(), 16).ok());
    ASSERT_EQ(memcmp(buffer1.data(), buffer2.data(), 16), 0);
    ASSERT_EQ(memcmp(buffer2.data(), buffer3.data(), 16), 0);

    // encrypt data at offset
    ASSERT_TRUE(streamIntel->Encrypt(offset, buffer1.data() + offset, len).ok());
    ASSERT_TRUE(streamEVP->Encrypt(offset, buffer2.data() + offset, len).ok());
    ASSERT_TRUE(streamSW->Encrypt(offset, buffer3.data() + offset, len).ok());

    // check encrypted data
    ASSERT_EQ(memcmp(buffer1.data() + offset, buffer2.data() + offset, len), 0);
    ASSERT_EQ(memcmp(buffer2.data() + offset, buffer3.data() + offset, len), 0);

    // decrypt data at offset
    ASSERT_TRUE(streamIntel->Decrypt(offset, buffer1.data() + offset, len).ok());
    ASSERT_TRUE(streamEVP->Decrypt(offset, buffer2.data() + offset, len).ok());
    ASSERT_TRUE(streamSW->Decrypt(offset, buffer3.data() + offset, len).ok());

    // check is it equal to original
    ASSERT_EQ(memcmp(buffer1.data(), buffer2.data(), kBuffSize), 0);
    ASSERT_EQ(memcmp(buffer2.data(), buffer3.data(), kBuffSize), 0);
    ASSERT_EQ(memcmp(buffer3.data(), buffer4.data(), kBuffSize), 0);
  }
}

TEST(EncryptionProviderTest, microbenchmark) {
  
  // hand rolled AES-256-CTR mode
  arangodb::enterprise::EncryptionProvider hwprovider(rocksdb::Slice(SAMPLE_KEY, 32),
                                                      /*allowAcceleration*/true);
  
  // openssl EVP AES-256-CTR mode
  arangodb::enterprise::EncryptionProvider evpprovider(rocksdb::Slice(SAMPLE_KEY, 32), /*allow*/false);

  // this is the hand-rolled CTR mode with the openssl software-only AES_encrypt
  arangodb::enterprise::AES256BlockCipher cipher(rocksdb::Slice(SAMPLE_KEY, 32));
  rocksdb::CTREncryptionProvider softprovider(cipher);
  
  ASSERT_EQ(hwprovider.GetPrefixLength(), softprovider.GetPrefixLength());
  ASSERT_EQ(evpprovider.GetPrefixLength(), softprovider.GetPrefixLength());

  std::string prefix;
  prefix.resize(softprovider.GetPrefixLength());
  
  rocksdb::EnvOptions opts;
  
  constexpr size_t kBuffSize = (1<<20) * 16;
  std::vector<char> buffer;
  buffer.resize(kBuffSize);
  for (size_t i = 0; i < kBuffSize; i++) {
    buffer[i] = RandomGenerator::interval(0, 255);
  }

  softprovider.CreateNewPrefix("", prefix.data(), prefix.size());
  rocksdb::Slice prefixSlice(prefix);
  
  std::unique_ptr<rocksdb::BlockAccessCipherStream> streamIntel, streamEVP, streamSf;
  ASSERT_TRUE(hwprovider.CreateCipherStream("", opts, prefixSlice, &streamIntel).ok());
  ASSERT_TRUE(evpprovider.CreateCipherStream("", opts, prefixSlice, &streamEVP).ok());
  ASSERT_TRUE(softprovider.CreateCipherStream("", opts, prefixSlice, &streamSf).ok());
  ASSERT_NE(streamIntel, nullptr);
  ASSERT_NE(streamEVP, nullptr);
  ASSERT_NE(streamSf, nullptr);

  std::cout << "Encrypting 16MB blocks of memory with AES-256-CTR\n"
            << "Benchmarking Intel NI accelerated variant..." << std::endl;

  const int reps = 128;
  auto start = std::chrono::steady_clock::now();
  for (int x = 0; x < reps; x++) {
    // encrypt data at offset
    ASSERT_TRUE(streamIntel->Encrypt(0, buffer.data(), kBuffSize).ok());
  }
  auto duration = std::chrono::steady_clock::now() - start;
  auto durationHw = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto avgDurationHw = std::chrono::duration_cast<std::chrono::milliseconds>(duration / reps);
  
  std::cout << "------------------------------" << std::endl;
  std::cout << "Algorithm\tTotal\tAvg" << std::endl;
  std::cout << "Intel NI\t" << durationHw.count() << "s\t"
            << avgDurationHw.count() << "ms" << std::endl;
  std::cout << "------------------------------" << std::endl;;
  std::cout << "\nBenchmarking AES_encrypt only variant..." << std::endl;

  start = std::chrono::steady_clock::now();
  for (int x = 0; x < reps; x++) {
    // encrypt data at offset
    ASSERT_TRUE(streamSf->Encrypt(0, buffer.data(), kBuffSize).ok());
  }
  duration = std::chrono::steady_clock::now() - start;
  auto durationSf = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto avgDurationSf = std::chrono::duration_cast<std::chrono::milliseconds>(duration / reps);
  
  std::cout << "------------------------------" << std::endl;
  std::cout << "Algorithm\tTotal\tAvg" << std::endl;
  std::cout << "AES_encrypt\t" << durationSf.count() << "s\t"
            << avgDurationSf.count() << "ms" << std::endl;
  std::cout << "Intel NI\t" << durationHw.count() << "s\t"
            << avgDurationHw.count() << "ms" << std::endl;
  std::cout << "------------------------------" << std::endl;
  std::cout << "\nBenchmarking OpenSSL EVP variant..." << std::endl;
  
  start = std::chrono::steady_clock::now();
  for (int x = 0; x < reps; x++) {
    // encrypt data at offset
    ASSERT_TRUE(streamEVP->Encrypt(0, buffer.data(), kBuffSize).ok());
  }
  duration = std::chrono::steady_clock::now() - start;
  auto durationEVP = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto avgDurationEVP = std::chrono::duration_cast<std::chrono::milliseconds>(duration / reps);
  
  std::cout << "------------------------------" << std::endl;
  std::cout << "Algorithm\tTotal\tAvg" << std::endl;
  std::cout << "AES_encrypt\t" << durationSf.count() << "s\t"
            << avgDurationSf.count() << "ms" << std::endl;
  std::cout << "Intel NI\t" << durationHw.count() << "s\t"
            << avgDurationHw.count() << "ms" << std::endl;
  std::cout << "OpenSSL EVP\t" << durationEVP.count() << "s\t"
  << avgDurationEVP.count() << "ms" << std::endl;
  std::cout << "------------------------------" << std::endl;
}

#endif
