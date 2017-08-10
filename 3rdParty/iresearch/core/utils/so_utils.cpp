//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#if defined (__GNUC__)
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  #include <boost/filesystem/operations.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "log.hpp"
#include "utils/utf8_path.hpp"

#include "so_utils.hpp"

#if defined(_MSC_VER) // Microsoft compiler
  #define NOMINMAX
  #include <windows.h>

  NS_LOCAL

  std::string dlerror() {
    auto error = GetLastError();

    if (!error) {
      return std::string();
    }

    LPVOID lpMsgBuf;
    auto bufLen = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&lpMsgBuf,
      0,
      NULL
    );

    if (!bufLen) {
      return std::string();
    }

    auto lpMsgStr = (LPTSTR)lpMsgBuf;
    std::string result(lpMsgStr, lpMsgStr+bufLen);

    LocalFree(lpMsgBuf);

    return result;
  }

  NS_END

#elif defined(__GNUC__) // GNU compiler
  #include <dlfcn.h>
#else
  #error define your copiler
#endif

NS_LOCAL

#if defined(_MSC_VER) // Microsoft compiler
  const std::string FILENAME_EXTENSION(".dll");
#elif defined(__GNUC__) // GNU compiler
  const std::string FILENAME_EXTENSION(".so");
#else
  const std::string FILENAME_EXTENSION("");
#endif

NS_END

NS_ROOT

void* load_library(const char* soname, int mode /* = 2 */) {
  if (!soname) {
    return nullptr;
  }

  std::string name(soname);

  name += FILENAME_EXTENSION;

#if defined(_MSC_VER) // Microsoft compiler
  UNUSED(mode);
  auto handle = static_cast<void*>(::LoadLibraryA(name.c_str()));
#elif defined(__GNUC__) // GNU compiler
  auto handle = dlopen(name.c_str(), mode);
#endif

  if (!handle) {
    #ifdef _WIN32
      IR_FRMT_ERROR("load failed of shared object: %s error: %s", name.c_str(), dlerror().c_str());
    #else
      IR_FRMT_ERROR("load failed of shared object: %s error: %s", name.c_str(), dlerror());
    #endif
  }

  return handle;
}

void* get_function(void* handle, const char* fname) {
#if defined(_MSC_VER) // Microsoft compiler
  return static_cast<void*>(::GetProcAddress(static_cast<HINSTANCE>(handle), fname));
#elif defined(__GNUC__) // GNU compiler
  return dlsym(handle, fname);
#endif
}

bool free_library(void* handle) {
#if defined(_MSC_VER) // Microsoft compiler
  return TRUE == ::FreeLibrary(static_cast<HINSTANCE>(handle));
#elif defined(__GNUC__) // GNU compiler
  return dlclose(handle);
#endif
}

void load_libraries(
  const std::string& path,
  const std::string& prefix,
  const std::string& suffix
) {
  ::boost::filesystem::path pluginPath(path);

  if (!::boost::filesystem::is_directory(pluginPath)) {
    auto u8path = (utf8_path(pluginPath)).utf8();

    IR_FRMT_INFO("library load failed, not a plugin path: %s", u8path.c_str());

    return; // no plugins directory
  }

  for (::boost::filesystem::directory_iterator itr(pluginPath), end; itr != end; ++itr) {
    if (::boost::filesystem::is_directory(itr->status())) {
      continue;
    }

    auto& file = itr->path();
    auto extension = utf8_path(file.extension()).utf8();

    if (FILENAME_EXTENSION != extension.c_str()) {
      continue; // skip non-library extensions
    }

    auto stem = utf8_path(file.stem()).utf8();

    if (stem.size() < prefix.size() + suffix.size() ||
        strncmp(stem.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        strncmp(stem.c_str() + stem.size() - suffix.size(), suffix.c_str(), suffix.size()) != 0) {
      continue; // filename does not match
    }

    auto u8path = (utf8_path(file.parent_path())/=stem).utf8(); // strip extension

    // FIMXE check double-load of same dll
    void* handle = load_library(u8path.c_str(), 1);

    if (!handle) {
      IR_FRMT_ERROR("library load failed for path: %s", u8path.c_str());
    }
  }
}

NS_END // ROOT
