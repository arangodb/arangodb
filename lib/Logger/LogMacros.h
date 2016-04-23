////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
///
/// Portions of the code are:
///
/// Copyright (c) 1999, Google Inc.
/// All rights reserved.
//
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions are
/// met:
//
///     * Redistributions of source code must retain the above copyright
///       notice, this list of conditions and the following disclaimer.
///     * Redistributions in binary form must reproduce the above
///       copyright notice, this list of conditions and the following
///       disclaimer
///       in the documentation and/or other materials provided with the
///       distribution.
///     * Neither the name of Google Inc. nor the names of its
///       contributors may be used to endorse or promote products derived
///       from this software without specific prior written permission.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
/// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
/// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
/// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
/// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
/// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
/// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
/// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
/// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
/// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
/// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///
/// Author: Ray Sidney
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOG_MACROS_H
#define ARANGODB_LOGGER_LOG_MACROS_H 1

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message
////////////////////////////////////////////////////////////////////////////////

#define LOG(a)                                                        \
  !arangodb::Logger::isEnabled((arangodb::LogLevel::a))               \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a)          \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message for a topic
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC(a, b)                                               \
  !arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b))          \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a) << (b)   \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message given that a condition is true
////////////////////////////////////////////////////////////////////////////////

#define LOG_IF(a, cond)                                               \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a)) && (cond))   \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a)          \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message for a topic given that a condition is true
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC_IF(a, b, cond)                                         \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b)) && (cond)) \
      ? (void)0                                                          \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()               \
                                  << (arangodb::LogLevel::a) << (b)      \
                                  << arangodb::Logger::LINE(__LINE__)    \
                                  << arangodb::Logger::FILE(__FILE__)    \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message every N.the time
////////////////////////////////////////////////////////////////////////////////

#define LOG_EVERY_N_VARNAME(base, line) LOG_EVERY_N_VARNAME_CONCAT(base, line)
#define LOG_EVERY_N_VARNAME_CONCAT(base, line) base##line

#define LOG_OCCURRENCES LOG_EVERY_N_VARNAME(occurrences_, __LINE__)
#define LOG_OCCURRENCES_MOD_N LOG_EVERY_N_VARNAME(occurrences_mod_n_, __LINE__)

#define LOG_EVERY_N(a, n)                                             \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0;          \
  ++LOG_OCCURRENCES;                                                  \
  if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n;        \
  if (LOG_OCCURRENCES_MOD_N == 1)                                     \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a)) &&           \
    (LOG_OCCURRENCES_MOD_N == 1))                                     \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a)          \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message for a topic every N.the time
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC_EVERY_N(a, b, n)                                    \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0;          \
  ++LOG_OCCURRENCES;                                                  \
  if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n;        \
  if (LOG_OCCURRENCES_MOD_N == 1)                                     \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b)) &&      \
    (LOG_OCCURRENCES_MOD_N == 1))                                     \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a) << (b)   \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief helper class for macros
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
class LoggerStream;

class LogVoidify {
 public:
  LogVoidify() {}
  void operator&(LoggerStream const&) {}
};
}

#endif
