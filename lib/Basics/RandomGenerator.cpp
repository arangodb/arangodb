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

#include <random>
#include <chrono>

#ifdef _WIN32
#include <Wincrypt.h>
#endif

#include "Logger/Logger.h"
#include "Basics/Exceptions.h"
#include "Basics/Thread.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// RandomDevice
// -----------------------------------------------------------------------------

unsigned long RandomDevice::seed() {
  unsigned long s = (unsigned long)time(0);

  struct timeval tv;
  int result = gettimeofday(&tv, 0);

  s ^= static_cast<unsigned long>(tv.tv_sec);
  s ^= static_cast<unsigned long>(tv.tv_usec);
  s ^= static_cast<unsigned long>(result);
  s ^= static_cast<unsigned long>(Thread::currentProcessId());

  return s;
}

int32_t RandomDevice::interval(int32_t left, int32_t right) {
  return random(left, right);
}

uint32_t RandomDevice::interval(uint32_t left, uint32_t right) {
  int32_t l = left + INT32_MIN;
  int32_t r = right + INT32_MIN;

  return static_cast<uint32_t>(random(l, r) - INT32_MIN);
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
      LOG(ERR) << "cannot generate small random number after " << count
               << " tries";
      r %= g;
      continue;
    }

    LOG(TRACE) << "random number too large, trying again";
    r = random();
  }

  r %= range;

  return left + static_cast<int32_t>(r);
}

// -----------------------------------------------------------------------------
// RandomDeviceDirect
// -----------------------------------------------------------------------------

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

  uint32_t random() {
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
      ssize_t r = TRI_READ(fd, ptr, (TRI_read_t)n);

      if (r == 0) {
        LOG(FATAL) << "read on random device failed: nothing read";
        FATAL_ERROR_EXIT();
      } else if (r < 0) {
        LOG(FATAL) << "read on random device failed: " << strerror(errno);
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

// -----------------------------------------------------------------------------
// RandomDeviceCombined
// -----------------------------------------------------------------------------

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

  uint32_t random() {
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
      ssize_t r = TRI_READ(fd, ptr, (TRI_read_t)n);

      if (r == 0) {
        LOG(FATAL) << "read on random device failed: nothing read";
        FATAL_ERROR_EXIT();
      } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
        LOG(INFO) << "not enough entropy (got " << (sizeof(buffer) - n)
                  << "), switching to pseudo-random";
        break;
      } else if (r < 0) {
        LOG(FATAL) << "read on random device failed: " << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      ptr += r;
      n -= r;

      rseed = buffer[0];

      LOG(TRACE) << "using seed " << (long unsigned int)rseed;
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

// -----------------------------------------------------------------------------
// RandomDeviceMersenne
// -----------------------------------------------------------------------------

class RandomDeviceMersenne : public RandomDevice {
 public:
  RandomDeviceMersenne()
      : engine(static_cast<unsigned int>(
            std::chrono::system_clock::now().time_since_epoch().count())) {}

  uint32_t random() { return engine(); }

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
      LOG(FATAL) << "read on random device failed: nothing read";
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

#endif

    case RandomType::COMBINED: {
      _device.reset(new RandomDeviceCombined<600>("/dev/random"));
      break;
    }

#ifdef _WIN32
    case RandomType::WIN32: {
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

void RandomGenerator::shutdown() {
  _device.reset(nullptr);
}
