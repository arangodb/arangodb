////////////////////////////////////////////////////////////////////////////////
/// @brief random helper class
///
/// @file
/// Thread-safe random generator
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RandomGenerator.h"

#include "BasicsC/mersenne.h"
#include "BasicsC/socket-utils.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// random helper functions
// -----------------------------------------------------------------------------

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief random version
////////////////////////////////////////////////////////////////////////////////

  Random::random_e version = Random::RAND_MERSENNE;

////////////////////////////////////////////////////////////////////////////////
/// @brief random lock
////////////////////////////////////////////////////////////////////////////////

  Mutex RandomLock;
}

// -----------------------------------------------------------------------------
// random device
// -----------------------------------------------------------------------------

namespace RandomHelper {
  class RandomDevice {
    public:
      virtual ~RandomDevice () {}
      virtual uint32_t random () = 0;

      static unsigned long getSeed () {
        unsigned long seed = (unsigned long) time(0);

#ifdef TRI_HAVE_GETTIMEOFDAY
        struct timeval tv;
        int result = gettimeofday(&tv, 0);

        seed ^= static_cast<unsigned long>(tv.tv_sec);
        seed ^= static_cast<unsigned long>(tv.tv_usec);
        seed ^= static_cast<unsigned long>(result);
#endif

        seed ^= static_cast<unsigned long>(Thread::currentProcessId());

        return seed;
      };

  };



  template<int N>
  class RandomDeviceDirect : public RandomDevice {
    public:
      RandomDeviceDirect (string path)
        : fd(1), pos(0)  {
        fd = TRI_OPEN(path.c_str(), O_RDONLY);

        if (fd < 0) {
          THROW_INTERNAL_ERROR("cannot open random source '" + path + "'");
        }

        fillBuffer();
      }



      ~RandomDeviceDirect () {
        TRI_CLOSE(fd);
      }


      uint32_t random () {
        if (pos >= N) {
          fillBuffer();
        }

        return buffer[pos++];
      }

    private:
      void fillBuffer () {
        size_t n = sizeof(buffer);
        char* ptr = reinterpret_cast<char*>(&buffer);

        while (0 < n) {
          ssize_t r = TRI_READ(fd, ptr, n);

          if (r == 0) {
            LOGGER_FATAL_AND_EXIT("read on random device failed: nothing read");
          }
          else if (r < 0) {
            LOGGER_FATAL_AND_EXIT("read on random device failed: " << strerror(errno));
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



  template<int N>
  class RandomDeviceCombined : public RandomDevice {
    public:
      RandomDeviceCombined (string path)
        : fd(0),
          pos(0),
          rseed(0) {
        fd = TRI_OPEN(path.c_str(), O_RDONLY);

        if (fd < 0) {
          THROW_INTERNAL_ERROR("cannot open random source '" + path + "'");
        }

        // ..............................................................................
        // Set the random number generator file to be non-blocking (not for windows)
        // ..............................................................................
        {
          #ifdef _WIN32
            abort();
          #else
            long flags = fcntl(fd, F_GETFL, 0);
            bool ok = (flags >= 0);
            if (ok) {
              flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
              ok = (flags >= 0);
            }
            if (! ok) {

              THROW_INTERNAL_ERROR("cannot switch random source '" + path + "' to non-blocking");
            }
          #endif
        }
        fillBuffer();
      }



      ~RandomDeviceCombined () {
        TRI_CLOSE(fd);
      }



      uint32_t random () {
        if (pos >= N) {
          fillBuffer();
        }

        return buffer[pos++];
      }

    private:
      void fillBuffer () {
        size_t n = sizeof(buffer);
        char* ptr = reinterpret_cast<char*>(&buffer);

        while (0 < n) {
          ssize_t r = TRI_READ(fd, ptr, n);

          if (r == 0) {
            LOGGER_FATAL_AND_EXIT("read on random device failed: nothing read");
          }
          else if (errno == EWOULDBLOCK) {
            LOGGER_INFO("not enough entropy (got " << (sizeof(buffer) - n) << " bytes), switching to pseudo-random");
            break;
          }
          else if (r < 0) {
            LOGGER_FATAL_AND_EXIT("read on random device failed: " << strerror(errno));
          }

          ptr += r;
          n -= r;

          rseed = buffer[0];

          LOGGER_TRACE("using seed " << rseed);
        }

        if (0 < n) {
          unsigned long seed = RandomDevice::getSeed();
          TRI_SeedMersenneTwister((uint32_t) (rseed ^ (uint32_t) seed));

          while (0 < n) {
            *ptr++ = TRI_Int32MersenneTwister();
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

  template<int N>
  class RandomDeviceWin32 : public RandomDevice {
#ifndef _WIN32
    public:
      RandomDeviceWin32 () { assert(false); }
      ~RandomDeviceWin32 () {}
      uint32_t random () { return 0;}
#else
    public:
      RandomDeviceWin32 () : cryptoHandle(0), pos(0)  {
        BOOL result;
        result = CryptAcquireContext(&cryptoHandle, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
        if (cryptoHandle == 0 || result == FALSE) {
          THROW_INTERNAL_ERROR("cannot create cryptographic windows handle");
        }
        fillBuffer();
      }


      ~RandomDeviceWin32 () {
        if (cryptoHandle != 0) {
          CryptReleaseContext(cryptoHandle, 0);
        }
      }


      uint32_t random () {
        if (pos >= N) {
          fillBuffer();
        }

        return buffer[pos++];
      }

    private:
      void fillBuffer () {
        size_t n = sizeof(buffer);
        BYTE* ptr = reinterpret_cast<BYTE*>(&buffer);

        // fill the buffer with random characters
        int result = CryptGenRandom(cryptoHandle, n, ptr);
        if (result == 0) {
          LOGGER_FATAL_AND_EXIT("read on random device failed: nothing read");
        }
        pos = 0;
      }

    private:
      HCRYPTPROV cryptoHandle;
      uint32_t buffer[N];
      size_t pos;
#endif
  };

  RandomDevice* randomDevice = 0;
  RandomDevice* urandomDevice = 0;
  RandomDevice* combinedDevice = 0;
  RandomDevice* win32Device = 0;
}

// -----------------------------------------------------------------------------
// uniform generator
// -----------------------------------------------------------------------------

namespace RandomHelper {
  class UniformGenerator {
    private:
      UniformGenerator (UniformGenerator const&);
      UniformGenerator& operator= (UniformGenerator const&);

    public:
      UniformGenerator (RandomDevice* device)
        : device(device) {
      }



      virtual ~UniformGenerator () {
      }



      int32_t random (int32_t left, int32_t right) {
        if (left >= right) {
          return left;
        }

        if (left == INT32_MIN && right == INT32_MAX) {
          return static_cast<int32_t>(device->random());
        }

        uint32_t range = static_cast<uint32_t>(right - left + 1);

        switch (range) {
          case 0x00000002: return power2(left, 0x00000002 - 1);
          case 0x00000004: return power2(left, 0x00000004 - 1);
          case 0x00000008: return power2(left, 0x00000008 - 1);
          case 0x00000010: return power2(left, 0x00000010 - 1);
          case 0x00000020: return power2(left, 0x00000020 - 1);
          case 0x00000040: return power2(left, 0x00000040 - 1);
          case 0x00000080: return power2(left, 0x00000080 - 1);
          case 0x00000100: return power2(left, 0x00000100 - 1);
          case 0x00000200: return power2(left, 0x00000200 - 1);
          case 0x00000400: return power2(left, 0x00000400 - 1);
          case 0x00000800: return power2(left, 0x00000800 - 1);
          case 0x00001000: return power2(left, 0x00001000 - 1);
          case 0x00002000: return power2(left, 0x00002000 - 1);
          case 0x00004000: return power2(left, 0x00004000 - 1);
          case 0x00008000: return power2(left, 0x00008000 - 1);
          case 0x00010000: return power2(left, 0x00010000 - 1);
          case 0x00020000: return power2(left, 0x00020000 - 1);
          case 0x00040000: return power2(left, 0x00040000 - 1);
          case 0x00080000: return power2(left, 0x00080000 - 1);
          case 0x00100000: return power2(left, 0x00100000 - 1);
          case 0x00200000: return power2(left, 0x00200000 - 1);
          case 0x00400000: return power2(left, 0x00400000 - 1);
          case 0x00800000: return power2(left, 0x00800000 - 1);
          case 0x01000000: return power2(left, 0x01000000 - 1);
          case 0x02000000: return power2(left, 0x02000000 - 1);
          case 0x04000000: return power2(left, 0x04000000 - 1);
          case 0x08000000: return power2(left, 0x08000000 - 1);
          case 0x10000000: return power2(left, 0x10000000 - 1);
          case 0x20000000: return power2(left, 0x20000000 - 1);
          case 0x40000000: return power2(left, 0x40000000 - 1);
          case 0x80000000: return power2(left, 0x80000000 - 1);
        }

        return other(left, range);
      }

    private:
      int32_t power2 (int32_t left, uint32_t mask) {
        return left + static_cast<int32_t>(device->random() & mask);
      }



      int32_t other (int32_t left, uint32_t range) {
        uint32_t g = UINT32_MAX - UINT32_MAX % range;
        uint32_t r = device->random();
        int count = 0;
        static int const MAX_COUNT = 20;

        while (r >= g) {
          if (++count >= MAX_COUNT) {
            LOGGER_ERROR("cannot generate small random number after " << count << " tries");
            r %= g;
            continue;
          }

          LOGGER_DEBUG("random number too large, trying again");
          r = device->random();
        }

        r %= range;

        return left + static_cast<int32_t>(r);
      }

    private:
      RandomDevice* device;
  };
}

// -----------------------------------------------------------------------------
// random helper class
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {
    namespace Random {

// -----------------------------------------------------------------------------
// implementation
// -----------------------------------------------------------------------------

      struct UniformIntegerImpl {
        virtual ~UniformIntegerImpl () {}
        virtual int32_t random (int32_t left, int32_t right) = 0;
      };



      // MERSENNE
      struct UniformIntegerMersenne : public UniformIntegerImpl {
        int32_t random (int32_t left, int32_t right) {
          const int32_t range = right - left + 1;
          int32_t result = (int32_t) TRI_Int32MersenneTwister();

          result = (int32_t) abs(result % range) + left;

          return result;
        }
      };



      // RANDOM DEVICE
      struct UniformIntegerRandom : public UniformIntegerImpl, private RandomHelper::UniformGenerator {
        UniformIntegerRandom (RandomHelper::RandomDevice* device)
          : RandomHelper::UniformGenerator(device) {
        }

        int32_t random (int32_t left, int32_t right) {
          return RandomHelper::UniformGenerator::random(left, right);
        }
      };


      // RANDOM DEVICE
      struct UniformIntegerWin32 : public UniformIntegerImpl, private RandomHelper::UniformGenerator {
        UniformIntegerWin32 (RandomHelper::RandomDevice* device) : RandomHelper::UniformGenerator(device) {
        }

        int32_t random (int32_t left, int32_t right) {
          return RandomHelper::UniformGenerator::random(left, right);
        }
      };



      // current implementation (see version at the top of the file)
      UniformIntegerImpl * uniformInteger = new UniformIntegerMersenne;

// -----------------------------------------------------------------------------
      // uniform integer generator
// -----------------------------------------------------------------------------

      int32_t UniformInteger::random () {
        MUTEX_LOCKER(RandomLock);

        if (uniformInteger == 0) {
          THROW_INTERNAL_ERROR("unknown random generator");
        }

        return uniformInteger->random(left, right);
      }

// -----------------------------------------------------------------------------
      // uniform character generator
// -----------------------------------------------------------------------------

      UniformCharacter::UniformCharacter (size_t length)
        : length(length),
          characters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
          generator(0, characters.size() - 1) {
      }



      UniformCharacter::UniformCharacter (string const& characters)
        : length(1),
          characters(characters),
          generator(0, characters.size() - 1) {
      }



      UniformCharacter::UniformCharacter (size_t length, string const& characters)
        : length(length),
          characters(characters),
          generator(0, characters.size() - 1) {
      }



      string UniformCharacter::random () {
        return random(length);
      }



      string UniformCharacter::random (size_t length) {
        string buffer;
        buffer.reserve(length);

        for (size_t i = 0; i < length; ++i) {
          size_t r = generator.random();

          buffer.push_back(characters[r]);
        }

        return buffer;
      }

// -----------------------------------------------------------------------------
      // public methods
// -----------------------------------------------------------------------------

      random_e selectVersion (random_e newVersion) {

        MUTEX_LOCKER(RandomLock);

        random_e oldVersion = version;
        version = newVersion;

        if (uniformInteger != 0) {
          delete uniformInteger;
          uniformInteger = 0;
        }


        switch (version) {

          case RAND_MERSENNE: {
            uniformInteger = new UniformIntegerMersenne;
            break;
           }

          case RAND_RANDOM: {
            if (RandomHelper::randomDevice == 0) {
              RandomHelper::randomDevice = new RandomHelper::RandomDeviceDirect<1024>("/dev/random");
            }

            uniformInteger = new UniformIntegerRandom(RandomHelper::randomDevice);

            break;
          }

          case RAND_URANDOM: {
            if (RandomHelper::urandomDevice == 0) {
              RandomHelper::urandomDevice = new RandomHelper::RandomDeviceDirect<1024>("/dev/urandom");
            }

            uniformInteger = new UniformIntegerRandom(RandomHelper::urandomDevice);

            break;
          }

          case RAND_COMBINED: {
            if (RandomHelper::combinedDevice == 0) {
              RandomHelper::combinedDevice = new RandomHelper::RandomDeviceCombined<600>("/dev/random");
            }

            uniformInteger = new UniformIntegerRandom(RandomHelper::combinedDevice);

            break;
          }

          case RAND_WIN32: {
            if (RandomHelper::win32Device == 0) {
              RandomHelper::win32Device = new RandomHelper::RandomDeviceWin32<1024>();
            }
            uniformInteger = new UniformIntegerWin32(RandomHelper::win32Device);
            break;
          }

          default: {
            THROW_INTERNAL_ERROR("unknown random generator");
          }

        }

        return oldVersion;
      }



      random_e currentVersion () {
        return version;
      }


      void shutdown () {
        if (RandomHelper::randomDevice != 0) {
          delete RandomHelper::randomDevice;
          RandomHelper::randomDevice = 0;
        }

        if (RandomHelper::urandomDevice != 0) {
          delete RandomHelper::urandomDevice;
          RandomHelper::urandomDevice = 0;
        }

        if (RandomHelper::combinedDevice != 0) {
          delete RandomHelper::combinedDevice;
          RandomHelper::combinedDevice = 0;
        }
      }


      bool isBlocking () {
        return version == RAND_RANDOM;
      }



      int32_t interval (int32_t left, int32_t right) {
        MUTEX_LOCKER(RandomLock);

        if (uniformInteger == 0) {
          THROW_INTERNAL_ERROR("unknown random generator");
        }

        return uniformInteger->random(left, right);
      }



      uint32_t interval (uint32_t left, uint32_t right) {
        MUTEX_LOCKER(RandomLock);

        if (uniformInteger == 0) {
          THROW_INTERNAL_ERROR("unknown random generator");
        }

        int32_t l = left + INT32_MIN;
        int32_t r = right + INT32_MIN;

        return uniformInteger->random(l, r) - INT32_MIN;
      }
    }
  }
}
