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
 * Purpose:	The public header file for the native VOL connector.
 */

#ifndef H5VLnative_H
#define H5VLnative_H

/* Public headers needed by this file */
#include "H5Apublic.h"  /* Attributes                           */
#include "H5VLpublic.h" /* Virtual Object Layer                 */

/*****************/
/* Public Macros */
/*****************/

/* Identifier for the native VOL connector */
#define H5VL_NATIVE (H5VL_native_register())

/* Characteristics of the native VOL connector */
#define H5VL_NATIVE_NAME    "native"
#define H5VL_NATIVE_VALUE   H5_VOL_NATIVE /* enum value */
#define H5VL_NATIVE_VERSION 0

/* Values for VOL connector attribute optional VOL operations */
/* NOTE: If new values are added here, the H5VL__native_introspect_opt_query
 *      routine must be updated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS
#define H5VL_NATIVE_ATTR_ITERATE_OLD 0 /* H5Aiterate (deprecated routine) */
#endif                                 /* H5_NO_DEPRECATED_SYMBOLS */
/* NOTE: If values over 1023 are added, the H5VL_RESERVED_NATIVE_OPTIONAL macro
 *      must be updated.
 */

#ifndef H5_NO_DEPRECATED_SYMBOLS
/* Parameters for attribute 'iterate old' operation */
typedef struct H5VL_native_attr_iterate_old_t {
    hid_t           loc_id;
    unsigned       *attr_num;
    H5A_operator1_t op;
    void           *op_data;
} H5VL_native_attr_iterate_old_t;

/* Parameters for native connector's attribute 'optional' operations */
typedef union H5VL_native_attr_optional_args_t {
    /* H5VL_NATIVE_ATTR_ITERATE_OLD */
    H5VL_native_attr_iterate_old_t iterate_old;
} H5VL_native_attr_optional_args_t;
#endif /* H5_NO_DEPRECATED_SYMBOLS */

/* Values for native VOL connector dataset optional VOL operations */
/* NOTE: If new values are added here, the H5VL__native_introspect_opt_query
 *      routine must be updated.
 */
#define H5VL_NATIVE_DATASET_FORMAT_CONVERT          0  /* H5Dformat_convert (internal) */
#define H5VL_NATIVE_DATASET_GET_CHUNK_INDEX_TYPE    1  /* H5Dget_chunk_index_type      */
#define H5VL_NATIVE_DATASET_GET_CHUNK_STORAGE_SIZE  2  /* H5Dget_chunk_storage_size    */
#define H5VL_NATIVE_DATASET_GET_NUM_CHUNKS          3  /* H5Dget_num_chunks            */
#define H5VL_NATIVE_DATASET_GET_CHUNK_INFO_BY_IDX   4  /* H5Dget_chunk_info            */
#define H5VL_NATIVE_DATASET_GET_CHUNK_INFO_BY_COORD 5  /* H5Dget_chunk_info_by_coord   */
#define H5VL_NATIVE_DATASET_CHUNK_READ              6  /* H5Dchunk_read                */
#define H5VL_NATIVE_DATASET_CHUNK_WRITE             7  /* H5Dchunk_write               */
#define H5VL_NATIVE_DATASET_GET_VLEN_BUF_SIZE       8  /* H5Dvlen_get_buf_size         */
#define H5VL_NATIVE_DATASET_GET_OFFSET              9  /* H5Dget_offset                */
#define H5VL_NATIVE_DATASET_CHUNK_ITER              10 /* H5Dchunk_iter                */
/* NOTE: If values over 1023 are added, the H5VL_RESERVED_NATIVE_OPTIONAL macro
 *      must be updated.
 */

/* Parameters for native connector's dataset 'chunk read' operation */
typedef struct H5VL_native_dataset_chunk_read_t {
    const hsize_t *offset;
    uint32_t       filters;
    void          *buf;
} H5VL_native_dataset_chunk_read_t;

/* Parameters for native connector's dataset 'chunk write' operation */
typedef struct H5VL_native_dataset_chunk_write_t {
    const hsize_t *offset;
    uint32_t       filters;
    uint32_t       size;
    const void    *buf;
} H5VL_native_dataset_chunk_write_t;

/* Parameters for native connector's dataset 'get vlen buf size' operation */
typedef struct H5VL_native_dataset_get_vlen_buf_size_t {
    hid_t    type_id;
    hid_t    space_id;
    hsize_t *size; /* Size of variable-length data buffer (OUT) */
} H5VL_native_dataset_get_vlen_buf_size_t;

/* Parameters for native connector's dataset 'get chunk storage size' operation */
typedef struct H5VL_native_dataset_get_chunk_storage_size_t {
    const hsize_t *offset; /* Offset of chunk */
    hsize_t       *size;   /* Size of chunk (OUT) */
} H5VL_native_dataset_get_chunk_storage_size_t;

/* Parameters for native connector's dataset 'get num chunks' operation */
typedef struct H5VL_native_dataset_get_num_chunks_t {
    hid_t    space_id; /* Space selection */
    hsize_t *nchunks;  /* # of chunk for space selection (OUT) */
} H5VL_native_dataset_get_num_chunks_t;

/* Parameters for native connector's dataset 'get chunk info by idx' operation */
typedef struct H5VL_native_dataset_get_chunk_info_by_idx_t {
    hid_t     space_id;    /* Space selection */
    hsize_t   chk_index;   /* Chunk index within space */
    hsize_t  *offset;      /* Chunk coordinates (OUT) */
    unsigned *filter_mask; /* Filter mask for chunk (OUT) */
    haddr_t  *addr;        /* Address of chunk in file (OUT) */
    hsize_t  *size;        /* Size of chunk in file (OUT) */
} H5VL_native_dataset_get_chunk_info_by_idx_t;

/* Parameters for native connector's dataset 'get chunk info by coord' operation */
typedef struct H5VL_native_dataset_get_chunk_info_by_coord_t {
    const hsize_t *offset;      /* Chunk coordinates */
    unsigned      *filter_mask; /* Filter mask for chunk (OUT) */
    haddr_t       *addr;        /* Address of chunk in file (OUT) */
    hsize_t       *size;        /* Size of chunk in file (OUT) */
} H5VL_native_dataset_get_chunk_info_by_coord_t;

/* Parameters for native connector's dataset 'optional' operations */
typedef union H5VL_native_dataset_optional_args_t {
    /* H5VL_NATIVE_DATASET_FORMAT_CONVERT */
    /* No args */

    /* H5VL_NATIVE_DATASET_GET_CHUNK_INDEX_TYPE */
    struct {
        H5D_chunk_index_t *idx_type; /* Type of chunk index (OUT) */
    } get_chunk_idx_type;

    /* H5VL_NATIVE_DATASET_GET_CHUNK_STORAGE_SIZE */
    H5VL_native_dataset_get_chunk_storage_size_t get_chunk_storage_size;

    /* H5VL_NATIVE_DATASET_GET_NUM_CHUNKS */
    H5VL_native_dataset_get_num_chunks_t get_num_chunks;

    /* H5VL_NATIVE_DATASET_GET_CHUNK_INFO_BY_IDX */
    H5VL_native_dataset_get_chunk_info_by_idx_t get_chunk_info_by_idx;

    /* H5VL_NATIVE_DATASET_GET_CHUNK_INFO_BY_COORD */
    H5VL_native_dataset_get_chunk_info_by_coord_t get_chunk_info_by_coord;

    /* H5VL_NATIVE_DATASET_CHUNK_READ */
    H5VL_native_dataset_chunk_read_t chunk_read;

    /* H5VL_NATIVE_DATASET_CHUNK_WRITE */
    H5VL_native_dataset_chunk_write_t chunk_write;

    /* H5VL_NATIVE_DATASET_GET_VLEN_BUF_SIZE */
    H5VL_native_dataset_get_vlen_buf_size_t get_vlen_buf_size;

    /* H5VL_NATIVE_DATASET_GET_OFFSET */
    struct {
        haddr_t *offset; /* Contiguous dataset's offset in the file (OUT) */
    } get_offset;

    /* H5VL_NATIVE_DATASET_CHUNK_ITER */
    struct {
        H5D_chunk_iter_op_t op;      /* Chunk iteration callback */
        void               *op_data; /* Context to pass to iteration callback */
    } chunk_iter;

} H5VL_native_dataset_optional_args_t;

/* Values for native VOL connector file optional VOL operations */
/* NOTE: If new values are added here, the H5VL__native_introspect_opt_query
 *      routine must be updated.
 */
#define H5VL_NATIVE_FILE_CLEAR_ELINK_CACHE            0  /* H5Fclear_elink_file_cache            */
#define H5VL_NATIVE_FILE_GET_FILE_IMAGE               1  /* H5Fget_file_image                    */
#define H5VL_NATIVE_FILE_GET_FREE_SECTIONS            2  /* H5Fget_free_sections                 */
#define H5VL_NATIVE_FILE_GET_FREE_SPACE               3  /* H5Fget_freespace                     */
#define H5VL_NATIVE_FILE_GET_INFO                     4  /* H5Fget_info1/2                       */
#define H5VL_NATIVE_FILE_GET_MDC_CONF                 5  /* H5Fget_mdc_config                    */
#define H5VL_NATIVE_FILE_GET_MDC_HR                   6  /* H5Fget_mdc_hit_rate                  */
#define H5VL_NATIVE_FILE_GET_MDC_SIZE                 7  /* H5Fget_mdc_size                      */
#define H5VL_NATIVE_FILE_GET_SIZE                     8  /* H5Fget_filesize                      */
#define H5VL_NATIVE_FILE_GET_VFD_HANDLE               9  /* H5Fget_vfd_handle                    */
#define H5VL_NATIVE_FILE_RESET_MDC_HIT_RATE           10 /* H5Freset_mdc_hit_rate_stats          */
#define H5VL_NATIVE_FILE_SET_MDC_CONFIG               11 /* H5Fset_mdc_config                    */
#define H5VL_NATIVE_FILE_GET_METADATA_READ_RETRY_INFO 12 /* H5Fget_metadata_read_retry_info      */
#define H5VL_NATIVE_FILE_START_SWMR_WRITE             13 /* H5Fstart_swmr_write                  */
#define H5VL_NATIVE_FILE_START_MDC_LOGGING            14 /* H5Fstart_mdc_logging                 */
#define H5VL_NATIVE_FILE_STOP_MDC_LOGGING             15 /* H5Fstop_mdc_logging                  */
#define H5VL_NATIVE_FILE_GET_MDC_LOGGING_STATUS       16 /* H5Fget_mdc_logging_status            */
#define H5VL_NATIVE_FILE_FORMAT_CONVERT               17 /* H5Fformat_convert                    */
#define H5VL_NATIVE_FILE_RESET_PAGE_BUFFERING_STATS   18 /* H5Freset_page_buffering_stats        */
#define H5VL_NATIVE_FILE_GET_PAGE_BUFFERING_STATS     19 /* H5Fget_page_buffering_stats          */
#define H5VL_NATIVE_FILE_GET_MDC_IMAGE_INFO           20 /* H5Fget_mdc_image_info                */
#define H5VL_NATIVE_FILE_GET_EOA                      21 /* H5Fget_eoa                           */
#define H5VL_NATIVE_FILE_INCR_FILESIZE                22 /* H5Fincrement_filesize                */
#define H5VL_NATIVE_FILE_SET_LIBVER_BOUNDS            23 /* H5Fset_latest_format/libver_bounds   */
#define H5VL_NATIVE_FILE_GET_MIN_DSET_OHDR_FLAG       24 /* H5Fget_dset_no_attrs_hint            */
#define H5VL_NATIVE_FILE_SET_MIN_DSET_OHDR_FLAG       25 /* H5Fset_dset_no_attrs_hint            */
#ifdef H5_HAVE_PARALLEL
#define H5VL_NATIVE_FILE_GET_MPI_ATOMICITY 26 /* H5Fget_mpi_atomicity                 */
#define H5VL_NATIVE_FILE_SET_MPI_ATOMICITY 27 /* H5Fset_mpi_atomicity                 */
#endif
#define H5VL_NATIVE_FILE_POST_OPEN 28 /* Adjust file after open, with wrapping context */
/* NOTE: If values over 1023 are added, the H5VL_RESERVED_NATIVE_OPTIONAL macro
 *      must be updated.
 */

/* Parameters for native connector's file 'get file image' operation */
typedef struct H5VL_native_file_get_file_image_t {
    size_t  buf_size;  /* Size of file image buffer */
    void   *buf;       /* Buffer for file image (OUT) */
    size_t *image_len; /* Size of file image (OUT) */
} H5VL_native_file_get_file_image_t;

/* Parameters for native connector's file 'get free sections' operation */
typedef struct H5VL_native_file_get_free_sections_t {
    H5F_mem_t        type;       /* Type of file memory to query */
    H5F_sect_info_t *sect_info;  /* Array of sections (OUT) */
    size_t           nsects;     /* Size of section array */
    size_t          *sect_count; /* Actual # of sections of type (OUT) */
} H5VL_native_file_get_free_sections_t;

/* Parameters for native connector's file 'get freespace' operation */
typedef struct H5VL_native_file_get_freespace_t {
    hsize_t *size; /* Size of free space (OUT) */
} H5VL_native_file_get_freespace_t;

/* Parameters for native connector's file 'get info' operation */
typedef struct H5VL_native_file_get_info_t {
    H5I_type_t   type;  /* Type of object */
    H5F_info2_t *finfo; /* Pointer to file info (OUT) */
} H5VL_native_file_get_info_t;

/* Parameters for native connector's file 'get metadata cache size' operation */
typedef struct H5VL_native_file_get_mdc_size_t {
    size_t   *max_size;        /* Maximum amount of cached data (OUT) */
    size_t   *min_clean_size;  /* Minimum amount of cached data to keep clean (OUT) */
    size_t   *cur_size;        /* Current amount of cached data (OUT) */
    uint32_t *cur_num_entries; /* Current # of cached entries (OUT) */
} H5VL_native_file_get_mdc_size_t;

/* Parameters for native connector's file 'get VFD handle' operation */
typedef struct H5VL_native_file_get_vfd_handle_t {
    hid_t  fapl_id;
    void **file_handle; /* File handle from VFD (OUT) */
} H5VL_native_file_get_vfd_handle_t;

/* Parameters for native connector's file 'get MDC logging status' operation */
typedef struct H5VL_native_file_get_mdc_logging_status_t {
    hbool_t *is_enabled;           /* Whether logging is enabled (OUT) */
    hbool_t *is_currently_logging; /* Whether currently logging (OUT) */
} H5VL_native_file_get_mdc_logging_status_t;

/* Parameters for native connector's file 'get page buffering stats' operation */
typedef struct H5VL_native_file_get_page_buffering_stats_t {
    unsigned *accesses;  /* Metadata/raw data page access counts (OUT) */
    unsigned *hits;      /* Metadata/raw data page hit counts (OUT) */
    unsigned *misses;    /* Metadata/raw data page miss counts (OUT) */
    unsigned *evictions; /* Metadata/raw data page eviction counts (OUT) */
    unsigned *bypasses;  /* Metadata/raw data page bypass counts (OUT) */
} H5VL_native_file_get_page_buffering_stats_t;

/* Parameters for native connector's file 'get MDC image info' operation */
typedef struct H5VL_native_file_get_mdc_image_info_t {
    haddr_t *addr; /* Address of image (OUT) */
    hsize_t *len;  /* Length of image (OUT) */
} H5VL_native_file_get_mdc_image_info_t;

/* Parameters for native connector's file 'set libver bounds' operation */
typedef struct H5VL_native_file_set_libver_bounds_t {
    H5F_libver_t low;  /* Lowest version possible */
    H5F_libver_t high; /* Highest version possible */
} H5VL_native_file_set_libver_bounds_t;

/* Parameters for native connector's file 'optional' operations */
typedef union H5VL_native_file_optional_args_t {
    /* H5VL_NATIVE_FILE_CLEAR_ELINK_CACHE */
    /* No args */

    /* H5VL_NATIVE_FILE_GET_FILE_IMAGE */
    H5VL_native_file_get_file_image_t get_file_image;

    /* H5VL_NATIVE_FILE_GET_FREE_SECTIONS */
    H5VL_native_file_get_free_sections_t get_free_sections;

    /* H5VL_NATIVE_FILE_GET_FREE_SPACE */
    H5VL_native_file_get_freespace_t get_freespace;

    /* H5VL_NATIVE_FILE_GET_INFO */
    H5VL_native_file_get_info_t get_info;

    /* H5VL_NATIVE_FILE_GET_MDC_CONF */
    struct {
        H5AC_cache_config_t *config; /* Pointer to MDC config (OUT) */
    } get_mdc_config;

    /* H5VL_NATIVE_FILE_GET_MDC_HR */
    struct {
        double *hit_rate; /* Metadata cache hit rate (OUT) */
    } get_mdc_hit_rate;

    /* H5VL_NATIVE_FILE_GET_MDC_SIZE */
    H5VL_native_file_get_mdc_size_t get_mdc_size;

    /* H5VL_NATIVE_FILE_GET_SIZE */
    struct {
        hsize_t *size; /* Size of file (OUT) */
    } get_size;

    /* H5VL_NATIVE_FILE_GET_VFD_HANDLE */
    H5VL_native_file_get_vfd_handle_t get_vfd_handle;

    /* H5VL_NATIVE_FILE_RESET_MDC_HIT_RATE */
    /* No args */

    /* H5VL_NATIVE_FILE_SET_MDC_CONFIG */
    struct {
        const H5AC_cache_config_t *config; /* Pointer to new MDC config */
    } set_mdc_config;

    /* H5VL_NATIVE_FILE_GET_METADATA_READ_RETRY_INFO */
    struct {
        H5F_retry_info_t *info; /* Pointer to metadata read retry info (OUT) */
    } get_metadata_read_retry_info;

    /* H5VL_NATIVE_FILE_START_SWMR_WRITE */
    /* No args */

    /* H5VL_NATIVE_FILE_START_MDC_LOGGING */
    /* No args */

    /* H5VL_NATIVE_FILE_STOP_MDC_LOGGING */
    /* No args */

    /* H5VL_NATIVE_FILE_GET_MDC_LOGGING_STATUS */
    H5VL_native_file_get_mdc_logging_status_t get_mdc_logging_status;

    /* H5VL_NATIVE_FILE_FORMAT_CONVERT */
    /* No args */

    /* H5VL_NATIVE_FILE_RESET_PAGE_BUFFERING_STATS */
    /* No args */

    /* H5VL_NATIVE_FILE_GET_PAGE_BUFFERING_STATS */
    H5VL_native_file_get_page_buffering_stats_t get_page_buffering_stats;

    /* H5VL_NATIVE_FILE_GET_MDC_IMAGE_INFO */
    H5VL_native_file_get_mdc_image_info_t get_mdc_image_info;

    /* H5VL_NATIVE_FILE_GET_EOA */
    struct {
        haddr_t *eoa; /* End of allocated file address space (OUT) */
    } get_eoa;

    /* H5VL_NATIVE_FILE_INCR_FILESIZE */
    struct {
        hsize_t increment; /* Amount to increment file size */
    } increment_filesize;

    /* H5VL_NATIVE_FILE_SET_LIBVER_BOUNDS */
    H5VL_native_file_set_libver_bounds_t set_libver_bounds;

    /* H5VL_NATIVE_FILE_GET_MIN_DSET_OHDR_FLAG */
    struct {
        hbool_t *minimize; /* Flag whether dataset object headers are minimal (OUT) */
    } get_min_dset_ohdr_flag;

    /* H5VL_NATIVE_FILE_SET_MIN_DSET_OHDR_FLAG */
    struct {
        hbool_t minimize; /* Flag whether dataset object headers should be minimal */
    } set_min_dset_ohdr_flag;

#ifdef H5_HAVE_PARALLEL
    /* H5VL_NATIVE_FILE_GET_MPI_ATOMICITY */
    struct {
        hbool_t *flag; /* Flag whether MPI atomicity is set for files (OUT) */
    } get_mpi_atomicity;

    /* H5VL_NATIVE_FILE_SET_MPI_ATOMICITY */
    struct {
        hbool_t flag; /* Flag whether to set MPI atomicity for files */
    } set_mpi_atomicity;
#endif /* H5_HAVE_PARALLEL */

    /* H5VL_NATIVE_FILE_POST_OPEN */
    /* No args */
} H5VL_native_file_optional_args_t;

/* Values for native VOL connector group optional VOL operations */
/* NOTE: If new values are added here, the H5VL__native_introspect_opt_query
 *      routine must be updated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS
#define H5VL_NATIVE_GROUP_ITERATE_OLD 0 /* HG5Giterate (deprecated routine) */
#define H5VL_NATIVE_GROUP_GET_OBJINFO 1 /* HG5Gget_objinfo (deprecated routine) */
#endif                                  /* H5_NO_DEPRECATED_SYMBOLS */
/* NOTE: If values over 1023 are added, the H5VL_RESERVED_NATIVE_OPTIONAL macro
 *      must be updated.
 */

#ifndef H5_NO_DEPRECATED_SYMBOLS
/* Parameters for group 'iterate old' operation */
typedef struct H5VL_native_group_iterate_old_t {
    H5VL_loc_params_t loc_params; /* Location parameters for iteration */
    hsize_t           idx;        /* Index of link to begin iteration at */
    hsize_t          *last_obj;   /* Index of last link looked at (OUT) */
    H5G_iterate_t     op;         /* Group (link) operator callback */
    void             *op_data;    /* Context to pass to iterator callback */
} H5VL_native_group_iterate_old_t;

/* Parameters for group 'get objinfo' operation */
typedef struct H5VL_native_group_get_objinfo_t {
    H5VL_loc_params_t loc_params;  /* Location parameters for iteration */
    hbool_t           follow_link; /* Whether to follow links for query */
    H5G_stat_t       *statbuf;     /* Pointer to object info struct (OUT) */
} H5VL_native_group_get_objinfo_t;

/* Parameters for native connector's group 'optional' operations */
typedef union H5VL_native_group_optional_args_t {
    /* H5VL_NATIVE_GROUP_ITERATE_OLD */
    H5VL_native_group_iterate_old_t iterate_old;

    /* H5VL_NATIVE_GROUP_GET_OBJINFO */
    H5VL_native_group_get_objinfo_t get_objinfo;
} H5VL_native_group_optional_args_t;
#endif /* H5_NO_DEPRECATED_SYMBOLS */

/* Values for native VOL connector object optional VOL operations */
/* NOTE: If new values are added here, the H5VL__native_introspect_opt_query
 *      routine must be updated.
 */
#define H5VL_NATIVE_OBJECT_GET_COMMENT              0 /* H5G|H5Oget_comment, H5Oget_comment_by_name   */
#define H5VL_NATIVE_OBJECT_SET_COMMENT              1 /* H5G|H5Oset_comment, H5Oset_comment_by_name   */
#define H5VL_NATIVE_OBJECT_DISABLE_MDC_FLUSHES      2 /* H5Odisable_mdc_flushes                       */
#define H5VL_NATIVE_OBJECT_ENABLE_MDC_FLUSHES       3 /* H5Oenable_mdc_flushes                        */
#define H5VL_NATIVE_OBJECT_ARE_MDC_FLUSHES_DISABLED 4 /* H5Oare_mdc_flushes_disabled                  */
#define H5VL_NATIVE_OBJECT_GET_NATIVE_INFO          5 /* H5Oget_native_info(_by_idx, _by_name)        */
/* NOTE: If values over 1023 are added, the H5VL_RESERVED_NATIVE_OPTIONAL macro
 *      must be updated.
 */

/* Parameters for native connector's object 'get comment' operation */
typedef struct H5VL_native_object_get_comment_t {
    size_t  buf_size;    /* Size of comment buffer */
    void   *buf;         /* Buffer for comment (OUT) */
    size_t *comment_len; /* Actual size of comment (OUT) */
} H5VL_native_object_get_comment_t;

/* Parameters for object 'get native info' operation */
typedef struct H5VL_native_object_get_native_info_t {
    unsigned           fields; /* Fields to retrieve */
    H5O_native_info_t *ninfo;  /* Native info (OUT) */
} H5VL_native_object_get_native_info_t;

/* Parameters for native connector's object 'optional' operations */
typedef union H5VL_native_object_optional_args_t {
    /* H5VL_NATIVE_OBJECT_GET_COMMENT */
    H5VL_native_object_get_comment_t get_comment;

    /* H5VL_NATIVE_OBJECT_SET_COMMENT */
    struct {
        const char *comment; /* Comment string to set for the object (IN) */
    } set_comment;

    /* H5VL_NATIVE_OBJECT_DISABLE_MDC_FLUSHES */
    /* No args */

    /* H5VL_NATIVE_OBJECT_ENABLE_MDC_FLUSHES */
    /* No args */

    /* H5VL_NATIVE_OBJECT_ARE_MDC_FLUSHES_DISABLED */
    struct {
        hbool_t *flag; /* Flag whether metadata cache flushes are disabled for this object (OUT) */
    } are_mdc_flushes_disabled;

    /* H5VL_NATIVE_OBJECT_GET_NATIVE_INFO */
    H5VL_native_object_get_native_info_t get_native_info;
} H5VL_native_object_optional_args_t;

/*******************/
/* Public Typedefs */
/*******************/

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

/*******************/
/* Public Typedefs */
/*******************/

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/* Token <--> address converters */

/**
 * \ingroup H5VLNAT
 *
 * \brief Convert a haddr_t address to a native VOL connector token
 *
 * \fgdta_loc_obj_id{loc_id}
 * \param[in] addr Object address
 * \param[out] token Object token
 *
 * \return \herr_t
 *
 * \details This API call maps pre-VOL haddr_t native file format addresses
 *          to the more generic H5O_token_t tokens used by the VOL.
 *
 * \since 1.12.0
 */
H5_DLL herr_t H5VLnative_addr_to_token(hid_t loc_id, haddr_t addr, H5O_token_t *token);
/**
 * \ingroup H5VLNAT
 *
 * \brief Convert a native VOL connector token to a haddr_t address
 *
 * \fgdta_loc_obj_id{loc_id}
 * \param[in] token Object token
 * \param[out] addr Object address
 *
 * \return \herr_t
 *
 * \details This API call maps generic H5O_token_t tokens used by the VOL to
 *          pre-VOL haddr_t native file format addresses.
 *
 * \since 1.12.0
 */
H5_DLL herr_t H5VLnative_token_to_addr(hid_t loc_id, H5O_token_t token, haddr_t *addr);

/** @private
 *
 * \brief Register the native VOL connector and retrieve an ID for it
 */
H5_DLL hid_t H5VL_native_register(void);

#ifdef __cplusplus
}
#endif

#endif /* H5VLnative_H */
