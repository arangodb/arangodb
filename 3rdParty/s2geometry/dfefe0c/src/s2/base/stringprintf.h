// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// NOTE: See third_party/absl/strings for more options.
//
// As of 2017q4, most use of these routines is considered legacy: use
// of absl::StrCat, absl::Substitute, or absl::StrFormat is preferred for
// performance and safety reasons.

#ifndef S2_BASE_STRINGPRINTF_H_
#define S2_BASE_STRINGPRINTF_H_

#include <cstdarg>
#include <string>
#include <vector>

#include "s2/base/port.h"

// Return a C++ string
extern string StringPrintf(const char* format, ...)
    // Tell the compiler to do printf format string checking.
    ABSL_PRINTF_ATTRIBUTE(1, 2);

// Store result into a supplied string and return it
extern const string& SStringPrintf(string* dst, const char* format, ...)
    // Tell the compiler to do printf format string checking.
    ABSL_PRINTF_ATTRIBUTE(2, 3);

// Append result to a supplied string
extern void StringAppendF(string* dst, const char* format, ...)
    // Tell the compiler to do printf format string checking.
    ABSL_PRINTF_ATTRIBUTE(2, 3);

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
//
// Implementation note: the va_list is never modified, this implementation
// always operates on copies.
extern void StringAppendV(string* dst, const char* format, va_list ap);

#endif  // S2_BASE_STRINGPRINTF_H_
