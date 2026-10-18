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
 * This file contains public declarations for the H5D module.
 */
#ifndef H5Dpublic_H
#define H5Dpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Ipublic.h" /* Identifiers                              */

/*****************/
/* Public Macros */
/*****************/

/* Macros used to "unset" chunk cache configuration parameters */
#define H5D_CHUNK_CACHE_NSLOTS_DEFAULT SIZE_MAX
#define H5D_CHUNK_CACHE_NBYTES_DEFAULT SIZE_MAX
#define H5D_CHUNK_CACHE_W0_DEFAULT     (-1.0)

/** Bit flags for the H5Pset/get_chunk_opts() \since 1.10.0 */
#define H5D_CHUNK_DONT_FILTER_PARTIAL_CHUNKS (0x0002u)

/*******************/
/* Public Typedefs */
/*******************/

//! <!-- [H5D_layout_t_snip] -->
/**
 * Values for the H5D_LAYOUT property
 */
typedef enum H5D_layout_t {
    H5D_LAYOUT_ERROR = -1, /**< error */
    H5D_COMPACT      = 0,  /**< raw data is small (< 64KB) */
    H5D_CONTIGUOUS   = 1,  /**< contiguous layout */
    H5D_CHUNKED      = 2,  /**< chunked or tiled layout */
    H5D_VIRTUAL      = 3,  /**< actual data is stored in other datasets */
    H5D_NLAYOUTS     = 4   /**< this one must be last! */
} H5D_layout_t;
//! <!-- [H5D_layout_t_snip] -->

//! <!-- [H5D_chunk_index_t_snip] -->
/**
 * Types of chunk index data structures
 */
typedef enum H5D_chunk_index_t {
    H5D_CHUNK_IDX_BTREE = 0, /**< v1 B-tree index (default)                */
    H5D_CHUNK_IDX_SINGLE =
        1, /**< Single Chunk index (cur dims[]=max dims[]=chunk dims[]; filtered & non-filtered) */
    H5D_CHUNK_IDX_NONE   = 2, /**< Implicit: No Index (#H5D_ALLOC_TIME_EARLY, non-filtered, fixed dims) */
    H5D_CHUNK_IDX_FARRAY = 3, /**< Fixed array (for 0 unlimited dims)       */
    H5D_CHUNK_IDX_EARRAY = 4, /**< Extensible array (for 1 unlimited dim)   */
    H5D_CHUNK_IDX_BT2    = 5, /**< v2 B-tree index (for >1 unlimited dims)  */
    H5D_CHUNK_IDX_NTYPES      /**< This one must be last!                   */
} H5D_chunk_index_t;
//! <!-- [H5D_chunk_index_t_snip] -->

//! <!-- [H5D_alloc_time_t_snip] -->
/**
 * Values for the space allocation time property
 */
typedef enum H5D_alloc_time_t {
    H5D_ALLOC_TIME_ERROR   = -1, /**< Error */
    H5D_ALLOC_TIME_DEFAULT = 0,  /**< Default (layout dependent) */
    H5D_ALLOC_TIME_EARLY   = 1,  /**< Allocate on creation */
    H5D_ALLOC_TIME_LATE    = 2,  /**< Allocate on first write */
    H5D_ALLOC_TIME_INCR    = 3   /**< Allocate incrementally (by chunk) */
} H5D_alloc_time_t;
//! <!-- [H5D_alloc_time_t_snip] -->

//! <!-- [H5D_space_status_t_snip] -->
/**
 * Values for the status of space allocation
 */
typedef enum H5D_space_status_t {
    H5D_SPACE_STATUS_ERROR          = -1, /**< Error */
    H5D_SPACE_STATUS_NOT_ALLOCATED  = 0,  /**< Space has not been allocated for this dataset. */
    H5D_SPACE_STATUS_PART_ALLOCATED = 1,  /**< Space has been partially allocated for this dataset.
                                               (Used only for datasets with chunked storage.) */
    H5D_SPACE_STATUS_ALLOCATED = 2        /**< Space has been allocated for this dataset. */
} H5D_space_status_t;
//! <!-- [H5D_space_status_t_snip] -->

//! <!-- [H5D_fill_time_t_snip] -->
/**
 * Values for time of writing fill value property
 */
typedef enum H5D_fill_time_t {
    H5D_FILL_TIME_ERROR = -1, /**< Error */
    H5D_FILL_TIME_ALLOC = 0,  /**< Fill on allocation */
    H5D_FILL_TIME_NEVER = 1,  /**< Never write fill values */
    H5D_FILL_TIME_IFSET = 2   /**< Fill if fill-value was set */
} H5D_fill_time_t;
//! <!-- [H5D_fill_time_t_snip] -->

//! <!-- [H5D_fill_value_t_snip] -->
/**
 * Values for fill value status
 */
typedef enum H5D_fill_value_t {
    H5D_FILL_VALUE_ERROR        = -1, /**< Error */
    H5D_FILL_VALUE_UNDEFINED    = 0,  /**< No fill value defined */
    H5D_FILL_VALUE_DEFAULT      = 1,  /**< Default fill-value */
    H5D_FILL_VALUE_USER_DEFINED = 2   /**< User-defined fill-value */
} H5D_fill_value_t;
//! <!-- [H5D_fill_value_t_snip] -->

//! <!-- [H5D_vds_view_t_snip] -->
/**
 * Values for VDS bounds option
 */
typedef enum H5D_vds_view_t {
    H5D_VDS_ERROR          = -1, /**< Error */
    H5D_VDS_FIRST_MISSING  = 0,  /**< Include all data before the first missing mapped data */
    H5D_VDS_LAST_AVAILABLE = 1   /**< Include all available mapped data */
} H5D_vds_view_t;
//! <!-- [H5D_vds_view_t_snip] -->

//! <!-- [H5D_append_cb_t_snip] -->
/**
 * \brief Callback for H5Pset_append_flush()
 *
 * \dset_id{dataset_id}
 * \param[in] cur_dims The current extent of the dataset's dimensions
 * \param[in,out] op_data User context
 *
 * \return \herr_t
 *
 * \since 1.10.0
 *
 */
typedef herr_t (*H5D_append_cb_t)(hid_t dataset_id, hsize_t *cur_dims, void *op_data);
//! <!-- [H5D_append_cb_t_snip] -->

//! <!-- [H5D_operator_t_snip] -->
/**
 * \brief Callback for H5Diterate()
 *
 * \param[in,out] elem Pointer to the memory buffer containing the current dataset
 *                     element
 * \param[in] type_id Datatype identifier of the elements stored in \p elem
 * \param[in] ndim Number of dimensions for the \p point array
 * \param[in] point Array containing the location of the element within
 *                  the original dataspace
 * \param[in,out] operator_data Pointer to any user-defined data associated with
 *                the operation
 * \return \herr_t_iter
 *
 * \since 1.10.2
 *
 */
typedef herr_t (*H5D_operator_t)(void *elem, hid_t type_id, unsigned ndim, const hsize_t *point,
                                 void *operator_data);
//! <!-- [H5D_operator_t_snip] -->

//! <!-- [H5D_scatter_func_t_snip] -->
/**
 * \brief Callback for H5Dscatter()
 *
 * \param[out] src_buf Pointer to the buffer holding the next set of elements to
 *                     scatter. On entry, the value of where \p src_buf points to
 *                     is undefined. The callback function should set \p src_buf
 *                     to point to the next set of elements.
 * \param[out] src_buf_bytes_used Pointer to the number of valid bytes in \p src_buf.
 *                                On entry, the value where \p src_buf_bytes_used points
 *                                to is undefined. The callback function should set
 *                                \p src_buf_bytes_used to the of valid bytes in \p src_buf.
 *                                This number must be a multiple of the datatype size.
 * \param[in,out] op_data User-defined pointer to data required by the callback
 *                        function. A pass-through of the \p op_data pointer provided
 *                        with the H5Dscatter() function call.
 * \return herr_t
 *
 * \details The callback function should always return at least one
 *          element in \p src_buf, and must not return more elements
 *          than are remaining to be scattered. This function will be
 *          repeatedly called until all elements to be scattered have
 *          been returned. The callback function should return zero (0)
 *          to indicate success, and a negative value to indicate failure.
 *
 * \since 1.10.2
 *
 */
typedef herr_t (*H5D_scatter_func_t)(const void **src_buf /*out*/, size_t *src_buf_bytes_used /*out*/,
                                     void *op_data);
//! <!-- [H5D_scatter_func_t_snip] -->

//! <!-- [H5D_gather_func_t_snip] -->
/**
 * \brief Callback for H5Dgather()
 *
 * \param[in] dst_buf Pointer to the destination buffer which has been filled
 *                    with the next set of elements gathered. This will always
 *                    be identical to the \p dst_buf passed to H5Dgather()
 * \param[in] dst_buf_bytes_used  Pointer to the number of valid bytes in
 *                                \p dst_buf. This number must be a multiple of
 *                                the datatype size.
 * \param[in,out] op_data User-defined pointer to data required by the callback
 *                        function; a pass-through of the \p op_data pointer
 *                        provided with the H5Dgather() function call.
 * \return \herr_t
 *
 * \details The callback function should process, store, or otherwise make use
 *          of the data returned in dst_buf before it returns, because the
 *          buffer will be overwritten unless it is the last call to the
 *          callback. This function will be repeatedly called until all gathered
 *          elements have been passed to the callback in dst_buf. The callback
 *          function should return zero (0) to indicate success, and a negative
 *          value to indicate failure.
 *
 * \since 1.10.2
 *
 */
typedef herr_t (*H5D_gather_func_t)(const void *dst_buf, size_t dst_buf_bytes_used, void *op_data);
//! <!-- [H5D_gather_func_t_snip] -->

//! <!-- [H5D_chunk_iter_op_t_snip] -->
/**
 * \brief Callback for H5Dchunk_iter()
 *
 * \param[in]     offset      Logical position of the chunk's first element in units of dataset elements
 * \param[in]     filter_mask Bitmask indicating the filters used when the chunk was written
 * \param[in]     addr        Chunk address in the file, taking the user block (if any) into account
 * \param[in]     size        Chunk size in bytes, 0 if the chunk does not exist
 * \param[in,out] op_data     Pointer to any user-defined data associated with
 *                            the operation.
 * \return \li Zero (#H5_ITER_CONT) causes the iterator to continue, returning
 *              zero when all elements have been processed.
 *          \li A positive value (#H5_ITER_STOP) causes the iterator to
 *              immediately return that value, indicating short-circuit success.
 *          \li A negative (#H5_ITER_ERROR) causes the iterator to immediately
 *              return that value, indicating failure.
 *
 * \callback_note
 *
 * \since 1.14.0
 *
 */
typedef int (*H5D_chunk_iter_op_t)(const hsize_t *offset, unsigned filter_mask, haddr_t addr, hsize_t size,
                                   void *op_data);
//! <!-- [H5D_chunk_iter_op_t_snip] -->

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/
#ifdef __cplusplus
extern "C" {
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Creates a new dataset and links it into the file
 *
 * \fgdta_loc_id
 * \param[in] name Name of the dataset to create
 * \type_id
 * \space_id
 * \lcpl_id
 * \dcpl_id
 * \dapl_id
 *
 * \return \hid_t{dataset}
 *
 * \details H5Dcreate2() creates a new dataset named \p name at
 *          the location specified by \p loc_id, and associates constant
 *          and initial persistent properties with that dataset, including
 *          the datatype \p dtype_id, the dataspace \p space_id, and
 *          other properties as specified by the dataset creation property
 *          list \p dcpl_id and the access property list \p dapl_id,
 *          respectively. Once created, the dataset is opened for access.
 *
 *          \p loc_id may specify a file, group, dataset, named datatype,
 *          or attribute.  If an attribute, dataset, or named datatype is
 *          specified, then the dataset will be created at the location
 *          where the attribute, dataset, or named datatype is attached.
 *
 *          \p name may be either an absolute path in the file or a relative
 *          path from \p loc_id naming the dataset.
 *
 *          If \p dtype_id is a committed datatype, and if the file location
 *          associated with the committed datatype is different from the
 *          file location where the dataset will be created, the datatype
 *          is copied and converted to a transient type.
 *
 *          The link creation property list, \p lcpl_id, governs the creation
 *          of the link(s) by which the new dataset is accessed and the
 *          creation of any intermediate groups that may be missing.
 *
 *          The datatype and dataspace properties and the dataset creation
 *          and access property lists are attached to the dataset, so the
 *          caller may derive new datatypes, dataspaces, and creation and
 *          access properties from the old ones and reuse them in calls to
 *          create additional datasets.  Once created, the dataset can be
 *          read from or written to. Reading data from a dataset that was
 *          not previously written, the HDF5 library will return default
 *          or user-defined fill values.
 *
 * \par Example
 * \snippet H5D_examples.c create
 *
 * \since 1.8.0
 *
 * \see H5Dopen2(), H5Dclose(), H5Tset_size()
 *
 */
H5_DLL hid_t H5Dcreate2(hid_t loc_id, const char *name, hid_t type_id, hid_t space_id, hid_t lcpl_id,
                        hid_t dcpl_id, hid_t dapl_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dcreate}
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Dcreate_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                             const char *name, hid_t type_id, hid_t space_id, hid_t lcpl_id, hid_t dcpl_id,
                             hid_t dapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Dcreate_async(hid_t loc_id, const char *name, hid_t type_id, hid_t space_id, hid_t lcpl_id,
                             hid_t dcpl_id, hid_t dapl_id, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Creates a dataset in a file without linking it into the file
 *        structure
 *
 * \fgdta_loc_id
 * \type_id
 * \space_id
 * \dcpl_id
 * \dapl_id
 *
 * \return \hid_t{dataset}
 *
 * \details H5Dcreate_anon() creates a dataset in the file specified
 *          by \p loc_id.
 *
 *          \p loc_id may specify a file, group, dataset, named datatype,
 *          or attribute.  If an attribute, dataset, or named datatype is
 *          specified, then the dataset will be created at the location
 *          where the attribute, dataset, or named datatype is attached.
 *
 *          The dataset's datatype and dataspace are specified by
 *          \p type_id and \p space_id, respectively. These are the
 *          datatype and dataspace of the dataset as they will exist in
 *          the file, which may differ from the datatype and dataspace
 *          in application memory.
 *
 *          H5Dcreate_anon() returns a new dataset identifier. Using
 *          this identifier, the new dataset must be linked into the
 *          HDF5 file structure with H5Olink() or it will be deleted
 *          when the file is closed.
 *
 * \since 1.8.0
 *
 * \see H5Olink(), H5Dcreate()
 *
 */
H5_DLL hid_t H5Dcreate_anon(hid_t loc_id, hid_t type_id, hid_t space_id, hid_t dcpl_id, hid_t dapl_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Opens an existing dataset
 *
 * \fgdta_loc_id
 * \param[in] name      Name of the dataset to open
 * \dapl_id
 *
 * \return \hid_t{dataset}
 *
 * \details H5Dopen2() opens the existing dataset specified
 *          by a location identifier and name, \p loc_id and \p name,
 *          respectively.
 *
 *          \p loc_id may specify a file, group, dataset, named datatype,
 *          or attribute.  If an attribute, dataset, or named datatype is
 *          specified then the dataset will be opened at the location
 *          where the attribute, dataset, or named datatype is attached.
 *
 * \since 1.8.0
 *
 * \see H5Dcreate2(), H5Dclose()
 *
 */
H5_DLL hid_t H5Dopen2(hid_t loc_id, const char *name, hid_t dapl_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dopen}
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Dopen_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                           const char *name, hid_t dapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Dopen_async(hid_t loc_id, const char *name, hid_t dapl_id, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 *\ingroup H5D
 *
 * \brief Returns an identifier for a copy of the dataspace for a dataset
 *
 * \dset_id
 *
 * \return \hid_t{dataspace}
 *
 * \details H5Dget_space() makes a copy of the dataspace of
 *          the dataset specified by \p dset_id. The function returns an
 *          identifier for the new copy of the dataspace.
 *
 *          A dataspace identifier returned from this function should
 *          be released with H5Sclose() when the identifier is no longer
 *          needed so that resource leaks will not occur.
 *
 * \since 1.0.0
 *
 * \par Example
 * \snippet H5D_examples.c update
 *
 * \see H5Sclose()
 *
 */
H5_DLL hid_t H5Dget_space(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dget_space}
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Dget_space_async(const char *app_file, const char *app_func, unsigned app_line, hid_t dset_id,
                                hid_t es_id);
#else
H5_DLL hid_t H5Dget_space_async(hid_t dset_id, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Determines whether space has been allocated for a dataset
 *
 * \dset_id
 * \param[out] allocation Space allocation status
 *
 * \return \herr_t
 *
 * \details H5Dget_space_status() determines whether space has been allocated
 *          for the dataset \p dset_id.
 *
 * \note \Bold{BUG:} Prior to the HDF5 1.14.0, 1.12.2 and 1.10.9 releases,
 *       H5Dget_space_status() may return incorrect space allocation status
 *       values for datasets with filters applied to them.
 *       H5Dget_space_status() calculated the space allocation status by
 *       comparing the sum of the sizes of all the allocated chunks in the
 *       dataset against the total data size of the dataset, as calculated by
 *       the number of elements in the dataset's dataspace multiplied by the
 *       dataset's datatype size. If the dataset had any compression filters
 *       applied to it and the dataset chunks were successfully compressed,
 *       the sum of the sizes of the allocated dataset chunks would generally
 *       always be less than the total data size of the dataset, and
 *       H5Dget_space_status() wouldn't ever return
 *       `H5D_SPACE_STATUS_ALLOCATED`.
 *
 * \since 1.6.0
 *
 */
H5_DLL herr_t H5Dget_space_status(hid_t dset_id, H5D_space_status_t *allocation);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Returns an identifier for a copy of the datatype for a dataset
 *
 * \dset_id
 *
 * \return \hid_t{datatype}
 *
 * \details H5Dget_type() returns an identifier of a copy of
 *          the datatype for a dataset.
 *
 *          If a dataset has a named datatype, then an identifier to the
 *          opened datatype is returned. Otherwise, the returned datatype
 *          is read-only.
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Dget_type(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Returns an identifier for a copy of the dataset creation
 *        property list for a dataset
 *
 * \dset_id
 *
 * \return \hid_t{dataset creation property list}
 *
 * \details H5Dget_create_plist() returns an identifier for
 *          a copy of the dataset creation property list associated with
 *          the dataset specified by \p dset_id.
 *
 *          The creation property list identifier should be released with
 *          H5Pclose() to prevent resource leaks.
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Dget_create_plist(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Returns the dataset access property list associated with
 *        a dataset
 *
 * \dset_id
 *
 * \return \hid_t{dataset access property list}
 *
 * \details H5Dget_access_plist() returns a copy of the
 *          dataset access property list used to open the specified
 *          dataset, \p dset_id. Modifications to the returned property
 *          list will have no effect on the dataset it was retrieved from.
 *
 *          The chunk cache parameters in the returned property lists will
 *          be those used by the dataset. If the properties in the file
 *          access property list were used to determine the dataset's
 *          chunk cache configuration, then those properties will be
 *          present in the returned dataset access property list. If
 *          the dataset does not use a chunked layout, then the chunk
 *          cache properties will be set to the default. The chunk cache
 *          properties in the returned list are considered to be “set”,
 *          and any use of this list will override the corresponding
 *          properties in the file's file access property list.
 *
 *          All link access properties in the returned list will be set
 *          to the default values.
 *
 * \since 1.8.3
 *
 */
H5_DLL hid_t H5Dget_access_plist(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Returns the amount of storage allocated for a dataset
 *
 * \dset_id
 *
 * \return Returns the amount of storage space, in bytes, or 0 (zero).
 *
 * \details H5Dget_storage_size() returns the amount of storage,
 *          in bytes, that is allocated in the file for the raw data of
 *          the dataset specified by \p dset_id.
 *          H5Dget_storage_size() reports only the space required to store
 *          the dataset elements, excluding any metadata.
 *          \li For contiguous datasets, the returned size equals the current
 *          allocated size of the raw data.
 *          \li For unfiltered chunked datasets, the returned size is the
 *          number of allocated chunks times the chunk size.
 *          \li For filtered chunked datasets, the returned size is the
 *          space required to store the filtered data. For example, if a
 *          compression filter is in use, H5Dget_storage_size() will return
 *          the total space required to store the compressed chunks.
 *
 * \note Note that H5Dget_storage_size() is not generally an
 *       appropriate function to use when determining the amount
 *       of memory required to work with a dataset. In such
 *       circumstances, you must determine the number of data
 *       points in a dataset and the size of an individual dataset
 *       element. H5Sget_simple_extent_npoints() and H5Tget_size()
 *       can be used to calculate that amount.
 *
 * \warning H5Dget_storage_size() does not differentiate between 0 (zero),
 *          the value returned for the storage size of a dataset
 *          with no stored values, and 0 (zero), the value returned to
 *          indicate an error.
 *
 * \since 1.2.0
 *
 */
H5_DLL hsize_t H5Dget_storage_size(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Returns the amount of storage allocated within the file for a
 * raw data chunk in a dataset
 *
 * \dset_id
 * \param[in]  offset   Logical offset in the dataset for the chunk to query
 * \param[out] chunk_bytes The size in bytes for the chunk
 *
 * \return \herr_t
 *
 * \details H5Dget_chunk_storage_size() returns the size in bytes
 *          allocated in the file for a raw data chunk as specified by
 *          its logical \p offset in the dataset \p dset_id. The size is
 *          returned in \p chunk_nbytes. It is the size of the compressed
 *          data if the chunk is filtered and the size may be zero if no
 *          storage is allocated yet for the dataset.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Dget_chunk_storage_size(hid_t dset_id, const hsize_t *offset, hsize_t *chunk_bytes);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Retrieves number of chunks that have nonempty intersection with a
 *        specified selection
 *
 * \dset_id
 * \param[in]  fspace_id   File dataspace selection identifier
 * \param[out] nchunks     Number of chunks in the selection
 *
 * \return \herr_t
 *
 * \details H5Dget_num_chunks() retrieves the number of chunks
 *          nchunks in a set of selected elements specified by \p fspace_id
 *          for a dataset specified by the identifier \p dset_id. If \p
 *          fspace_id is #H5S_ALL, the function will retrieve the total
 *          number of chunks stored for the dataset.
 *
 *          \p fspace_id specifies the file dataspace selection.  It is
 *          intended to take #H5S_ALL for specifying the current selection.
 *
 *          \note Please be aware that this function currently does not
 *          support non-trivial selections, thus \p fspace_id has no
 *          effect. Also, the implementation does not handle the #H5S_ALL
 *          macro correctly.  As a workaround, application can get
 *          the dataspace for the dataset using H5Dget_space() and pass that
 *          in for \p fspace_id.  This will be fixed in a future release.
 *
 * \since 1.10.5
 *
 */
H5_DLL herr_t H5Dget_num_chunks(hid_t dset_id, hid_t fspace_id, hsize_t *nchunks);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Retrieves information about a chunk specified by its coordinates
 *
 * \dset_id
 * \param[in]  offset      Logical position of the chunk's first element in units of dataset elements
 * \param[out] filter_mask Bitmask indicating the filters used when the chunk was written
 * \param[out] addr        Chunk address in the file, taking the user block (if any) into account
 * \param[out] size        Chunk size in bytes, 0 if the chunk does not exist
 *
 * \return \herr_t
 *
 * \details H5Dget_chunk_info_by_coord() retrieves the \p filter_mask, \p size,
 *          and \p addr for a chunk in the dataset specified by \p dset_id,
 *          using the coordinates specified by \p offset.
 *
 *          If the queried chunk does not exist in the file, \p size will
 *          be set to 0, \p addr to \c HADDR_UNDEF, and the buffer \p
 *          filter_mask will not be modified.
 *
 *          \p offset is a pointer to a one-dimensional array with a size
 *          equal to the dataset's rank. Each element is the logical
 *          position of the chunk's first element in a dimension.
 *
 * \note    Prior to HDF5 1.14.4, the reported address did not take the
 *          user block into account.
 *
 * \since 1.10.5
 *
 */
H5_DLL herr_t H5Dget_chunk_info_by_coord(hid_t dset_id, const hsize_t *offset, unsigned *filter_mask,
                                         haddr_t *addr, hsize_t *size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Iterate over all chunks of a chunked dataset
 *
 * \dset_id
 * \param[in]  dxpl_id  Identifier of a transfer property list
 * \param[in]  cb       User callback function, called for every chunk.
 * \param[in]  op_data  User-defined pointer to data required by op
 *
 * \return \herr_t
 *
 * \details H5Dchunk_iter iterates over all chunks in the dataset, calling the
 *          user supplied callback with the details of the chunk and the supplied
 *          context \p op_data.
 *
 * \note    Prior to HDF5 1.14.4, the address passed to the callback did not take
 *          the user block into account.
 *
 * \par Example
 * For each chunk, print the allocated chunk size (0 for unallocated chunks).
 * \snippet H5D_examples.c H5Dchunk_iter_cb
 * Iterate over all chunked datasets and chunks in a file.
 * \snippet H5D_examples.c H5Ovisit_cb
 *
 * \callback_note
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5Dchunk_iter(hid_t dset_id, hid_t dxpl_id, H5D_chunk_iter_op_t cb, void *op_data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Retrieves information about a chunk specified by its index
 *
 * \dset_id
 * \param[in]  fspace_id File dataspace selection identifier (See Note below)
 * \param[in]  chk_idx   Index of the chunk
 * \param[out] offset    Logical position of the chunk's first element in units of dataset elements
 * \param[out] filter_mask Bitmask indicating the filters used when the chunk was written
 * \param[out] addr      Chunk address in the file, taking the user block (if any) into account
 * \param[out] size      Chunk size in bytes, 0 if the chunk does not exist
 *
 * \return \herr_t
 *
 * \details H5Dget_chunk_info() retrieves the offset coordinates,
 *          \p offset, filter mask, \p filter_mask, size, \p size, and address
 *          \p addr for the dataset specified by the identifier \p dset_id and the chunk
 *          specified by the index \p index. The chunk belongs to a set of
 *          chunks in the selection specified by \p fspace_id. If the queried
 *          chunk does not exist in the file, the size will be set to 0 and
 *          address to #HADDR_UNDEF. The value pointed to by filter_mask will
 *          not be modified. \c NULL can be passed in for any \p out parameters.
 *
 * \note    Prior to HDF5 1.14.4, the reported address did not take the
 *          user block into account.
 *
 *          \p chk_idx is the chunk index in the selection. The index value
 *          may have a value of 0 up to the number of chunks stored in
 *          the file that has a nonempty intersection with the file
 *          dataspace selection.
 *
 *          \note As of 1.10.5, the dataspace intersection is not yet
 *          supported. Hence, the index is of all the written chunks.
 *
 *          \p fspace_id specifies the file dataspace selection.  It is
 *          intended to take #H5S_ALL to specify the current selection.
 *
 *          \note Please be aware that this function currently does not
 *          support non-trivial selections; thus \p fspace_id has no
 *          effect. Also, the implementation does not handle the #H5S_ALL
 *          macro correctly.  As a workaround, an application can get
 *          the dataspace for the dataset using H5Dget_space() and pass that
 *          in for \p fspace_id.  This will be fixed in a future release.
 *
 * \since 1.10.5
 *
 */
H5_DLL herr_t H5Dget_chunk_info(hid_t dset_id, hid_t fspace_id, hsize_t chk_idx, hsize_t *offset,
                                unsigned *filter_mask, haddr_t *addr, hsize_t *size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Returns dataset address in file
 *
 * \dset_id
 *
 * \return Returns the offset in bytes; otherwise, returns #HADDR_UNDEF,
 *         a negative value.
 *
 * \details H5Dget_offset() returns the address in the file of
 *          the dataset, \p dset_id. That address is expressed as the
 *          offset in bytes from the beginning of the file.
 *
 * \since 1.6.0
 *
 */
H5_DLL haddr_t H5Dget_offset(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Reads raw data from a dataset into a provided buffer
 *
 * \dset_id                 Identifier of the dataset to read from
 * \param[in] mem_type_id   Identifier of the memory datatype
 * \param[in] mem_space_id  Identifier of the memory dataspace
 * \param[in] file_space_id Identifier of the dataset's dataspace in the file
 * \param[in] dxpl_id       Identifier of a transfer property list
 * \param[out] buf          Buffer to receive data read from file
 *
 * \return \herr_t
 *
 * \details H5Dread() reads a dataset, specified by its identifier
 *          \p dset_id, from the file into an application memory buffer \p
 *          buf. Data transfer properties are defined by the argument \p
 *          dxpl_id. The memory datatype of the (partial) dataset
 *          is identified by the identifier \p mem_type_id. The part
 *          of the dataset to read is defined by \p mem_space_id and \p
 *          file_space_id.
 *
 *          \p file_space_id is used to specify only the selection within
 *          the file dataset's dataspace. Any dataspace specified in \p
 *          file_space_id is ignored by the library and the dataset's
 *          dataspace is always used. \p file_space_id can be the constant
 *          #H5S_ALL, which indicates that the entire file dataspace,
 *          as defined by the current dimensions of the dataset, is to
 *          be selected.
 *
 *          \p mem_space_id is used to specify both the memory dataspace
 *          and the selection within that dataspace. \p mem_space_id can
 *          be the constant #H5S_ALL, in which case the file dataspace is
 *          used for the memory dataspace and the selection defined with \p
 *          file_space_id is used for the selection within that dataspace.
 *
 *          The number of elements selected in the memory dataspace \Emph{must}
 *          be equal to the number of elements selected in the file dataspace.
 *
 *          The behavior of the library for the various combinations of
 *          valid dataspace identifiers and #H5S_ALL for the \p mem_space_id
 *          and the \p file_space_id parameters is described below:
 *
 *          <table>
 *            <tr>
 *              <th>mem_space_id</th>
 *              <th>file_space_id</th>
 *              <th>Behavior</th>
 *            </tr>
 *            <tr>
 *              <td>valid dataspace ID</td>
 *              <td>valid dataspace ID</td>
 *              <td>\p mem_space_id specifies the memory dataspace and the
 *                  selection within it. \p file_space_id specifies the
 *                  selection within the file dataset's dataspace.</td>
 *            </tr>
 *            <tr>
 *              <td>#H5S_ALL</td>
 *              <td>valid dataspace ID</td>
 *              <td>The file dataset's dataspace is used for the memory
 *                  dataspace and the selection specified with \p file_space_id
 *                  specifies the selection within it. The combination of the
 *                  file dataset's dataspace and the selection from
 *                  \p file_space_id is used for memory also.</td>
 *            </tr>
 *            <tr>
 *              <td>valid dataspace ID</td>
 *              <td>#H5S_ALL</td>
 *              <td>\p mem_space_id specifies the memory dataspace and the
 *                  selection within it. The selection within the file
 *                  dataset's dataspace is set to the "all" selection.</td>
 *            </tr>
 *            <tr>
 *              <td>#H5S_ALL</td>
 *              <td>#H5S_ALL</td>
 *              <td>The file dataset's dataspace is used for the memory
 *                  dataspace and the selection within the memory dataspace
 *                  is set to the "all" selection. The selection within the
 *                  file dataset's dataspace is set to the "all" selection.</td>
 *            </tr>
 *          </table>
 *
 * \note If no storage space was allocated for the dataset
 *       and a fill value is defined, the returned buffer \p buf
 *       is filled with the fill value.
 *
 * \par Example
 * \snippet H5D_examples.c read
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Dread(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                      hid_t dxpl_id, void *buf /*out*/);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Reads raw data from a set of datasets into the provided buffers
 *
 * \param[in] count         Number of datasets to read from
 * \param[in] dset_id       Identifiers of the datasets to read from
 * \param[in] mem_type_id   Identifiers of the memory datatypes
 * \param[in] mem_space_id  Identifiers of the memory dataspaces
 * \param[in] file_space_id Identifiers of the datasets' dataspaces in the file
 * \param[in] dxpl_id       Identifier of a transfer property list
 * \param[out] buf          Buffers to receive data read from file
 *
 * \return \herr_t
 *
 * \details H5Dread_multi() reads data from \p count datasets, whose identifiers
 *          are listed in the \p dset_id array, from the file into multiple
 *          application memory buffers listed in the \p buf array. Data transfer
 *          properties are defined by the argument \p dxpl_id. The memory
 *          datatypes of each dataset are listed by identifier in the \p
 *          mem_type_id array. The parts of each dataset to read are listed by
 *          identifier in the \p file_space_id array, and the parts of each
 *          application memory buffer to read to are listed by identifier in the
 *          \p mem_space_id array. All array parameters have length \p count.
 *
 *          This function will produce the same results as \p count calls to
 *          H5Dread(). Information listed in that function about the specifics
 *          of its behavior also applies to H5Dread_multi(). By calling
 *          H5Dread_multi() instead of multiple calls to H5Dread(), however, the
 *          library can in some cases pass information about the entire I/O
 *          operation to the file driver, which can improve performance.
 *
 *          All datasets must be in the same HDF5 file, and each unique dataset
 *          may only be listed once. If this function is called collectively in
 *          parallel, each rank must pass exactly the same list of datasets in
 *          \p dset_id , though the other parameters may differ.
 *
 * \since 1.14.0
 *
 * \see H5Dread()
 *
 */
H5_DLL herr_t H5Dread_multi(size_t count, hid_t dset_id[], hid_t mem_type_id[], hid_t mem_space_id[],
                            hid_t file_space_id[], hid_t dxpl_id, void *buf[] /*out*/);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dread}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Dread_async(const char *app_file, const char *app_func, unsigned app_line, hid_t dset_id,
                            hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id,
                            void *buf /*out*/, hid_t es_id);
#else
H5_DLL herr_t H5Dread_async(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                            hid_t dxpl_id, void *buf /*out*/, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dread_multi}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Dread_multi_async(const char *app_file, const char *app_func, unsigned app_line, size_t count,
                                  hid_t dset_id[], hid_t mem_type_id[], hid_t mem_space_id[],
                                  hid_t file_space_id[], hid_t dxpl_id, void *buf[] /*out*/, hid_t es_id);
#else
H5_DLL herr_t H5Dread_multi_async(size_t count, hid_t dset_id[], hid_t mem_type_id[], hid_t mem_space_id[],
                                  hid_t file_space_id[], hid_t dxpl_id, void *buf[] /*out*/, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Writes raw data from a buffer to a dataset
 *
 * \param[in] dset_id        Identifier of the dataset to read from
 * \param[in] mem_type_id    Identifier of the memory datatype
 * \param[in] mem_space_id   Identifier of the memory dataspace
 * \param[in] file_space_id  Identifier of the dataset's dataspace in the file
 * \dxpl_id
 * \param[out] buf           Buffer with data to be written to the file
 *
 * \return \herr_t
 *
 * \details H5Dwrite() writes a (partial) dataset, specified by
 *          its identifier \p dset_id, from the application memory buffer \p
 *          buf into the file. Data transfer properties are defined by the
 *          argument \p dxpl_id. The memory datatype of the (partial)
 *          dataset is identified by the identifier \p mem_type_id. The
 *          part of the dataset to write is defined by \p mem_space_id
 *          and \p file_space_id.
 *
 *          If \p mem_type_id is either a fixed-length or variable-length
 *          string, it is important to set the string length when defining
 *          the datatype. String datatypes are derived from #H5T_C_S1
 *          (or #H5T_FORTRAN_S1 for Fortran codes), which defaults
 *          to 1 character in size. See H5Tset_size() and Creating
 *          variable-length string datatypes.
 *
 *          \p file_space_id is used to specify only the selection within
 *          the file dataset's dataspace. Any dataspace specified in \p
 *          file_space_id is ignored by the library and the dataset's
 *          dataspace is always used. \p file_space_id can be the constant
 *          #H5S_ALL, which indicates that the entire file dataspace,
 *          as defined by the current dimensions of the dataset, is to
 *          be selected.
 *
 *          \p mem_space_id is used to specify both the memory dataspace
 *          and the selection within that dataspace. mem_space_id can be
 *          the constant #H5S_ALL, in which case the file dataspace is
 *          used for the memory dataspace and the selection defined with \p
 *          file_space_id is used for the selection within that dataspace.
 *
 *          The behavior of the library for the various combinations of
 *          valid dataspace IDs and #H5S_ALL for the mem_space_id and
 *          thefile_space_id parameters is described below:
 *
 *          <table>
 *          <tr><th>\c mem_space_id</th>
 *          <th>\c file_space_id</th>
 *          <th>Behavior</th></tr>
 *          <tr><td>valid dataspace ID</td>
 *              <td>valid dataspace ID</td>
 *              <td>\p mem_space_id specifies the memory dataspace and the
 *                  selection within it. \p file_space_id specifies the
 *                  selection within the file dataset's dataspace.</td></tr>
 *          <tr><td>#H5S_ALL</td>
 *              <td>valid dataspace ID</td>
 *              <td>The file dataset's dataspace is used for the memory
 *                  dataspace and the selection specified with \p file_space_id
 *                  specifies the selection within it. The combination of the
 *                  file dataset's dataspace and the selection from \p
 *                  file_space_id is used for memory also. valid dataspace
 *                  ID</td></tr>
 *          <tr><td>valid dataspace ID</td>
 *              <td>#H5S_ALL</td>
 *              <td>\p mem_space_id specifies the memory dataspace and the
 *                  selection within it. The selection within the file
 *                  dataset's dataspace is set to "all" selection.</td></tr>
 *          <tr><td>#H5S_ALL</td>
 *              <td>#H5S_ALL</td>
 *              <td>The file dataset's dataspace is used for the memory
 *                  dataspace and the selection within the memory dataspace is
 *                  set to the "all" selection. The selection within the file
 *                  dataset's dataspace is set to the "all"
 *                  selection.</td></tr>
 *          </table>
 *          Setting an "all" selection indicates that the entire dataspace,
 *          as defined by the current dimensions of a dataspace, will
 *          be selected. The number of elements selected in the memory
 *          dataspace must match the number of elements selected in the
 *          file dataspace.
 *
 *          \p dxpl_id can be the constant #H5P_DEFAULT, in which
 *          case the default data transfer properties are used.
 *
 *          Writing to a dataset will fail if the HDF5 file was not opened
 *          with write access permissions.
 *
 *          If the dataset's space allocation time is set to
 *          #H5D_ALLOC_TIME_LATE or #H5D_ALLOC_TIME_INCR and the space for
 *          the dataset has not yet been allocated, that space is allocated
 *          when the first raw data is written to the dataset. Unused space
 *          in the dataset will be written with fill values at the same
 *          time if the dataset's fill time is set to #H5D_FILL_TIME_IFSET
 *          or #H5D_FILL_TIME_ALLOC.
 *
 * \par_compr_note
 *
 * \attention If a dataset's storage layout is 'compact', care must be
 *          taken when writing data to the dataset in parallel. A compact
 *          dataset's raw data is cached in memory and may be flushed
 *          to the file from any of the parallel processes, so parallel
 *          applications should always attempt to write identical data to
 *          the dataset from all processes.
 *
 * \par Example
 * \snippet H5D_examples.c update
 *
 * \since 1.0.0
 *
 * \see H5Pset_fill_time(), H5Pset_alloc_time()
 *
 */
H5_DLL herr_t H5Dwrite(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                       hid_t dxpl_id, const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Writes raw data from a set buffers to a set of datasets
 *
 * \param[in] count         Number of datasets to write to
 * \param[in] dset_id       Identifiers of the datasets to write to
 * \param[in] mem_type_id   Identifiers of the memory datatypes
 * \param[in] mem_space_id  Identifiers of the memory dataspaces
 * \param[in] file_space_id Identifiers of the datasets' dataspaces in the file
 * \param[in] dxpl_id       Identifier of a transfer property list
 * \param[in] buf           Buffers with data to be written to the file
 *
 * \return \herr_t
 *
 * \details H5Dwrite_multi() writes data to \p count datasets, whose identifiers
 *          are listed in the \p dset_id array, from multiple application memory
 *          buffers listed in the \p buf array. Data transfer properties are
 *          defined by the argument \p dxpl_id. The memory datatypes of each
 *          dataset are listed by identifier in the \p mem_type_id array. The
 *          parts of each dataset to write are listed by identifier in the \p
 *          file_space_id array, and the parts of each application memory buffer
 *          to write from are listed by identifier in the \p mem_space_id array.
 *          All array parameters have length \p count.
 *
 *          This function will produce the same results as \p count calls to
 *          H5Dwrite(). Information listed in that function's documentation
 *          about the specifics of its behaviour also apply to H5Dwrite_multi().
 *          By calling H5Dwrite_multi() instead of multiple calls to H5Dwrite(),
 *          however, the library can in some cases pass information about the
 *          entire I/O operation to the file driver, which can improve
 *          performance.
 *
 *          All datasets must be in the same HDF5 file, and each unique dataset
 *          may only be listed once. If this function is called collectively in
 *          parallel, each rank must pass exactly the same list of datasets in
 *          \p dset_id , though the other parameters may differ.
 *
 * \since 1.14.0
 *
 * \see H5Dwrite()
 *
 */
H5_DLL herr_t H5Dwrite_multi(size_t count, hid_t dset_id[], hid_t mem_type_id[], hid_t mem_space_id[],
                             hid_t file_space_id[], hid_t dxpl_id, const void *buf[]);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dwrite}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Dwrite_async(const char *app_file, const char *app_func, unsigned app_line, hid_t dset_id,
                             hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id,
                             const void *buf, hid_t es_id);
#else
H5_DLL herr_t H5Dwrite_async(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                             hid_t dxpl_id, const void *buf, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dwrite_multi}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Dwrite_multi_async(const char *app_file, const char *app_func, unsigned app_line,
                                   size_t count, hid_t dset_id[], hid_t mem_type_id[], hid_t mem_space_id[],
                                   hid_t file_space_id[], hid_t dxpl_id, const void *buf[], hid_t es_id);
#else
H5_DLL herr_t H5Dwrite_multi_async(size_t count, hid_t dset_id[], hid_t mem_type_id[], hid_t mem_space_id[],
                                   hid_t file_space_id[], hid_t dxpl_id, const void *buf[], hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Writes a raw data chunk from a buffer directly to a dataset in a file
 *
 * \dset_id
 * \dxpl_id
 * \param[in]  filters  Mask for identifying the filters in use
 * \param[in]  offset   Logical position of the chunk's first element in the
 *                      dataspace
 * \param[in]  data_size    Size of the actual data to be written in bytes
 * \param[in]  buf          Buffer containing data to be written to the chunk
 *
 * \return \herr_t
 *
 * \details H5Dwrite_chunk() writes a raw data chunk as specified
 *          by its logical offset \p offset in a chunked dataset \p dset_id
 *          from the application memory buffer \p buf to the dataset in
 *          the file. Typically, the data in \p buf is preprocessed in
 *          memory by a custom transformation, such as compression. The
 *          chunk will bypass the library's internal data transfer
 *          pipeline, including filters, and will be written directly to
 *          the file. Only one chunk can be written with this function.
 *
 *          \p filters is a mask providing a record of which filters are
 *          used with the chunk. The default value of the mask is
 *          zero (0), indicating that all enabled filters are applied. A
 *          filter is skipped if the bit corresponding to the filter's
 *          position in the pipeline (0 ≤ position < 32) is turned on.
 *          This mask is saved with the chunk in the file.
 *
 *          \p offset is an array specifying the logical position of the
 *          first element of the chunk in the dataset's dataspace. The
 *          length of the offset array must equal the number of dimensions,
 *          or rank, of the dataspace. The values in offset must not exceed
 *          the dimension limits and must specify a point that falls on
 *          a dataset chunk boundary.
 *
 *          \p data_size is the size in bytes of the chunk, representing
 *          the number of bytes to be read from the buffer \p buf. If the
 *          data chunk has been precompressed, \p data_size should be the
 *          size of the compressed data.
 *
 *          \p buf is the memory buffer containing data to be written to
 *          the chunk in the file.
 *
 * \attention Exercise caution when using H5Dread_chunk() and
 *          H5Dwrite_chunk(), as they read and write data chunks directly
 *          in a file. H5Dwrite_chunk() bypasses hyperslab selection, the
 *          conversion of data from one datatype to another, and the filter
 *          pipeline to write the chunk. Developers should have experience
 *          with these processes before using this function.
 *
 * \note    H5Dread_chunk() and H5Dwrite_chunk() are currently not supported
 *          with parallel HDF5 and do not support variable-length types.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Dwrite_chunk(hid_t dset_id, hid_t dxpl_id, uint32_t filters, const hsize_t *offset,
                             size_t data_size, const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Reads a raw data chunk directly from a dataset in a file into
 * a buffer
 *
 * \dset_id
 * \dxpl_id
 * \param[in]  offset   Logical position of the chunk's first element in the
 *                      dataspace
 * \param[in,out]  filters  Mask for identifying the filters in use
 * \param[out]  buf     Buffer containing data to be read from the chunk
 *
 * \return \herr_t
 *
 * \details H5Dread_chunk() reads a raw data chunk as specified by
 *          its logical offset \p offset in a chunked dataset \p dset_id
 *          from the dataset in the file into the application memory
 *          buffer \p buf. The data in \p buf is read directly from the
 *          file bypassing the library's internal data transfer pipeline,
 *          including filters.
 *
 *          \p offset is an array specifying the logical position of the
 *          first element of the chunk in the dataset's dataspace. The
 *          length of the \p offset array must equal the number of dimensions,
 *          or rank, of the dataspace. The values in \p offset must not exceed
 *          the dimension limits and must specify a point that falls on
 *          a dataset chunk boundary.
 *
 *          The mask \p filters indicates which filters were used when the
 *          chunk was written. A zero value (all bits 0) indicates that all
 *          enabled filters are applied on the chunk. A filter is skipped if
 *          the bit corresponding to the filter's position in the pipeline
 *          (0 ≤ position < 32) is turned on.
 *
 *          \p buf is the memory buffer containing the chunk read from
 *          the dataset in the file.
 *
 * \attention Exercise caution when using H5Dread_chunk() and
 *          H5Dwrite_chunk(), as they read and write data chunks directly
 *          in a file. H5Dwrite_chunk() bypasses hyperslab selection, the
 *          conversion of data from one datatype to another, and the filter
 *          pipeline to write the chunk. Developers should have experience
 *          with these processes before using this function. Please see
 *          \ref subsec_hldo_direct_chunk_using for more information.
 *
 * \note H5Dread_chunk() and H5Dwrite_chunk() are currently not supported
 *       with parallel HDF5 and do not support variable-length datatypes.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Dread_chunk(hid_t dset_id, hid_t dxpl_id, const hsize_t *offset, uint32_t *filters,
                            void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Iterates over all selected elements in a dataspace
 *
 * \param[in,out] buf Buffer containing the elements to iterate over
 * \type_id
 * \space_id
 * \param[in] op Function pointer
 * \param[in,out] operator_data User-defined data
 *
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \details H5Diterate() iterates over all the data elements
 *          in the memory buffer \p buf, executing the callback function
 *          \p op once for each such data element.
 *
 * \attention Unlike other HDF5 iterators, this iteration operation cannot
 *            be restarted at the point of exit; a second H5Diterate()
 *            call will always restart at the beginning.
 *
 * \warning   Modifying the selection of \p space_id during iteration
 *            will lead to undefined behavior.
 *
 * \callback_note
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Diterate(void *buf, hid_t type_id, hid_t space_id, H5D_operator_t op, void *operator_data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Determines the number of bytes required to store variable-length
 *        (VL) data
 *
 * \dset_id
 * \type_id
 * \space_id
 * \param[out] size Size in bytes of the memory buffer required to store
 *        the VL data
 *
 * \return \herr_t
 *
 * \details H5Dvlen_get_buf_size() determines the number of bytes
 *          required to store the VL data from the dataset, using \p
 *          space_id for the selection in the dataset on disk and the \p
 *          type_id for the memory representation of the VL data in memory.
 *          \p size is returned with the number of bytes required to store
 *          the VL data in memory.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Dvlen_get_buf_size(hid_t dset_id, hid_t type_id, hid_t space_id, hsize_t *size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Fills dataspace elements with a fill value in a memory buffer
 *
 * \param[in] fill          Pointer to the fill value to be used
 * \param[in] fill_type_id  Fill value datatype identifier
 * \param[in,out] buf       Pointer to the memory buffer containing the
 *                          selection to be filled
 * \param[in] buf_type_id   Datatype of dataspace elements to be filled
 * \space_id
 *
 * \return \herr_t
 *
 * \details H5Dfill() fills the dataspace selection, \p space_id, in memory
 *          with the fill value specified in \p fill. If \p fill is NULL,
 *          a fill value of 0 (zero) is used.
 *
 *          \p fill_type_id specifies the datatype of the fill value.
 *          \p buf specifies the buffer in which the fill elements
 *          will be written. \p buf_type_id specifies the datatype of
 *          those data elements.
 *
 * \note Note that if the fill value datatype differs from the memory
 *       buffer datatype, the fill value will be converted to the memory
 *       buffer datatype before filling the selection.
 *
 * \note Applications sometimes write data only to portions of an
 *       allocated dataset. It is often useful in such cases to fill
 *       the unused space with a known fill value.
 *
 * \see H5Pset_fill_value(), H5Pget_fill_value(), H5Pfill_value_defined(),
 *      H5Pset_fill_time(), H5Pget_fill_time(), H5Pcreate(), H5Dcreate_anon()
 * \since 1.6.0
 *
 */
H5_DLL herr_t H5Dfill(const void *fill, hid_t fill_type_id, void *buf, hid_t buf_type_id, hid_t space_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Changes the sizes of a dataset's dimensions
 *
 * \dset_id
 * \param[in] size[]   Array containing the new magnitude of each dimension
 *                     of the dataset
 *
 * \return \herr_t
 *
 * \details H5Dset_extent() sets the current dimensions of the
 *          chunked dataset \p dset_id to the sizes specified in size.
 *
 *          \p size is a 1-dimensional array with n elements, where \p n is
 *          the rank of the dataset's current dataspace.
 *
 *          This function can be applied to the following datasets:
 *          - A chunked dataset with unlimited dimensions
 *          - A chunked dataset with fixed dimensions if the new dimension
 *          sizes are less than the maximum sizes set with maxdims (see
 *          H5Screate_simple())
 *          - An external dataset with unlimited dimensions
 *          - An external dataset with fixed dimensions if the new dimension
 *          sizes are less than the maximum sizes set with \p maxdims
 *
 *          Note that external datasets are always contiguous and can be
 *          extended only along the first dimension.
 *
 *          Space on disk is immediately allocated for the new dataset extent if
 *          the dataset's space allocation time is set to #H5D_ALLOC_TIME_EARLY.
 *
 *          Fill values will be written to the dataset in either of the
 *          following situations, but not otherwise:
 *
 *          - If the dataset's fill time is set to #H5D_FILL_TIME_IFSET and a
 *            fill value is defined (see H5Pset_fill_time() and
 *            H5Pset_fill_value())
 *          - If the dataset's fill time is set to #H5D_FILL_TIME_ALLOC
 *            (see H5Pset_alloc_time())
 *
 * \note If the sizes specified in \p size array are smaller than the dataset's
 *       current dimension sizes, H5Dset_extent() will reduce the dataset's
 *       dimension sizes to the specified values. It is the user application's
 *       responsibility to ensure that valuable data is not lost as
 *       H5Dset_extent() does not check.
 *
 * \note Except for external datasets, H5Dset_extent() is for use with
 *       chunked datasets only, not contiguous datasets.
 *
 * \note A call to H5Dset_extent() affects the dataspace of a dataset.  If a
 *       dataspace handle was opened for a dataset prior to a call to
 *       H5Dset_extent() then that dataspace handle will no longer reflect the
 *       correct dataspace extent of the dataset. H5Dget_space() must be called
 *       (after closing the previous handle) to obtain the current dataspace
 *       extent.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Dset_extent(hid_t dset_id, const hsize_t size[]);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dset_extent}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Dset_extent_async(const char *app_file, const char *app_func, unsigned app_line,
                                  hid_t dset_id, const hsize_t size[], hid_t es_id);
#else
H5_DLL herr_t H5Dset_extent_async(hid_t dset_id, const hsize_t size[], hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Flushes all buffers associated with a dataset to disk
 *
 * \dset_id
 *
 * \return \herr_t
 *
 * \details H5Dflush() causes all buffers associated with a
 *          dataset to be immediately flushed to disk without removing
 *          the data from the cache.
 *
 *          \note HDF5 does not possess full control over buffering.
 *          H5Dflush() flushes the internal HDF5 buffers and then asks the
 *          operating system (the OS) to flush the system buffers for the
 *          open files. After that, the OS is responsible for ensuring
 *          that the data is actually flushed to disk.
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5Dflush(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Refreshes all buffers associated with a dataset
 *
 * \dset_id
 *
 * \return \herr_t
 *
 * \details H5Drefresh() causes all buffers associated with a
 *          dataset to be cleared and immediately re-loaded with updated
 *          contents from disk.
 *
 *          This function essentially closes the dataset, evicts all
 *          metadata associated with it from the cache, and then re-opens
 *          the dataset. The reopened dataset is automatically re-registered
 *          with the same identifier.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Drefresh(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Scatters data into a selection within a memory buffer
 *
 * \param[in]  op       Callback function which provides data to be scattered
 * \param[in]  op_data  User-defined pointer to data required by op
 * \param[in]  type_id  Identifier for the datatype describing the data in
 *                      both the source and destination buffers
 * \param[in]  dst_space_id    Identifier for the dataspace for destination
 * \param[out] dst_buf  Destination buffer which the data will be scattered to
 *
 * \return \herr_t
 *
 * \details H5Dscatter() retrieves data from the supplied callback
 *          \p op and scatters it to the supplied buffer \p dst_buf in a
 *          manner similar to data being written to a dataset.
 *
 *          \p dst_space_id is a dataspace that defines the extent of \p
 *          dst_buf and the selection within it to scatter the data to.
 *
 *          \p type_id is the datatype of the data to be scattered in both
 *          the source and destination buffers.
 *
 *          \p dst_buf must be at least as large as the number of elements
 *          in the extent of \p dst_space_id times the size in bytes of
 *          \p type_id.
 *
 *          To retrieve the data to be scattered, H5Dscatter() repeatedly
 *          calls \p op, which should return a valid source buffer, until
 *          enough data to fill the selection has been retrieved.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Dscatter(H5D_scatter_func_t op, void *op_data, hid_t type_id, hid_t dst_space_id,
                         void *dst_buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Gathers data from a selection within a memory buffer
 * raw data chunk in a dataset
 *
 * \param[in]  src_space_id  Dataspace identifier for the source buffer
 * \param[in]  src_buf   Source buffer which the data will be gathered from
 * \param[in]  type_id   Datatype identifier for the source
 * \param[in]  dst_buf_size   Size in bytes of \p dst_buf
 * \param[out] dst_buf   Destination buffer for the gathered data
 * \param[in]  op   Callback function which handles the gathered data
 * \param[in]  op_data   User-defined pointer to data required by \p op
 *
 * \return \herr_t
 *
 * \details H5Dgather() retrieves data from a selection within the supplied
 *          buffer src_buf and passes it to the supplied callback function
 *          \p op in a contiguous form.
 *
 *          The dataspace \p src_space_id describes both the dimensions of
 *          the source buffer and the selection within the source buffer
 *          to gather data from.
 *
 *          \p src_buf must be at least the size of the gathered data, that
 *          is, the number of elements in the extent of \p src_space_id
 *          times the size in bytes of \p type_id.
 *
 *          The datatype \p type_id describes the data in both the source
 *          and destination buffers. This information is used to calculate
 *          the element size.
 *
 *          The data is gathered into \p dst_buf, which needs to be large
 *          enough to hold all the data if the callback function \p op is
 *          not provided.
 *
 *          \p op is a callback function that handles the gathered data.
 *          It is optional if \p dst_buf is large enough to hold all of the
 *          gathered data; required otherwise.
 *
 *          If no callback function is provided, H5Dgather() simply gathers
 *          the data into \p dst_buf and returns. If a callback function is
 *          provided, H5Dgather() repeatedly gathers up to \p dst_buf_size
 *          bytes to process the serialized data.
 *
 *          The callback function \p op should process, store, or otherwise,
 *          make use of the data returned in \p dst_buf before it returns,
 *          because the buffer will be overwritten unless it is the last
 *          call to the callback. This function will be repeatedly called
 *          until all gathered elements have been passed to the callback
 *          in \p dst_buf. The callback function should return zero (0)
 *          to indicate success, and a negative value to indicate failure.
 *
 * \callback_note
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Dgather(hid_t src_space_id, const void *src_buf, hid_t type_id, size_t dst_buf_size,
                        void *dst_buf, H5D_gather_func_t op, void *op_data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Closes the specified dataset
 *
 * \dset_id
 *
 * \return \herr_t
 *
 * \details H5Dclose() terminates access to a dataset via the identifier
 *          \p dset_id and releases the underlying resources.
 *
 * \par Example
 * \snippet H5D_examples.c read
 *
 * \since 1.0.0
 *
 * \see H5Dcreate2(), H5Dopen2()
 *
 */
H5_DLL herr_t H5Dclose(hid_t dset_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Dclose}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Dclose_async(const char *app_file, const char *app_func, unsigned app_line, hid_t dset_id,
                             hid_t es_id);
#else
H5_DLL herr_t H5Dclose_async(hid_t dset_id, hid_t es_id);
#endif
/// \cond DEV
/* Internal API routines */
H5_DLL herr_t H5Ddebug(hid_t dset_id);
H5_DLL herr_t H5Dformat_convert(hid_t dset_id);
H5_DLL herr_t H5Dget_chunk_index_type(hid_t did, H5D_chunk_index_t *idx_type);
/// \endcond

/// \cond DEV
/* API Wrappers for async routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5D_MODULE
#define H5Dcreate_async(...)      H5Dcreate_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dopen_async(...)        H5Dopen_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dget_space_async(...)   H5Dget_space_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dread_async(...)        H5Dread_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dread_multi_async(...)  H5Dread_multi_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dwrite_async(...)       H5Dwrite_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dwrite_multi_async(...) H5Dwrite_multi_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dset_extent_async(...)  H5Dset_extent_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Dclose_async(...)       H5Dclose_async(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 *      be passed in by language wrapper or library layer on top of HDF5.
 */
#define H5Dcreate_async_wrap     H5_NO_EXPAND(H5Dcreate_async)
#define H5Dopen_async_wrap       H5_NO_EXPAND(H5Dopen_async)
#define H5Dget_space_async_wrap  H5_NO_EXPAND(H5Dget_space_async)
#define H5Dread_async_wrap       H5_NO_EXPAND(H5Dread_async)
#define H5Dwrite_async_wrap      H5_NO_EXPAND(H5Dwrite_async)
#define H5Dset_extent_async_wrap H5_NO_EXPAND(H5Dset_extent_async)
#define H5Dclose_async_wrap      H5_NO_EXPAND(H5Dclose_async)
#endif /* H5D_MODULE */
/// \endcond

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/** v1 B-tree index \since 1.10.0 */
#define H5D_CHUNK_BTREE H5D_CHUNK_IDX_BTREE

/* Formerly used to support the H5DOread/write_chunk() API calls.
 * These symbols are no longer used in the library.
 */
/* Property names for H5DOwrite_chunk */
#define H5D_XFER_DIRECT_CHUNK_WRITE_FLAG_NAME     "direct_chunk_flag"
#define H5D_XFER_DIRECT_CHUNK_WRITE_FILTERS_NAME  "direct_chunk_filters"
#define H5D_XFER_DIRECT_CHUNK_WRITE_OFFSET_NAME   "direct_chunk_offset"
#define H5D_XFER_DIRECT_CHUNK_WRITE_DATASIZE_NAME "direct_chunk_datasize"
/* Property names for H5DOread_chunk */
#define H5D_XFER_DIRECT_CHUNK_READ_FLAG_NAME    "direct_chunk_read_flag"
#define H5D_XFER_DIRECT_CHUNK_READ_OFFSET_NAME  "direct_chunk_read_offset"
#define H5D_XFER_DIRECT_CHUNK_READ_FILTERS_NAME "direct_chunk_read_filters"

/* Typedefs */

/* Function prototypes */
/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Creates a dataset at the specified location
 *
 * \fgdta_loc_id
 * \param[in] name Name of the dataset to create
 * \type_id
 * \space_id
 * \dcpl_id
 *
 * \return \hid_t{dataset}
 *
 * \deprecation_note{H5Dcreate2() or the macro H5Dcreate()}
 *
 * \details H5Dcreate1() creates a data set with a name, \p name, in the
 *          location specified by the identifier \p loc_id. \p loc_id may be a
 *          file, group, dataset, named datatype or attribute.  If an attribute,
 *          dataset, or named datatype is specified for \p loc_id then the
 *          dataset will be created at the location where the attribute,
 *          dataset, or named datatype is attached.
 *
 *          \p name can be a relative path based at \p loc_id or an absolute
 *          path from the root of the file. Use of this function requires that
 *          any intermediate groups specified in the path already exist.
 *
 *          The dataset's datatype and dataspace are specified by \p type_id and
 *          \p space_id, respectively. These are the datatype and dataspace of
 *          the dataset as it will exist in the file, which may differ from the
 *          datatype and dataspace in application memory.
 *
 *          Names within a group are unique: H5Dcreate1() will return an error
 *          if a link with the name specified in name already exists at the
 *          location specified in \p loc_id.
 *
 *          As is the case for any object in a group, the length of a dataset
 *          name is not limited.
 *
 *          \p dcpl_id is an #H5P_DATASET_CREATE property list created with \p
 *          H5reate1() and initialized with various property list functions
 *          described in Property List Interface.
 *
 *          H5Dcreate() and H5Dcreate_anon() return an error if the dataset's
 *          datatype includes a variable-length (VL) datatype and the fill value
 *          is undefined, i.e., set to \c NULL in the dataset creation property
 *          list. Such a VL datatype may be directly included, indirectly
 *          included as part of a compound or array datatype, or indirectly
 *          included as part of a nested compound or array datatype.
 *
 *          H5Dcreate() and H5Dcreate_anon() return a dataset identifier for
 *          success or a negative value for failure. The dataset identifier
 *          should eventually be closed by calling H5Dclose() to release the
 *          resources it uses.
 *
 *          See H5Dcreate_anon() for a discussion of the differences between
 *          H5Dcreate() and H5Dcreate_anon().
 *
 *          The HDF5 library provides flexible means of specifying a fill value,
 *          of specifying when space will be allocated for a dataset, and of
 *          specifying when fill values will be written to a dataset.
 *
 * \version 1.8.0 Function H5Dcreate() renamed to H5Dcreate1() and deprecated in this release.
 * \since 1.0.0
 *
 * \see H5Dopen2(), H5Dclose(), H5Tset_size()
 *
 */
H5_DLL hid_t H5Dcreate1(hid_t loc_id, const char *name, hid_t type_id, hid_t space_id, hid_t dcpl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Opens an existing dataset
 *
 * \fgdta_loc_id
 * \param[in] name Name of the dataset to access
 *
 * \return \hid_t{dataset}
 *
 * \deprecation_note{H5Dopen2() or the macro H5Dopen()}
 *
 * \details H5Dopen1() opens an existing dataset for access at the location
 *          specified by \p loc_id.  \p loc_id may be a file, group, dataset,
 *          named datatype or attribute.  If an attribute, dataset, or named
 *          datatype is specified for loc_id then the dataset will be opened at
 *          the location where the attribute, dataset, or named datatype is
 *          attached. name is a dataset name and is used to identify the dataset
 *          in the file.
 *
 *          A dataset opened with this function should be closed with H5Dclose()
 *          when the dataset is no longer needed so that resource leaks will not
 *          develop.
 *
 * \version 1.8.0 Function H5Dopen() renamed to H5Dopen1() and deprecated in this release.
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Dopen1(hid_t loc_id, const char *name);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Extends a dataset
 *
 * \dset_id
 * \param[in] size Array containing the new size of each dimension
 *
 * \return \herr_t
 *
 * \deprecation_note{H5Dset_extent()}
 *
 * \details H5Dextend() verifies that the dataset is at least of size \p size,
 *          extending it if necessary. The length of \p size is the same as
 *          that of the dataspace of the dataset being changed.
 *
 *          This function can be applied to the following datasets:
 *          \li Any dataset with unlimited dimensions
 *          \li A dataset with fixed dimensions if the current dimension sizes
 *              are less than the maximum sizes set with \c maxdims
 *              (see H5Screate_simple())
 *
 *          Space on disk is immediately allocated for the new dataset extent if
 *          the dataset's space allocation time is set to
 *          #H5D_ALLOC_TIME_EARLY. Fill values will be written to the dataset if
 *          the dataset's fill time is set to #H5D_FILL_TIME_IFSET or
 *          #H5D_FILL_TIME_ALLOC. (See H5Pset_fill_time() and
 *          H5Pset_alloc_time().)
 *
 *          This function ensures that the dataset dimensions are of at least
 *          the sizes specified in size. The function H5Dset_extent() must be
 *          used if the dataset dimension sizes are to be reduced.
 *
 * \version 1.8.0 Function deprecated in this release. Parameter size
 *                syntax changed to \TText{const hsize_t size[]} in this release.
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Dextend(hid_t dset_id, const hsize_t size[]);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5D
 *
 * \brief Reclaims variable-length (VL) datatype memory buffers
 *
 * \type_id
 * \space_id
 * \dxpl_id
 * \param[in] buf Pointer to the buffer to be reclaimed
 *
 * \return \herr_t
 *
 * \deprecation_note{H5Treclaim()}
 *
 * \details H5Dvlen_reclaim() reclaims memory buffers created to store VL
 *          datatypes.
 *
 *          The \p type_id must be the datatype stored in the buffer. The \p
 *          space_id describes the selection for the memory buffer to free the
 *          VL datatypes within. The \p dxpl_id is the dataset transfer property
 *          list that was used for the I/O transfer to create the buffer. And
 *          \p buf is the pointer to the buffer to be reclaimed.
 *
 *          The VL structures (\ref hvl_t) in the user's buffer are modified to
 *          zero out the VL information after the memory has been reclaimed.
 *
 *          If nested VL datatypes were used to create the buffer, this routine
 *          frees them from the bottom up, releasing all the memory without
 *          creating memory leaks.
 *
 * \version 1.12.0 Function was deprecated
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Dvlen_reclaim(hid_t type_id, hid_t space_id, hid_t dxpl_id, void *buf);

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#ifdef __cplusplus
}
#endif
#endif /* H5Dpublic_H */
