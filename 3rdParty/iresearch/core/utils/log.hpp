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
#include "stdarg.h"

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

namespace iresearch {
namespace logger {

// use a prefx that does not clash with any predefined macros (e.g. win32 'ERROR')
enum level_t {
  IRL_FATAL,
  IRL_ERROR,
  IRL_WARN,
  IRL_INFO,
  IRL_DEBUG,
  IRL_TRACE
};

////////////////////////////////////////////////////////////////////////////////
/// @brief log appender callback
/// @param context user defined context supplied with the appender callback
/// @param function source function name. Could be a nullptr.
/// @param file source file name. Could be a nullptr
/// @param line source line number
/// @param level message log level
/// @param message text to log. Null terminated. Could be a nullptr.
/// @param message_len length of message text in bytes not including null terminator.
////////////////////////////////////////////////////////////////////////////////
using log_appender_callback_t = void(*)(void* context, const char* function,
                                        const char* file, int line, level_t level,
                                        const char* message, size_t message_len);

IRESEARCH_API bool enabled(level_t level);

// Backward compatible fd-appender control functions
IRESEARCH_API void output(level_t level, FILE* out); // nullptr == /dev/null
IRESEARCH_API void output_le(level_t level, FILE* out); // nullptr == /dev/null
// Custom appender control functions
IRESEARCH_API void output(level_t level, log_appender_callback_t appender, void* context); // nullptr == appender -> log level disabled
IRESEARCH_API void output_le(level_t level, log_appender_callback_t appender, void* context); // nullptr == appender -> log level disabled
IRESEARCH_API void log(const char* function, const char* file, int line,
                       level_t level, const char* message, size_t len);
IRESEARCH_API void stack_trace(level_t level);
IRESEARCH_API void stack_trace(level_t level, const std::exception_ptr& eptr);
IRESEARCH_API irs::logger::level_t stack_trace_level(); // stack trace output level
IRESEARCH_API void stack_trace_level(level_t level); // stack trace output level

#ifndef _MSC_VER
  // +1 to skip stack_trace_nomalloc(...)
  void IRESEARCH_API stack_trace_nomalloc(level_t level, int fd, size_t skip = 1);
#endif

namespace detail {
// not everyone who includes header actually logs something, that`s ok
[[maybe_unused]] static void log_formatted(const char* function, const char* file, int line,
                                           level_t level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const ptrdiff_t required_len = vsnprintf(nullptr, 0, format, args);
  va_end(args);
  if (required_len > 0) {
    std::string buf(size_t(required_len) + 1, 0);
    va_list args1;
    va_start(args1, format);
    vsnprintf(buf.data(), buf.size(), format, args1);
    va_end(args1);
    log(function, file, line, level, buf.data(), size_t(required_len));
  }
}
} // detail
} // logger
}

#if defined(_MSC_VER)
  #define IR_LOG_FORMATED(level, format, ...) \
    if (::iresearch::logger::enabled(level)) \
      ::iresearch::logger::detail::log_formatted(IRESEARCH_CURRENT_FUNCTION, __FILE__, __LINE__, level, format, __VA_ARGS__)

  #define IR_FRMT_FATAL(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_FATAL, format, __VA_ARGS__)
  #define IR_FRMT_ERROR(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_ERROR, format, __VA_ARGS__)
  #define IR_FRMT_WARN(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_WARN, format, __VA_ARGS__)
  #define IR_FRMT_INFO(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_INFO, format, __VA_ARGS__)
  #define IR_FRMT_DEBUG(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_DEBUG, format, __VA_ARGS__)
  #define IR_FRMT_TRACE(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_TRACE, format, __VA_ARGS__)
#else // use a GNU extension for ignoring the trailing comma: ', ##__VA_ARGS__'
  #define IR_LOG_FORMATED(level, format, ...) \
    if (::iresearch::logger::enabled(level)) \
      ::iresearch::logger::detail::log_formatted(IRESEARCH_CURRENT_FUNCTION, __FILE__, __LINE__, level, format, ##__VA_ARGS__)

  #define IR_FRMT_FATAL(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_FATAL, format, ##__VA_ARGS__)
  #define IR_FRMT_ERROR(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_ERROR, format, ##__VA_ARGS__)
  #define IR_FRMT_WARN(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_WARN, format, ##__VA_ARGS__)
  #define IR_FRMT_INFO(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_INFO, format, ##__VA_ARGS__)
  #define IR_FRMT_DEBUG(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_DEBUG, format, ##__VA_ARGS__)
  #define IR_FRMT_TRACE(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_TRACE, format, ##__VA_ARGS__)
#endif

#define IR_LOG_EXCEPTION() \
  if (::iresearch::logger::enabled(::iresearch::logger::stack_trace_level())) { \
    IR_LOG_FORMATED(::iresearch::logger::stack_trace_level(), "@%s\n Exception stack trace:",IRESEARCH_CURRENT_FUNCTION); \
    ::iresearch::logger::stack_trace(::iresearch::logger::stack_trace_level(), std::current_exception()); \
  }
#define IR_LOG_STACK_TRACE() \
  if (::iresearch::logger::enabled(::iresearch::logger::stack_trace_level())) { \
    IR_LOG_FORMATED(::iresearch::logger::stack_trace_level(), "@%s\nstack trace:", IRESEARCH_CURRENT_FUNCTION); \
    ::iresearch::logger::stack_trace(::iresearch::logger::stack_trace_level()); \
  }

#endif
