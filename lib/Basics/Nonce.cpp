////////////////////////////////////////////////////////////////////////////////
/// @brief nonces
///
/// @file
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
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Nonce.h"

#include <math.h>

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/RandomGenerator.h"
#include "Basics/StringUtils.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// statistic nonce buffer
// -----------------------------------------------------------------------------

namespace {
  Mutex MutexNonce;

  size_t SizeNonces = 16777216;

  uint32_t* TimestampNonces = 0;

  uint32_t StatisticsNonces[32][5] = {
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }
  };
}

namespace triagens {
  namespace basics {
    namespace Nonce {

// -----------------------------------------------------------------------------
      // static functions
// -----------------------------------------------------------------------------

      void create (size_t size) {
        if (SizeNonces < 64) {
          SizeNonces = 64;
        }

        if (TimestampNonces != 0) {
          delete[] TimestampNonces;
        }

        TimestampNonces = new uint32_t[size];

        memset(TimestampNonces, 0, sizeof(uint32_t) * size);

        for (size_t i = 0;  i < 32;  ++i) {
          for (size_t j = 0;  j < 5;  ++j) {
            StatisticsNonces[i][j] = 0;
          }
        }
      }



      void destroy () {
        if (TimestampNonces != 0) {
          delete[] TimestampNonces;
        }
      }


      string createNonce () {
        uint32_t timestamp = time(0);
        uint32_t rand1 = Random::interval(0U, UINT32_MAX);
        uint32_t rand2 = Random::interval(0U, UINT32_MAX);

        uint8_t buffer[12];

        buffer[0] = (timestamp >> 24) & 0xFF;
        buffer[1] = (timestamp >> 16) & 0xFF;
        buffer[2] = (timestamp >>  8) & 0xFF;
        buffer[3] = (timestamp      ) & 0xFF;

        memcpy(buffer + 4, &rand1, 4);
        memcpy(buffer + 8, &rand2, 4);

        return StringUtils::encodeBase64U(string((char*) buffer, 12));
      }

      bool checkAndMark (string const& nonce) {

        if (nonce.length() != 12) {
          return false;
        }

        uint8_t const* buffer = (uint8_t const*) nonce.c_str();

        uint32_t timestamp = (uint32_t(buffer[0]) << 24)
                         | (uint32_t(buffer[1]) << 16)
                         | (uint32_t(buffer[2]) <<  8)
                         |  uint32_t(buffer[3]);

        uint64_t random = (uint64_t(buffer[ 4]) << 56)
                      | (uint64_t(buffer[ 5]) << 48)
                      | (uint64_t(buffer[ 6]) << 40)
                      | (uint64_t(buffer[ 7]) << 32)
                      | (uint64_t(buffer[ 8]) << 24)
                      | (uint64_t(buffer[ 9]) << 16)
                      | (uint64_t(buffer[10]) << 8)
                      |  uint64_t(buffer[11]);

        return checkAndMark(timestamp, random);
      }

      bool checkAndMark (uint32_t timestamp, uint64_t random) {
        MUTEX_LOCKER(MutexNonce);

        if (TimestampNonces == 0) {
          LOGGER_INFO("setting nonce hash size to '" << SizeNonces << "'" );
          create(SizeNonces);
        }

        int proofs = 0;

        // first count to avoid miscounts if two hashes are equal
        if (timestamp > TimestampNonces[random % (SizeNonces - 3)]) {
          proofs++;
        }

        if (timestamp > TimestampNonces[random % (SizeNonces - 17)]) {
          proofs++;
        }

        if (timestamp > TimestampNonces[random % (SizeNonces - 33)]) {
          proofs++;
        }

        if (timestamp > TimestampNonces[random % (SizeNonces - 63)]) {
          proofs++;
        }

        // statistics, compute the log2 of the age and increment the proofs count
        uint32_t now = time(0);
        uint32_t age = 1;

        if (timestamp < now) {
          age = now - timestamp;
        }

        uint32_t l2age = 0;

        while (1 < age) {
          l2age += 1;
          age >>= 1;
        }

        LOGGER_TRACE( "age of timestamp " << timestamp << " is " << age << " (log " << l2age << ")" );

        StatisticsNonces[l2age][proofs]++;

        // mark the nonce as used
        if (timestamp > TimestampNonces[random % (SizeNonces - 3)]) {
          TimestampNonces[random % (SizeNonces - 3)] = timestamp;
        }

        if (timestamp > TimestampNonces[random % (SizeNonces - 17)]) {
          TimestampNonces[random % (SizeNonces - 17)] = timestamp;
        }

        if (timestamp > TimestampNonces[random % (SizeNonces - 33)]) {
          TimestampNonces[random % (SizeNonces - 33)] = timestamp;
        }

        if (timestamp > TimestampNonces[random % (SizeNonces - 63)]) {
          TimestampNonces[random % (SizeNonces - 63)] = timestamp;
        }

        return 0 < proofs;
      }



      vector<Statistics> statistics () {
        MUTEX_LOCKER(MutexNonce);

        size_t const N = 4;
        vector<Statistics> result;

        for (uint32_t l2age = 0, age = 1;  l2age < 32;  ++l2age) {
          uint32_t T = 0;
          uint32_t coeff = 1;
          double S0 = 1.0;
          double x = 1.0;

          for (size_t i = 1;  i < N + 1;  ++i) {
            T = T + StatisticsNonces[l2age][i];
            coeff = coeff * (N - i + 1) / i;
            S0 = S0 * pow((double)(StatisticsNonces[l2age][i] / coeff), (double)((4 * N + 2 - 6 * i) / (N * N - N)));
            x = x * pow((double)(StatisticsNonces[l2age][i] / coeff), (double)((12 * i - 6 * N - 6) / (N * N * N - N)));
          }

          Statistics current;

          current.age = age;
          current.checks = T + StatisticsNonces[l2age][0];
          current.isUnused = T;
          current.isUsed = StatisticsNonces[l2age][0];
          current.marked = (uint32_t) (StatisticsNonces[l2age][0] - S0);
          current.falselyUsed = (uint32_t) S0;
          current.fillingDegree = 1 / (1 + x);

          result.push_back(current);

          age *= 2U;
        }

        return result;
      }
    }
  }
}

