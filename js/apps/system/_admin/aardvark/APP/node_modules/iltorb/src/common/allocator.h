#ifndef ILTORB_ALLOCATOR_H
#define ILTORB_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

struct Allocator {
  Allocator() : allocated_unreported_memory(0) {}

  int64_t allocated_unreported_memory;

  struct AllocatedBuffer {
    size_t size;
    /* char data[...]; */
  };

  void* Alloc(size_t size);
  void Free(void* address);

  static AllocatedBuffer* GetBufferInfo(void* address);
  void ReportMemoryToV8();

  // Brotli-style parameter order.
  static void* Alloc(void* opaque, size_t size);
  static void Free(void* opaque, void* address);

  // Like Free, but in node::Buffer::FreeCallback style.
  static void NodeFree(char* address, void* opaque) {
    return Free(opaque, address);
  }
};

#endif
