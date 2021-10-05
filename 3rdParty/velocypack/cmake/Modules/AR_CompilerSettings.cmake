include(TargetArch)
target_architecture(ARCH)
find_package(SSE)

# compiler options

if(CMAKE_COMPILER_IS_GNUCXX)
    message(STATUS "Compiler type GNU: ${CMAKE_CXX_COMPILER}")
    set(BASE_COMPILER_OPTIONS "-Wall -Wextra")

    if(Coverage)
        set(BASE_COMPILER_OPTIONS "${BASE_COMPILER_OPTIONS} -fprofile-arcs -ftest-coverage")
    endif()

    set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} ${BASE_COMPILER_OPTIONS}")
    set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} ${BASE_COMPILER_OPTIONS} -O0 -g -DVELOCYPACK_DEBUG=1")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} ${BASE_COMPILER_OPTIONS} -Os")
    set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} ${BASE_COMPILER_OPTIONS} -O3 -fomit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BASE_COMPILER_OPTIONS} -O3 -g")

    set(CanUseSSE42 ${SSE4_2_FOUND} CACHE BOOL "Use SSE4.2 optimizations")
elseif(CMAKE_COMPILER_IS_CLANGCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # using regular Clang or AppleClang
    message(STATUS "Compiler type CLANG: ${CMAKE_CXX_COMPILER}")
    set(BASE_COMPILER_OPTIONS "-Wall -Wextra")

    set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} ${BASE_COMPILER_OPTIONS}")
    set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} ${BASE_COMPILER_OPTIONS} -O0 -g -DVELOCYPACK_DEBUG=1")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} ${BASE_COMPILER_OPTIONS} -Os")
    set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} ${BASE_COMPILER_OPTIONS} -O3 -fomit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BASE_COMPILER_OPTIONS} -O3 -g")

    set(CanUseSSE42 ${SSE4_2_FOUND} CACHE BOOL "Use SSE4.2 optimizations")

elseif(MSVC) 
    message(STATUS "Compiler type MSVC: ${CMAKE_CXX_COMPILER}")
    add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")

    foreach (flag_var
           CMAKE_CXX_FLAGS
           CMAKE_CXX_FLAGS_DEBUG
           CMAKE_CXX_FLAGS_RELEASE
           CMAKE_CXX_FLAGS_MINSIZEREL
           CMAKE_CXX_FLAGS_RELWITHDEBINFO)

    # Make MSVC correctly report the C++ version in __cplusplus; without it,
    # it always reports 199711L (i.e. C++98).
    set(${flag_var} "${${flag_var}} /Zc:__cplusplus")

  endforeach()

  # https://msdn.microsoft.com/en-us/library/aa267384%28VS.60%29.aspx
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /LTCG")
  set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /SUBSYSTEM:CONSOLE")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SUBSYSTEM:CONSOLE")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /SUBSYSTEM:CONSOLE")

  set(CanUseSSE42 True CACHE BOOL "Use SSE4.2 optimizations")
else()
  # unknown compiler
  message(STATUS "Compiler type UNKNOWN: ${CMAKE_CXX_COMPILER}")
  set(BASE_COMPILER_OPTIONS "-Wall -Wextra")
  set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} ${BASE_COMPILER_OPTIONS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} ${BASE_COMPILER_OPTIONS} -O0 -g -DVELOCYPACK_DEBUG=1")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} ${BASE_COMPILER_OPTIONS} -Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} ${BASE_COMPILER_OPTIONS} -O3 -fomit-frame-pointer")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BASE_COMPILER_OPTIONS} -O3 -g")
endif()

# options
if(CanUseSSE42)
  option(EnableSSE "Build using SSE4.2 optimizations" ON)
else()
  option(EnableSSE "Build using SSE4.2 optimizations" OFF)
endif()
  
message(STATUS "Building with SSE 4.2 optimizations enabled: ${EnableSSE}")
if(EnableSSE AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
  add_definitions("-DASM_OPTIMIZATIONS=1")
else()
  add_definitions("-DASM_OPTIMIZATIONS=0")
endif()
