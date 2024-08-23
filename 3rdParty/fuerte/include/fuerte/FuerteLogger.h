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

// note setting any of the following defines to 1 will make fuerte log the
// specific messages to the ArangoDB logger!
#ifndef ENABLE_FUERTE_LOG_ERROR
#define ENABLE_FUERTE_LOG_ERROR 0
#endif

#ifndef ENABLE_FUERTE_LOG_DEBUG
#define ENABLE_FUERTE_LOG_DEBUG 0
#endif

#ifndef ENABLE_FUERTE_LOG_TRACE
#define ENABLE_FUERTE_LOG_TRACE 0
#endif

#ifndef ENABLE_FUERTE_LOG_HTTPTRACE
#define ENABLE_FUERTE_LOG_HTTPTRACE 0
#endif

#include <iostream>

#if ENABLE_FUERTE_LOG_ERROR > 0 || ENABLE_FUERTE_LOG_DEBUG > 0 || ENABLE_FUERTE_LOG_TRACE > 0 || ENABLE_FUERTE_LOG_HTTPTRACE > 0
#include <sstream>
#include <string_view>

// this extern is defined in LoggerFeature.cpp, and connects fuerte to the ArangoDB logger.
// to make it work, it is necessary to also enable the LogHackWriter definition in
// LoggerFeature.cpp, which is normally not compiled into the executable.
extern void LogHackWriter(std::string_view p);

class LogHack {
  std::stringstream _s;
 public:
  LogHack() {}
  ~LogHack() { LogHackWriter(_s.view()); }

  typedef std::basic_ostream<char, std::char_traits<char> > CoutType;
  typedef CoutType& (*StandardEndLine)(CoutType&);
  LogHack& operator<<(StandardEndLine manip) { return *this; }

  template<typename T> LogHack& operator<<(T const& o) try { 
    _s << o; 
    return *this; 
  } catch (...) {
    return *this; 
  }
};
#endif

#if ENABLE_FUERTE_LOG_ERROR > 0
#define FUERTE_LOG_ERROR LogHack{}
#else
#define FUERTE_LOG_ERROR if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_DEBUG > 0
#define FUERTE_LOG_DEBUG LogHack{}
#else
#define FUERTE_LOG_DEBUG if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_TRACE > 0
#define FUERTE_LOG_TRACE LogHack{}
#else
#define FUERTE_LOG_TRACE if (0) std::cout
#endif

#if ENABLE_FUERTE_LOG_HTTPTRACE > 0
#define FUERTE_LOG_HTTPTRACE LogHack{} << "[http] "
#else
#define FUERTE_LOG_HTTPTRACE if (0) std::cout
#endif
