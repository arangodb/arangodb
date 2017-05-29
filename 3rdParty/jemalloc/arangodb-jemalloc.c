/* this file will be included in "pages.c" */

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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#define ARANGODB_JEMALLOC 1

// jemalloc needs to be configured such that
//
// - munmap is disabled
// - DSS_PREC_DEFAULT is set to dss_prec_secondary

// path to datafile
static char const* adb_datafile_path;

// datafile counter
static uint64_t adb_datafile_counter = 0;

// maximum size allowed
static uint64_t adb_maximum_size = 0;

// total size allocated
static uint64_t adb_total_size = 0;

// set a datafile path and a limit, path must be static memory!
void adb_jemalloc_set_limit(size_t limit, char const* path) {
  __atomic_store_n(&adb_datafile_path, path, __ATOMIC_SEQ_CST);
  __atomic_store_n(&adb_maximum_size, (uint64_t)limit, __ATOMIC_SEQ_CST);
}

// create a datafile for virtual memory
static int adb_create_datafile(size_t n) {
  int fd;
  ssize_t res;
  char zeros[4 * 1024];
  char filename[PATH_MAX];
  uint64_t counter =
      __atomic_add_fetch(&adb_datafile_counter, (uint64_t)1, __ATOMIC_SEQ_CST);

  char const* name = __atomic_load_n(&adb_datafile_path, __ATOMIC_SEQ_CST);
  char* end;

  if (name == NULL) {
    strcpy(filename, "./vm.");
    end = filename + 5;
  } else {
    size_t m = strlen(name);

    if (m == 0) {
      strcpy(filename, "./vm.");
      end = filename + 5;
    } else {
      if (m >= sizeof(filename) - 11) {
        return -1;
      }

      strcpy(filename, name);
      strcat(filename + m, "vm.");

      end = filename + m + 3;
    }
  }

  *end++ = ((counter / 1000000) % 10) + '0';
  *end++ = ((counter / 100000) % 10) + '0';
  *end++ = ((counter / 10000) % 10) + '0';
  *end++ = ((counter / 1000) % 10) + '0';
  *end++ = ((counter / 100) % 10) + '0';
  *end++ = ((counter / 10) % 10) + '0';
  *end++ = ((counter / 1) % 10) + '0';

  *end = '\0';

  fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    malloc_printf("failed to open vm file %s, errno %d\n", filename, (int)errno);
    return -1;
  }

  memset(zeros, 0, sizeof(zeros));

  lseek(fd, 0, SEEK_SET);

  while (0 < n) {
    size_t l = sizeof(zeros);

    if (n < l) {
      l = n;
    }

    res = write(fd, zeros, l);

    if (res < 0) {
      unlink(filename);
      malloc_printf("failed to create vm file %s, errno %d\n", filename,
                    (int)errno);
      return -1;
    }

    n -= res;
  }

  lseek(fd, 0, SEEK_SET);

  return fd;
}

void* adb_mmap(void* addr, size_t length, int prot, int flags) {
  if (addr != NULL) {
    return MAP_FAILED;
  }

  uint64_t maximum = __atomic_load_n(&adb_maximum_size, __ATOMIC_SEQ_CST);
  uint64_t total = __atomic_load_n(&adb_total_size, __ATOMIC_SEQ_CST);

  void* ret;

  if (maximum == 0 || total < maximum) {
    ret = mmap(addr, length, prot, flags, -1, 0);

    if (maximum == 0 || ret != MAP_FAILED) {
      if (ret != MAP_FAILED) {
        __atomic_add_fetch(&adb_total_size, (uint64_t)length, __ATOMIC_SEQ_CST);
      }

#ifdef MADV_NOHUGEPAGE
      if (ret != MAP_FAILED) {
        madvise(ret, length, MADV_NOHUGEPAGE);
      }
#endif

      return ret;
    }
  }

  int fd = adb_create_datafile(length);

  if (fd < 0) {
    return MAP_FAILED;
  }

  ret = mmap(addr, length, prot,
             (flags & ~((MAP_PRIVATE | MAP_ANON))) | MAP_SHARED, fd, 0);

  if (ret == MAP_FAILED) {
    close(fd);
    return ret;
  }

  __atomic_add_fetch(&adb_total_size, (uint64_t)length, __ATOMIC_SEQ_CST);

  return ret;
}
