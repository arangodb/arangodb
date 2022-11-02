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
/// @author Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <string_view>

#include "Nonce.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace {
// protects access to nonces
Mutex mutex;

std::unique_ptr<uint32_t[]> nonces;
}  // namespace

namespace arangodb::basics::Nonce {

std::string createNonce() {
  uint32_t timestamp = static_cast<uint32_t>(time(nullptr));
  uint32_t rand1 = RandomGenerator::interval(UINT32_MAX);
  uint32_t rand2 = RandomGenerator::interval(UINT32_MAX);

  uint8_t buffer[12];

  buffer[0] = (timestamp >> 24) & 0xFF;
  buffer[1] = (timestamp >> 16) & 0xFF;
  buffer[2] = (timestamp >> 8) & 0xFF;
  buffer[3] = (timestamp)&0xFF;

  memcpy(buffer + 4, &rand1, 4);
  memcpy(buffer + 8, &rand2, 4);

  return StringUtils::encodeBase64U(
      std::string_view(reinterpret_cast<char const*>(&buffer[0]), 12));
}

bool checkAndMark(std::string const& nonce) {
  if (nonce.size() != 12) {
    return false;
  }

  uint8_t const* buffer = (uint8_t const*)nonce.data();

  uint32_t timestamp = (uint32_t(buffer[0]) << 24) |
                       (uint32_t(buffer[1]) << 16) |
                       (uint32_t(buffer[2]) << 8) | uint32_t(buffer[3]);

  uint64_t random = (uint64_t(buffer[4]) << 56) | (uint64_t(buffer[5]) << 48) |
                    (uint64_t(buffer[6]) << 40) | (uint64_t(buffer[7]) << 32) |
                    (uint64_t(buffer[8]) << 24) | (uint64_t(buffer[9]) << 16) |
                    (uint64_t(buffer[10]) << 8) | uint64_t(buffer[11]);

  return checkAndMark(timestamp, random);
}

bool checkAndMark(uint32_t timestamp, uint64_t random) {
  MUTEX_LOCKER(mutexLocker, ::mutex);

  // allocate nonces buffer lazily upon first access
  if (::nonces == nullptr) {
    LOG_TOPIC("5a658", TRACE, arangodb::Logger::FIXME)
        << "creating nonces with size: " << numNonces;
    ::nonces = std::make_unique<uint32_t[]>(numNonces);

    memset(::nonces.get(), 0, sizeof(uint32_t) * numNonces);
  }

  TRI_ASSERT(::nonces != nullptr);
  uint32_t* allNonces = ::nonces.get();

  int proofs = 0;

  // first count to avoid miscounts if two hashes are equal
  if (timestamp > allNonces[random % (numNonces - 3)]) {
    proofs++;
  }

  if (timestamp > allNonces[random % (numNonces - 17)]) {
    proofs++;
  }

  if (timestamp > allNonces[random % (numNonces - 33)]) {
    proofs++;
  }

  if (timestamp > allNonces[random % (numNonces - 63)]) {
    proofs++;
  }

  // statistics, compute the log2 of the age and increment the proofs count
  uint32_t now = static_cast<uint32_t>(time(nullptr));
  uint32_t age = 1;

  if (timestamp < now) {
    age = now - timestamp;
  }

  uint32_t l2age = 0;

  while (1 < age) {
    l2age += 1;
    age >>= 1;
  }

  LOG_TOPIC("562a6", TRACE, arangodb::Logger::FIXME)
      << "age of timestamp " << timestamp << " is " << age << " (log " << l2age
      << ")";

  // mark the nonce as used
  if (timestamp > allNonces[random % (numNonces - 3)]) {
    allNonces[random % (numNonces - 3)] = timestamp;
  }

  if (timestamp > allNonces[random % (numNonces - 17)]) {
    allNonces[random % (numNonces - 17)] = timestamp;
  }

  if (timestamp > allNonces[random % (numNonces - 33)]) {
    allNonces[random % (numNonces - 33)] = timestamp;
  }

  if (timestamp > allNonces[random % (numNonces - 63)]) {
    allNonces[random % (numNonces - 63)] = timestamp;
  }

  return 0 < proofs;
}

}  // namespace arangodb::basics::Nonce
