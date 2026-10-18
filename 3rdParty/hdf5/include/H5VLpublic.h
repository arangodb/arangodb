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
 * This file contains public declarations for the H5VL (VOL) module.
 */

#ifndef H5VLpublic_H
#define H5VLpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Ipublic.h" /* Identifiers                              */

/*****************/
/* Public Macros */
/*****************/

/**
 * \ingroup H5VLDEF
 * \brief Version number of VOL class struct & callbacks
 *
 * \details Each VOL connector must set the 'version' field in the H5VL_class_t
 *          struct to the version of the H5VL_class_t struct that the connector
 *          implements.  The HDF5 library will reject connectors with
 *          incompatible structs.
 */
#define H5VL_VERSION 3

/* VOL connector identifier values
 * These are H5VL_class_value_t values, NOT hid_t values!
 */
/** Invalid ID for VOL connector ID \since 1.12.0 */
#define H5_VOL_INVALID (-1)
/** Native HDF5 file format VOL connector \since 1.12.0 */
#define H5_VOL_NATIVE 0
/** VOL connector IDs below this value are reserved for library use \since 1.12.0 */
#define H5_VOL_RESERVED 256
/** Maximum VOL connector ID \since 1.12.0 */
#define H5_VOL_MAX 65535

/*
 * Capability flags for VOL connectors
 */
/** No special connector capabilities \since 1.12.0 */
#define H5VL_CAP_FLAG_NONE 0x0000000000000000
/** Connector is threadsafe \since 1.12.0 */
#define H5VL_CAP_FLAG_THREADSAFE 0x0000000000000001
/** Connector performs operations asynchronously\since 1.12.0 */
#define H5VL_CAP_FLAG_ASYNC 0x0000000000000002
/** Connector produces native file format \since 1.12.0 */
#define H5VL_CAP_FLAG_NATIVE_FILES 0x0000000000000004
/** H5A create/delete/exists/open/close/read/write \since 1.12.0 */
#define H5VL_CAP_FLAG_ATTR_BASIC 0x0000000000000008
/** All other H5A API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_ATTR_MORE 0x0000000000000010
/** H5D create/open/close/read/write \since 1.12.0 */
#define H5VL_CAP_FLAG_DATASET_BASIC 0x0000000000000020
/** All other H5D API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_DATASET_MORE 0x0000000000000040
/** H5F create/open/close/read/write \since 1.12.0 */
#define H5VL_CAP_FLAG_FILE_BASIC 0x0000000000000080
/** All other H5F API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_FILE_MORE 0x0000000000000100
/** H5G create/open/close \since 1.12.0 */
#define H5VL_CAP_FLAG_GROUP_BASIC 0x0000000000000200
/** All other H5G API calls\since 1.12.0 */
#define H5VL_CAP_FLAG_GROUP_MORE 0x0000000000000400
/** H5L exists/delete \since 1.12.0 */
#define H5VL_CAP_FLAG_LINK_BASIC 0x0000000000000800
/** All other H5L API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_LINK_MORE 0x0000000000001000
/** H5M create/open/close/get*type/get_count/put/get/exists/delete \since 1.12.0 */
#define H5VL_CAP_FLAG_MAP_BASIC 0x0000000000002000
/** All other H5M API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_MAP_MORE 0x0000000000004000
/** H5O open/close/exists \since 1.12.0 */
#define H5VL_CAP_FLAG_OBJECT_BASIC 0x0000000000008000
/** All other H5O API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_OBJECT_MORE 0x0000000000010000
/** H5Rdestroy \since 1.12.0 */
#define H5VL_CAP_FLAG_REF_BASIC 0x0000000000020000
/** All other H5R API calls \since 1.12.0 */
#define H5VL_CAP_FLAG_REF_MORE 0x0000000000040000
/** Connector supports object references \since 1.12.0 */
#define H5VL_CAP_FLAG_OBJ_REF 0x0000000000080000
/** Connector supports regional references \since 1.12.0 */
#define H5VL_CAP_FLAG_REG_REF 0x0000000000100000
/** Connector supports attribute references \since 1.12.0 */
#define H5VL_CAP_FLAG_ATTR_REF 0x0000000000200000
/** Connector supports stored datatypes \since 1.12.0 */
#define H5VL_CAP_FLAG_STORED_DATATYPES 0x0000000000400000
/** Connector tracks creation order \since 1.12.0 */
#define H5VL_CAP_FLAG_CREATION_ORDER 0x0000000000800000
/** Connector supports iteration functions \since 1.12.0 */
#define H5VL_CAP_FLAG_ITERATE 0x0000000001000000
/** Connector can return a meaningful storage size \since 1.12.0 */
#define H5VL_CAP_FLAG_STORAGE_SIZE 0x0000000002000000
/** "by index" API calls are supported \since 1.12.0 */
#define H5VL_CAP_FLAG_BY_IDX 0x0000000004000000
/** Connector can return the property lists used to create an object \since 1.12.0 */
#define H5VL_CAP_FLAG_GET_PLIST 0x0000000008000000
/** flush/refresh calls are supported \since 1.12.0 */
#define H5VL_CAP_FLAG_FLUSH_REFRESH 0x0000000010000000
/** External links are supported \since 1.12.0 */
#define H5VL_CAP_FLAG_EXTERNAL_LINKS 0x0000000020000000
/** Hard links are supported \since 1.12.0 */
#define H5VL_CAP_FLAG_HARD_LINKS 0x0000000040000000
/** Soft links are supported \since 1.12.0 */
#define H5VL_CAP_FLAG_SOFT_LINKS 0x0000000080000000
/** User-defined links are supported \since 1.12.0 */
#define H5VL_CAP_FLAG_UD_LINKS 0x0000000100000000
/** Connector tracks creation, etc. times \since 1.12.0 */
#define H5VL_CAP_FLAG_TRACK_TIMES 0x0000000200000000
/** H5Fmount/unmount supported \since 1.12.0 */
#define H5VL_CAP_FLAG_MOUNT 0x0000000400000000
/** Connector implements a filter pipeline \since 1.12.0 */
#define H5VL_CAP_FLAG_FILTERS 0x0000000800000000
/** Connector allows fill values to be set \since 1.12.0 */
#define H5VL_CAP_FLAG_FILL_VALUES 0x0000001000000000

/**
 * \ingroup H5VLDEF
 *
 * Flags to return from H5VLquery_optional API and 'opt_query' callbacks
 *
 * \details Operations which access multiple objects' data or metadata in a
 *          container should be registered as file-level optional operations.
 *          (e.g. "H5Dwrite_multi" takes a list of datasets to write data to, so
 *          a VOL connector that implemented it should register it as an optional
 *          file operation, and pass-through VOL connectors that are stacked above
 *          the connector that registered it should assume that dataset elements
 *          for _any_ dataset in the file could be written to)
 */
/** VOL connector supports this operation \since 1.14.0 */
#define H5VL_OPT_QUERY_SUPPORTED 0x0001
/** Operation reads data for object \since 1.14.0 */
#define H5VL_OPT_QUERY_READ_DATA 0x0002
/** Operation writes data for object \since 1.14.0 */
#define H5VL_OPT_QUERY_WRITE_DATA 0x0004
/** Operation reads metadata for object \since 1.14.0 */
#define H5VL_OPT_QUERY_QUERY_METADATA 0x0008
/** Operation modifies metadata for object \since 1.14.0 */
#define H5VL_OPT_QUERY_MODIFY_METADATA 0x0010
/** Operation is collective (operations without this flag are assumed to be independent) \since 1.14.0 */
#define H5VL_OPT_QUERY_COLLECTIVE 0x0020
/** Operation may NOT be executed asynchronously \since 1.14.0 */
#define H5VL_OPT_QUERY_NO_ASYNC 0x0040
/** Operation involves multiple objects \since 1.14.0 */
#define H5VL_OPT_QUERY_MULTI_OBJ 0x0080

/*******************/
/* Public Typedefs */
/*******************/

/**
 * \ingroup H5VLDEF
 *
 * \brief VOL connector identifiers.
 *
 * \details Values 0 through 255 are for connectors defined by the HDF5
 *          library. Values 256 through 511 are available for testing new
 *          connectors. Subsequent values should be obtained from the HDF5
 *          development team at mailto:help@hdfgroup.org.
 */
//! <!-- [H5VL_class_value_t_snip] -->
typedef int H5VL_class_value_t;
//! <!-- [H5VL_class_value_t_snip] -->

/**
 * \ingroup H5VLDEF
 * \details Enum type for each VOL subclass
 *          (Used for various queries, etc)
 */
typedef enum H5VL_subclass_t {
    H5VL_SUBCLS_NONE,     /**< Operations outside of a subclass */
    H5VL_SUBCLS_INFO,     /**< 'Info' subclass */
    H5VL_SUBCLS_WRAP,     /**< 'Wrap' subclass */
    H5VL_SUBCLS_ATTR,     /**< 'Attribute' subclass */
    H5VL_SUBCLS_DATASET,  /**< 'Dataset' subclass */
    H5VL_SUBCLS_DATATYPE, /**< 'Named datatype' subclass */
    H5VL_SUBCLS_FILE,     /**< 'File' subclass */
    H5VL_SUBCLS_GROUP,    /**< 'Group' subclass */
    H5VL_SUBCLS_LINK,     /**< 'Link' subclass */
    H5VL_SUBCLS_OBJECT,   /**< 'Object' subclass */
    H5VL_SUBCLS_REQUEST,  /**< 'Request' subclass */
    H5VL_SUBCLS_BLOB,     /**< 'Blob' subclass */
    H5VL_SUBCLS_TOKEN     /**< 'Token' subclass */
                          /* NOTE: if more operations are added, the
                           * H5VL_opt_vals_g[] array size should be updated.
                           */
} H5VL_subclass_t;

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
 * \ingroup H5VL
 * \brief Registers a new VOL connector by name
 *
 * \param[in] connector_name Connector name
 * \vipl_id
 * \return \hid_t{VOL connector}
 *
 * \details H5VLregister_connector_by_name() registers a new VOL connector with
 *          the name \p connector_name as a member of the virtual object layer
 *          class. This VOL connector identifier is good until the library is
 *          closed or the connector is unregistered.
 *
 *          \p vipl_id is either #H5P_DEFAULT or the identifier of a VOL
 *          initialization property list of class #H5P_VOL_INITIALIZE created
 *          with H5Pcreate(). When created, this property list contains no
 *          library properties. If a VOL connector author decides that
 *          initialization-specific data are needed, they can be added to the
 *          empty list and retrieved by the connector in the VOL connector's
 *          initialize callback. Use of the VOL initialization property list is
 *          uncommon, as most VOL-specific properties are added to the file
 *          access property list via the connector's API calls which set the
 *          VOL connector for the file open/create. For more information, see
 *          \ref_vol_doc.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5VLregister_connector_by_name(const char *connector_name, hid_t vipl_id);
/**
 * \ingroup H5VL
 * \brief Registers a new VOL connector by value
 *
 * \param[in] connector_value Connector value
 * \vipl_id
 * \return \hid_t{VOL connector}
 *
 * \details H5VLregister_connector_by_value() registers a new VOL connector
 *          with value connector_value as a member of the virtual object layer
 *          class. This VOL connector identifier is good until the library is
 *          closed or the connector is unregistered.
 *
 *          \p connector_value has a type of H5VL_class_value_t, which is
 *          defined in H5VLpublic.h as follows:
 *          \snippet this H5VL_class_value_t_snip
 *
 *          Valid VOL connector identifiers can have values from 0 through 255
 *          for connectors defined by the HDF5 library. Values 256 through 511
 *          are available for testing new connectors. Subsequent values should
 *          be obtained by contacting the The HDF Help Desk.
 *
 *          \p vipl_id is either #H5P_DEFAULT or the identifier of a VOL
 *          initialization property list of class #H5P_VOL_INITIALIZE created
 *          with H5Pcreate(). When created, this property list contains no
 *          library properties. If a VOL connector author decides that
 *          initialization-specific data are needed, they can be added to the
 *          empty list and retrieved by the connector in the VOL connector's
 *          initialize callback. Use of the VOL initialization property list is
 *          uncommon, as most VOL-specific properties are added to the file
 *          access property list via the connector's API calls which set the
 *          VOL connector for the file open/create. For more information, see
 *          the \ref_vol_doc.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5VLregister_connector_by_value(H5VL_class_value_t connector_value, hid_t vipl_id);
/**
 * \ingroup H5VL
 * \brief Tests whether a VOL class has been registered under a certain name
 *
 * \param[in] name Alleged name of connector
 * \return \htri_t
 *
 * \details H5VLis_connector_registered_by_name() tests whether a VOL class has
 *          been registered or not, according to the supplied connector name
 *          \p name.
 *
 * \since 1.12.0
 */
H5_DLL htri_t H5VLis_connector_registered_by_name(const char *name);
/**
 * \ingroup H5VL
 * \brief Tests whether a VOL class has been registered for a given value
 *
 * \param[in] connector_value Connector value
 * \return \htri_t
 *
 * \details H5VLis_connector_registered_by_value() tests whether a VOL class
 *          has been registered, according to the supplied connector value \p
 *          connector_value.
 *
 *          \p connector_value has a type of H5VL_class_value_t, which is
 *          defined in H5VLpublic.h as follows:
 *          \snippet this H5VL_class_value_t_snip
 *
 *          Valid VOL connector identifiers can have values from 0 through 255
 *          for connectors defined by the HDF5 library. Values 256 through 511
 *          are available for testing new connectors. Subsequent values should
 *          be obtained by contacting the The HDF Help Desk.
 *
 * \since 1.12.0
 */
H5_DLL htri_t H5VLis_connector_registered_by_value(H5VL_class_value_t connector_value);
/**
 * \ingroup H5VL
 * \brief Retrieves the VOL connector identifier for a given object identifier
 *
 * \obj_id
 * \return \hid_t{VOL connector}
 *
 * \details H5VLget_connector_id() retrieves the registered VOL connector
 *          identifier for the specified object identifier \p obj_id. The VOL
 *          connector identifier must be closed with H5VLclose() when no longer
 *          in use.
 *
 * \since 1.12.0
 */
H5_DLL hid_t H5VLget_connector_id(hid_t obj_id);
/**
 * \ingroup H5VL
 * \brief Retrieves the identifier for a registered VOL connector name
 *
 * \param[in] name Connector name
 * \return \hid_t{VOL connector}
 *
 * \details H5VLget_connector_id_by_name() retrieves the identifier for a
 *          registered VOL connector with the name \p name. The identifier must
 *          be closed with H5VLclose() when no longer in use.
 *
 * \since 1.12.0
 */
H5_DLL hid_t H5VLget_connector_id_by_name(const char *name);
/**
 * \ingroup H5VL
 * \brief Retrieves the identifier for a registered VOL connector value
 *
 * \param[in] connector_value Connector value
 * \return \hid_t{VOL connector}
 *
 * \details H5VLget_connector_id_by_value() retrieves the identifier for a
 *          registered VOL connector with the value \p connector_value. The
 *          identifier will need to be closed by H5VLclose().
 *
 *          \p connector_value has a type of H5VL_class_value_t, which is
 *          defined in H5VLpublic.h as follows:
 *          \snippet this H5VL_class_value_t_snip
 *
 *          Valid VOL connector identifiers can have values from 0 through 255
 *          for connectors defined by the HDF5 library. Values 256 through 511
 *          are available for testing new connectors. Subsequent values should
 *          be obtained by contacting the The HDF Help Desk.
 *
 * \since 1.12.0
 */
H5_DLL hid_t H5VLget_connector_id_by_value(H5VL_class_value_t connector_value);
/**
 * \ingroup H5VL
 * \brief Retrieves a connector name for a VOL
 *
 * \obj_id{id} or file identifier
 * \param[out] name Connector name
 * \param[in] size Maximum length of the name to retrieve
 * \return Returns the length of the connector name on success, and a negative value on failure.
 *
 * \details H5VLget_connector_name() retrieves up to \p size elements of the
 *          VOL name \p name associated with the object or file identifier \p
 *          id.
 *
 *          Passing in a NULL pointer for size will return the size of the
 *          connector name. This can be used to determine the size of the
 *          buffer to allocate for the name.
 *
 * \since 1.12.0
 */
H5_DLL ssize_t H5VLget_connector_name(hid_t id, char *name /*out*/, size_t size);
/**
 * \ingroup H5VL
 * \brief Closes a VOL connector identifier
 *
 * \param[in] connector_id Connector identifier
 * \return \herr_t
 *
 * \details H5VLclose() closes a VOL connector identifier. This does not affect
 *          the file access property lists which have been defined to use this
 *          VOL connector or files which are already opened under this
 *          connector.
 *
 * \since 1.12.0
 */
H5_DLL herr_t H5VLclose(hid_t connector_id);
/**
 * \ingroup H5VL
 * \brief Removes a VOL connector identifier from the library
 *
 * \param[in] connector_id Connector identifier
 * \return \herr_t
 *
 * \details H5VLunregister_connector() removes a VOL connector identifier from
 *          the library. This does not affect the file access property lists
 *          which have been defined to use the VOL connector or any files which
 *          are already opened with this connector.
 *
 * \attention H5VLunregister_connector() will fail if attempting to unregister
 *            the native VOL connector.
 *
 * \since 1.12.0
 */
H5_DLL herr_t H5VLunregister_connector(hid_t connector_id);
/**
 * \ingroup H5VL
 * \brief Determine if a VOL connector supports a particular
 *        optional callback operation.
 *
 * \obj_id
 * \param[in] subcls VOL subclass
 * \param[in] opt_type Option type
 * \param[out] flags Operation flags
 * \return \herr_t
 *
 * \since 1.12.1
 */
H5_DLL herr_t H5VLquery_optional(hid_t obj_id, H5VL_subclass_t subcls, int opt_type, uint64_t *flags);
/**
 * \ingroup H5VL
 * \brief Determines whether an object ID represents a native
 *        VOL connector object.
 *
 * \obj_id
 * \param[out] is_native Boolean determining whether object is a native VOL connector object
 * \return \herr_t
 *
 * \since 1.12.2
 */
H5_DLL herr_t H5VLobject_is_native(hid_t obj_id, hbool_t *is_native);

#ifdef __cplusplus
}
#endif

#endif /* H5VLpublic_H */
