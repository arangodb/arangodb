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
 * Purpose: The public header file for the Hadoop Distributed File System
 *          (hdfs) virtual file driver (VFD)
 */

#ifndef H5FDhdfs_H
#define H5FDhdfs_H

#ifdef H5_HAVE_LIBHDFS

/** Initializer for the hdfs VFD */
#define H5FD_HDFS (H5FDperform_init(H5FD_hdfs_init))

/** Identifier for the hdfs VFD */
#define H5FD_HDFS_VALUE H5_VFD_HDFS

#else

/** Initializer for the hdfs VFD (disabled) \since 1.8.22 */
#define H5FD_HDFS       (H5I_INVALID_HID)

/** Identifier for the hdfs VFD (disabled) */
#define H5FD_HDFS_VALUE H5_VFD_INVALID

#endif /* H5_HAVE_LIBHDFS */

#ifdef H5_HAVE_LIBHDFS
#ifdef __cplusplus
extern "C" {
#endif

/**
 * The version number of the H5FD_hdfs_fapl_t configuration
 * structure for the #H5FD_HDFS driver
 */
#define H5FD__CURR_HDFS_FAPL_T_VERSION 1

/** Max size of the node name \since 1.8.22 1.10.6 */
#define H5FD__HDFS_NODE_NAME_SPACE 128
/** Max size of the user name \since 1.8.22 1.10.6 */
#define H5FD__HDFS_USER_NAME_SPACE 128
/** Max size of the kerberos cache path \since 1.8.22 1.10.6 */
#define H5FD__HDFS_KERB_CACHE_PATH_SPACE 128

/**
 *\struct H5FD_hdfs_fapl_t
 * \brief Configuration structure for H5Pset_fapl_hdfs() / H5Pget_fapl_hdfs()
 *
 * \details H5FD_hdfs_fapl_t is a public structure that is used to pass
 *          configuration data to the #H5FD_HDFS driver via a File Access
 *          Property List. A pointer to an instance of this structure is
 *          a parameter to H5Pset_fapl_hdfs() and H5Pget_fapl_hdfs().
 *
 * \var int32_t H5FD_hdfs_fapl_t::version
 *      Version number of the H5FD_hdfs_fapl_t structure. Any instance passed
 *      to H5Pset_fapl_hdfs() / H5Pget_fapl_hdfs() must have a recognized version
 *      number or an error will be raised. Currently, this field should be set
 *      to #H5FD__CURR_HDFS_FAPL_T_VERSION.
 *
 * \var char H5FD_hdfs_fapl_t::namenode_name[H5FD__HDFS_NODE_NAME_SPACE + 1]
 *      Name of "Name Node" to access as the HDFS server
 *
 * \var int32_t H5FD_hdfs_fapl_t::namenode_port
 *      Port number to use to connect with Name Node
 *
 * \var char H5FD_hdfs_fapl_t::user_name[H5FD__HDFS_USER_NAME_SPACE + 1]
 *      Username to use when accessing file
 *
 * \var char H5FD_hdfs_fapl_t::kerberos_ticket_cache[H5FD__HDFS_KERB_CACHE_PATH_SPACE + 1]
 *      Path to the location of the Kerberos authentication cache
 *
 * \var int32_t H5FD_hdfs_fapl_t::stream_buffer_size
 *      Size (in bytes) of the file read stream buffer
 */
typedef struct H5FD_hdfs_fapl_t {
    int32_t version;
    char    namenode_name[H5FD__HDFS_NODE_NAME_SPACE + 1];
    int32_t namenode_port;
    char    user_name[H5FD__HDFS_USER_NAME_SPACE + 1];
    char    kerberos_ticket_cache[H5FD__HDFS_KERB_CACHE_PATH_SPACE + 1];
    int32_t stream_buffer_size;
} H5FD_hdfs_fapl_t;

/** @private
 *
 * \brief Private initializer for the hdfs VFD
 */
H5_DLL hid_t H5FD_hdfs_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Modifies the file access property list to use the #H5FD_HDFS driver
 *
 * \fapl_id
 * \param[in] fa Pointer to #H5FD_HDFS driver configuration structure
 *
 * \returns \herr_t
 *
 * \details H5Pset_fapl_hdfs() modifies the file access property list to use the
 *          #H5FD_HDFS driver.
 *
 * \since 1.10.6
 */
H5_DLL herr_t H5Pset_fapl_hdfs(hid_t fapl_id, H5FD_hdfs_fapl_t *fa);

/**
 * \ingroup FAPL
 *
 * \brief Queries a File Access Property List for #H5FD_HDFS file driver properties
 *
 * \fapl_id
 * \param[out] fa_out Pointer to #H5FD_HDFS driver configuration structure
 * \returns \herr_t
 *
 * \details H5Pget_fapl_hdfs() queries the #H5FD_HDFS driver properties as set
 *          by H5Pset_fapl_hdfs().
 *
 * \since 1.10.6
 */
H5_DLL herr_t H5Pget_fapl_hdfs(hid_t fapl_id, H5FD_hdfs_fapl_t *fa_out);

#ifdef __cplusplus
}
#endif
#endif /* H5_HAVE_LIBHDFS */

#endif /* ifndef H5FDhdfs_H */
