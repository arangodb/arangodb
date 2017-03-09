// //////////////////////////////////////////////////////////
// MemoryMapped.cpp
// Copyright (c) 2013 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//

#include "Pregel/MemoryMapped.h"

#include "ApplicationFeatures/PageSizeFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

#include <iomanip>
#include <sstream>

#include <cstdio>
#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new anonymous datafile
///
/// this is only supported on certain platforms (Linux, MacOS)
////////////////////////////////////////////////////////////////////////////////


MemoryMapped* MemoryMapped::create(size_t initialSize) {
  
#ifdef TRI_HAVE_ANONYMOUS_MMAP
#ifdef TRI_MMAP_ANONYMOUS
  // fd -1 is required for "real" anonymous regions
  int fd = -1;
  int flags = TRI_MMAP_ANONYMOUS | MAP_PRIVATE;
#else
  // ugly workaround if MAP_ANONYMOUS is not available
  int fd = TRI_OPEN("/dev/zero", O_RDWR | TRI_O_CLOEXEC);
  
  if (fd == -1) {
    return nullptr;
  }
  
  int flags = MAP_PRIVATE;
#endif
#endif
  
  // memory map the data
  void* data;
  void* mmHandle;
  int res = TRI_MMFile(nullptr, initialSize, PROT_WRITE | PROT_READ, flags,
                       fd, &mmHandle, 0, &data);
  
#ifdef MAP_ANONYMOUS
  // nothing to do
#else
  // close auxilliary file
  TRI_CLOSE(fd);
  fd = -1;
#endif
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map anonymous region: " << TRI_last_error();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "The database directory might reside on a shared folder "
    "(VirtualBox, VMWare) or an NFS "
    "mounted volume which does not allow memory mapped files.";
    return nullptr;
  }
  
  return new MemoryMapped(fd, mmHandle, initialSize, data);
}



/// open file, mappedBytes = 0 maps the whole file
MemoryMapped::MemoryMapped(int fd, size_t initialSize, void* ptr)
    : _fd(fd),
      _size(initialSize),
      _hint(hint),
      _mappedBytes(mappedBytes),
      _file(0),
#ifdef _MSC_VER
      _mmHandle(NULL),
#endif
  open(filename, mappedBytes, hint);
}

/// close file (see close() )
MemoryMapped::~MemoryMapped() { close(); }




/// get OS page size (for remap)
int MemoryMapped::getpagesize() {
#ifdef _MSC_VER
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwAllocationGranularity;
#else
  return sysconf(_SC_PAGESIZE);  //::getpagesize();
#endif
}
