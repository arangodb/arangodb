# - Locate jemalloc library
# Defines:
#
#  JEMALLOC_FOUND
#  JEMALLOC_INCLUDE_DIR
#  JEMALLOC_INCLUDE_DIRS (not cached)
#  JEMALLOC_jemalloc_LIBRARY
#  JEMALLOC_LIBRARIES (not cached)
#  JEMALLOC_LIBRARY_DIRS (not cached)
#  PPROF_EXECUTABLE

find_path(JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h)
foreach(component jemalloc)
  find_library(JEMALLOC_${component}_LIBRARY NAMES ${component})
  mark_as_advanced(JEMALLOC_${component}_LIBRARY)
endforeach()

find_program(PPROF_EXECUTABLE NAMES pprof
             HINTS ${JEMALLOC_INCLUDE_DIR}/../bin)

set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
set(JEMALLOC_LIBRARIES ${JEMALLOC_jemalloc_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set JEMALLOC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(jemalloc DEFAULT_MSG JEMALLOC_INCLUDE_DIR JEMALLOC_LIBRARIES)

mark_as_advanced(JEMALLOC_FOUND JEMALLOC_INCLUDE_DIR PPROF_EXECUTABLE)

if(JEMALLOC_jemalloc_LIBRARY)
  get_filename_component(JEMALLOC_LIBRARY_DIRS ${JEMALLOC_jemalloc_LIBRARY} PATH)
endif()
