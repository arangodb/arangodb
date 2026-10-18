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
 * Purpose:	The public header file for the onion virtual file driver (VFD)
 */
#ifndef H5FDonion_H
#define H5FDonion_H

/** Initializer for the onion VFD */
#define H5FD_ONION (H5FDperform_init(H5FD_onion_init))

/** Identifier for the onion VFD */
#define H5FD_ONION_VALUE H5_VFD_ONION

/** Current version of the onion VFD fapl info struct */
#define H5FD_ONION_FAPL_INFO_VERSION_CURR 1

#define H5FD_ONION_FAPL_INFO_CREATE_FLAG_ENABLE_PAGE_ALIGNMENT                                               \
    (0x0001u) /**<                                                                                           \
               * Onion history metadata will align to page_size.                                             \
               * Partial pages of unused space will occur in the file,                                       \
               * but may improve read performance from the backing store                                     \
               * on some systems.                                                                            \
               * If disabled (0), padding will not be inserted to align                                      \
               * to page boundaries.                                                                         \
               */

/**
 * Max length of a comment.
 * The buffer is defined to be this size + 1 to handle the NUL.
 */
#define H5FD_ONION_FAPL_INFO_COMMENT_MAX_LEN 255

/**
 * Indicates that you want the latest revision.
 */
#define H5FD_ONION_FAPL_INFO_REVISION_ID_LATEST UINT64_MAX

/**
 * Indicates how the new onion data will be stored.
 */
typedef enum H5FD_onion_target_file_constant_t {
    H5FD_ONION_STORE_TARGET_ONION, /**<
                                    * Onion history is stored in a single, separate "onion
                                    * file". Shares filename and path as hdf5 file (if any),
                                    * with only a different filename extension.
                                    */
} H5FD_onion_target_file_constant_t;

/**
 * Stores fapl information for creating onion VFD files.
 */
typedef struct H5FD_onion_fapl_info_t {
    uint8_t version;                                /**<
                                                     * Future-proofing identifier. Informs struct membership.
                                                     * Must equal H5FD_ONION_FAPL_VERSION_CURR to be considered valid.
                                                     */
    hid_t backing_fapl_id;                          /**<
                                                     * Backing or 'child' FAPL ID to handle I/O with the
                                                     * underlying backing store. It must use the same backing driver as the
                                                     * original file.
                                                     */
    uint32_t page_size;                             /**<
                                                     * page_size:   Size of the amended data pages. If opening an existing file,
                                                     *              must equal the existing page size or zero. If creating a new
                                                     *              file or an initial revision of an existing file, must be a
                                                     *              power of 2.
                                                     *
                                                     */
    H5FD_onion_target_file_constant_t store_target; /**<
                                                     * Identifies where the history data is stored.
                                                     */
    uint64_t revision_num;                          /**<
                                                     * Which revision to open. Valid values are 0 (the original file) or the
                                                     *              revision number of an existing revision.
                                                     *              H5FD_ONION_FAPL_INFO_REVISION_ID_LATEST refers to the most
                                                     *              recently-created revision in the history.
                                                     */
    uint8_t force_write_open;                       /**<
                                                     *              Flag to ignore the write-lock flag in the onion data
                                                     *              and attempt to open the file write-only anyway.
                                                     *              This may be relevant if, for example, the library crashed
                                                     *              while the file was open in write mode and the write-lock
                                                     *              flag was not cleared.
                                                     *              Must equal H5FD_ONION_FAPL_FLAG_FORCE_OPEN to enable.
                                                     *
                                                     */
    uint8_t creation_flags;                         /**<
                                                     * Flag used only when instantiating an onion file.
                                                     * If the relevant bit is set to a nonzero value, its feature
                                                     * will be enabled.
                                                     */
    char comment[H5FD_ONION_FAPL_INFO_COMMENT_MAX_LEN +
                 1]; /**<
                      * User-supplied NULL-terminated comment for a revision to be
                      * written.
                      * Cannot be longer than H5FD_ONION_FAPL_COMMENT_MAX_LEN.
                      * Ignored if part of a FAPL used to open in read mode.
                      */
} H5FD_onion_fapl_info_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the onion VFD
 */
H5_DLL hid_t H5FD_onion_init(void);

/**
 * --------------------------------------------------------------------------
 * \ingroup FAPL
 *
 * \brief get the onion info from the file access property list
 *
 * \fapl_id
 * \param[out] fa_out The pointer to the structure H5FD_onion_fapl_info_t
 *
 * \return \herr_t
 *
 * \details H5Pget_fapl_onion() retrieves the structure H5FD_onion_fapl_info_t
 *          from the file access property list that is set for the onion VFD
 *          driver.
 *
 * \since 1.14.0
 */
H5_DLL herr_t H5Pget_fapl_onion(hid_t fapl_id, H5FD_onion_fapl_info_t *fa_out);

/**
 * --------------------------------------------------------------------------
 * \ingroup FAPL
 *
 * \brief set the onion info for the file access property list
 *
 * \fapl_id
 * \param[in] fa The pointer to the structure H5FD_onion_fapl_info_t
 *
 * \return \herr_t
 *
 * \details H5Pset_fapl_onion() sets the structure H5FD_onion_fapl_info_t
 *          for the file access property list that is set for the onion VFD
 *          driver.
 *
 * \since 1.14.0
 */
H5_DLL herr_t H5Pset_fapl_onion(hid_t fapl_id, const H5FD_onion_fapl_info_t *fa);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5VFD
 *
 * \brief get the number of revisions
 *
 * \param[in] filename The name of the onion file
 * \param[in] fapl_id The ID of the file access property list
 * \param[out] revision_count The number of revisions
 *
 * \return \herr_t
 *
 * \details H5FDonion_get_revision_count() returns the number of revisions
 *          for an onion file. It takes the file name and file access property
 *          list that is set for the onion VFD driver.
 *
 *
 * \since 1.14.0
 */
H5_DLL herr_t H5FDonion_get_revision_count(const char *filename, hid_t fapl_id, uint64_t *revision_count);

#ifdef __cplusplus
}
#endif

#endif /* H5FDonion_H */
