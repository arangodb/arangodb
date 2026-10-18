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
 * This file contains public declarations for the HDF5 module.
 */
#ifndef H5public_H
#define H5public_H

/* Include files for public use... */
/*
 * Since H5pubconf.h is a generated header file, it is messy to try
 * to put a #ifndef H5pubconf_H ... #endif guard in it.
 * HDF5 has set an internal rule that it is being included here.
 * Source files should NOT include H5pubconf.h directly but include
 * it via H5public.h.  The #ifndef H5public_H guard above would
 * prevent repeated include.
 */
#include "H5pubconf.h" /* From configure */

/* API Version macro wrapper definitions */
#include "H5version.h"

#ifdef H5_HAVE_FEATURES_H
#include <features.h> /* For setting POSIX, BSD, etc. compatibility */
#endif

/* C library header files for things that appear in HDF5 public headers */
#ifdef __cplusplus
#include <cinttypes>
#else
#include <inttypes.h>
#endif
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Unlike most sys/ headers, which are POSIX-only, sys/types.h is available
 * on Windows, though it doesn't necessarily contain all the POSIX types
 * we need for HDF5 (e.g. ssize_t).
 */
#ifdef H5_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef H5_HAVE_PARALLEL
/* Don't link against MPI C++ bindings */
#ifndef MPICH_SKIP_MPICXX
#define MPICH_SKIP_MPICXX 1
#endif
#ifndef OMPI_SKIP_MPICXX
#define OMPI_SKIP_MPICXX 1
#endif
#include <mpi.h>
#ifndef MPI_FILE_NULL /* MPIO may be defined in mpi.h already */
#include <mpio.h>
#endif
#endif

/* Macro to hide a symbol from further preprocessor substitutions */
#define H5_NO_EXPAND(x) (x)

/* Version numbers */
/**
 * For major interface/format changes
 */
#define H5_VERS_MAJOR 1
/**
 * For minor interface/format changes
 */
#define H5_VERS_MINOR 14
/**
 * For tweaks, bug-fixes, or development
 */
#define H5_VERS_RELEASE 6
/**
 * For pre-releases like \c snap0. Empty string for official releases.
 */
#define H5_VERS_SUBRELEASE ""
/**
 * Short version string
 */
#define H5_VERS_STR "1.14.6"
/**
 * Full version string
 */
#define H5_VERS_INFO "HDF5 library version: 1.14.6"

#define H5check() H5check_version(H5_VERS_MAJOR, H5_VERS_MINOR, H5_VERS_RELEASE)

/* macros for comparing the version */
/**
 * \brief Determines whether the version of the library being used is greater
 *        than or equal to the specified version
 *
 * \param[in] Maj Major version number - A non-negative integer value
 * \param[in] Min Minor version number - A non-negative integer value
 * \param[in] Rel Release version number - A non-negative integer value
 * \return A value of 1 is returned if the library version is greater than
 *          or equal to the version number specified.\n
 *          A value of 0 is returned if the library version is less than the
 *          version number specified.\n
 *          A library version is greater than the specified version number if
 *          its major version is larger than the specified major version
 *          number. If the major version numbers are the same, it is greater
 *          than the specified version number if its minor version is larger
 *          than the specified minor version number. If the minor version
 *          numbers are the same, then a library version would be greater than
 *          the specified version number if its release number is larger than
 *          the specified release number.
 *
 * \details The #H5_VERSION_GE and #H5_VERSION_LE macros are used at compile
 *          time to conditionally include or exclude code based on the version
 *          of the HDF5 library against which an application will be linked.
 *
 *          The #H5_VERSION_GE macro compares the version of the HDF5 library
 *          being used against the version number specified in the parameters.
 *
 *          For more information about release versioning, see \ref RELVERSION.
 *
 * \since 1.8.7
 *
 */
#define H5_VERSION_GE(Maj, Min, Rel)                                                                         \
    (((H5_VERS_MAJOR == Maj) && (H5_VERS_MINOR == Min) && (H5_VERS_RELEASE >= Rel)) ||                       \
     ((H5_VERS_MAJOR == Maj) && (H5_VERS_MINOR > Min)) || (H5_VERS_MAJOR > Maj))

/**
 * \brief Determines whether the version of the library being used is less
 *        than or equal to the specified version
 *
 * \param[in] Maj Major version number - A non-negative integer value
 * \param[in] Min Minor version number - A non-negative integer value
 * \param[in] Rel Release version number - A non-negative integer value
 * \return A value of 1 is returned if the library version is less than
 *          or equal to the version number specified.\n
 *          A value of 0 is returned if the library version is greater than the
 *          version number specified.\n
 *          A library version is less than the specified version number if
 *          its major version is smaller than the specified major version
 *          number. If the major version numbers are the same, it is smaller
 *          than the specified version number if its minor version is smaller
 *          than the specified minor version number. If the minor version
 *          numbers are the same, then a library version would be smaller than
 *          the specified version number if its release number is smaller than
 *          the specified release number.
 *
 * \details The #H5_VERSION_GE and #H5_VERSION_LE macros are used at compile
 *          time to conditionally include or exclude code based on the version
 *          of the HDF5 library against which an application will be linked.
 *
 *          The #H5_VERSION_LE macro compares the version of the HDF5 library
 *          being used against the version number specified in the parameters.
 *
 *          For more information about release versioning, see \ref RELVERSION.
 *
 * \since 1.8.7
 *
 */
#define H5_VERSION_LE(Maj, Min, Rel)                                                                         \
    (((H5_VERS_MAJOR == Maj) && (H5_VERS_MINOR == Min) && (H5_VERS_RELEASE <= Rel)) ||                       \
     ((H5_VERS_MAJOR == Maj) && (H5_VERS_MINOR < Min)) || (H5_VERS_MAJOR < Maj))

/* Macros for various environment variables that HDF5 interprets */
/**
 * Used to specify the name of an HDF5 Virtual File Driver to use as
 * the default file driver for file access. Setting this environment
 * variable overrides the default file driver for File Access Property
 * Lists.
 */
#define HDF5_DRIVER "HDF5_DRIVER"
/**
 * Used to specify a configuration string for the HDF5 Virtual File
 * Driver being used for file access.
 */
#define HDF5_DRIVER_CONFIG "HDF5_DRIVER_CONFIG"
/**
 * Used to specify the name of an HDF5 Virtual Object Layer Connector
 * to use as the default VOL connector for file access. Setting this
 * environment variable overrides the default VOL connector for File
 * Access Property Lists.
 */
#define HDF5_VOL_CONNECTOR "HDF5_VOL_CONNECTOR"
/**
 * Used to specify a delimiter-separated (currently, ';' for Windows
 * and ':' for other systems) list of paths that HDF5 should search
 * when loading plugins.
 */
#define HDF5_PLUGIN_PATH "HDF5_PLUGIN_PATH"
/**
 * Used to control the loading of HDF5 plugins at runtime. If this
 * environment variable is set to the special string "::" (defined
 * in H5PLpublic.h as H5PL_NO_PLUGIN), then dynamic loading of any
 * HDF5 plugins will be disabled. No other values are valid for this
 * environment variable.
 */
#define HDF5_PLUGIN_PRELOAD "HDF5_PLUGIN_PRELOAD"
/**
 * Used to control whether HDF5 uses file locking when creating or
 * opening a file. Valid values for this environment variable are
 * as follows:
 *
 *  "TRUE" or "1"  - Request that file locks should be used
 *  "FALSE" or "0" - Request that file locks should NOT be used
 *  "BEST_EFFORT"  - Request that file locks should be used and
 *                     that any locking errors caused by file
 *                     locking being disabled on the system
 *                     should be ignored
 */
#define HDF5_USE_FILE_LOCKING "HDF5_USE_FILE_LOCKING"
/**
 * Used to instruct HDF5 not to cleanup files created during testing.
 */
#define HDF5_NOCLEANUP "HDF5_NOCLEANUP"

/**
 * Status return values.  Failed integer functions in HDF5 result almost
 * always in a negative value (unsigned failing functions sometimes return
 * zero for failure) while successful return is non-negative (often zero).
 * The negative failure value is most commonly -1, but don't bet on it.
 *
 * The proper way to detect failure is something like:
 * \code
 * if((dset = H5Dopen2(file, name)) < 0)
 *    fprintf(stderr, "unable to open the requested dataset\n");
 * \endcode
 */
typedef int herr_t;

/**
 * C99-style Boolean type. Successful return values are zero (false) or positive
 * (true). The typical true value is 1 but don't bet on it.
 *
 * \deprecated Now that we require C99, hbool_t is typedef'd to C99's bool
 *             and hbool_t is considered deprecated. Due to its long-standing,
 *             widespread use, we have no plans to remove the hbool_t typedef
 *             from the public API, though we will probably switch to using
 *             bool in the public API starting in the next major release of HDF5.
 * \attention Boolean functions cannot fail.
 */
#include <stdbool.h>
typedef bool hbool_t;
/**
 * Three-valued Boolean type. Functions that return #htri_t however return zero
 * (false), positive (true), or negative (failure).
 *
 * The proper way to test for truth from a #htri_t function is:
 * \code
 * if ((retval = H5Tcommitted(type)) > 0) {
 *     printf("data type is committed\n");
 * } else if (!retval) {
 *     printf("data type is not committed\n");
 * } else {
 *     printf("error determining whether data type is committed\n");
 * }
 * \endcode
 */
typedef int htri_t;

/* The signed version of size_t
 *
 * ssize_t is POSIX and not defined in any C standard. It's used in some
 * public HDF5 API calls so this work-around will define it if it's not
 * present.
 *
 * Use of ssize_t should be discouraged in new code.
 */
#if H5_SIZEOF_SSIZE_T == 0
/* Undefine this size, we will re-define it in one of the sections below */
#undef H5_SIZEOF_SSIZE_T
#if H5_SIZEOF_SIZE_T == H5_SIZEOF_INT
typedef int ssize_t;
#define H5_SIZEOF_SSIZE_T H5_SIZEOF_INT
#elif H5_SIZEOF_SIZE_T == H5_SIZEOF_LONG
typedef long ssize_t;
#define H5_SIZEOF_SSIZE_T H5_SIZEOF_LONG
#elif H5_SIZEOF_SIZE_T == H5_SIZEOF_LONG_LONG
typedef long long ssize_t;
#define H5_SIZEOF_SSIZE_T H5_SIZEOF_LONG_LONG
#else /* Can't find matching type for ssize_t */
#error "nothing appropriate for ssize_t"
#endif
#endif

/**
 * The size of file objects.
 *
 * \internal Defined as a (minimum) 64-bit integer type.
 */
typedef uint64_t hsize_t;

#ifdef H5_HAVE_PARALLEL
#define HSIZE_AS_MPI_TYPE MPI_UINT64_T
#endif

/**
 * The size of file objects. Used when negative values are needed to indicate errors.
 *
 * \internal Defined as a (minimum) 64-bit integer type. Use of hssize_t
 * should be discouraged in new code.
 */
typedef int64_t hssize_t;
/** d print conversion specifier for signed integer type \since 1.10.8 */
#define PRIdHSIZE PRId64
/** i print conversion specifier for signed integer type \since 1.10.8 */
#define PRIiHSIZE PRIi64
/** o print conversion specifier for signed integer type \since 1.10.8 */
#define PRIoHSIZE PRIo64
/** u print conversion specifier for signed integer type \since 1.10.8 */
#define PRIuHSIZE PRIu64
/** x print conversion specifier for signed integer type \since 1.10.8 */
#define PRIxHSIZE PRIx64
/** X print conversion specifier for signed integer type \since 1.10.8 */
#define PRIXHSIZE          PRIX64
#define H5_SIZEOF_HSIZE_T  8
#define H5_SIZEOF_HSSIZE_T 8
/** Represents the largest possible value of uint64_t \since 1.10.0 */
#define HSIZE_UNDEF UINT64_MAX

/**
 * The address of an object in the file.
 *
 * \internal Defined as a (minimum) 64-bit unsigned integer type.
 */
typedef uint64_t haddr_t;
/** d print conversion specifier for unsigned integer type \since 1.8.23 */
#define PRIdHADDR PRId64
/** o print conversion specifier for unsigned integer type \since 1.8.23 */
#define PRIoHADDR PRIo64
/** u print conversion specifier for unsigned integer type \since 1.8.23 */
#define PRIuHADDR PRIu64
/** x print conversion specifier for unsigned integer type \since 1.8.23 */
#define PRIxHADDR PRIx64
/** X print conversion specifier for unsigned integer type \since 1.8.23 */
#define PRIXHADDR           PRIX64
#define H5_SIZEOF_HADDR_T   8
#define HADDR_UNDEF         UINT64_MAX
#define H5_PRINTF_HADDR_FMT "%" PRIuHADDR
#define HADDR_MAX           (HADDR_UNDEF - 1)

#ifdef H5_HAVE_PARALLEL
#define HADDR_AS_MPI_TYPE MPI_UINT64_T
#endif

//! <!-- [H5_iter_order_t_snip] -->
/**
 * Common iteration orders
 */
typedef enum {
    H5_ITER_UNKNOWN = -1, /**< Unknown order */
    H5_ITER_INC,          /**< Increasing order */
    H5_ITER_DEC,          /**< Decreasing order */
    H5_ITER_NATIVE,       /**< No particular order, whatever is fastest */
    H5_ITER_N             /**< Number of iteration orders */
} H5_iter_order_t;
//! <!-- [H5_iter_order_t_snip] -->

/* Iteration callback values */
/* (Actually, any positive value will cause the iterator to stop and pass back
 *      that positive value to the function that called the iterator)
 */
#define H5_ITER_ERROR (-1) /**< Error, stop iteration */
#define H5_ITER_CONT  (0)  /**< Continue iteration */
#define H5_ITER_STOP  (1)  /**< Stop iteration, short-circuit success */

//! <!-- [H5_index_t_snip] -->
/**
 * The types of indices on links in groups/attributes on objects.
 * Primarily used for "<do> <foo> by index" routines and for iterating over
 * links in groups/attributes on objects.
 */
typedef enum H5_index_t {
    H5_INDEX_UNKNOWN = -1, /**< Unknown index type                   */
    H5_INDEX_NAME,         /**< Index on names                       */
    H5_INDEX_CRT_ORDER,    /**< Index on creation order              */
    H5_INDEX_N             /**< Number of indices defined            */
} H5_index_t;
//! <!-- [H5_index_t_snip] -->

/**
 * Storage info struct used by H5O_info_t and H5F_info_t
 */
//! <!-- [H5_ih_info_t_snip] -->
typedef struct H5_ih_info_t {
    hsize_t index_size; /**< btree and/or list */
    hsize_t heap_size;
} H5_ih_info_t;
//! <!-- [H5_ih_info_t_snip] -->

/**
 * The maximum size allowed for tokens
 * \details Tokens are unique and permanent identifiers that are
 *          used to reference HDF5 objects in a container. This allows
 *          for 128-bit tokens
 *
 * \since 1.12.0
 */
#define H5O_MAX_TOKEN_SIZE (16)

//! <!-- [H5O_token_t_snip] -->
/**
 * Type for object tokens
 *
 * \internal (Hoisted here, since it's used by both the
 *            H5Lpublic.h and H5Opublic.h headers)
 */
typedef struct H5O_token_t {
    uint8_t __data[H5O_MAX_TOKEN_SIZE];
} H5O_token_t;
//! <!-- [H5O_token_t_snip] -->

/**
 * Library shutdown callback, used by H5atclose().
 */
typedef void (*H5_atclose_func_t)(void *ctx);

/* API adapter header (defines H5_DLL, etc.) */
#include "H5api_adpt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Functions in H5.c */
/**
 * \ingroup H5
 * \brief Initializes the HDF5 library
 * \return \herr_t
 *
 * \details H5open() initializes the HDF5 library.
 *
 * \details When the HDF5 library is used in a C application, the library is
 *          automatically initialized when the first HDF5 function call is
 *          issued. If one finds that an HDF5 library function is failing
 *          inexplicably, H5open() can be called first. It is safe to call
 *          H5open() before an application issues any other function calls to
 *          the HDF5 library, as there are no damaging side effects in calling
 *          it more than once.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5open(void);
/**
 * \ingroup H5
 * \brief Registers a callback for the library to invoke when it's closing.
 * \param[in] func The function pointer to invoke
 * \param[in] ctx Context to pass to \p func when invoked
 * \return \herr_t
 *
 * \details H5atclose() registers a callback that the HDF5 library will invoke
 *          when closing.  The full capabilities of the HDF5 library are
 *          available to callbacks invoked through this mechanism, and library
 *          shutdown will only begin in earnest when all callbacks have been
 *          invoked and have returned.
 *
 *          Registered callbacks are invoked in LIFO order, similar to the
 *          Standard C 'atexit' routine.  For example, if 'func1' is registered,
 *          then 'func2', when the library is closing 'func2', will
 *          be invoked first, then 'func1'.
 *
 *          The \p ctx pointer will be passed to \p func when it's invoked.
 *          NULL is allowed for \p ctx.
 *
 *          If the HDF5 library is initialized and closed more than once, the
 *          \p func callback must be registered within each open/close cycle.
 *
 * \since 1.14.0
 */
H5_DLL herr_t H5atclose(H5_atclose_func_t func, void *ctx);
/**
 * \ingroup H5
 * \brief Flushes all data to disk, closes all open objects, and releases memory
 * \return \herr_t
 *
 * \details H5close() flushes all data to disk, closes all open HDF5 objects,
 *          and cleans up all memory used by the HDF5 library. This function is
 *          generally called when the application calls exit(), but may be
 *          called earlier in the event of an emergency shutdown or out of a
 *          desire to free all resources used by the HDF5 library.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5close(void);
/**
 * \ingroup H5
 * \brief Instructs library not to install atexit() cleanup routine
 * \return \herr_t
 *
 * \details H5dont_atexit() indicates to the library that an atexit() cleanup
 *          routine should not be installed. The major purpose for using this
 *          function is in situations where the library is dynamically linked
 *          into an application and is un-linked from the application before
 *          exit() gets called. In those situations, a routine installed with
 *          atexit() would jump to a routine that was no longer in memory,
 *          causing errors.
 *
 * \attention In order to be effective, this routine \Emph{must} be called
 *            before any other HDF5 function calls, and must be called each
 *            time the library is loaded/linked into the application (the first
 *            time and after it's been unloaded).
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5dont_atexit(void);
/**
 * \ingroup H5
 * \brief Garbage collects on all free-lists of all types
 * \return \herr_t
 *
 * \details H5garbage_collect() walks through all garbage collection routines
 *          of the library, freeing any unused memory.
 *
 *          It is not required that H5garbage_collect() be called at any
 *          particular time; it is only necessary for certain situations where
 *          the application has performed actions that cause the library to
 *          allocate many objects. The application should call
 *          H5garbage_collect() if it eventually releases those objects and
 *          wants to reduce the memory used by the library from the peak usage
 *          required.
 *
 * \note The library automatically garbage collects all the free lists when the
 *       application ends.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5garbage_collect(void);
/**
 * \ingroup H5
 * \brief Sets free-list size limits
 *
 * \param[in] reg_global_lim The cumulative limit, in bytes, on memory used for
 *                           all regular free lists (Default: 1MB)
 * \param[in] reg_list_lim The limit, in bytes, on memory used for each regular
 *                         free list (Default: 64KB)
 * \param[in] arr_global_lim The cumulative limit, in bytes, on memory used for
 *                           all array free lists (Default: 4MB)
 * \param[in] arr_list_lim The limit, in bytes, on memory used for each array
 *                         free list (Default: 256KB)
 * \param[in] blk_global_lim The cumulative limit, in bytes, on memory used for
 *                           all block free lists and, separately, for all
 *                           factory free lists (Default: 16MB)
 * \param[in] blk_list_lim The limit, in bytes, on memory used for each block
 *                         or factory free list (Default: 1MB)
 * \return \herr_t
 *
 * \details H5set_free_list_limits() sets size limits on all types of free
 *          lists. The HDF5 library uses free lists internally to manage
 *          memory. The types of free lists used are as follows:
 *          \li Regular free lists manage memory for single internal data
 *              structures.
 *          \li Array free lists manage memory for arrays of internal
 *              data structures.
 *          \li Block free lists manage memory for arbitrarily-sized blocks
 *              of bytes.
 *          \li Factory free lists manage memory for fixed-size blocks of
 *              bytes.
 *
 *          The parameters specify global and per-list limits; for example, \p
 *          reg_global_limit and \p reg_list_limit limit the accumulated size
 *          of all regular free lists and the size of each individual regular
 *          free list, respectively. Therefore, if an application sets a 1Mb
 *          limit on each of the global lists, up to 4Mb of total storage might
 *          be allocated, 1Mb for each of the regular, array, block, and
 *          factory type lists.
 *
 *          The settings specified for block free lists are duplicated for
 *          factory free lists. Therefore, increasing the global limit on block
 *          free lists by x bytes will increase the potential free list memory
 *          usage by 2x bytes.
 *
 *          Using a value of -1 for a limit means that no limit is set for the
 *          specified type of free list.
 *
 * \version 1.8.3 Function changed in this release to set factory free list
 *                memory limits.
 *
 * \since 1.4.0
 */
H5_DLL herr_t H5set_free_list_limits(int reg_global_lim, int reg_list_lim, int arr_global_lim,
                                     int arr_list_lim, int blk_global_lim, int blk_list_lim);
/**
 * \ingroup H5
 * \brief Gets the current size of the free lists used to manage memory
 *
 * \param[out] reg_size The current size of all "regular" free list memory used
 * \param[out] arr_size The current size of all "array" free list memory used
 * \param[out] blk_size The current size of all "block" free list memory used
 * \param[out] fac_size The current size of all "factory" free list memory used
 * \return \herr_t
 *
 * \details H5get_free_list_sizes() obtains the current size of the different
 *          kinds of free lists that the library uses to manage memory. The
 *          free list sizes can be set with H5set_free_list_limits() and
 *          garbage collected with H5garbage_collect(). These lists are global
 *          for the entire library.
 *
 * \since 1.10.7
 */
H5_DLL herr_t H5get_free_list_sizes(size_t *reg_size, size_t *arr_size, size_t *blk_size, size_t *fac_size);
/**
 * \ingroup H5
 * \brief Returns the HDF library release number
 *
 * \param[out] majnum The major version number of the library
 * \param[out] minnum The minor version number of the library
 * \param[out] relnum The release version number of the library
 * \return \herr_t
 *
 * \details H5get_libversion() retrieves the major, minor, and release numbers
 *          of the version of the HDF5 library which is linked to the
 *          application.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5get_libversion(unsigned *majnum, unsigned *minnum, unsigned *relnum);
/**
 * \ingroup H5
 * \brief Verifies that HDF5 library versions are consistent
 *
 * \param[in] majnum HDF5 library major version number
 * \param[in] minnum HDF5 library minor version number
 * \param[in] relnum HDF5 library release number
 * \return \herr_t
 *
 * \details H5check_version() verifies that the version of the HDF5 library
 *          with which an application was compiled, as indicated by the passed
 *          parameters, matches the version of the HDF5 library against which
 *          the application is currently linked.
 *
 *          \p majnum is the major version number of the HDF library with which
 *          the application was compiled, \p minnum is the minor version
 *          number, and \p relnum is the release number. Consider the following
 *          example:
 *
 *          An official HDF5 release is labelled as follows:
 *          HDF5 Release \TText{\<majnum\>.\<minnum\>.\<relnum\>}\n
 *          For example, in HDF5 Release 1.8.5:
 *          \li 1 is the major version number, \p majnum.
 *          \li 8 is the minor version number, \p minnum.
 *          \li 5 is the release number, \p relnum.
 *
 *          As stated above, H5check_version() first verifies that the version
 *          of the HDF5 library with which an application was compiled matches
 *          the version of the HDF5 library against which the application is
 *          currently linked. If this check fails, H5check_version() causes the
 *          application to abort (by means of a standard C abort() call) and
 *          prints information that is usually useful for debugging. This
 *          precaution is taken to avoid the risks of data corruption or
 *          segmentation faults.
 *
 *          The most common cause of this failure is that an application was
 *          compiled with one version of HDF5 and is dynamically linked with a
 *          different version different version.
 *
 *          If the above test passes, H5check_version() proceeds to verify the
 *          consistency of additional library version information. This is
 *          designed to catch source code inconsistencies that do not normally
 *          cause failures; if this check reveals an inconsistency, an
 *          informational warning is printed but the application is allowed to
 *          run.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5check_version(unsigned majnum, unsigned minnum, unsigned relnum);
/**
 * \ingroup H5
 * \brief Checks whether the HDF5 library is closing.
 * \param[out] is_terminating Flag indicating whether library is shutting down
 * \return \herr_t
 *
 * \details H5is_library_terminating() queries whether the HDF5 library is in
 *          the process of shutting down.  The \p is_terminating flag will only
 *          be set to true after shutdown starts, it will be false before the
 *          library has been initialized, while the library is initialized, and
 *          after it has been closed.  The value of \p is_terminating is
 *          undefined if this routine fails.
 *
 * \since 1.14.0
 */
H5_DLL herr_t H5is_library_terminating(hbool_t *is_terminating);
/**
 * \ingroup H5
 * \brief Determines whether the HDF5 library was built with the thread-safety
 *        feature enabled
 *
 * \param[out] is_ts Boolean value indicating whether the library was built
 *                   with thread-safety enabled
 * \return \herr_t
 *
 * \details The HDF5 library, although not internally multi-threaded, can be
 *          built with a thread-safety feature enabled that protects internal
 *          data structures with a mutex. In certain circumstances, it may be
 *          useful to determine, at run-time, whether the linked HDF5 library
 *          was built with the thread-safety feature enabled.
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5is_library_threadsafe(hbool_t *is_ts);
/**
 * \ingroup H5
 * \brief Frees memory allocated by the HDF5 library
 *
 * \param[in] mem Buffer to be freed. Can be NULL
 * \return \herr_t
 *
 * \details H5free_memory() frees the memory that has been allocated by the caller
 *          with H5allocate_memory() or by the HDF5 library on behalf of the
 *          caller.
 *
 *          H5Tget_member_name() provides an example of memory allocation on
 *          behalf of the caller: The function returns a buffer containing the
 *          name of a compound datatype member. It is the caller's
 *          responsibility to eventually free that buffer with H5free_memory().
 *
 * \attention It is especially important to use this function to free memory
 *            allocated by the library on Windows. The C standard library is
 *            implemented in dynamic link libraries (DLLs) known as the C
 *            run-time (CRT). Each version of Visual Studio comes with two CRT
 *            DLLs (debug and release) and allocating and freeing across DLL
 *            boundaries can cause resource leaks and subtle bugs due to heap
 *            corruption.\n
 *            Only use this function to free memory allocated by the HDF5
 *            Library. It will generally not be safe to use this function to
 *            free memory allocated by any other means.\n
 *            Even when using this function, it is still best to ensure that
 *            all components of a C application are built with the same version
 *            of Visual Studio and build (debug or release) and thus linked
 *            against the same CRT.
 *
 * \see H5allocate_memory(), H5resize_memory()
 *
 * \since 1.8.13
 *
 */
H5_DLL herr_t H5free_memory(void *mem);
/**
 * \ingroup H5
 * \brief Allocates memory that will be freed later internally.
 *
 * \param[in] size The size in bytes of the buffer to be allocated
 * \param[in] clear Flag whether the new buffer is to be initialized with 0
 *
 * \return On success, returns pointer to newly allocated buffer or returns
 *         NULL if size is 0 (zero).\n
 *         Returns NULL on failure.
 *
 * \details H5allocate_memory() allocates a memory buffer of size bytes that
 *          will later be freed internally by the HDF5 library.
 *
 *          The boolean \p clear parameter specifies whether the buffer should
 *          be initialized. If clear is \c true, all bits in the buffer are to be
 *          set to 0 (zero); if clear is \c false, the buffer will not be
 *          initialized.
 *
 *          This function is intended to have the semantics of malloc() and
 *          calloc(). However, unlike malloc() and calloc(), which allow for a
 *          "special" pointer to be returned instead of NULL, this function
 *          always returns NULL on failure or when size is set to 0 (zero).
 *
 * \note At this time, the only intended use for this function is to allocate
 *       memory that will be returned to the library as a data buffer from a
 *       third-party filter.
 *
 * \attention To avoid heap corruption, allocated memory should be freed using
 *            the same library that initially allocated it. In most cases, the
 *            HDF5 API uses resources that are allocated and freed either
 *            entirely by the user or entirely by the library, so this is not a
 *            problem. In rare cases, however, HDF5 API calls will free the memory
 *            that the user allocated. This function allows the user to safely
 *            allocate this memory.\n
 *            It is particularly important to use this function to allocate
 *            memory in Microsoft Windows environments. In Windows, the C
 *            standard library is implemented in dynamic link libraries (DLLs)
 *            known as the C run-time (CRT). Each version of Visual Studio
 *            comes with multiple versions of the CRT DLLs (debug, release, et
 *            cetera) and allocating and freeing memory across DLL boundaries
 *            can cause resource leaks and subtle bugs due to heap corruption.\n
 *            Even when using this function, it is best where possible to
 *            ensure that all components of a C application are built with the
 *            same version of Visual Studio and configuration (Debug or
 *            Release), and thus linked against the same CRT.\n
 *            Use this function only to allocate memory inside third-party HDF5
 *            filters. It will generally not be safe to use this function to
 *            allocate memory for any other purpose.
 *
 * \see H5free_memory(), H5resize_memory()
 *
 * \since 1.8.15
 *
 */
H5_DLL void *H5allocate_memory(size_t size, hbool_t clear);
/**
 * \ingroup H5
 * \brief Resizes and, if required, re-allocates memory that will later be
 *        freed internally by the HDF5 library
 *
 * \param[in] mem Pointer to a buffer to be resized. May be NULL
 * \param[in] size New size of the buffer, in bytes

 *
 * \return On success, returns pointer to resized or reallocated buffer
 *         or returns NULL if size is 0 (zero).\n
 *         Returns NULL on failure.
 *
 * \details H5resize_memory() takes a pointer to an existing buffer and resizes
 *          the buffer to match the value in \p size. If necessary, the buffer
 *          is reallocated. If \p size is 0, the buffer is released.
 *
 *          The input buffer must either be NULL or have been allocated by
 *          H5allocate_memory() since the input buffer may be freed by the
 *          library.
 *
 *          For certain behaviors, the pointer \p mem may be passed in as NULL.
 *
 *          This function is intended to have the semantics of realloc():
 *
 *          <table>
 *            <tr><td>\TText{H5resize_memory(buffer, size)}</td>
 *                <td>Resizes buffer. Returns pointer to resized buffer.</td></tr>
 *            <tr><td>\TText{H5resize_memory(NULL, size)}</td>
 *                <td>Allocates memory using HDF5 Library allocator.
 *                    Returns pointer to new buffer</td></tr>
 *            <tr><td>\TText{H5resize_memory(buffer, 0)}</td>
 *                <td>Frees memory using HDF5 Library allocator.
 *                    Returns NULL.</td></tr>
 *            <tr><td>\TText{H5resize_memory(NULL, 0)}</td>
 *                <td>Returns NULL (undefined in C standard).</td></tr>
 *          </table>
 *
 *          Unlike realloc(), which allows for a "special pointer to be
 *          returned instead of NULL, this function always returns NULL on
 *          failure or when size is 0 (zero).
 *
 * \note At this time, the only intended use for this function is to resize or
 *       reallocate memory that will be returned to the library (and eventually
 *       to the user) as a data buffer from a third-party HDF5 filter.
 *
 * \attention To avoid heap corruption, allocated memory should be freed using
 *            the same library that initially allocated it. In most cases, the
 *            HDF5 API uses resources that are allocated and freed either
 *            entirely by the user or entirely by the library, so this is not a
 *            problem. In rare cases, however, HDF5 API calls will free memory
 *            that the user allocated. This function allows the user to safely
 *            allocate this memory.\n
 *            It is particularly important to use this function to resize
 *            memory on Microsoft Windows systems. In Windows, the C standard
 *            library is implemented in dynamic link libraries (DLLs) known as
 *            the C run-time (CRT). Each version of Visual Studio comes with
 *            multiple versions of the CRT DLLs (debug, release, et cetera) and
 *            allocating and freeing memory across DLL boundaries can cause
 *            resource leaks and subtle bugs due to heap corruption.\n
 *            Even when using this function, it is still best to ensure that
 *            all components of a C application are built with the same version
 *            of Visual Studio and the same configuration (Debug or Release),
 *            and thus linked against the same CRT.\n
 *            Only use this function to resize memory inside third-party HDF5
 *            filters. It will generally not be safe to use this function to
 *            resize memory for any other purpose.
 *
 * \see H5allocate_memory(), H5free_memory()
 *
 * \since 1.8.15
 *
 */
H5_DLL void *H5resize_memory(void *mem, size_t size);

#ifdef __cplusplus
}
#endif
#endif /* H5public_H */
