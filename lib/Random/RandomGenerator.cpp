////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RandomGenerator.h"

#include <chrono>
#include <random>

#ifdef _WIN32
#include <Wincrypt.h>
#endif

#include "Basics/Exceptions.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/Thread.h"
#include "Basics/hashes.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// RandomDevice
// -----------------------------------------------------------------------------

unsigned long RandomDevice::seed() {

  // Random device ---
  size_t dev = std::random_device()();

  // Thread ID -------
  auto tid =  std::hash<std::thread::id>()(std::this_thread::get_id());

  // Time now --------
  for (unsigned short i = 0; i < 50; ++i) {
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
  }
  auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::high_resolution_clock::now().time_since_epoch()).count();

  return (unsigned long)(dev + tid + now);
  
}

int32_t RandomDevice::interval(int32_t left, int32_t right) {
  return random(left, right);
}

uint32_t RandomDevice::interval(uint32_t left, uint32_t right) {
  int32_t l = left + INT32_MIN;
  int32_t r = right + INT32_MIN;

  return static_cast<uint32_t>(static_cast<int64_t>(random(l, r)) - INT32_MIN);
}

int32_t RandomDevice::random(int32_t left, int32_t right) {
  if (left >= right) {
    return left;
  }

  if (left == INT32_MIN && right == INT32_MAX) {
    return static_cast<int32_t>(random());
  }

  uint32_t range = static_cast<uint32_t>(right - left + 1);
  
  switch (range) {
    case 0x00000002:
      return power2(left, 0x00000002 - 1);
    case 0x00000004:
      return power2(left, 0x00000004 - 1);
    case 0x00000008:
      return power2(left, 0x00000008 - 1);
    case 0x00000010:
      return power2(left, 0x00000010 - 1);
    case 0x00000020:
      return power2(left, 0x00000020 - 1);
    case 0x00000040:
      return power2(left, 0x00000040 - 1);
    case 0x00000080:
      return power2(left, 0x00000080 - 1);
    case 0x00000100:
      return power2(left, 0x00000100 - 1);
    case 0x00000200:
      return power2(left, 0x00000200 - 1);
    case 0x00000400:
      return power2(left, 0x00000400 - 1);
    case 0x00000800:
      return power2(left, 0x00000800 - 1);
    case 0x00001000:
      return power2(left, 0x00001000 - 1);
    case 0x00002000:
      return power2(left, 0x00002000 - 1);
    case 0x00004000:
      return power2(left, 0x00004000 - 1);
    case 0x00008000:
      return power2(left, 0x00008000 - 1);
    case 0x00010000:
      return power2(left, 0x00010000 - 1);
    case 0x00020000:
      return power2(left, 0x00020000 - 1);
    case 0x00040000:
      return power2(left, 0x00040000 - 1);
    case 0x00080000:
      return power2(left, 0x00080000 - 1);
    case 0x00100000:
      return power2(left, 0x00100000 - 1);
    case 0x00200000:
      return power2(left, 0x00200000 - 1);
    case 0x00400000:
      return power2(left, 0x00400000 - 1);
    case 0x00800000:
      return power2(left, 0x00800000 - 1);
    case 0x01000000:
      return power2(left, 0x01000000 - 1);
    case 0x02000000:
      return power2(left, 0x02000000 - 1);
    case 0x04000000:
      return power2(left, 0x04000000 - 1);
    case 0x08000000:
      return power2(left, 0x08000000 - 1);
    case 0x10000000:
      return power2(left, 0x10000000 - 1);
    case 0x20000000:
      return power2(left, 0x20000000 - 1);
    case 0x40000000:
      return power2(left, 0x40000000 - 1);
    case 0x80000000:
      return power2(left, 0x80000000 - 1);
  }

  return other(left, range);
}

int32_t RandomDevice::power2(int32_t left, uint32_t mask) {
  return left + static_cast<int32_t>(random() & mask);
}

int32_t RandomDevice::other(int32_t left, uint32_t range) {
  uint32_t g = UINT32_MAX - UINT32_MAX % range;
  uint32_t r = random();
  int count = 0;
  static int const MAX_COUNT = 20;

  TRI_ASSERT(g > 0);

  while (r >= g) {
    if (++count >= MAX_COUNT) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot generate small random number after " << count
               << " tries";
      r %= g;
      continue;
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "random number too large, trying again";
    r = random();
  }

  r %= range;

  return left + static_cast<int32_t>(r);
}

// -----------------------------------------------------------------------------
// RandomDeviceDirect
// -----------------------------------------------------------------------------

#ifndef _WIN32

namespace {
template <int N>
class RandomDeviceDirect : public RandomDevice {
 public:
  explicit RandomDeviceDirect(std::string const& path) : fd(-1), pos(0) {
    fd = TRI_OPEN(path.c_str(), O_RDONLY | TRI_O_CLOEXEC);

    if (fd < 0) {
      std::string message("cannot open random source '" + path + "'");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
    }

    fillBuffer();
  }

  ~RandomDeviceDirect() {
    if (fd >= 0) {
      TRI_CLOSE(fd);
    }
  }

  uint32_t random() override {
    if (pos >= N) {
      fillBuffer();
    }

    return buffer[pos++];
  }

 private:
  void fillBuffer() {
    size_t n = sizeof(buffer);

    char* ptr = reinterpret_cast<char*>(&buffer);

    while (0 < n) {
      ssize_t r = TRI_READ(fd, ptr, static_cast<TRI_read_t>(n));

      if (r == 0) {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "read on random device failed: nothing read";
        FATAL_ERROR_EXIT();
      } else if (r < 0) {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "read on random device failed: " << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      ptr += r;
      n -= r;
    }

    pos = 0;
  }

 private:
  int fd;
  uint32_t buffer[N];
  size_t pos;
};
}

#endif

// -----------------------------------------------------------------------------
// RandomDeviceCombined
// -----------------------------------------------------------------------------

#ifndef _WIN32

namespace {
template <int N>
class RandomDeviceCombined : public RandomDevice {
 public:
  explicit RandomDeviceCombined(std::string const& path)
      : fd(-1), pos(0), rseed(0) {
    fd = TRI_OPEN(path.c_str(), O_RDONLY | TRI_O_CLOEXEC);

    if (fd < 0) {
      std::string message("cannot open random source '" + path + "'");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
    }

    // Set the random number generator file to be non-blocking (not for windows)
    {
      long flags = fcntl(fd, F_GETFL, 0);
      bool ok = (flags >= 0);
      if (ok) {
        flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        ok = (flags >= 0);
      }
      if (!ok) {
        std::string message("cannot switch random source '" + path +
                            "' to non-blocking");
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
      }
    }

    fillBuffer();
  }

  ~RandomDeviceCombined() {
    if (fd >= 0) {
      TRI_CLOSE(fd);
    }
  }

  uint32_t random() override {
    if (pos >= N) {
      fillBuffer();
    }

    return buffer[pos++];
  }

 private:
  void fillBuffer() {
    size_t n = sizeof(buffer);
    char* ptr = reinterpret_cast<char*>(&buffer);

    while (0 < n) {
      ssize_t r = TRI_READ(fd, ptr, static_cast<TRI_read_t>(n));

      if (r == 0) {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "read on random device failed: nothing read";
        FATAL_ERROR_EXIT();
      } else if (r < 0) { 
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
          LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "not enough entropy (got " << (sizeof(buffer) - n)
                    << "), switching to pseudo-random";
          break;
        }
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "read on random device failed: " << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      ptr += r;
      n -= r;

      rseed = buffer[0];

      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "using seed " << rseed;
    }

    if (0 < n) {
      std::mt19937 engine;
      unsigned long seed = RandomDevice::seed();
      engine.seed((uint32_t)(rseed ^ (uint32_t)seed));

      while (0 < n) {
        *ptr++ = engine();
        --n;
      }
    }

    pos = 0;
  }

 private:
  int fd;
  uint32_t buffer[N];
  size_t pos;

  uint32_t rseed;
};
}

#endif

// -----------------------------------------------------------------------------
// RandomDeviceMersenne
// -----------------------------------------------------------------------------

class RandomDeviceMersenne : public RandomDevice {
 public:
  RandomDeviceMersenne()
      : engine((uint_fast32_t)RandomDevice::seed()) {}

  uint32_t random() override { return engine(); }
  void seed(uint64_t seed) { engine.seed(static_cast<decltype(engine)::result_type>(seed)); }

  std::mt19937 engine;
};

// -----------------------------------------------------------------------------
// RandomDeviceWin32
// -----------------------------------------------------------------------------

#ifdef _WIN32

namespace {
template <int N>
class RandomDeviceWin32 : public RandomDevice {
 public:
  RandomDeviceWin32() : cryptoHandle(0), pos(0) {
    BOOL result;
    result = CryptAcquireContext(&cryptoHandle, nullptr, nullptr, PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    if (cryptoHandle == 0 || result == FALSE) {
      std::string message("cannot create cryptographic windows handle");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
    }

    fillBuffer();
  }

  ~RandomDeviceWin32() {
    if (cryptoHandle != 0) {
      CryptReleaseContext(cryptoHandle, 0);
    }
  }

  uint32_t random() {
    if (pos >= N) {
      fillBuffer();
    }

    return buffer[pos++];
  }

 private:
  void fillBuffer() {
    DWORD n = sizeof(buffer);
    BYTE* ptr = reinterpret_cast<BYTE*>(&buffer);

    // fill the buffer with random characters
    int result = CryptGenRandom(cryptoHandle, n, ptr);
    if (result == 0) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "read on random device failed: nothing read";
      FATAL_ERROR_EXIT();
    }
    pos = 0;
  }

 private:
  HCRYPTPROV cryptoHandle;
  uint32_t buffer[N];
  size_t pos;
};
}

#endif

// -----------------------------------------------------------------------------
// RandomGenerator
// -----------------------------------------------------------------------------

Mutex RandomGenerator::_lock;
std::unique_ptr<RandomDevice> RandomGenerator::_device(nullptr);

void RandomGenerator::initialize(RandomType type) {
  MUTEX_LOCKER(guard, _lock);

  switch (type) {
    case RandomType::MERSENNE: {
      _device.reset(new RandomDeviceMersenne());
      break;
    }

#ifndef _WIN32
    case RandomType::RANDOM: {
      _device.reset(new RandomDeviceDirect<1024>("/dev/random"));
      break;
    }

    case RandomType::URANDOM: {
      _device.reset(new RandomDeviceDirect<1024>("/dev/urandom"));
      break;
    }

    case RandomType::COMBINED: {
      _device.reset(new RandomDeviceCombined<600>("/dev/random"));
      break;
    }

#endif

#ifdef _WIN32
    case RandomType::WINDOWS_CRYPT: {
      _device.reset(new RandomDeviceWin32<1024>());
      break;
    }
#endif

    default: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unknown random generator");
    }
  }
}

void RandomGenerator::shutdown() { _device.reset(nullptr); }

int16_t RandomGenerator::interval(int16_t left, int16_t right) {
  uint16_t value = static_cast<int16_t>(
      interval(static_cast<int32_t>(left), static_cast<int32_t>(right)));

  TRI_ASSERT(value >= left && value <= right);
  return value;
}

int32_t RandomGenerator::interval(int32_t left, int32_t right) {
  MUTEX_LOCKER(locker, _lock);

  if (_device == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "random generator not initialized");
  }

  int32_t value = _device->interval(left, right);
  TRI_ASSERT(value >= left && value <= right);
  return value;
}

int64_t RandomGenerator::interval(int64_t left, int64_t right) {
  if (left >= right) {
    return left;
  }

  int64_t value;

  if (left == INT64_MIN && right == INT64_MAX) {
    uint64_t r1 = interval(UINT32_MAX);
    uint64_t r2 = interval(UINT32_MAX);

    value = static_cast<int64_t>((r1 << 32) | r2);
    TRI_ASSERT(value >= left && value <= right);
    return value;
  }

  if (left < 0) {
    if (right < 0) {
      uint64_t high = static_cast<uint64_t>(-left);
      uint64_t low = static_cast<uint64_t>(-right);
      uint64_t d = high - low;
      value = left + static_cast<int64_t>(interval(d));
      TRI_ASSERT(value >= left && value <= right);
      return value;
    }

    uint64_t low = static_cast<uint64_t>(-left);
    uint64_t d = low + static_cast<uint64_t>(right);
    uint64_t dRandom = interval(d);

    if (dRandom < low) {
      value = -1 - static_cast<int64_t>(dRandom);
    } else {
      value = static_cast<int64_t>(dRandom - low);
    }
    TRI_ASSERT(value >= left && value <= right);
    return value;
  } else {
    uint64_t high = static_cast<uint64_t>(right);
    uint64_t low = static_cast<uint64_t>(left);
    uint64_t d = high - low;
    value = left + static_cast<int64_t>(interval(d));
    TRI_ASSERT(value >= left && value <= right);
    return value;
  }
}

uint16_t RandomGenerator::interval(uint16_t right) {
  uint16_t value = static_cast<uint16_t>(interval(static_cast<uint32_t>(right)));
  TRI_ASSERT(value <= right);
  return value;
}

uint32_t RandomGenerator::interval(uint32_t right) {
  MUTEX_LOCKER(locker, _lock);

  if (_device == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "random generator not initialized");
  }
  
  uint32_t value = _device->interval(static_cast<uint32_t>(0), right);
  TRI_ASSERT(value <= right);
  return value;
}

uint64_t RandomGenerator::interval(uint64_t right) {
  if (right == 0) {
    return 0;
  }
  uint64_t value;

  if (right == UINT64_MAX) {
    uint64_t r1 = interval(UINT32_MAX);
    uint64_t r2 = interval(UINT32_MAX);

    value = (r1 << 32) | r2;
  } else {
    uint32_t high = static_cast<uint32_t>(right >> 32);
    uint64_t highMax = (static_cast<uint64_t>(high)) << 32;
    uint64_t highRandom = (static_cast<uint64_t>(interval(high))) << 32;

    if (highRandom == highMax) {
      uint32_t low = static_cast<uint32_t>(right - highMax);
      uint64_t lowRandom = static_cast<uint64_t>(interval(low));
      value = highRandom | lowRandom;
    } else {
      uint64_t lowRandom = static_cast<uint64_t>(interval(UINT32_MAX));
      value = highRandom | lowRandom;
    }
  }
  TRI_ASSERT(value <= right);
  return value;
}

void RandomGenerator::seed(uint64_t seed) {
  MUTEX_LOCKER(locker, _lock);
  if (!_device) {
    throw std::runtime_error("Random device not yet initialized!");
  }
  if(RandomDeviceMersenne* dev = dynamic_cast<RandomDeviceMersenne*>(_device.get())) {
    dev->seed(seed);
    return;
  }
  throw std::runtime_error("Random device is not mersenne and cannot be seeded!");
}
