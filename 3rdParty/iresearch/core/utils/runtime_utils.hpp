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

#ifndef IRESEARCH_RUNTIME_UTILS_H
#define IRESEARCH_RUNTIME_UTILS_H

namespace iresearch {

inline const char* getenv(const char* name) noexcept {
  #ifdef _MSC_VER
    #pragma warning(disable : 4996)
  #endif

    return std::getenv(name);

  #ifdef _MSC_VER
    #pragma warning(default : 4996)
  #endif
}

inline bool localtime(struct tm& buf, const time_t& time) noexcept {
  // use a thread safe conversion function
  #ifdef _MSC_VER
    return 0 == ::localtime_s(&buf, &time);
  #else
    return nullptr != ::localtime_r(&time, &buf);
  #endif
}

inline int setenv(const char *name, const char *value, bool overwrite) noexcept {
  #ifdef _MSC_VER
    UNUSED(overwrite);
    return _putenv_s(name, value);  // OVERWRITE is always true for MSVC
  #else
    return ::setenv(name, value, overwrite);
  #endif
}

inline int unsetenv(const char* name) noexcept {
  #ifdef _MSC_VER
    return _putenv_s(name, "");
  #else
    return ::unsetenv(name);
  #endif
}

}

#endif
