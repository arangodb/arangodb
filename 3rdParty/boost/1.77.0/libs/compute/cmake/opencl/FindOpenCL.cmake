# - Try to find OpenCL
# This module tries to find an OpenCL implementation on your system. It supports
# AMD / ATI, Apple and NVIDIA implementations.
#
# To set the paths manually, define these environment variables:
# OpenCL_INCPATH    - Include path (e.g. OpenCL_INCPATH=/opt/cuda/4.0/cuda/include)
# OpenCL_LIBPATH    - Library path (e.h. OpenCL_LIBPATH=/usr/lib64/nvidia)
#
# Once done this will define
#  OpenCL_FOUND         - system has OpenCL
#  OpenCL_INCLUDE_DIRS  - the OpenCL include directory
#  OpenCL_LIBRARIES     - link these to use OpenCL

FIND_PACKAGE(PackageHandleStandardArgs)

SET (OpenCL_VERSION_STRING "0.1.0")
SET (OpenCL_VERSION_MAJOR 0)
SET (OpenCL_VERSION_MINOR 1)
SET (OpenCL_VERSION_PATCH 0)

IF (APPLE)

    FIND_LIBRARY(OpenCL_LIBRARIES OpenCL DOC "OpenCL lib for OSX")
    FIND_PATH(OpenCL_INCLUDE_DIRS OpenCL/cl.h DOC "Include for OpenCL on OSX")
    FIND_PATH(_OpenCL_CPP_INCLUDE_DIRS OpenCL/cl.hpp DOC "Include for OpenCL CPP bindings on OSX")

ELSE (APPLE)

    IF (WIN32)
	# The AMD SDK currently installs both x86 and x86_64 libraries
	# This is only a hack to find out architecture
	IF( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	    SET(OpenCL_LIB_DIR "$ENV{AMDAPPSDKROOT}/lib/x86_64")
	ELSE( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	    SET(OpenCL_LIB_DIR "$ENV{AMDAPPSDKROOT}/lib/x86")
	ENDIF( CMAKE_SIZEOF_VOID_P EQUAL 8 )

	FIND_LIBRARY(OpenCL_LIBRARIES OpenCL.lib PATHS
	    ${OpenCL_LIB_DIR} $ENV{OpenCL_LIBPATH} $ENV{CUDA_LIB_PATH})

	GET_FILENAME_COMPONENT(_OpenCL_INC_CAND ${OpenCL_LIB_DIR}/../../include ABSOLUTE)

	# On Win32 search relative to the library
	FIND_PATH(OpenCL_INCLUDE_DIRS CL/cl.h PATHS
	    ${_OpenCL_INC_CAND} $ENV{OpenCL_INCPATH} $ENV{CUDA_INC_PATH})
	FIND_PATH(_OpenCL_CPP_INCLUDE_DIRS CL/cl.hpp PATHS
	    ${_OpenCL_INC_CAND} $ENV{OpenCL_INCPATH} $ENV{CUDA_INC_PATH})

    ELSE (WIN32)

	# Unix style platforms
	FIND_LIBRARY(OpenCL_LIBRARIES OpenCL
	    PATHS ENV LD_LIBRARY_PATH ENV OpenCL_LIBPATH
	    )

	GET_FILENAME_COMPONENT(OpenCL_LIB_DIR ${OpenCL_LIBRARIES} PATH)
	GET_FILENAME_COMPONENT(_OpenCL_INC_CAND ${OpenCL_LIB_DIR}/../../include ABSOLUTE)

	# The AMD SDK currently does not place its headers
	# in /usr/include, therefore also search relative
	# to the library
	FIND_PATH(OpenCL_INCLUDE_DIRS CL/cl.h PATHS "$ENV{CUDADIR}/include" ${_OpenCL_INC_CAND} "/usr/local/cuda/include" "/opt/cuda/include" "/opt/AMDAPP/include" ENV OpenCL_INCPATH)
	FIND_PATH(_OpenCL_CPP_INCLUDE_DIRS CL/cl.hpp PATHS "$ENV{CUDADIR}/include" ${_OpenCL_INC_CAND} "/usr/local/cuda/include" "/opt/cuda/include" "/opt/AMDAPP/include" ENV OpenCL_INCPATH)
	# for Mali GPUs, libmali must be linked
	FIND_LIBRARY(OpenCL_LIBRARIES_MALI mali
		PATHS ENV LD_LIBRARY_PATH ENV OpenCL_LIBPATH "/usr/lib64" "$ENV{CUDADIR}/lib64"
	)
	IF (OpenCL_LIBRARIES_MALI)
		SET(OpenCL_LIBRARIES ${OpenCL_LIBRARIES} ${OpenCL_LIBRARIES_MALI})
	ENDIF (OpenCL_LIBRARIES_MALI)
    ENDIF (WIN32)

ENDIF (APPLE)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenCL DEFAULT_MSG OpenCL_LIBRARIES OpenCL_INCLUDE_DIRS)

IF(_OpenCL_CPP_INCLUDE_DIRS)
    SET( OpenCL_HAS_CPP_BINDINGS TRUE )
    LIST( APPEND OpenCL_INCLUDE_DIRS ${_OpenCL_CPP_INCLUDE_DIRS} )
    # This is often the same, so clean up
    LIST( REMOVE_DUPLICATES OpenCL_INCLUDE_DIRS )
ENDIF(_OpenCL_CPP_INCLUDE_DIRS)

MARK_AS_ADVANCED( OpenCL_INCLUDE_DIRS )
