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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_PROCESS_UTILS_H
#define IRESEARCH_PROCESS_UTILS_H

#ifdef _WIN32
  typedef int pid_t;
#endif

namespace iresearch {

IRESEARCH_API pid_t get_pid();
bool is_running(pid_t pid);
bool is_valid_pid(const char* buf);

}

#endif // IRESEARCH_PROCESS_UTILS_H
