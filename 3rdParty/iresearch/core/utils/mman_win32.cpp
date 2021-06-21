////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)

#include "shared.hpp"
#include "mman_win32.hpp"
#include "log.hpp"

#include <windows.h>
#include <errno.h>
#include <io.h>

namespace {

DWORD page_protection(int prot) noexcept {
  switch (prot) {
    case PROT_NONE:
      return PAGE_NOACCESS;

    case PROT_READ:
      return PAGE_READONLY;

    case PROT_WRITE:
    case PROT_READ | PROT_WRITE:
      return PAGE_READWRITE;

    case PROT_EXEC:
      return PAGE_EXECUTE;

    case PROT_EXEC | PROT_READ:
      return PAGE_EXECUTE_READ;

    case PROT_EXEC | PROT_WRITE:
    case PROT_EXEC | PROT_READ | PROT_WRITE:
      return PAGE_EXECUTE_READWRITE;
  }

  IR_FRMT_ERROR("Can't convert protection level %d, use PAGE_NOACCESS", prot);

  return PAGE_NOACCESS; // fallback
}

DWORD file_protection(int prot) noexcept {
  DWORD access = 0;

  if (PROT_NONE == prot) {
    return access;
  }

  if ((prot & PROT_READ) != 0) {
    access |= FILE_MAP_READ;
  }

  if ((prot & PROT_WRITE) != 0) {
    access |= FILE_MAP_WRITE;
  }

  if ((prot & PROT_EXEC) != 0) {
    access |= FILE_MAP_EXECUTE;
  }

  return access;
}

} //namespace

void* mmap(void* /*addr*/, size_t len, int prot, int flags, int fd, OffsetType off) {
  const OffsetType maxSize = off + static_cast<OffsetType>(len);

  const DWORD dwFileOffsetLow = (sizeof(OffsetType) <= sizeof(DWORD))
      ? static_cast<DWORD>(off)
      : static_cast<DWORD>(off & UINT32_C(0xFFFFFFFF));

  const DWORD dwFileOffsetHigh = (sizeof(OffsetType) <= sizeof(DWORD))
      ? static_cast<DWORD>(0)
      : static_cast<DWORD>((off >> 32) & UINT32_C(0xFFFFFFFF));

  const DWORD dwMaxSizeLow = (sizeof(OffsetType) <= sizeof(DWORD))
      ? static_cast<DWORD>(maxSize)
      : static_cast<DWORD>(maxSize & UINT32_C(0xFFFFFFFF));

  const DWORD dwMaxSizeHigh = (sizeof(OffsetType) <= sizeof(DWORD))
      ? static_cast<DWORD>(0)
      : static_cast<DWORD>((maxSize >> 32) & UINT32_C(0xFFFFFFFF));

  const DWORD protect = page_protection(prot);

  const DWORD desiredAccess = file_protection(prot);

  errno = 0;

  if (len == 0
      || (flags & MAP_FIXED) != 0  // Unsupported flag combinations
      || prot == PROT_EXEC) {      // Usupported protection combinations
    errno = EINVAL;
    return MAP_FAILED;
  }

  HANDLE handle = ((flags & MAP_ANONYMOUS) == 0)
      ? (HANDLE)_get_osfhandle(fd)
      : INVALID_HANDLE_VALUE;

  if ((flags & MAP_ANONYMOUS) == 0 && handle == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return MAP_FAILED;
  }

  HANDLE mapping = CreateFileMapping(handle, NULL, protect, dwMaxSizeHigh, dwMaxSizeLow, NULL);

  if (NULL == mapping) {
    errno = GetLastError();
    return MAP_FAILED;
  }

  void* map = MapViewOfFile(mapping, desiredAccess, dwFileOffsetHigh, dwFileOffsetLow, len);

  // According to (https://msdn.microsoft.com/en-us/library/windows/desktop/aa366537(v=vs.85).aspx)
  // Mapped views of a file mapping object maintain internal references to the object,
  // and a file mapping object does not close until all references to it are released.
  // Therefore, to fully close a file mapping object, an application must unmap all mapped views
  // of the file mapping object by calling UnmapViewOfFile and close the file mapping object
  // handle by calling CloseHandle. These functions can be called in any order.
  CloseHandle(mapping);

  if (NULL == map) {
    errno = GetLastError();
    return MAP_FAILED;
  }

  return map;
}

int munmap(void *addr, size_t) {
  if (UnmapViewOfFile(addr)) {
    return 0;
  }

  errno = GetLastError();

  return -1;
}

int mprotect(void *addr, size_t len, int prot) {
  DWORD newProtect = page_protection(prot);
  DWORD oldProtect = 0;

  if (VirtualProtect(addr, len, newProtect, &oldProtect)) {
    return 0;
  }

  errno = GetLastError();

  return -1;
}

int msync(void *addr, size_t len, int /*flags*/) {
  if (FlushViewOfFile(addr, len)) {
    return 0;
  }

  errno = GetLastError();

  return -1;
}

int mlock(const void *addr, size_t len) {
  if (VirtualLock(LPVOID(addr), len)) {
    return 0;
  }

  errno = GetLastError();

  return -1;
}

int munlock(const void *addr, size_t len) {
  if (VirtualUnlock(LPVOID(addr), len)) {
    return 0;
  }

  errno = GetLastError();

  return -1;
}

int madvise(void*, size_t, int) {
  return 0;
}

#endif // defined(_MSC_VER)
