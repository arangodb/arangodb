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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/operating-system.h"

#include "Basics/PageSize.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

using namespace arangodb;

namespace {

// Windows variant for getpagesize()
#ifdef _WIN32
int pageSizeImpl() {
  SYSTEM_INFO systemInfo;
  GetSystemInfo(&systemInfo);
  return systemInfo.dwPageSize;
}
#else
int pageSizeImpl() {
  return getpagesize();
}
#endif

struct PageSizeCache {
  PageSizeCache() : cachedValue(pageSizeImpl()) {}
  int cachedValue;
};

PageSizeCache const cache;

} // namespace

/// @brief return page size from cache
int arangodb::PageSize::getValue() {
  return ::cache.cachedValue;
}
