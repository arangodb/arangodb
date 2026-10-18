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
 * Purpose:	The public header file for the POSOX I/O (sec2 - "POSIX section 2")
 *          virtual file driver (VFD)
 */
#ifndef H5FDsec2_H
#define H5FDsec2_H

/** Initializer for the sec2 VFD */
#define H5FD_SEC2 (H5FDperform_init(H5FD_sec2_init))

/** Identifier for the sec2 VFD */
#define H5FD_SEC2_VALUE H5_VFD_SEC2

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the sec2 VFD
 */
H5_DLL hid_t H5FD_sec2_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Modifies the file access property list to use the #H5FD_SEC2 driver
 *
 * \fapl_id
 *
 * \returns \herr_t
 *
 * \details H5Pset_fapl_sec2() modifies the file access property list to use the
 *          #H5FD_SEC2 driver.
 *
 * \since 1.4.0
 */
H5_DLL herr_t H5Pset_fapl_sec2(hid_t fapl_id);

#ifdef __cplusplus
}
#endif

#endif
