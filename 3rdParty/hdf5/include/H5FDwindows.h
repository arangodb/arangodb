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
 * Purpose:	The public header file for the Windows virtual file driver (VFD)
 *
 *          This VFD uses no Win32 API calls directly (though it may be
 *          rewritten to do so in the future). It is currently defined to
 *          be the sec2 VFD.
 */
#ifndef H5FDwindows_H
#define H5FDwindows_H

/** Initializer for the Windows VFD */
#define H5FD_WINDOWS (H5FD_sec2_init())

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \ingroup FAPL
 *
 * \brief Sets the Windows I/O driver
 *
 * \fapl_id
 * \returns \herr_t
 *
 * \details H5Pset_fapl_windows() sets the default HDF5 Windows I/O driver on
 *          Windows systems.
 *
 *          Since the HDF5 library uses this driver, #H5FD_WINDOWS, by default
 *          on Windows systems, it is not normally necessary for a user
 *          application to call H5Pset_fapl_windows(). While it is not
 *          recommended, there may be times when a user chooses to set a
 *          different HDF5 driver, such as the standard I/O driver (#H5FD_STDIO)
 *          or the sec2 driver (#H5FD_SEC2), in a Windows
 *          application. H5Pset_fapl_windows() is provided so that the
 *          application can return to the Windows I/O driver when the time
 *          comes.
 *
 *          Only the Windows driver is tested on Windows systems; other drivers
 *          are used at the application's and the user's risk.
 *
 *          Furthermore, the Windows driver is tested and available only on
 *          Windows systems; it is not available on non-Windows systems.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Pset_fapl_windows(hid_t fapl_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* H5FDwindows_H */
