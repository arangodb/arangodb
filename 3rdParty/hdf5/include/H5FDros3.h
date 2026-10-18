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
 * Purpose:	The public header file for the read-only S3 (ros3) virtual file driver (VFD)
 */
#ifndef H5FDros3_H
#define H5FDros3_H

#ifdef H5_HAVE_ROS3_VFD

/** Initializer for the ros3 VFD \since 1.8.22 */
#define H5FD_ROS3 (H5FDperform_init(H5FD_ros3_init))

/** Identifier for the ros3 VFD \since 1.14.0 */
#define H5FD_ROS3_VALUE H5_VFD_ROS3

#else
/** Initializer for the ros3 VFD (disabled) \since 1.8.22 */
#define H5FD_ROS3       (H5I_INVALID_HID)

/** Identifier for the ros3 VFD (disabled) \since 1.14.0 */
#define H5FD_ROS3_VALUE H5_VFD_INVALID
#endif

#ifdef H5_HAVE_ROS3_VFD

/****************************************************************************
 *
 * Structure: H5FD_ros3_fapl_t
 *
 * Purpose:
 *
 *     H5FD_ros3_fapl_t is a public structure that is used to pass S3
 *     authentication data to the appropriate S3 VFD via the FAPL.  A pointer
 *     to an instance of this structure is a parameter to H5Pset_fapl_ros3()
 *     and H5Pget_fapl_ros3().
 *
 *
 *
 * `version` (int32_t)
 *
 *     Version number of the H5FD_ros3_fapl_t structure.  Any instance passed
 *     to the above calls must have a recognized version number, or an error
 *     will be flagged.
 *
 *     This field should be set to H5FD_CURR_ROS3_FAPL_T_VERSION.
 *
 * `authenticate` (hbool_t)
 *
 *     Flag true or false whether or not requests are to be authenticated
 *     with the AWS4 algorithm.
 *     If true, `aws_region`, `secret_id`, and `secret_key` must be populated.
 *     If false, those three components are unused.
 *
 * `aws_region` (char[])
 *
 *     String: name of the AWS "region" of the host, e.g. "us-east-1".
 *
 * `secret_id` (char[])
 *
 *     String: "Access ID" for the resource.
 *
 * `secret_key` (char[])
 *
 *     String: "Secret Access Key" associated with the ID and resource.
 *
 ****************************************************************************/

/**
 * \def H5FD_CURR_ROS3_FAPL_T_VERSION
 * The version number of the H5FD_ros3_fapl_t configuration
 * structure for the $H5FD_ROS3 driver.
 */
#define H5FD_CURR_ROS3_FAPL_T_VERSION 1

/**
 * \def H5FD_ROS3_MAX_REGION_LEN
 * Maximum string length for specifying the region of the S3 bucket.
 *
 * \since 1.10.6
 */
#define H5FD_ROS3_MAX_REGION_LEN 32
/**
 * \def H5FD_ROS3_MAX_SECRET_ID_LEN
 * Maximum string length for specifying the security ID.
 *
 * \since 1.10.6
 */
#define H5FD_ROS3_MAX_SECRET_ID_LEN 128
/**
 * \def H5FD_ROS3_MAX_SECRET_KEY_LEN
 * Maximum string length for specifying the security key.
 *
 * \since 1.10.6
 */
#define H5FD_ROS3_MAX_SECRET_KEY_LEN 128
/**
 * \def H5FD_ROS3_MAX_SECRET_TOK_LEN
 * Maximum string length for specifying the session/security token.
 */
#define H5FD_ROS3_MAX_SECRET_TOK_LEN 4096

/**
 * \struct H5FD_ros3_fapl_t
 * \brief Configuration structure for H5Pset_fapl_ros3() / H5Pget_fapl_ros3().
 *
 * \details H5FD_ros_fapl_t is a public structure that is used to pass
 *          configuration data to the #H5FD_ROS3 driver via a File Access
 *          Property List. A pointer to an instance of this structure is
 *          a parameter to H5Pset_fapl_ros3() and H5Pget_fapl_ros3().
 *
 * \var int32_t H5FD_ros3_fapl_t::version
 *      Version number of the H5FD_ros3_fapl_t structure. Any instance passed
 *      to H5Pset_fapl_ros3() / H5Pget_fapl_ros3() must have a recognized version
 *      number or an error will be raised. Currently, this field should be set
 *      to #H5FD_CURR_ROS3_FAPL_T_VERSION.
 *
 * \var hbool_t H5FD_ros3_fapl_t::authenticate
 *      A Boolean which specifies if security credentials should be used for
 *      accessing a S3 bucket.
 *      If true, `aws_region`, `secret_id`, and `secret_key` must be populated.
 *      If false, those three components are unused.
 *
 * \var char H5FD_ros3_fapl_t::aws_region[H5FD_ROS3_MAX_REGION_LEN + 1]
 *      A string which specifies the AWS region of the S3 bucket, e.g. "us-east-1".
 *
 * \var char H5FD_ros3_fapl_t::secret_id[H5FD_ROS3_MAX_SECRET_ID_LEN + 1]
 *      A string which specifies the security ID.
 *
 * \var char H5FD_ros3_fapl_t::secret_key[H5FD_ROS3_MAX_SECRET_KEY_LEN + 1]
 *      A string which specifies the security key.
 */
typedef struct H5FD_ros3_fapl_t {
    int32_t version;
    hbool_t authenticate;
    char    aws_region[H5FD_ROS3_MAX_REGION_LEN + 1];
    char    secret_id[H5FD_ROS3_MAX_SECRET_ID_LEN + 1];
    char    secret_key[H5FD_ROS3_MAX_SECRET_KEY_LEN + 1];
} H5FD_ros3_fapl_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the ros3 VFD
 */
H5_DLL hid_t H5FD_ros3_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Queries a File Access Property List for #H5FD_ROS3 file driver properties.
 *
 * \fapl_id
 * \param[out] fa_out Pointer to #H5FD_ROS3 driver configuration structure.
 * \returns \herr_t
 */
H5_DLL herr_t H5Pget_fapl_ros3(hid_t fapl_id, H5FD_ros3_fapl_t *fa_out);

/**
 * \ingroup FAPL
 *
 * \brief Modifies the specified File Access Property List to use the #H5FD_ROS3 driver.
 *
 * \fapl_id
 * \param[in] fa Pointer to #H5FD_ROS3 driver configuration structure.
 * \returns \herr_t
 */
H5_DLL herr_t H5Pset_fapl_ros3(hid_t fapl_id, const H5FD_ros3_fapl_t *fa);

/**
 * \ingroup FAPL
 *
 * \brief Queries a File Access Property List for #H5FD_ROS3 file driver session/security
 *        token.
 *
 * \fapl_id
 * \param[in] size Size of the provided char array for storing the session/security token.
 * \param[out] token Session/security token.
 * \returns \herr_t
 *
 * \since 1.14.2
 */
H5_DLL herr_t H5Pget_fapl_ros3_token(hid_t fapl_id, size_t size, char *token);

/**
 * \ingroup FAPL
 *
 * \brief Modifies the specified File Access Property List to use the #H5FD_ROS3 driver
 *        by adding the specified session/security token.
 *
 * \fapl_id
 * \param[in] token Session/security token.
 * \returns \herr_t
 *
 * \details H5Pset_fapl_ros3_token() modifies an existing File Access Property List which
 *          is used by #H5FD_ROS3 driver by adding or updating the session/security token
 *          of the property list. Be aware, to set the token first you need to create
 *          a proper File Access Property List using H5Pset_fapl_ros() and use this list
 *          as input argument of the function H5Pset_fapl_ros3_token().
 *
 *          Note, the session token is only needed when you want to access a S3 bucket
 *          using temporary security credentials.
 *
 * \since 1.14.2
 */
H5_DLL herr_t H5Pset_fapl_ros3_token(hid_t fapl_id, const char *token);

#ifdef __cplusplus
}
#endif

#endif /* H5_HAVE_ROS3_VFD */

#endif /* ifndef H5FDros3_H */
