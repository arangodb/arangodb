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

#ifndef H5FDpublic_H
#define H5FDpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Fpublic.h" /* Files                                    */
#include "H5Ipublic.h" /* Identifiers                              */

/*****************/
/* Public Macros */
/*****************/

#define H5FD_VFD_DEFAULT 0 /* Default VFL driver value */

/* VFD identifier values
 * These are H5FD_class_value_t values, NOT hid_t values!
 */
#define H5_VFD_INVALID   ((H5FD_class_value_t)(-1))
#define H5_VFD_SEC2      ((H5FD_class_value_t)(0))
#define H5_VFD_CORE      ((H5FD_class_value_t)(1))
#define H5_VFD_LOG       ((H5FD_class_value_t)(2))
#define H5_VFD_FAMILY    ((H5FD_class_value_t)(3))
#define H5_VFD_MULTI     ((H5FD_class_value_t)(4))
#define H5_VFD_STDIO     ((H5FD_class_value_t)(5))
#define H5_VFD_SPLITTER  ((H5FD_class_value_t)(6))
#define H5_VFD_MPIO      ((H5FD_class_value_t)(7))
#define H5_VFD_DIRECT    ((H5FD_class_value_t)(8))
#define H5_VFD_MIRROR    ((H5FD_class_value_t)(9))
#define H5_VFD_HDFS      ((H5FD_class_value_t)(10))
#define H5_VFD_ROS3      ((H5FD_class_value_t)(11))
#define H5_VFD_SUBFILING ((H5FD_class_value_t)(12))
#define H5_VFD_IOC       ((H5FD_class_value_t)(13))
#define H5_VFD_ONION     ((H5FD_class_value_t)(14))

/* VFD IDs below this value are reserved for library use. */
#define H5_VFD_RESERVED 256

/* Maximum VFD ID */
#define H5_VFD_MAX 65535

/* Define VFL driver features that can be enabled on a per-driver basis */
/* These are returned with the 'query' function pointer in H5FD_class_t */
/*
 * Defining H5FD_FEAT_AGGREGATE_METADATA for a VFL driver means that
 * the library will attempt to allocate a larger block for metadata and
 * then sub-allocate each metadata request from that larger block.
 */
#define H5FD_FEAT_AGGREGATE_METADATA 0x00000001
/*
 * Defining H5FD_FEAT_ACCUMULATE_METADATA for a VFL driver means that
 * the library will attempt to cache metadata as it is written to the file
 * and build up a larger block of metadata to eventually pass to the VFL
 * 'write' routine.
 *
 * Distinguish between updating the metadata accumulator on writes and
 * reads.  This is particularly (perhaps only, even) important for MPI-I/O
 * where we guarantee that writes are collective, but reads may not be.
 * If we were to allow the metadata accumulator to be written during a
 * read operation, the application would hang.
 */
#define H5FD_FEAT_ACCUMULATE_METADATA_WRITE 0x00000002
#define H5FD_FEAT_ACCUMULATE_METADATA_READ  0x00000004
#define H5FD_FEAT_ACCUMULATE_METADATA                                                                        \
    (H5FD_FEAT_ACCUMULATE_METADATA_WRITE | H5FD_FEAT_ACCUMULATE_METADATA_READ)
/*
 * Defining H5FD_FEAT_DATA_SIEVE for a VFL driver means that
 * the library will attempt to cache raw data as it is read from/written to
 * a file in a "data sieve" buffer.  See Rajeev Thakur's papers:
 *  http://www.mcs.anl.gov/~thakur/papers/romio-coll.ps.gz
 *  http://www.mcs.anl.gov/~thakur/papers/mpio-high-perf.ps.gz
 */
#define H5FD_FEAT_DATA_SIEVE 0x00000008
/*
 * Defining H5FD_FEAT_AGGREGATE_SMALLDATA for a VFL driver means that
 * the library will attempt to allocate a larger block for "small" raw data
 * and then sub-allocate "small" raw data requests from that larger block.
 */
#define H5FD_FEAT_AGGREGATE_SMALLDATA 0x00000010
/*
 * Defining H5FD_FEAT_IGNORE_DRVRINFO for a VFL driver means that
 * the library will ignore the driver info that is encoded in the file
 * for the VFL driver.  (This will cause the driver info to be eliminated
 * from the file when it is flushed/closed, if the file is opened R/W).
 */
#define H5FD_FEAT_IGNORE_DRVRINFO 0x00000020
/**
 * Defining the H5FD_FEAT_DIRTY_DRVRINFO_LOAD for a VFL driver means that
 * the library will mark the driver info dirty when the file is opened
 * R/W.  This will cause the driver info to be re-encoded when the file
 * is flushed/closed.
 *
 * \since 1.10.0
 */
#define H5FD_FEAT_DIRTY_DRVRINFO_LOAD 0x00000040
/*
 * Defining H5FD_FEAT_POSIX_COMPAT_HANDLE for a VFL driver means that
 * the handle for the VFD (returned with the 'get_handle' callback) is
 * of type 'int' and is compatible with POSIX I/O calls.
 */
#define H5FD_FEAT_POSIX_COMPAT_HANDLE 0x00000080

/**
 * Defining H5FD_FEAT_HAS_MPI for a VFL driver means that
 * the driver makes use of MPI communication and code may retrieve
 * communicator/rank information from it
 *
 * \since 1.8.15
 */
#define H5FD_FEAT_HAS_MPI 0x00000100

#define H5FD_FEAT_ALLOCATE_EARLY 0x00000200
/**< Defining the H5FD_FEAT_ALLOCATE_EARLY for a VFL driver will force
 *   the library to use the H5D_ALLOC_TIME_EARLY on dataset create
 *   instead of the default H5D_ALLOC_TIME_LATE
 *
 * \since 1.8.15
 */

/*
 * Defining H5FD_FEAT_ALLOW_FILE_IMAGE for a VFL driver means that
 * the driver is able to use a file image in the fapl as the initial
 * contents of a file.
 */
#define H5FD_FEAT_ALLOW_FILE_IMAGE 0x00000400
/*
 * Defining H5FD_FEAT_CAN_USE_FILE_IMAGE_CALLBACKS for a VFL driver
 * means that the driver is able to use callbacks to make a copy of the
 * image to store in memory.
 */
#define H5FD_FEAT_CAN_USE_FILE_IMAGE_CALLBACKS 0x00000800
/**
 * Defining H5FD_FEAT_SUPPORTS_SWMR_IO for a VFL driver means that the
 * driver supports the single-writer/multiple-readers I/O pattern.
 *
 * \since 1.10.0
 */
#define H5FD_FEAT_SUPPORTS_SWMR_IO 0x00001000
/**
 * Defining H5FD_FEAT_USE_ALLOC_SIZE for a VFL driver
 * means that the library will just pass the allocation size to the
 * the driver's allocation callback which will eventually handle alignment.
 * This is specifically used for the multi/split driver.
 *
 * \since 1.10.1
 */
#define H5FD_FEAT_USE_ALLOC_SIZE 0x00002000
/**
 * Defining H5FD_FEAT_PAGED_AGGR for a VFL driver
 * means that the driver needs special file space mapping for paged aggregation.
 * This is specifically used for the multi/split driver.
 *
 * \since 1.10.1
 */
#define H5FD_FEAT_PAGED_AGGR 0x00004000
/**
 * Defining H5FD_FEAT_DEFAULT_VFD_COMPATIBLE for a VFL driver
 * that creates a file which is compatible with the default VFD.
 * Generally, this means that the VFD creates a single file that follows
 * the canonical HDF5 file format.
 * Regarding the Splitter VFD specifically, only drivers with this flag
 * enabled may be used as the Write-Only (W/O) channel driver.
 *
 * \since 1.10.2
 */
#define H5FD_FEAT_DEFAULT_VFD_COMPATIBLE 0x00008000
/*
 * Defining H5FD_FEAT_MEMMANAGE for a VFL driver means that
 * the driver uses special memory management routines or wishes
 * to do memory management in a specific manner. Therefore, HDF5
 * should request that the driver handle any memory management
 * operations when appropriate.
 */
#define H5FD_FEAT_MEMMANAGE 0x00010000

/* ctl function definitions: */
#define H5FD_CTL_OPC_RESERVED 512 /* Opcodes below this value are reserved for library use */
#define H5FD_CTL_OPC_EXPER_MIN                                                                               \
    H5FD_CTL_OPC_RESERVED /* Minimum opcode value available for experimental use                             \
                           */
#define H5FD_CTL_OPC_EXPER_MAX                                                                               \
    (H5FD_CTL_OPC_RESERVED + 511) /* Maximum opcode value available for experimental use */

/* ctl function op codes: */
#define H5FD_CTL_INVALID_OPCODE              0
#define H5FD_CTL_TEST_OPCODE                 1
#define H5FD_CTL_GET_MPI_COMMUNICATOR_OPCODE 2
#define H5FD_CTL_GET_MPI_INFO_OPCODE         9
#define H5FD_CTL_GET_MPI_RANK_OPCODE         3
#define H5FD_CTL_GET_MPI_SIZE_OPCODE         4
#define H5FD_CTL_MEM_ALLOC                   5
#define H5FD_CTL_MEM_FREE                    6
#define H5FD_CTL_MEM_COPY                    7
#define H5FD_CTL_GET_MPI_FILE_SYNC_OPCODE    8

/* ctl function flags: */

/* Definitions:
 *
 * WARNING: While the following definitions of Terminal
 * and Passthrough VFDs should be workable for now, they
 * have to be adjusted as our use cases for VFDs expand.
 *
 *                                   JRM -- 8/4/21
 *
 *
 * Terminal VFD: Lowest VFD in the VFD stack through
 * which all VFD calls pass.  Note that this definition
 * is situational.  For example, the sec2 VFD is typically
 * terminal.  However, in the context of the family file
 * driver, it is not -- the family file driver is the
 * bottom VFD through which all VFD calls pass, and thus
 * it is terminal.
 *
 * Similarly, on the splitter VFD, a sec2 VFD on the
 * R/W channel is terminal, but a sec2 VFD on the W/O
 * channel is not.
 *
 *
 * Pass through VFD:  Any VFD that relays all VFD calls
 * (with the possible exception of some non-I/O related
 * calls) to underlying VFD(s).
 */

/* Unknown op codes should be ignored silently unless the
 * H5FD_CTL_FAIL_IF_UNKNOWN_FLAG is set.
 *
 * On terminal VFDs, unknown op codes should generate an
 * error unconditionally if this flag is set.
 *
 * On pass through VFDs, unknown op codes should be routed
 * to the underlying VFD(s) as indicated by any routing
 * flags.  In the absence of such flags, the VFD should
 * generate an error.
 */
#define H5FD_CTL_FAIL_IF_UNKNOWN_FLAG 0x0001

/* The H5FD_CTL_ROUTE_TO_TERMINAL_VFD_FLAG is used only
 * by non-ternminal VFDs, and only applies to unknown
 * opcodes. (known op codes should be handled as
 * appropriate.)
 *
 * If this flag is set for an unknown op code, that
 * op code should be passed to the next VFD down
 * the VFD stack en-route to the terminal VFD.
 * If that VFD does not support the ctl call, the
 * pass through VFD should fail or succeed as directed
 * by the  H5FD_CTL_FAIL_IF_UNKNOWN_FLAG.
 */
#define H5FD_CTL_ROUTE_TO_TERMINAL_VFD_FLAG 0x0002

/*******************/
/* Public Typedefs */
/*******************/

/*
 * File driver identifiers.
 *
 * Values 0 through 255 are for drivers defined by the HDF5 library.
 * Values 256 through 511 are available for testing new drivers.
 * Subsequent values should be obtained from the HDF5 development
 * team at mailto:help@hdfgroup.org.
 */
typedef int H5FD_class_value_t;

/* Types of allocation requests: see H5Fpublic.h  */
typedef enum H5F_mem_t H5FD_mem_t;

/**
 * Define enum for the source of file image callbacks
 */
//! <!-- [H5FD_file_image_op_t_snip] -->
typedef enum {
    H5FD_FILE_IMAGE_OP_NO_OP,
    H5FD_FILE_IMAGE_OP_PROPERTY_LIST_SET,
    /**< Passed to the \p image_malloc and \p image_memcpy callbacks when a
     * file image buffer is to be copied while being set in a file access
     * property list (FAPL)*/
    H5FD_FILE_IMAGE_OP_PROPERTY_LIST_COPY,
    /**< Passed to the \p image_malloc and \p image_memcpy callbacks
     * when a file image buffer is to be copied when a FAPL is copied*/
    H5FD_FILE_IMAGE_OP_PROPERTY_LIST_GET,
    /**<Passed to the \p image_malloc and \p image_memcpy callbacks when
     * a file image buffer is to be copied while being retrieved from a FAPL*/
    H5FD_FILE_IMAGE_OP_PROPERTY_LIST_CLOSE,
    /**<Passed to the \p image_free callback when a file image
     * buffer is to be released during a FAPL close operation*/
    H5FD_FILE_IMAGE_OP_FILE_OPEN,
    /**<Passed to the \p image_malloc and
     * \p image_memcpy callbackswhen a
     * file image buffer is to be copied during a file open operation \n
     * While the file image being opened will typically be copied from a
     * FAPL, this need not always be the case. For example, the core file
     * driver, also known as the memory file driver, takes its initial
     * image from a file.*/
    H5FD_FILE_IMAGE_OP_FILE_RESIZE,
    /**<Passed to the \p image_realloc callback when a file driver needs
     * to resize an image buffer*/
    H5FD_FILE_IMAGE_OP_FILE_CLOSE
    /**<Passed to the \p image_free callback when an image buffer is to
     * be released during a file close operation*/
} H5FD_file_image_op_t;
//! <!-- [H5FD_file_image_op_t_snip] -->

/**
 * Define structure to hold file image callbacks
 */
//! <!-- [H5FD_file_image_callbacks_t_snip] -->
typedef struct {
    /**
     * \param[in] size Size in bytes of the file image buffer to allocate
     * \param[in] file_image_op A value from H5FD_file_image_op_t indicating
     *                          the operation being performed on the file image
     *                          when this callback is invoked
     * \param[in] udata Value passed in in the H5Pset_file_image_callbacks
     *            parameter \p udata
     */
    //! <!-- [image_malloc_snip] -->
    void *(*image_malloc)(size_t size, H5FD_file_image_op_t file_image_op, void *udata);
    //! <!-- [image_malloc_snip] -->
    /**
     * \param[in] dest Address of the destination buffer
     * \param[in] src Address of the source buffer
     * \param[in] size Size in bytes of the file image buffer to allocate
     * \param[in] file_image_op A value from #H5FD_file_image_op_t indicating
     *                          the operation being performed on the file image
     *                          when this callback is invoked
     * \param[in] udata Value passed in in the H5Pset_file_image_callbacks
     *            parameter \p udata
     */
    //! <!-- [image_memcpy_snip] -->
    void *(*image_memcpy)(void *dest, const void *src, size_t size, H5FD_file_image_op_t file_image_op,
                          void *udata);
    //! <!-- [image_memcpy_snip] -->
    /**
     * \param[in] ptr Pointer to the buffer being reallocated
     * \param[in] size Size in bytes of the file image buffer to allocate
     * \param[in] file_image_op A value from #H5FD_file_image_op_t indicating
     *                          the operation being performed on the file image
     *                          when this callback is invoked
     * \param[in] udata Value passed in in the H5Pset_file_image_callbacks
     *            parameter \p udata
     */
    //! <!-- [image_realloc_snip] -->
    void *(*image_realloc)(void *ptr, size_t size, H5FD_file_image_op_t file_image_op, void *udata);
    //! <!-- [image_realloc_snip] -->
    /**
     * \param[in] ptr Pointer to the buffer being reallocated
     * \param[in] file_image_op A value from #H5FD_file_image_op_t indicating
     *                          the operation being performed on the file image
     *                          when this callback is invoked
     * \param[in] udata Value passed in in the H5Pset_file_image_callbacks
     *            parameter \p udata
     */
    //! <!-- [image_free_snip] -->
    herr_t (*image_free)(void *ptr, H5FD_file_image_op_t file_image_op, void *udata);
    //! <!-- [image_free_snip] -->
    /**
     * \param[in] udata Value passed in in the H5Pset_file_image_callbacks
     *            parameter \p udata
     */
    //! <!-- [udata_copy_snip] -->
    void *(*udata_copy)(void *udata);
    //! <!-- [udata_copy_snip] -->
    /**
     * \param[in] udata Value passed in in the H5Pset_file_image_callbacks
     *            parameter \p udata
     */
    //! <!-- [udata_free_snip] -->
    herr_t (*udata_free)(void *udata);
    //! <!-- [udata_free_snip] -->
    /**
     * \brief The final field in the #H5FD_file_image_callbacks_t struct,
     *        provides a pointer to user-defined data. This pointer will be
     *        passed to the image_malloc, image_memcpy, image_realloc, and
     *        image_free callbacks. Define udata as NULL if no user-defined
     *        data is provided.
     */
    void *udata;
} H5FD_file_image_callbacks_t;
//! <!-- [H5FD_file_image_callbacks_t_snip] -->

/**
 * Define structure to hold "ctl memory copy" parameters
 */
//! <!-- [H5FD_ctl_memcpy_args_t_snip] -->
typedef struct H5FD_ctl_memcpy_args_t {
    void       *dstbuf;  /**< Destination buffer */
    hsize_t     dst_off; /**< Offset within destination buffer */
    const void *srcbuf;  /**< Source buffer */
    hsize_t     src_off; /**< Offset within source buffer */
    size_t      len;     /**< Length of data to copy from source buffer */
} H5FD_ctl_memcpy_args_t;
//! <!-- [H5FD_ctl_memcpy_args_t_snip] -->

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes */

/**
 * \ingroup H5VFD
 *
 * \brief Allows querying a VFD ID for features before the file is opened
 *
 * \param[in] driver_id Virtual File Driver (VFD) ID
 * \param[out] flags VFD flags supported
 *
 * \return \herr_t
 *
 * \details Queries a virtual file driver (VFD) for feature flags. Takes a
 *          VFD hid_t so it can be used before the file is opened. For example,
 *          this could be used to check if a VFD supports SWMR.
 *
 * \note The flags obtained here are just those of the base driver and
 *       do not take any configuration options (e.g., set via a fapl
 *       call) into consideration.
 *
 * \since 1.10.2
 */
H5_DLL herr_t H5FDdriver_query(hid_t driver_id, unsigned long *flags /*out*/);

#ifdef __cplusplus
}
#endif
#endif
