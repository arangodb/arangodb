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

#pragma once

#include <optional>

#include "Logger/LogVoidify.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#define ARANGO_INTERNAL_LOG_HELPER(id)              \
  ::arangodb::Logger::LINE(__LINE__)                \
      << ::arangodb::Logger::FILE(__FILE__)         \
      << ::arangodb::Logger::FUNCTION(__FUNCTION__) \
      << ::arangodb::Logger::LOGID((id))

/// @brief logs a message for a topic given that a condition is true.
/// `security` is a `std::optional<::arangodb::Security>`, which marks
/// the log message as a security event of a specific category (or
/// `std::nullopt` if it is not a security event). if
/// Logger::alwaysLogSecurityEvents() is set (FedRAMP High mode), security
/// events are logged regardless of the configured log levels.
/// note: `security` may be evaluated more than once, so it must not have
/// side effects.
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// in maintainer mode, we *intentionally and always build all log messages* if
// the condition is true.
// we do this to find any errors when building log messages in low log levels
// (e.g. trace) that would otherwise go undetected. we store a boolean in the
// LoggerStream then to indicate whether we want to emit the log message or
// not.
#define ARANGO_INTERNAL_LOG_STREAM(level, topic, security, cond)              \
  !(cond) ? (void)nullptr                                                     \
          : ::arangodb::LogVoidify() &                                        \
                (::arangodb::LoggerStream(                                    \
                     ::arangodb::Logger::isEnabled(                           \
                         (::arangodb::LogLevel::level), (topic), (security)), \
                     (security))                                              \
                 << (::arangodb::LogLevel::level))

#else
// outside of maintainer mode, we check if the log message should be emitted,
// and only then build the log log message. this is a performance optimization
// so we can save constructing log messages which will not be emitted anyway.
#define ARANGO_INTERNAL_LOG_STREAM(level, topic, security, cond)          \
  !(::arangodb::Logger::isEnabled((::arangodb::LogLevel::level), (topic), \
                                  (security)) &&                          \
    (cond))                                                               \
      ? (void)nullptr                                                     \
      : ::arangodb::LogVoidify() & (::arangodb::LoggerStream((security))  \
                                    << (::arangodb::LogLevel::level))

#endif

/// LOG_TOPIC_IF can be called with four arguments (id, level, topic, cond),
/// in which case the message is not a security event, or with five arguments
/// (id, level, topic, security, cond), where `security` is a value of the
/// `::arangodb::Security` enum.
#define ARANGO_INTERNAL_LOG_TOPIC_IF_5(id, level, topic, security, cond) \
  ARANGO_INTERNAL_LOG_STREAM(level, topic, security, (cond))             \
      << (topic) << ARANGO_INTERNAL_LOG_HELPER((id))
#define ARANGO_INTERNAL_LOG_TOPIC_IF_4(id, level, topic, cond) \
  ARANGO_INTERNAL_LOG_TOPIC_IF_5(id, level, topic, ::std::nullopt, cond)
#define ARANGO_INTERNAL_LOG_TOPIC_IF_SELECT(_1, _2, _3, _4, _5, name, ...) name
#define LOG_TOPIC_IF(...)                                               \
  ARANGO_INTERNAL_LOG_TOPIC_IF_SELECT(__VA_ARGS__,                      \
                                      ARANGO_INTERNAL_LOG_TOPIC_IF_5,   \
                                      ARANGO_INTERNAL_LOG_TOPIC_IF_4, ) \
  (__VA_ARGS__)

/// @brief logs a message for a topic.
/// this simply redirects to LOG_TOPIC_IF(...) with an always true condition.
/// it can be called with three arguments (id, level, topic), in which case
/// the message is not a security event, or with four arguments
/// (id, level, topic, security), where `security` is a value of the
/// `::arangodb::Security` enum.
#define ARANGO_INTERNAL_LOG_TOPIC_3(id, level, topic) \
  ARANGO_INTERNAL_LOG_TOPIC_IF_5(id, level, topic, ::std::nullopt, true)
#define ARANGO_INTERNAL_LOG_TOPIC_4(id, level, topic, security) \
  ARANGO_INTERNAL_LOG_TOPIC_IF_5(id, level, topic, security, true)
#define ARANGO_INTERNAL_LOG_TOPIC_SELECT(_1, _2, _3, _4, name, ...) name
#define LOG_TOPIC(...)                                                       \
  ARANGO_INTERNAL_LOG_TOPIC_SELECT(__VA_ARGS__, ARANGO_INTERNAL_LOG_TOPIC_4, \
                                   ARANGO_INTERNAL_LOG_TOPIC_3, )            \
  (__VA_ARGS__)

/// @brief logs a message for debugging during development
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define LOG_DEVEL_LEVEL ERR
#else
#define LOG_DEVEL_LEVEL DEBUG
#endif

#define LOG_DEVEL \
  LOG_TOPIC("xxxxx", LOG_DEVEL_LEVEL, ::arangodb::Logger::FIXME) << "###### "

#define LOG_DEVEL_IF(cond)                                                  \
  LOG_TOPIC_IF("xxxxx", LOG_DEVEL_LEVEL, ::arangodb::Logger::FIXME, (cond)) \
      << "###### "
