////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_LOG_H
#define IRESEARCH_LOG_H

#include <string>
#include <iostream>
#include "shared.hpp"

#if defined(_MSC_VER) || defined(__MINGW32__)
  #define IR_FILEPATH_SPECIFIER  "%ws"
  #define IR_UINT32_T_SPECIFIER  "%u"
  #define IR_UINT64_T_SPECIFIER  "%I64u"
  #define IR_SIZE_T_SPECIFIER    "%Iu"
  #define IR_SSIZE_T_SPECIFIER   "%Id"
  #define IR_PTRDIFF_T_SPECIFIER "%Id"
#elif defined(__APPLE__)
  #define IR_FILEPATH_SPECIFIER  "%s"
  #define IR_UINT32_T_SPECIFIER  "%u"
  #define IR_UINT64_T_SPECIFIER  "%llu"
  #define IR_SIZE_T_SPECIFIER    "%zu"
  #define IR_SSIZE_T_SPECIFIER   "%zd"
  #define IR_PTRDIFF_T_SPECIFIER "%zd"
#elif defined(__GNUC__)
  #define IR_FILEPATH_SPECIFIER  "%s"
  #define IR_UINT32_T_SPECIFIER  "%u"
  #define IR_UINT64_T_SPECIFIER  "%lu"
  #define IR_SIZE_T_SPECIFIER    "%zu"
  #define IR_SSIZE_T_SPECIFIER   "%zd"
  #define IR_PTRDIFF_T_SPECIFIER "%zd"
#else
  static_assert(false, "Unknown size_t, ssize_t, ptrdiff_t specifiers");
#endif

NS_ROOT
NS_BEGIN(logger)

// use a prefx that does not clash with any predefined macros (e.g. win32 'ERROR')
enum level_t {
  IRL_FATAL,
  IRL_ERROR,
  IRL_WARN,
  IRL_INFO,
  IRL_DEBUG,
  IRL_TRACE
};

IRESEARCH_API bool enabled(level_t level);
IRESEARCH_API FILE* output(level_t level);
IRESEARCH_API void output(level_t level, FILE* out); // nullptr == /dev/null
IRESEARCH_API void output_le(level_t level, FILE* out); // nullptr == /dev/null
IRESEARCH_API void stack_trace(level_t level);
IRESEARCH_API void stack_trace(level_t level, const std::exception_ptr& eptr);
IRESEARCH_API irs::logger::level_t stack_trace_level(); // stack trace output level
IRESEARCH_API void stack_trace_level(level_t level); // stack trace output level
IRESEARCH_API std::ostream& stream(level_t level);

#ifndef _MSC_VER
  // +1 to skip stack_trace_nomalloc(...)
  void IRESEARCH_API stack_trace_nomalloc(level_t level, size_t skip = 1);
#endif

NS_END // logger
NS_END

#if defined(_MSC_VER)
  #define IR_LOG_FORMATED(level, prefix, format, ...) \
    if (::iresearch::logger::enabled(level)) \
      std::fprintf(::iresearch::logger::output(level), "%s: %s:%u " format "\n", prefix, __FILE__, __LINE__, __VA_ARGS__)

  #define IR_FRMT_FATAL(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_FATAL, "FATAL", format, __VA_ARGS__)
  #define IR_FRMT_ERROR(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_ERROR, "ERROR", format, __VA_ARGS__)
  #define IR_FRMT_WARN(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_WARN, "WARN", format, __VA_ARGS__)
  #define IR_FRMT_INFO(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_INFO, "INFO", format, __VA_ARGS__)
  #define IR_FRMT_DEBUG(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_DEBUG, "DEBUG", format, __VA_ARGS__)
  #define IR_FRMT_TRACE(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_TRACE, "TRACE", format, __VA_ARGS__)
#else // use a GNU extension for ignoring the trailing comma: ', ##__VA_ARGS__'
  #define IR_LOG_FORMATED(level, prefix, format, ...) \
    if (::iresearch::logger::enabled(level)) \
      std::fprintf(::iresearch::logger::output(level), "%s: %s:%u " format "\n", prefix, __FILE__, __LINE__, ##__VA_ARGS__)

  #define IR_FRMT_FATAL(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_FATAL, "FATAL", format, ##__VA_ARGS__)
  #define IR_FRMT_ERROR(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_ERROR, "ERROR", format, ##__VA_ARGS__)
  #define IR_FRMT_WARN(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_WARN, "WARN", format, ##__VA_ARGS__)
  #define IR_FRMT_INFO(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_INFO, "INFO", format, ##__VA_ARGS__)
  #define IR_FRMT_DEBUG(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_DEBUG, "DEBUG", format, ##__VA_ARGS__)
  #define IR_FRMT_TRACE(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_TRACE, "TRACE", format, ##__VA_ARGS__)
#endif

#define IR_LOG_EXCEPTION() \
  if (::iresearch::logger::enabled(::iresearch::logger::stack_trace_level())) { \
    IR_LOG_FORMATED(::iresearch::logger::stack_trace_level(), "EXCEPTION", "@%s\nstack trace:", __FUNCTION__); \
    ::iresearch::logger::stack_trace(::iresearch::logger::stack_trace_level(), std::current_exception()); \
  }
#define IR_LOG_STACK_TRACE() \
  if (::iresearch::logger::enabled(::iresearch::logger::stack_trace_level())) { \
    IR_LOG_FORMATED(::iresearch::logger::stack_trace_level(), "STACK_TRACE", "@%s\nstack trace:", __FUNCTION__); \
    ::iresearch::logger::stack_trace(::iresearch::logger::stack_trace_level()); \
  }

#endif
