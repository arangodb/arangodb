if (SANITIZE_ADDRESS)
  include(SanitizeAddress)
  set(IRS_SANITIZE "address")
endif ()
if (SANITIZE_LEAK)
  include(SanitizeLeak)
  set(IRS_SANITIZE "leak")
endif ()
if (SANITIZE_MEMORY)
  include(SanitizeMemory)
  set(IRS_SANITIZE "memory")
endif ()
if (SANITIZE_THREAD)
  include(SanitizeThread)
  set(IRS_SANITIZE "thread")
endif ()
if (SANITIZE_UNDEFINED)
  include(SanitizeUndefined)
  set(IRS_SANITIZE "undefined")
endif ()

if (IRS_SANITIZE AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  # Nicer stack trace
  # TODO(MBkkt) Maybe -fsanitize-recover=?
  add_compile_options(
    -fno-omit-frame-pointer
  )
endif ()
