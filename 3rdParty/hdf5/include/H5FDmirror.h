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
 * Purpose:	The public header file for the mirror virtual file driver (VFD)
 */

#ifndef H5FDmirror_H
#define H5FDmirror_H

#ifdef H5_HAVE_MIRROR_VFD

/** Initializer for the mirror VFD \since 1.10.7 */
#define H5FD_MIRROR (H5FDperform_init(H5FD_mirror_init))

/** Identifier for the mirror VFD */
#define H5FD_MIRROR_VALUE H5_VFD_MIRROR

/** Magic number to identify the H5FD_mirror_fapl_t struct */
#define H5FD_MIRROR_FAPL_MAGIC 0xF8DD514C

/**
 * The version number of the H5FD_mirror_fapl_t configuration
 * structure for the #H5FD_MIRROR driver
 */
#define H5FD_MIRROR_CURR_FAPL_T_VERSION 1

/** Max size of the remote_ip array in H5FD_mirror_fapl_t */
#define H5FD_MIRROR_MAX_IP_LEN 45 /* Max size of an IPv4-mapped IPv6 address */

/**
 *\struct H5FD_mirror_fapl_t
 * \brief Configuration structure for H5Pset_fapl_mirror() / H5Pget_fapl_mirror()
 *
 * \details H5FD_mirror_fapl_t is a public structure that is used to pass
 *          configuration data to the #H5FD_MIRROR driver via a File Access
 *          Property List. A pointer to an instance of this structure is
 *          a parameter to H5Pset_fapl_mirror() and H5Pget_fapl_mirror().
 *
 * \var uint32_t H5FD_mirror_fapl_t::magic
 *      Semi-unique number to sanity-check pointers to this structure type.
 *      Must equal H5FD_MIRROR_FAPL_MAGIC to be considered valid.
 *
 * \var uint32_t H5FD_mirror_fapl_t::version
 *      Version number of the H5FD_mirror_fapl_t structure. Any instance passed
 *      to H5Pset_fapl_mirror() / H5Pget_fapl_mirror() must have a recognized version
 *      number or an error will be raised. Currently, this field should be set
 *      to #H5FD_MIRROR_CURR_FAPL_T_VERSION.
 *
 * \var int H5FD_mirror_fapl_t::handshake_port
 *      Port number on the remote host.
 *
 * \var char H5FD_mirror_fapl_t::remote_ip[H5FD_MIRROR_MAX_IP_LEN + 1]
 *      IP address string of the remote host.
 */
typedef struct H5FD_mirror_fapl_t {
    uint32_t magic;
    uint32_t version;
    int      handshake_port;
    char     remote_ip[H5FD_MIRROR_MAX_IP_LEN + 1];
} H5FD_mirror_fapl_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the mirror VFD
 */
H5_DLL hid_t H5FD_mirror_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Queries a File Access Property List for #H5FD_MIRROR file driver properties
 *
 * \fapl_id
 * \param[out] fa_out Pointer to #H5FD_MIRROR driver configuration structure
 * \returns \herr_t
 *
 * \details H5Pget_fapl_mirror() queries the #H5FD_MIRROR driver properties as set
 *          by H5Pset_fapl_mirror().
 *
 * \since 1.10.7
 */
H5_DLL herr_t H5Pget_fapl_mirror(hid_t fapl_id, H5FD_mirror_fapl_t *fa_out);

/**
 * \ingroup FAPL
 *
 * \brief Modifies the file access property list to use the #H5FD_MIRROR driver
 *
 * \fapl_id
 * \param[in] fa Pointer to #H5FD_MIRROR driver configuration structure
 *
 * \returns \herr_t
 *
 * \details H5Pset_fapl_mirror() modifies the file access property list to use the
 *          #H5FD_MIRROR driver.
 *
 * \since 1.10.7
 */
H5_DLL herr_t H5Pset_fapl_mirror(hid_t fapl_id, H5FD_mirror_fapl_t *fa);

#ifdef __cplusplus
}
#endif

#else /* H5_HAVE_MIRROR_VFD */

#define H5FD_MIRROR (H5I_INAVLID_HID)

#endif /* H5_HAVE_MIRROR_VFD */

#endif /* H5FDmirror_H */
