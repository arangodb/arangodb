////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBFormat.h"

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace {
// little endian
inline uint16_t uint16FromPersistentLE(char const* p) {
  return arangodb::rocksutils::uintFromPersistentLittleEndian<uint16_t>(p);
}
inline uint32_t uint32FromPersistentLE(char const* p) {
  return arangodb::rocksutils::uintFromPersistentLittleEndian<uint32_t>(p);
}
inline uint64_t uint64FromPersistentLE(char const* p) {
  return arangodb::rocksutils::uintFromPersistentLittleEndian<uint64_t>(p);
}
inline void uint16ToPersistentLE(std::string& p, uint16_t value) {
  arangodb::rocksutils::uintToPersistentLittleEndian<uint16_t>(p, value);
}
inline void uint32ToPersistentLE(std::string& p, uint32_t value) {
  arangodb::rocksutils::uintToPersistentLittleEndian<uint32_t>(p, value);
}
inline void uint64ToPersistentLE(std::string& p, uint64_t value) {
  arangodb::rocksutils::uintToPersistentLittleEndian<uint64_t>(p, value);
}
// Big endian
inline uint16_t uint16FromPersistentBE(char const* p) {
  return arangodb::rocksutils::uintFromPersistentBigEndian<uint16_t>(p);
}
inline uint32_t uint32FromPersistentBE(char const* p) {
  return arangodb::rocksutils::uintFromPersistentBigEndian<uint32_t>(p);
}
inline uint64_t uint64FromPersistentBE(char const* p) {
  return arangodb::rocksutils::uintFromPersistentBigEndian<uint64_t>(p);
}
inline void uint16ToPersistentBE(std::string& p, uint16_t value) {
  arangodb::rocksutils::uintToPersistentBigEndian<uint16_t>(p, value);
}
inline void uint32ToPersistentBE(std::string& p, uint32_t value) {
  arangodb::rocksutils::uintToPersistentBigEndian<uint32_t>(p, value);
}
inline void uint64ToPersistentBE(std::string& p, uint64_t value) {
  arangodb::rocksutils::uintToPersistentBigEndian<uint64_t>(p, value);
}
}  // namespace

namespace arangodb {
namespace rocksutils {

RocksDBEndianness rocksDBEndianness = RocksDBEndianness::Invalid;

uint16_t (*uint16FromPersistent)(char const* p) = nullptr;
uint32_t (*uint32FromPersistent)(char const* p) = nullptr;
uint64_t (*uint64FromPersistent)(char const* p) = nullptr;

void (*uint16ToPersistent)(std::string& p, uint16_t value) = nullptr;
void (*uint32ToPersistent)(std::string& p, uint32_t value) = nullptr;
void (*uint64ToPersistent)(std::string& p, uint64_t value) = nullptr;

void setRocksDBKeyFormatEndianess(RocksDBEndianness e) {
  rocksDBEndianness = e;

  if (e == RocksDBEndianness::Little) {
    LOG_TOPIC("799b9", DEBUG, Logger::ENGINES) << "using little-endian keys";
    uint16FromPersistent = &uint16FromPersistentLE;
    uint32FromPersistent = &uint32FromPersistentLE;
    uint64FromPersistent = &uint64FromPersistentLE;
    uint16ToPersistent = &uint16ToPersistentLE;
    uint32ToPersistent = &uint32ToPersistentLE;
    uint64ToPersistent = &uint64ToPersistentLE;
    return;
  } else if (e == RocksDBEndianness::Big) {
    LOG_TOPIC("5e446", DEBUG, Logger::ENGINES) << "using big-endian keys";
    uint16FromPersistent = &uint16FromPersistentBE;
    uint32FromPersistent = &uint32FromPersistentBE;
    uint64FromPersistent = &uint64FromPersistentBE;
    uint16ToPersistent = &uint16ToPersistentBE;
    uint32ToPersistent = &uint32ToPersistentBE;
    uint64ToPersistent = &uint64ToPersistentBE;
    return;
  }
  LOG_TOPIC("b8243", FATAL, Logger::ENGINES) << "Invalid key endianness";
  FATAL_ERROR_EXIT();
}

}  // namespace rocksutils
}  // namespace arangodb
