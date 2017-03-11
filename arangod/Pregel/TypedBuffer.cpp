// //////////////////////////////////////////////////////////
// MemoryMapped.cpp
// Copyright (c) 2013 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//

#include "Pregel/MemoryMapped.h"

#include "Basics/files.h"
#include "FileUtils.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"

#include <cstdio>
#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;

#ifdef TRI_HAVE_ANONYMOUS_MMAP
TypedBuffer* TypedBuffer::createMMap(size_t entries, int flags) {
  
#ifdef TRI_MMAP_ANONYMOUS
  // fd -1 is required for "real" anonymous regions
  int fd = -1;
  int flags = TRI_MMAP_ANONYMOUS | MAP_SHARED;
#else
  // ugly workaround if MAP_ANONYMOUS is not available
  int fd = TRI_OPEN("/dev/zero", O_RDWR | TRI_O_CLOEXEC);
  if (fd == -1) {
    return nullptr;
  }
  
  int flags = MAP_PRIVATE;
#endif
  
  // memory map the data
  size_t mappedSize = sizeof(T) * entries;
  void* data;
  void* mmHandle;
  int res = TRI_MMFile(nullptr, mappedSize, PROT_WRITE | PROT_READ, flags,
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
  
  T* ptr = (T*)data;
  return new TypedBuffer(StaticStrings::Empty, fd, mmHandle, initialSize, ptr);
}
#else

TypedBuffer* TypedBuffer::createMMap(size_t entries, int flags) {
  
  double tt = TRI_microtime();
  std::string file = "pregel_" + std::to_string((uint64_t)tt) + ".mmap";
  std::string filename = FileUtils::buildFilename(TRI_GetTempPath(), file);
  size_t mappedSize = sizeof(T) * entries;
  
  int fd = TRI_CreateDatafile(filename, mappedSize);
  if (fd < 0) {
    // an error occurred
    return nullptr;
  }
  
  // memory map the data
  void* data;
  void* mmHandle;
  int flags = MAP_SHARED;
#ifdef __linux__
  // try populating the mapping already
  flags |= MAP_POPULATE;
#endif
  int res = TRI_MMFile(0, mappedSize, PROT_WRITE | PROT_READ, flags, fd,
                       &mmHandle, 0, &data);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_CLOSE(fd);
    
    // remove empty file
    TRI_UnlinkFile(filename.c_str());
    
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map file '" << filename << "': '" << TRI_errno_string(res) << "'";
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "The database directory might reside on a shared folder "
    "(VirtualBox, VMWare) or an NFS-mounted volume which does not allow memory mapped files.";
    return nullptr;
  }
  
  // create datafile structure
  T* ptr = (T*)data;
  return new TypedBuffer(entries, ptr);
}
#endif

TypedBuffer* TypedBuffer::createInMemory(size_t entries) {
  return new TypedBuffer(entries, new T[entries]);
}

int TypedBuffer::close()

void TypedBuffer::sequentialAccess() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_SEQUENTIAL);
}

void TypedBuffer::randomAccess() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_RANDOM);
}

void TypedBuffer::willNeed() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_WILLNEED);
}

void TypedBuffer::dontNeed() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_DONTNEED);
}

bool TypedBuffer::readWrite() {
  return (TRI_ProtectMMFile(_data, _initSize, PROT_READ | PROT_WRITE, _fd) == TRI_ERROR_NO_ERROR);
}

/// close file (see close() )
TypedBuffer::~TypedBuffer() { close(); }

/// get OS page size (for remap)
int TypedBuffer::getpagesize() {
#ifdef _MSC_VER
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwAllocationGranularity;
#else
  return sysconf(_SC_PAGESIZE);  //::getpagesize();
#endif
}
