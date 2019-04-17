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
/// @author Kaveh Vahedipour
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
/// @brief container traits
////////////////////////////////////////////////////////////////////////////////

namespace container_traits {

using tc = char[2];

template<typename T> struct is_container {
  static tc& test(...);

  template <typename U>
  static char test(U&&, decltype(std::begin(std::declval<U>()))* = 0);
  static constexpr bool value = sizeof(test(std::declval<T>())) == 1;
};

template < typename T > struct is_associative {
  static tc& test(...) ;
  
  template < typename U >
  static char test(U&&, typename U::key_type* = 0) ;
  static constexpr bool value = sizeof( test( std::declval<T>() ) ) == 1 ;
};

}

template < typename T > struct is_container :
  std::conditional<(container_traits::is_container<T>::value || std::is_array<T>::value)
                   && !std::is_same<char *, typename std::decay<T>::type>::value
                   && !std::is_same<char const*, typename std::decay<T>::type>::value
                   && !std::is_same<unsigned char *, typename std::decay<T>::type>::value
                   && !std::is_same<unsigned char const*, typename std::decay<T>::type>::value
                   && !std::is_same<T, std::string>::value
                   && !std::is_same<T, const std::string>::value, std::true_type, std::false_type >::type {};

template < typename T > struct is_associative :
  std::conditional< container_traits::is_container<T>::value && container_traits::is_associative<T>::value,
                    std::true_type, std::false_type >::type {};

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

template<bool b>
struct conpar {
  static char const open;
  static char const close;
};

template< bool B, class T = void >
using enable_if_t = typename std::enable_if<B,T>::type;

template<typename T>
enable_if_t<is_container<T>::value, std::ostream&>
operator<< (std::ostream& o, T const& t) {
  o << conpar<is_associative<T>::value>::open;
  bool first = true;  
  for (auto const& i : t) {
    if (first) {
      o << " ";
      first = false;
    } else {
      o << ", ";
    }
    o << i ;
  }
  o << " " << conpar<is_associative<T>::value>::close;
  return o;  
}

#endif
