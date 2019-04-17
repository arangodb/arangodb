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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_DEBUGGING_H
#define ARANGODB_BASICS_DEBUGGING_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

#include <ostream>

/// @brief macro TRI_IF_FAILURE
/// this macro can be used in maintainer mode to make the server fail at
/// certain locations in the C code. The points at which a failure is actually
/// triggered can be defined at runtime using TRI_AddFailurePointDebugging().
#ifdef ARANGODB_ENABLE_FAILURE_TESTS

#define TRI_IF_FAILURE(what) if (TRI_ShouldFailDebugging(what))

#else

#define TRI_IF_FAILURE(what) if (false)

#endif

/// @brief cause a segmentation violation
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_SegfaultDebugging(char const* value);
#else
inline void TRI_SegfaultDebugging(char const*) {}
#endif

/// @brief check whether we should fail at a failure point
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
bool TRI_ShouldFailDebugging(char const* value);
#else
inline constexpr bool TRI_ShouldFailDebugging(char const*) { return false; }
#endif

/// @brief add a failure point
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_AddFailurePointDebugging(char const* value);
#else
inline void TRI_AddFailurePointDebugging(char const*) {}
#endif

/// @brief remove a failure point
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_RemoveFailurePointDebugging(char const* value);
#else
inline void TRI_RemoveFailurePointDebugging(char const*) {}
#endif

/// @brief clear all failure points
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_ClearFailurePointsDebugging();
#else
inline void TRI_ClearFailurePointsDebugging() {}
#endif

/// @brief returns whether failure point debugging can be used
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
inline constexpr bool TRI_CanUseFailurePointsDebugging() { return true; }
#else
inline constexpr bool TRI_CanUseFailurePointsDebugging() { return false; }
#endif

/// @brief appends a backtrace to the string provided
void TRI_GetBacktrace(std::string& btstr);

/// @brief prints a backtrace on stderr
void TRI_PrintBacktrace();

/// @brief logs a backtrace in log level warning
void TRI_LogBacktrace();

/// @brief flushes the logger and shuts it down
void TRI_FlushDebugging();
void TRI_FlushDebugging(char const* file, int line, char const* message);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump pair contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& stream, std::pair<T1, T2> const& obj) {
  stream << '(' << obj.first << ", " << obj.second << ')';
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump vector contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename T>
std::ostream& operator<<(std::ostream& stream, std::vector<T> const& data) {
  bool first = true;

  stream << "[";
  for (auto const& it : data) {
    if (first) {
      stream << " ";
      first = false;
    } else {
      stream << ", ";
    }
    stream << it;
  }
  stream << " ]";

  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump deque contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename T>
std::ostream& operator<<(std::ostream& stream, std::deque<T> const& data) {
  bool first = true;

  stream << "[";
  for (auto const& it : data) {
    if (first) {
      stream << " ";
      first = false;
    } else {
      stream << ", ";
    }
    stream << it;
  }
  stream << " ]";

  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump set contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename T>
std::ostream& operator<<(std::ostream& stream, std::set<T> const& data) {
  bool first = true;

  stream << "{";
  for (auto const& it : data) {
    if (first) {
      stream << " ";
      first = false;
    } else {
      stream << ", ";
    }
    stream << it;
  }
  stream << " }";

  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump unordered_set contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename T>
std::ostream& operator<<(std::ostream& stream, std::unordered_set<T> const& data) {
  bool first = true;

  stream << "{";
  for (auto const& it : data) {
    if (first) {
      stream << " ";
      first = false;
    } else {
      stream << ", ";
    }
    stream << it;
  }
  stream << " }";

  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump unordered_map contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename V>
std::ostream& operator<<(std::ostream& stream, std::unordered_map<K, V> const& data) {
  bool first = true;

  stream << "{";
  for (auto const& it : data) {
    if (first) {
      stream << " ";
      first = false;
    } else {
      stream << ", ";
    }
    stream << it.first << ": " << it.second;
  }
  stream << " }";

  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump map contents to an ostream
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename V>
std::ostream& operator<<(std::ostream& stream, std::map<K, V> const& data) {
  bool first = true;

  stream << "{";
  for (auto const& it : data) {
    if (first) {
      stream << " ";
      first = false;
    } else {
      stream << ", ";
    }
    stream << it.first << ": " << it.second;
  }
  stream << " }";

  return stream;
}

#endif
