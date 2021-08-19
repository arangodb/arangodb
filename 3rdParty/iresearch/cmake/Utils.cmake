macro(SET_FIND_LIBRARY_OPTIONS _prefixes _suffixes)
  set(_CMAKE_FIND_LIBRARY_PREFIXES "${CMAKE_FIND_LIBRARY_PREFIXES}")
  set(_CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES}")
  
  set(CMAKE_FIND_LIBRARY_PREFIXES "${_prefixes}" CACHE INTERNAL "" FORCE )
  set(CMAKE_FIND_LIBRARY_SUFFIXES "${_suffixes}" CACHE INTERNAL "" FORCE ) 
endmacro()

macro(RESTORE_FIND_LIBRARY_OPTIONS)
  set(CMAKE_FIND_LIBRARY_PREFIXES "${_CMAKE_FIND_LIBRARY_PREFIXES}" CACHE INTERNAL "" FORCE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES "${_CMAKE_FIND_LIBRARY_SUFFIXES}" CACHE INTERNAL "" FORCE)
endmacro()

macro(ADD_OPTION_GPROF DEFAULT)
  if (CMAKE_COMPILER_IS_GNUCXX)
      option(USE_GPROF "Compile using -pg for gprof output" ${DEFAULT})

      if(USE_GPROF)
          message(STATUS "Using gprof output for ${CMAKE_PROJECT_NAME}")
          set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
          set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
          set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
      else()
          message(STATUS "NOT using gprof output for ${CMAKE_PROJECT_NAME}")
      endif()
  else()
      message(STATUS "gprof generation NOT AVAILABLE - Not a GNU compiler")
  endif()
endmacro()

# expands ~ to user home directory
#
# usage:
# expand_path("~/code" x)
function(expand_path in outvar)
  string(SUBSTRING ${in} 0 1 first)
  if(NOT ${first} STREQUAL "~")
    set(${outvar} ${in} PARENT_SCOPE)
    return()
  endif()

  if(WIN32 AND NOT CYGWIN)
    set(home $ENV{USERPROFILE})
  else()
    set(home $ENV{HOME})
  endif()

  if(NOT home)
    set(${outvar} ${in} PARENT_SCOPE)
    return()
  endif()

  string(SUBSTRING ${in} 1 -1 tail)
  if(CMAKE_VERSION VERSION_LESS 3.20)
    file(TO_CMAKE_PATH ${home}${tail} out)
  else()
    cmake_path(CONVERT ${home}${tail} TO_CMAKE_PATH_LIST out)
  endif()

  set(${outvar} ${out} PARENT_SCOPE)
endfunction(expand_path)
