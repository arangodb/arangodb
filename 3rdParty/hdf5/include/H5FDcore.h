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
 * Purpose:	The public header file for the core virtual file driver (VFD)
 */
#ifndef H5FDcore_H
#define H5FDcore_H

/** Initializer for the core VFD */
#define H5FD_CORE (H5FDperform_init(H5FD_core_init))

/** Identifier for the core VFD */
#define H5FD_CORE_VALUE H5_VFD_CORE

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the core VFD
 */
H5_DLL hid_t H5FD_core_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Modifies the file access property list to use the #H5FD_CORE driver
 *
 * \fapl_id
 * \param[in] increment Size, in bytes, of memory increments
 * \param[in] backing_store Boolean flag indicating whether to write the file
 *            contents to disk when the file is closed
 * \returns \herr_t
 *
 * \details H5Pset_fapl_core() modifies the file access property list to use the
 *          #H5FD_CORE driver.
 *
 *          The #H5FD_CORE driver enables an application to work with a file in
 *          memory, speeding reads and writes as no disk access is made. File
 *          contents are stored only in memory until the file is closed. The \p
 *          backing_store parameter determines whether file contents are ever
 *          written to disk.
 *
 *          \p increment specifies the increment by which allocated memory is to
 *          be increased each time more memory is required.
 *
 *          While using H5Fcreate() to create a core file, if the \p
 *          backing_store is set to 1 (true), the file contents are flushed to a
 *          file with the same name as this core file when the file is closed or
 *          access to the file is terminated in memory.
 *
 *          The application is allowed to open an existing file with #H5FD_CORE
 *          driver. While using H5Fopen() to open an existing file, if the \p
 *          backing_store is set to 1 (true) and the \c flags for H5Fopen() is set to
 *          #H5F_ACC_RDWR, any change to the file contents are saved to the file
 *          when the file is closed. If \p backing_store is set to 0 (false) and the \c
 *          flags for H5Fopen() is set to #H5F_ACC_RDWR, any change to the file
 *          contents will be lost when the file is closed. If the flags for
 *          H5Fopen() is set to #H5F_ACC_RDONLY, no change to the file is
 *          allowed either in memory or on file.
 *
 * \note Currently this driver cannot create or open family or multi files.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pset_fapl_core(hid_t fapl_id, size_t increment, hbool_t backing_store);

/**
 * \ingroup FAPL
 *
 * \brief Queries core file driver properties
 *
 * \fapl_id
 * \param[out] increment Size, in bytes, of memory increments
 * \param[out] backing_store Boolean flag indicating whether to write the file
 *             contents to disk when the file is closed
 * \returns \herr_t
 *
 * \details H5Pget_fapl_core() queries the #H5FD_CORE driver properties as set
 *          by H5Pset_fapl_core().
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pget_fapl_core(hid_t fapl_id, size_t *increment /*out*/, hbool_t *backing_store /*out*/);
#ifdef __cplusplus
}
#endif

#endif
