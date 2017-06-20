//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_LOG_H
#define IRESEARCH_LOG_H

#include <string>
#include <iostream>
#include "shared.hpp"

#if defined(_MSC_VER) || defined(__MINGW32__)
  #define IR_SIZE_T_SPECIFIER    "%Iu"
  #define IR_SSIZE_T_SPECIFIER   "%Id"
  #define IR_PTRDIFF_T_SPECIFIER "%Id"
#elif defined(__GNUC__)
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
IRESEARCH_API std::ostream& stream(level_t level);

#ifndef _MSC_VER
  // +1 to skip stack_trace_nomalloc(...)
  void IRESEARCH_API stack_trace_nomalloc(level_t level, size_t skip = 1);
#endif

NS_END // logger
NS_END

NS_LOCAL

FORCE_INLINE CONSTEXPR iresearch::logger::level_t exception_stack_trace_level() {
  return iresearch::logger::IRL_DEBUG;
}

NS_END

#define IR_LOG_FORMATED(level, prefix, format, ...) \
  std::fprintf(::iresearch::logger::output(level), "%s: %s:%u " format "\n", prefix, __FILE__, __LINE__, __VA_ARGS__)
#define IR_LOG_STREM(level, prefix) \
  ::iresearch::logger::stream(level) << prefix << " " << __FILE__ << ":" << __LINE__ << " "

#define IR_FRMT_FATAL(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_FATAL, "FATAL", format, __VA_ARGS__)
#define IR_FRMT_ERROR(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_ERROR, "ERROR", format, __VA_ARGS__)
#define IR_FRMT_WARN(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_WARN, "WARN", format, __VA_ARGS__)
#define IR_FRMT_INFO(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_INFO, "INFO", format, __VA_ARGS__)
#define IR_FRMT_DEBUG(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_DEBUG, "DEBUG", format, __VA_ARGS__)
#define IR_FRMT_TRACE(format, ...) IR_LOG_FORMATED(::iresearch::logger::IRL_TRACE, "TRACE", format, __VA_ARGS__)

#define IR_STRM_FATAL() IR_LOG_STREM(::iresearch::logger::IRL_FATAL, "FATAL")
#define IR_STRM_ERROR() IR_LOG_STREM(::iresearch::logger::IRL_ERROR, "ERROR")
#define IR_STRM_WARN() IR_LOG_STREM(::iresearch::logger::IRL_WARN, "WARN")
#define IR_STRM_INFO() IR_LOG_STREM(::iresearch::logger::IRL_INFO, "INFO")
#define IR_STRM_DEBUG() IR_LOG_STREM(::iresearch::logger::IRL_DEBUG, "DEBUG")
#define IR_STRM_TRACE() IR_LOG_STREM(::iresearch::logger::IRL_TRACE, "TRACE")

#define IR_EXCEPTION() \
  IR_LOG_FORMATED(exception_stack_trace_level(), "EXCEPTION", "@%s\nstack trace:", __FUNCTION__); \
  ::iresearch::logger::stack_trace(exception_stack_trace_level(), std::current_exception());
#define IR_STACK_TRACE() \
  IR_LOG_FORMATED(exception_stack_trace_level(), "STACK_TRACE", "@%s\nstack trace:", __FUNCTION__); \
  ::iresearch::logger::stack_trace(exception_stack_trace_level());

#endif