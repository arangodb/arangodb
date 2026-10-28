////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Random/RandomGenerator.h"

#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/EncryptionProvider.h"
#endif

using namespace arangodb;

#ifdef USE_ENTERPRISE

namespace {
static const char* SAMPLE_KEY = "01234567890123456789012345678901";
}

TEST(EncryptionProviderTest, microbenchmark) {
  // openssl EVP AES-256-CTR mode
  arangodb::enterprise::EncryptionProvider evpprovider(
      rocksdb::Slice(SAMPLE_KEY, 16384));

  // this is the hand-rolled CTR mode with the openssl software-only AES_encrypt
  auto cipher = std::make_shared<arangodb::enterprise::AES256BlockCipher>(
      rocksdb::Slice(SAMPLE_KEY, 16384));
  rocksdb::CTREncryptionProvider softprovider(cipher);

  ASSERT_EQ(evpprovider.GetPrefixLength(), softprovider.GetPrefixLength());

  std::string prefix;
  prefix.resize(softprovider.GetPrefixLength());

  rocksdb::EnvOptions opts;

  constexpr size_t kBuffSize = (1 << 20) * 16;
  std::vector<char> buffer;
  buffer.resize(kBuffSize);
  for (size_t i = 0; i < kBuffSize; i++) {
    buffer[i] = RandomGenerator::interval(0, 255);
  }

  softprovider.CreateNewPrefix("", prefix.data(), prefix.size());
  rocksdb::Slice prefixSlice(prefix);

  std::unique_ptr<rocksdb::BlockAccessCipherStream> streamEVP, streamSf;
  ASSERT_TRUE(
      evpprovider.CreateCipherStream("", opts, prefixSlice, &streamEVP).ok());
  ASSERT_TRUE(
      softprovider.CreateCipherStream("", opts, prefixSlice, &streamSf).ok());
  ASSERT_NE(streamEVP, nullptr);
  ASSERT_NE(streamSf, nullptr);

  std::cout << "Encrypting 16MB blocks of memory with AES-256-CTR\n"
            << "Benchmarking Intel NI accelerated variant..." << std::endl;

  const int reps = 128;
  std::cout << "\nBenchmarking AES_encrypt only variant..." << std::endl;

  auto start = std::chrono::steady_clock::now();
  for (int x = 0; x < reps; x++) {
    // encrypt data at offset
    ASSERT_TRUE(streamSf->Encrypt(0, buffer.data(), kBuffSize).ok());
  }
  auto duration = std::chrono::steady_clock::now() - start;
  auto durationSf = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto avgDurationSf =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration / reps);

  std::cout << "------------------------------" << std::endl;
  std::cout << "Algorithm\tTotal\tAvg" << std::endl;
  std::cout << "AES_encrypt\t" << durationSf.count() << "s\t"
            << avgDurationSf.count() << "ms" << std::endl;
  std::cout << "------------------------------" << std::endl;
  std::cout << "\nBenchmarking OpenSSL EVP variant..." << std::endl;

  start = std::chrono::steady_clock::now();
  for (int x = 0; x < reps; x++) {
    // encrypt data at offset
    ASSERT_TRUE(streamEVP->Encrypt(0, buffer.data(), kBuffSize).ok());
  }
  duration = std::chrono::steady_clock::now() - start;
  auto durationEVP = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto avgDurationEVP =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration / reps);

  std::cout << "------------------------------" << std::endl;
  std::cout << "Algorithm\tTotal\tAvg" << std::endl;
  std::cout << "AES_encrypt\t" << durationSf.count() << "s\t"
            << avgDurationSf.count() << "ms" << std::endl;
  std::cout << "OpenSSL EVP\t" << durationEVP.count() << "s\t"
            << avgDurationEVP.count() << "ms" << std::endl;
  std::cout << "------------------------------" << std::endl;
}

#endif
