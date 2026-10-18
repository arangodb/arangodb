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
 * Purpose:	The public header file for the splitter virtual file driver (VFD)
 */

#ifndef H5FDsplitter_H
#define H5FDsplitter_H

/** Initializer for the splitter VFD */
#define H5FD_SPLITTER (H5FDperform_init(H5FD_splitter_init))

/** Identifier for the splitter VFD */
#define H5FD_SPLITTER_VALUE H5_VFD_SPLITTER

/** The version of the H5FD_splitter_vfd_config_t structure used */
#define H5FD_CURR_SPLITTER_VFD_CONFIG_VERSION 1

/**
 * Maximum length of a filename/path string in the Write-Only channel,
 * including the NULL-terminator. \since 1.10.7
 */
#define H5FD_SPLITTER_PATH_MAX 4096

/** Semi-unique constant used to help identify structure pointers \since 1.10.7 */
#define H5FD_SPLITTER_MAGIC 0x2B916880

//! <!-- [H5FD_splitter_vfd_config_t_snip] -->
/**
 * Configuration options for setting up the Splitter VFD
 */
typedef struct H5FD_splitter_vfd_config_t {
    int32_t      magic;   /**< Magic number to identify this struct. Must be \p H5FD_SPLITTER_MAGIC. */
    unsigned int version; /**< Version number of this struct. Currently must be \p
                             H5FD_CURR_SPLITTER_VFD_CONFIG_VERSION. */
    hid_t rw_fapl_id;     /**< File-access property list for setting up the read/write channel. Can be \p
                             H5P_DEFAULT. */
    hid_t wo_fapl_id; /**< File-access property list for setting up the read-only channel. The selected VFD
                         must support the \p H5FD_FEAT_DEFAULT_VFD_COMPATIBLE flag. Can be \p H5P_DEFAULT. */
    char wo_path[H5FD_SPLITTER_PATH_MAX + 1];       /**< Path to the write-only file */
    char log_file_path[H5FD_SPLITTER_PATH_MAX + 1]; /**< Path to the log file, which will be created on HDF5
                                                       file open (existing files will be clobbered). Can be
                                                       NULL, in which case no logging output is generated. */
    hbool_t ignore_wo_errs;                         /**< Whether to ignore errors on the write-only channel */
} H5FD_splitter_vfd_config_t;
//! <!-- [H5FD_splitter_vfd_config_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the splitter VFD
 */
H5_DLL hid_t H5FD_splitter_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Sets the file access property list to use the splitter driver
 *
 * \fapl_id
 * \param[in] config_ptr Configuration options for the VFD
 * \returns \herr_t
 *
 * \details H5Pset_fapl_splitter() sets the file access property list identifier,
 *          \p fapl_id, to use the splitter driver.
 *
 *          The splitter VFD echoes file manipulation (e.g. create, truncate)
 *          and write calls to a second, write-only file.
 *
 *          \note The splitter VFD should not be confused with the split VFD,
 *          which is a simplification of the multi VFD and creates separate
 *          files for metadata and data.
 *
 * \since 1.10.7, 1.12.1
 */
H5_DLL herr_t H5Pset_fapl_splitter(hid_t fapl_id, H5FD_splitter_vfd_config_t *config_ptr);

/**
 * \ingroup FAPL
 *
 * \brief Gets splitter driver properties from the the file access property list
 *
 * \fapl_id
 * \param[out] config_ptr Configuration options for the VFD
 * \returns \herr_t
 *
 * \details H5Pset_fapl_splitter() sets the file access property list identifier,
 *          \p fapl_id, to use the splitter driver.
 *
 *          The splitter VFD echoes file manipulation (e.g. create, truncate)
 *          and write calls to a second file.
 *
 *          \note The splitter VFD should not be confused with the split VFD,
 *          which is a simplification of the multi VFD and creates separate
 *          files for metadata and data.
 *
 * \since 1.10.7, 1.12.1
 */
H5_DLL herr_t H5Pget_fapl_splitter(hid_t fapl_id, H5FD_splitter_vfd_config_t *config_ptr);

#ifdef __cplusplus
}
#endif

#endif
