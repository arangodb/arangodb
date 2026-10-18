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
 * Purpose:	The public header file for the family virtual file driver (VFD)
 */
#ifndef H5FDfamily_H
#define H5FDfamily_H

/** Initializer for the family VFD */
#define H5FD_FAMILY (H5FDperform_init(H5FD_family_init))

/** Identifier for the family VFD */
#define H5FD_FAMILY_VALUE H5_VFD_FAMILY

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the family VFD
 */
H5_DLL hid_t H5FD_family_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Sets the file access property list to use the family driver
 *
 * \fapl_id
 * \param[in] memb_size Size in bytes of each file member
 * \param[in] memb_fapl_id Identifier of file access property list for
 *            each family member
 * \returns \herr_t
 *
 * \details H5Pset_fapl_family() sets the file access property list identifier,
 *          \p fapl_id, to use the family driver.
 *
 *          \p memb_size is the size in bytes of each file member. This size
 *          will be saved in file when the property list \p fapl_id is used to
 *          create a new file. If \p fapl_id is used to open an existing file,
 *          \p memb_size has to be equal to the original size saved in file. A
 *          failure with an error message indicating the correct member size
 *          will be returned if \p memb_size does not match the size saved. If
 *          any user does not know the original size, #H5F_FAMILY_DEFAULT can be
 *          passed in. The library will retrieve the saved size.
 *
 *          \p memb_fapl_id is the identifier of the file access property list
 *          to be used for each family member.
 *
 * \version 1.8.0 Behavior of the \p memb_size parameter was changed.
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pset_fapl_family(hid_t fapl_id, hsize_t memb_size, hid_t memb_fapl_id);

/**
 * \ingroup FAPL
 *
 * \brief Returns file access property list information
 *
 * \fapl_id
 * \param[out] memb_size Size in bytes of each file member
 * \param[out] memb_fapl_id Identifier of file access property list for
 *             each family member
 * \returns \herr_t
 *
 * \details H5Pget_fapl_family() returns file access property list for use with
 *          the family driver. This information is returned through the output
 *          parameters.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pget_fapl_family(hid_t fapl_id, hsize_t *memb_size /*out*/, hid_t *memb_fapl_id /*out*/);

#ifdef __cplusplus
}
#endif

#endif
