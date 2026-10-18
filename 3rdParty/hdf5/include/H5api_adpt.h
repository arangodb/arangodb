/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * H5api_adpt.h
 * Used for the HDF5 dll project
 */
#ifndef H5API_ADPT_H
#define H5API_ADPT_H

/* This will only be defined if HDF5 was built with CMake */
#ifdef H5_BUILT_AS_DYNAMIC_LIB

#if defined(hdf5_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_DLL    __declspec(dllexport)
#define H5_DLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_DLL    __attribute__((visibility("default")))
#define H5_DLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_DLL    __declspec(dllimport)
#define H5_DLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_DLL    __attribute__((visibility("default")))
#define H5_DLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5_DLL
#define H5_DLL
#define H5_DLLVAR extern
#endif /* _HDF5DLL_ */

#if defined(hdf5_test_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5TEST_DLL    __declspec(dllexport)
#define H5TEST_DLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5TEST_DLL    __attribute__((visibility("default")))
#define H5TEST_DLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5TEST_DLL    __declspec(dllimport)
#define H5TEST_DLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5TEST_DLL    __attribute__((visibility("default")))
#define H5TEST_DLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5TEST_DLL
#define H5TEST_DLL
#define H5TEST_DLLVAR extern
#endif /* H5TEST_DLL */

#if defined(hdf5_tools_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5TOOLS_DLL    __declspec(dllexport)
#define H5TOOLS_DLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5TOOLS_DLL    __attribute__((visibility("default")))
#define H5TOOLS_DLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5TOOLS_DLL    __declspec(dllimport)
#define H5TOOLS_DLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5TOOLS_DLL    __attribute__((visibility("default")))
#define H5TOOLS_DLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5TOOLS_DLL
#define H5TOOLS_DLL
#define H5TOOLS_DLLVAR extern
#endif /* H5TOOLS_DLL */

#if defined(hdf5_cpp_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_DLLCPP    __declspec(dllexport)
#define H5_DLLCPPVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_DLLCPP    __attribute__((visibility("default")))
#define H5_DLLCPPVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_DLLCPP    __declspec(dllimport)
#define H5_DLLCPPVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_DLLCPP    __attribute__((visibility("default")))
#define H5_DLLCPPVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5_DLLCPP
#define H5_DLLCPP
#define H5_DLLCPPVAR extern
#endif /* H5_DLLCPP */

#if defined(hdf5_hl_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_HLDLL    __declspec(dllexport)
#define H5_HLDLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_HLDLL    __attribute__((visibility("default")))
#define H5_HLDLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_HLDLL    __declspec(dllimport)
#define H5_HLDLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_HLDLL    __attribute__((visibility("default")))
#define H5_HLDLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5_HLDLL
#define H5_HLDLL
#define H5_HLDLLVAR extern
#endif /* H5_HLDLL */

#if defined(hdf5_hl_cpp_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_HLCPPDLL    __declspec(dllexport)
#define H5_HLCPPDLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_HLCPPDLL    __attribute__((visibility("default")))
#define H5_HLCPPDLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_HLCPPDLL    __declspec(dllimport)
#define H5_HLCPPDLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_HLCPPDLL    __attribute__((visibility("default")))
#define H5_HLCPPDLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5_HLCPPDLL
#define H5_HLCPPDLL
#define H5_HLCPPDLLVAR extern
#endif /* H5_HLCPPDLL */

#if defined(hdf5_f90cstub_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_FCDLL    __declspec(dllexport)
#define H5_FCDLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_FCDLL    __attribute__((visibility("default")))
#define H5_FCDLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_FCDLL    __declspec(dllimport)
#define H5_FCDLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_FCDLL    __attribute__((visibility("default")))
#define H5_FCDLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5_FCDLL
#define H5_FCDLL
#define H5_FCDLLVAR extern
#endif /* H5_FCDLL */

#if defined(hdf5_test_f90cstub_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_FCTESTDLL    __declspec(dllexport)
#define H5_FCTESTDLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_FCTESTDLL    __attribute__((visibility("default")))
#define H5_FCTESTDLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define H5_FCTESTDLL    __declspec(dllimport)
#define H5_FCTESTDLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define H5_FCTESTDLL    __attribute__((visibility("default")))
#define H5_FCTESTDLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef H5_FCTESTDLL
#define H5_FCTESTDLL
#define H5_FCTESTDLLVAR extern
#endif /* H5_FCTESTDLL */

#if defined(hdf5_hl_f90cstub_shared_EXPORTS)
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define HDF5_HL_F90CSTUBDLL    __declspec(dllexport)
#define HDF5_HL_F90CSTUBDLLVAR extern __declspec(dllexport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define HDF5_HL_F90CSTUBDLL    __attribute__((visibility("default")))
#define HDF5_HL_F90CSTUBDLLVAR extern __attribute__((visibility("default")))
#endif
#else
#if defined(_MSC_VER) /* MSVC Compiler Case */
#define HDF5_HL_F90CSTUBDLL    __declspec(dllimport)
#define HDF5_HL_F90CSTUBDLLVAR __declspec(dllimport)
#elif (__GNUC__ >= 4) /* GCC 4.x has support for visibility options */
#define HDF5_HL_F90CSTUBDLL    __attribute__((visibility("default")))
#define HDF5_HL_F90CSTUBDLLVAR extern __attribute__((visibility("default")))
#endif
#endif

#ifndef HDF5_HL_F90CSTUBDLL
#define HDF5_HL_F90CSTUBDLL
#define HDF5_HL_F90CSTUBDLLVAR extern
#endif /* HDF5_HL_F90CSTUBDLL */

#else
#define H5_DLL
#define H5_DLLVAR extern
#define H5TEST_DLL
#define H5TEST_DLLVAR extern
#define H5TOOLS_DLL
#define H5TOOLS_DLLVAR extern
#define H5_DLLCPP
#define H5_DLLCPPVAR extern
#define H5_HLDLL
#define H5_HLDLLVAR extern
#define H5_HLCPPDLL
#define H5_HLCPPDLLVAR extern
#define H5_FCDLL
#define H5_FCDLLVAR extern
#define H5_FCTESTDLL
#define H5_FCTESTDLLVAR extern
#define HDF5_HL_F90CSTUBDLL
#define HDF5_HL_F90CSTUBDLLVAR extern
#endif /* H5_BUILT_AS_DYNAMIC_LIB */

#endif /* H5API_ADPT_H */
