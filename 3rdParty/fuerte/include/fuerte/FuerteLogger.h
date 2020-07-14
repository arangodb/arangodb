////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

// Please leave the following debug code in for the next time we have to
// debug fuerte.
#if 0
#include <sstream>
#include <iostream>

extern void LogHackWriter(char const* p);

class LogHack {
  std::stringstream _s;
 public:
  LogHack() {};
  ~LogHack() { LogHackWriter(_s.str().c_str()); };
  template<typename T> LogHack& operator<<(T const& o) { _s << o; return *this; }
  typedef std::basic_ostream<char, std::char_traits<char> > CoutType;
  typedef CoutType& (*StandardEndLine)(CoutType&);
  LogHack& operator<<(StandardEndLine manip) { return *this; }
};
#endif

#ifndef ARANGO_CXX_DRIVER_FUERTE_LOGGER
#define ARANGO_CXX_DRIVER_FUERTE_LOGGER 1

#ifndef ENABLE_FUERTE_LOG_ERROR
#define ENABLE_FUERTE_LOG_ERROR 1
#endif

#ifndef ENABLE_FUERTE_LOG_DEBUG
#define ENABLE_FUERTE_LOG_DEBUG 0
#endif

#ifndef ENABLE_FUERTE_LOG_TRACE
#define ENABLE_FUERTE_LOG_TRACE 0
#endif

#ifndef ENABLE_FUERTE_LOG_VSTTRACE
#define ENABLE_FUERTE_LOG_VSTTRACE 0
#endif

#ifndef ENABLE_FUERTE_LOG_VST_CHUNK_TRACE
#define ENABLE_FUERTE_LOG_VST_CHUNK_TRACE 0
#endif

#ifndef ENABLE_FUERTE_LOG_HTTPTRACE
#define ENABLE_FUERTE_LOG_HTTPTRACE 0
#endif

#if defined(ENABLE_FUERTE_LOG_TRACE) || defined(ENABLE_FUERTE_LOG_DEBUG) || \
    defined(ENABLE_FUERTE_LOG_ERROR)
#include <iostream>
#endif

#if ENABLE_FUERTE_LOG_ERROR > 0
#define FUERTE_LOG_ERROR std::cout
#else
#define FUERTE_LOG_ERROR \
  if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_DEBUG > 0
#define FUERTE_LOG_DEBUG std::cout
#else
#define FUERTE_LOG_DEBUG \
  if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_TRACE > 0
#define FUERTE_LOG_TRACE std::cout
#else
#define FUERTE_LOG_TRACE \
  if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_VSTTRACE > 0
#define FUERTE_LOG_VSTTRACE std::cout << "[vst] "
#else
#define FUERTE_LOG_VSTTRACE \
  if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_VSTCHUNKTRACE > 0
#define FUERTE_LOG_VSTCHUNKTRACE std::cout << "[vst] "
#else
#define FUERTE_LOG_VSTCHUNKTRACE \
  if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_HTTPTRACE > 0
#define FUERTE_LOG_HTTPTRACE std::cout << "[http] "
#else
#define FUERTE_LOG_HTTPTRACE \
  if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_NODE > 0
#define FUERTE_LOG_NODE std::cout
#else
#define FUERTE_LOG_NODE \
  if (0) std::cout
#endif

#endif
