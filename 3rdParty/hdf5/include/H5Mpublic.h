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
 * This file contains public declarations for the H5M module.
 *
 * NOTE:    This is an experimental API. Everything in the H5M package
 *          is subject to revision in a future release.
 */
#ifndef H5Mpublic_H
#define H5Mpublic_H

#include "H5public.h"   /* Generic Functions                        */
#include "H5Ipublic.h"  /* Identifiers                              */
#include "H5VLpublic.h" /* Virtual Object Layer                     */

/* Exposes VOL connector types, so it needs the connector header */
#include "H5VLconnector.h"

/*****************/
/* Public Macros */
/*****************/

/* Macros defining operation IDs for map VOL callbacks (implemented using the
 * "optional" VOL callback)
 */
/** Callback operation ID for map create \since 1.12.0 */
#define H5VL_MAP_CREATE 1
/** Callback operation ID for map open \since 1.12.0 */
#define H5VL_MAP_OPEN 2
/** Callback operation ID for getting an associated value from a map \since 1.12.0 */
#define H5VL_MAP_GET_VAL 3
/** Callback operation ID for checking if a value exists in a map \since 1.12.0 */
#define H5VL_MAP_EXISTS 4
/** Callback operation ID for putting a key-value pair to a map \since 1.12.0 */
#define H5VL_MAP_PUT 5
/** Callback operation ID for map get callback \since 1.12.0 */
#define H5VL_MAP_GET 6
/** Callback operation ID for map specific operation \since 1.12.0 */
#define H5VL_MAP_SPECIFIC 7
/** Currently unused \since 1.12.0 */
#define H5VL_MAP_OPTIONAL 8
/** Callback operation ID for terminating access to a map \since 1.12.0 */
#define H5VL_MAP_CLOSE 9

/*******************/
/* Public Typedefs */
/*******************/

/**
 * Types for map GET callback
 */
typedef enum H5VL_map_get_t {
    H5VL_MAP_GET_MAPL,     /**< Callback operation ID for getting map access property list   \since 1.12.0 */
    H5VL_MAP_GET_MCPL,     /**< Callback operation ID for getting map creation property list \since 1.12.0 */
    H5VL_MAP_GET_KEY_TYPE, /**< Callback operation ID for getting the key datatype for a map \since 1.12.0 */
    H5VL_MAP_GET_VAL_TYPE, /**< Callback operation ID for getting the value datatype for a map \since 1.12.0
                            */
    H5VL_MAP_GET_COUNT /**< Callback operation ID for getting the number of key-value pairs stored in a map
                          \since 1.12.0 */
} H5VL_map_get_t;

/**
 * Types for map SPECIFIC callback
 */
typedef enum H5VL_map_specific_t {
    H5VL_MAP_ITER,  /**< Callback operation ID for iterating over all key-value pairs stored in the map
                       \since 1.12.0 */
    H5VL_MAP_DELETE /**< Callback operation ID for deleting a key-value pair stored in the map \since 1.12.0
                     */
} H5VL_map_specific_t;

//! <!-- [H5M_iterate_t_snip] -->
/**
 * Callback for H5Miterate()
 */
typedef herr_t (*H5M_iterate_t)(hid_t map_id, const void *key, void *op_data);
//! <!-- [H5M_iterate_t_snip] -->

/**
 * Parameters for map operations
 */
typedef union H5VL_map_args_t {

    /** H5VL_MAP_CREATE */
    struct {
        H5VL_loc_params_t loc_params;  /**< Location parameters for object */
        const char       *name;        /**< Name of new map object */
        hid_t             lcpl_id;     /**< Link creation property list for map */
        hid_t             key_type_id; /**< Datatype for map keys */
        hid_t             val_type_id; /**< Datatype for map values */
        hid_t             mcpl_id;     /**< Map creation property list */
        hid_t             mapl_id;     /**< Map access property list */
        void             *map;         /**< Pointer to newly created map object (OUT) */
    } create;

    /** H5VL_MAP_OPEN */
    struct {
        H5VL_loc_params_t loc_params; /**< Location parameters for object */
        const char       *name;       /**< Name of new map object */
        hid_t             mapl_id;    /**< Map access property list */
        void             *map;        /**< Pointer to newly created map object (OUT) */
    } open;

    /** H5VL_MAP_GET_VAL */
    struct {
        hid_t       key_mem_type_id;   /**< Memory datatype for key */
        const void *key;               /**< Pointer to key */
        hid_t       value_mem_type_id; /**< Memory datatype for value */
        void       *value;             /**< Buffer for value (OUT) */
    } get_val;

    /** H5VL_MAP_EXISTS */
    struct {
        hid_t       key_mem_type_id; /**< Memory datatype for key */
        const void *key;             /**< Pointer to key */
        hbool_t     exists;          /**< Flag indicating whether key exists in map (OUT) */
    } exists;

    /** H5VL_MAP_PUT */
    struct {
        hid_t       key_mem_type_id;   /**< Memory datatype for key */
        const void *key;               /**< Pointer to key */
        hid_t       value_mem_type_id; /**< Memory datatype for value */
        const void *value;             /**< Pointer to value */
    } put;

    /** H5VL_MAP_GET */
    struct {
        H5VL_map_get_t get_type; /**< 'get' operation to perform */

        /** Parameters for each operation */
        union {
            /** H5VL_MAP_GET_MAPL */
            struct {
                hid_t mapl_id; /**< Get map access property list ID (OUT) */
            } get_mapl;

            /** H5VL_MAP_GET_MCPL */
            struct {
                hid_t mcpl_id; /**< Get map creation property list ID (OUT) */
            } get_mcpl;

            /** H5VL_MAP_GET_KEY_TYPE */
            struct {
                hid_t type_id; /**< Get datatype ID for map's keys (OUT) */
            } get_key_type;

            /** H5VL_MAP_GET_VAL_TYPE */
            struct {
                hid_t type_id; /**< Get datatype ID for map's values (OUT) */
            } get_val_type;

            /** H5VL_MAP_GET_COUNT */
            struct {
                hsize_t count; /**< Get number of key-value pairs in the map (OUT) */
            } get_count;
        } args;
    } get;

    /** H5VL_MAP_SPECIFIC */
    struct {
        H5VL_map_specific_t specific_type;
        /**< 'specific' operation to perform */

        /** Parameters for each operation */
        union {
            /* H5VL_MAP_ITER specific operation */
            struct {
                H5VL_loc_params_t loc_params;      /**< Location parameters for object */
                hsize_t           idx;             /**< Start/end iteration index (IN/OUT) */
                hid_t             key_mem_type_id; /**< Memory datatype for key */
                H5M_iterate_t     op;              /**< Iteration callback routine */
                void             *op_data;         /**< Pointer to callback context */
            } iterate;

            /* H5VL_MAP_DELETE specific operation */
            struct {
                H5VL_loc_params_t loc_params;      /**< Location parameters for object */
                hid_t             key_mem_type_id; /**< Memory datatype for key */
                const void       *key;             /**< Pointer to key */
            } del;
        } args;
    } specific;

    /** H5VL_MAP_OPTIONAL */
    /* Unused */

    /** H5VL_MAP_CLOSE */
    /* No args */
} H5VL_map_args_t;

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/
#ifdef __cplusplus
extern "C" {
#endif

/* The map API is only built when requested since there's no support in
 * the native file format at this time. It's only supported in a few VOL
 * connectors.
 */
#ifdef H5_HAVE_MAP_API

/**
 * \ingroup H5M
 *
 * \brief Creates a map object
 *
 * \fgdta_loc_id
 * \param[in] name Map object name
 * \type_id{key_type_id}
 * \type_id{val_type_id}
 * \lcpl_id
 * \mcpl_id
 * \mapl_id
 * \return \hid_t{map object}
 *
 * \details H5Mcreate() creates a new map object for storing key-value
 *          pairs. The in-file datatype for keys is defined by \p key_type_id
 *          and the in-file datatype for values is defined by \p val_type_id. \p
 *          loc_id specifies the location to create the map object and \p
 *          name specifies the name of the link to the map object relative to
 *          \p loc_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mcreate(hid_t loc_id, const char *name, hid_t key_type_id, hid_t val_type_id, hid_t lcpl_id,
                       hid_t mcpl_id, hid_t mapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Mcreate}
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Mcreate_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                             const char *name, hid_t key_type_id, hid_t val_type_id, hid_t lcpl_id,
                             hid_t mcpl_id, hid_t mapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Mcreate_async(hid_t loc_id, const char *name, hid_t key_type_id, hid_t val_type_id,
                             hid_t lcpl_id, hid_t mcpl_id, hid_t mapl_id, hid_t es_id);
#endif

/**
 * \ingroup H5M
 *
 * \brief Creates a map object without linking it into a file
 *
 * \fgdta_loc_id
 * \type_id{key_type_id}
 * \type_id{val_type_id}
 * \mcpl_id
 * \mapl_id
 * \return \hid_t{map object}
 *          The resulting ID should be linked into the file with H5Olink or it
 *          will be deleted when closed.
 *
 * \details H5Mcreate_anon() creates a new map object for storing key-value
 *          pairs. The in-file datatype for keys is defined by \p key_type_id
 *          and the in-file datatype for values is defined by \p val_type_id. \p
 *          loc_id specifies the file to create the map object, but no link to
 *          the object is created.  Other options can be specified through the
 *          property lists \p mcpl_id and \p mapl_id.
 *
 *          The new map should be linked into the group hierarchy before being
 *          closed or it will be deleted. The map should be closed when the
 *          caller no longer requires it.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mcreate_anon(hid_t loc_id, hid_t key_type_id, hid_t val_type_id, hid_t mcpl_id, hid_t mapl_id);

/**
 * \ingroup H5M
 *
 * \brief Opens a map object
 *
 * \fgdta_loc_id{loc_id}
 * \param[in] name Map object name relative to \p loc_id
 * \mapl_id
 * \return \hid_t{map object}
 *
 * \details H5Mopen() finds a map object specified by \p name under the location
 *          specified by \p loc_id. The map object should be close with
 *          H5Mclose() when the application is not longer interested in
 *          accessing it.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mopen(hid_t loc_id, const char *name, hid_t mapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Mopen}
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Mopen_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                           const char *name, hid_t mapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Mopen_async(hid_t loc_id, const char *name, hid_t mapl_id, hid_t es_id);
#endif

/**
 * \ingroup H5M
 *
 * \brief Terminates access to a map object
 *
 * \map_id
 * \return \herr_t
 *
 * \details H5Mclose() closes access to a map object specified by \p map_id and
 *          releases resources used by it.
 *
 *          It is illegal to subsequently use that same map identifier in calls
 *          to other map functions.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Mclose(hid_t map_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Mclose}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Mclose_async(const char *app_file, const char *app_func, unsigned app_line, hid_t map_id,
                             hid_t es_id);
#else
H5_DLL herr_t H5Mclose_async(hid_t map_id, hid_t es_id);
#endif

/**
 * \ingroup H5M
 *
 * \brief Gets key datatype for a map object
 *
 * \map_id
 * \return \hid_t{datatype}
 *
 * \details H5Mget_key_type() retrieves key datatype as stored in the file for a
 *          map object specified by \p map_id and returns identifier for the
 *          datatype.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mget_key_type(hid_t map_id);

/**
 * \ingroup H5M
 *
 * \brief Gets value datatype for a map object
 *
 * \map_id
 * \return \hid_t{datatype}
 *
 * \details H5Mget_val_type() retrieves value datatype as stored in the file for
 *          a map object specified by \p map_id and returns identifier for the
 *          datatype .
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mget_val_type(hid_t map_id);

/**
 * \ingroup H5M
 *
 * \brief Gets creation property list for a map object
 *
 * \map_id
 * \return \hid_t{map creation property list}
 *
 * \details H5Mget_create_plist() returns an identifier for a copy of the
 *          creation property list for a map object specified by \p map_id.
 *
 *          The creation property list identifier should be released with
 *          H5Pclose() to prevent resource leaks.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mget_create_plist(hid_t map_id);

/**
 * \ingroup H5M
 *
 * \brief Gets access property list for a map object
 *
 * \map_id
 * \return \hid_t{map access property list}
 *
 * \details H5Mget_access_plist() returns an identifier for a copy of the access
 *          property list for a map object specified by \p map_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Mget_access_plist(hid_t map_id);

/**
 * \ingroup H5M
 *
 * \brief Retrieves the number of key-value pairs in a map object
 *
 * \map_id
 * \param[out] count The number of key-value pairs stored in the map object
 * \dxpl_id
 * \return \herr_t
 *
 * \details H5Mget_count() retrieves the number of key-value pairs stored in a
 *          map specified by map_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Mget_count(hid_t map_id, hsize_t *count, hid_t dxpl_id);

/**
 * \ingroup H5M
 *
 * \brief Adds a key-value pair to a map object
 *
 * \map_id
 * \type_id{key_mem_type_id}
 * \param[in] key Pointer to key buffer
 * \type_id{val_mem_type_id}
 * \param[in] value Pointer to value buffer
 * \dxpl_id
 * \return \herr_t
 *
 * \details H5Mput() adds a key-value pair to a map object specified by \p
 *          map_id, or updates the value for the specified key if one was set
 *          previously.
 *
 *          \p key_mem_type_id and \p val_mem_type_id specify the datatypes for
 *          the provided key and value buffers, and if different from those used
 *          to create the map object, the key and value will be internally
 *          converted to the datatypes for the map object.
 *
 *          Any further options can be specified through the property list
 *          \p dxpl_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Mput(hid_t map_id, hid_t key_mem_type_id, const void *key, hid_t val_mem_type_id,
                     const void *value, hid_t dxpl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Mput}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Mput_async(const char *app_file, const char *app_func, unsigned app_line, hid_t map_id,
                           hid_t key_mem_type_id, const void *key, hid_t val_mem_type_id, const void *value,
                           hid_t dxpl_id, hid_t es_id);
#else
H5_DLL herr_t H5Mput_async(hid_t map_id, hid_t key_mem_type_id, const void *key, hid_t val_mem_type_id,
                           const void *value, hid_t dxpl_id, hid_t es_id);
#endif

/**
 * \ingroup H5M
 *
 * \brief Retrieves a key-value pair from a map object
 *
 * \map_id
 * \type_id{key_mem_type_id}
 * \param[in] key Pointer to key buffer
 * \type_id{val_mem_type_id}
 * \param[out] value Pointer to value buffer
 * \dxpl_id
 * \return \herr_t
 *
 * \details H5Mget() retrieves from a map object specified by \p map_id, the
 *          value associated with the provided key \p key. \p key_mem_type_id
 *          and \p val_mem_type_id specify the datatypes for the provided key
 *          and value buffers. If if the datatype specified by \p
 *          key_mem_type_id is different from that used to create the map object
 *          the key will be internally converted to the datatype for the map
 *          object for the query, and if the datatype specified by \p
 *          val_mem_type_id is different from that used to create the map object
 *          the returned value will be converted to have a datatype as specified
 *          by \p val_mem_type_id before the function returns.
 *
 *          Any further options can be specified through the property list
 *          \p dxpl_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Mget(hid_t map_id, hid_t key_mem_type_id, const void *key, hid_t val_mem_type_id, void *value,
                     hid_t dxpl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Mget}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Mget_async(const char *app_file, const char *app_func, unsigned app_line, hid_t map_id,
                           hid_t key_mem_type_id, const void *key, hid_t val_mem_type_id, void *value,
                           hid_t dxpl_id, hid_t es_id);
#else
H5_DLL herr_t H5Mget_async(hid_t map_id, hid_t key_mem_type_id, const void *key, hid_t val_mem_type_id,
                           void *value, hid_t dxpl_id, hid_t es_id);
#endif

/**
 * \ingroup H5M
 *
 * \brief Checks if provided key exists in a map object
 *
 * \map_id
 * \type_id{key_mem_type_id}
 * \param[in] key Pointer to key buffer
 * \param[out] exists Pointer to a buffer to return the existence status
 * \dxpl_id
 * \return \herr_t
 *
 * \details H5Mexists() checks if the provided key is stored in the map object
 *          specified by \p map_id. If \p key_mem_type_id is different from that
 *          used to create the map object the key will be internally converted
 *          to the datatype for the map object for the query.
 *
 *          Any further options can be specified through the property list
 *          \p dxpl_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Mexists(hid_t map_id, hid_t key_mem_type_id, const void *key, hbool_t *exists, hid_t dxpl_id);

/**
 * \ingroup H5M
 *
 * \brief Iterates over all key-value pairs in a map object
 *
 * \map_id
 * \param[in,out] idx iteration index
 * \type_id{key_mem_type_id}
 * \param[in] op User-defined iterator function
 * \op_data
 * \dxpl_id
 * \return \herr_t
 *
 * \details H5Miterate() iterates over all key-value pairs stored in the map
 *          object specified by \p map_id, making the callback specified by \p
 *          op for each. The \p idx parameter is an in/out parameter that may be
 *          used to restart a previously interrupted iteration. At the start of
 *          iteration \p idx should be set to 0, and to restart iteration at the
 *          same location on a subsequent call to H5Miterate(), \p idx should be
 *          the same value as returned by the previous call. Iterate callback is
 *          defined as:
 *          \snippet this H5M_iterate_t_snip
 *          The \p key parameter is the buffer for the key for this iteration,
 *          converted to the datatype specified by \p key_mem_type_id. The \p
 *          op_data parameter is a simple pass through of the value passed to
 *          H5Miterate(), which can be used to store application-defined data for
 *          iteration. A negative return value from this function will cause
 *          H5Miterate() to issue an error, while a positive return value will
 *          cause H5Miterate() to stop iterating and return this value without
 *          issuing an error. A return value of zero allows iteration to continue.
 *
 *          Any further options can be specified through the property list \p dxpl_id.
 *
 *  \warning Adding or removing key-value pairs to the map during iteration
 *           will lead to undefined behavior.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Miterate(hid_t map_id, hsize_t *idx, hid_t key_mem_type_id, H5M_iterate_t op, void *op_data,
                         hid_t dxpl_id);

/**
 * \ingroup H5M
 *
 * \brief Iterates over all key-value pairs in a map object
 *
 * \loc_id
 * \param[in] map_name Map object name relative to the location specified by \p loc_id
 * \param[in,out] idx Iteration index
 * \type_id{key_mem_type_id}
 * \param[in] op User-defined iterator function
 * \op_data
 * \dxpl_id
 * \lapl_id
 * \return \herr_t
 *
 * \details H5Miterate_by_name() iterates over all key-value pairs stored in the
 *          map object specified by \p map_id, making the callback specified by
 *          \p op for each. The \p idx parameter is an in/out parameter that may
 *          be used to restart a previously interrupted iteration. At the start
 *          of iteration \p idx should be set to 0, and to restart iteration at
 *          the same location on a subsequent call to H5Miterate(), \p idx
 *          should be the same value as returned by the previous call. Iterate
 *          callback is defined as:
 *          \snippet this H5M_iterate_t_snip
 *          The\p key parameter is the buffer for the key for this iteration,
 *          converted to the datatype specified by \p key_mem_type_id. The \p
 *          op_data parameter is a simple pass through of the value passed to
 *          H5Miterate(), which can be used to store application-defined data
 *          for iteration. A negative return value from this function will cause
 *          H5Miterate() to issue an error, while a positive return value will cause
 *          H5Miterate() to stop iterating and return this value without issuing an
 *          error. A return value of zero allows iteration to continue.
 *
 *          Any further options can be specified through the property list \p dxpl_id.
 *
 *  \warning Adding or removing key-value pairs to the map during iteration
 *           will lead to undefined behavior.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Miterate_by_name(hid_t loc_id, const char *map_name, hsize_t *idx, hid_t key_mem_type_id,
                                 H5M_iterate_t op, void *op_data, hid_t dxpl_id, hid_t lapl_id);

/**
 * \ingroup H5M
 *
 * \brief Deletes a key-value pair from a map object
 *
 * \map_id
 * \type_id{key_mem_type_id}
 * \param[in] key Pointer to key buffer
 * \dxpl_id
 * \return \herr_t
 *
 * \details H5Mdelete() deletes a key-value pair from the map object specified
 *          by \p map_id. \p key_mem_type_id specifies the datatype for the
 *          provided key buffer key, and if different from that used to create
 *          the map object, the key will be internally converted to the datatype
 *          for the map object.
 *
 *          Any further options can be specified through the property list \p dxpl_id.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Mdelete(hid_t map_id, hid_t key_mem_type_id, const void *key, hid_t dxpl_id);

/// \cond DEV
/* API Wrappers for async routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5M_MODULE
#define H5Mcreate_async(...) H5Mcreate_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Mopen_async(...)   H5Mopen_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Mclose_async(...)  H5Mclose_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Mput_async(...)    H5Mput_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Mget_async(...)    H5Mget_async(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 * be passed in by language wrapper or library layer on top of HDF5. */
#define H5Mcreate_async_wrap H5_NO_EXPAND(H5Mcreate_async)
#define H5Mopen_async_wrap   H5_NO_EXPAND(H5Mopen_async)
#define H5Mclose_async_wrap  H5_NO_EXPAND(H5Mclose_async)
#define H5Mput_async_wrap    H5_NO_EXPAND(H5Mput_async)
#define H5Mget_async_wrap    H5_NO_EXPAND(H5Mget_async)
#endif /* H5M_MODULE */
/// \endcond

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS
#endif /* H5_NO_DEPRECATED_SYMBOLS */

#endif /*  H5_HAVE_MAP_API */

#ifdef __cplusplus
}
#endif

#endif /* H5Mpublic_H */
