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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_MEMORY__MAP__WIN32_H
#define ARANGODB_BASICS_MEMORY__MAP__WIN32_H 1

#include "Basics/Common.h"

#ifdef TRI_HAVE_WIN32_MMAP

#include <Windows.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief undefine this so anonymous memory mapping is disabled
///
/// anonymous memory mapping may or may not work on Windows
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Flags used when we create a memory map -- dummy flags for windows for now
////////////////////////////////////////////////////////////////////////////////

#define MAP_SHARED 0x01    /* Share changes */
#define MAP_PRIVATE 0x02   /* Changes are private */
#define MAP_TYPE 0x0f      /* Mask for type of mapping */
#define MAP_FIXED 0x10     /* Interpret addr exactly */
#define MAP_ANONYMOUS 0x20 /* don't use a file */

#define TRI_MMAP_ANONYMOUS MAP_ANONYMOUS

////////////////////////////////////////////////////////////////////////////////
// Define some dummy flags which are ignored under windows.
// Under windows only the MS_SYNC flag makes sense, that is, all memory map
// file flushes are synchronous.
////////////////////////////////////////////////////////////////////////////////

#define MS_ASYNC 1      /* sync memory asynchronously */
#define MS_INVALIDATE 2 /* invalidate the caches */
#define MS_SYNC 4       /* synchronous memory sync */

#define PROT_READ 0x1  /* Page can be read.  */
#define PROT_WRITE 0x2 /* Page can be written.  */
#define PROT_EXEC 0x4  /* Page can be executed.  */
#define PROT_NONE 0x0  /* Page can not be accessed.  */
#define PROT_GROWSDOWN \
  0x01000000 /* Extend change to start of growsdown vma (mprotect only).  */
#define PROT_GROWSUP \
  0x02000000 /* Extend change to start of growsup vma (mprotect only).  */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief constants for TRI_MMFileAdvise, not on Windows
////////////////////////////////////////////////////////////////////////////////

#define TRI_MADVISE_SEQUENTIAL 0
#define TRI_MADVISE_RANDOM 0
#define TRI_MADVISE_WILLNEED 0
#define TRI_MADVISE_DONTNEED 0
#define TRI_MADVISE_DONTDUMP 0

#endif
