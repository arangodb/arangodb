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
 * Purpose:	The public header file for the multi virtual file driver (VFD)
 */
#ifndef H5FDmulti_H
#define H5FDmulti_H

/** Initializer for the multi VFD */
#define H5FD_MULTI (H5FDperform_init(H5FD_multi_init))

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the multi VFD
 */
H5_DLL hid_t H5FD_multi_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Sets up use of the multi-file driver
 *
 * \fapl_id
 * \param[in] memb_map Maps memory usage types to other memory usage types
 * \param[in] memb_fapl Property list for each memory usage type
 * \param[in] memb_name Name generator for names of member files
 * \param[in] memb_addr The offsets within the virtual address space, from 0
 *           (zero) to #HADDR_MAX, at which each type of data storage begins
 * \param[in] relax Allows read-only access to incomplete file sets when \c true
 * \returns \herr_t
 *
 * \details H5Pset_fapl_multi() sets the file access property list \p fapl_id to
 *          use the multi-file driver.
 *
 *          The multi-file driver enables different types of HDF5 data and
 *          metadata to be written to separate files. These files are viewed by
 *          the HDF5 library and the application as a single virtual HDF5 file
 *          with a single HDF5 file address space. The types of data that can be
 *          broken out into separate files include raw data, the superblock,
 *          B-tree data, global heap data, local heap data, and object
 *          headers. At the programmer's discretion, two or more types of data
 *          can be written to the same file while other types of data are
 *          written to separate files.
 *
 *          The array \p memb_map maps memory usage types to other memory usage
 *          types and is the mechanism that allows the caller to specify how
 *          many files are created. The array contains #H5FD_MEM_NTYPES entries,
 *          which are either the value #H5FD_MEM_DEFAULT or a memory usage
 *          type. The number of unique values determines the number of files
 *          that are opened.
 *
 *          The array \p memb_fapl contains a property list for each memory
 *          usage type that will be associated with a file.
 *
 *          The array \p memb_name should be a name generator (a
 *          \TText{printf}-style format with a \TText{%s} which will be replaced
 *          with the name passed to H5FDopen(), usually from H5Fcreate() or
 *          H5Fopen()).
 *
 *          The array \p memb_addr specifies the offsets within the virtual
 *          address space, from 0 (zero) to #HADDR_MAX, at which each type of
 *          data storage begins.
 *
 *          If \p relax is set to 1 (true), then opening an existing file for
 *          read-only access will not fail if some file members are
 *          missing. This allows a file to be accessed in a limited sense if
 *          just the meta data is available.
 *
 *          Default values for each of the optional arguments are as follows:
 *          <table>
 *          <tr>
 *          <td>\p memb_map</td>
 *          <td>The default member map contains the value #H5FD_MEM_DEFAULT for each element.</td>
 *          </tr>
 *          <tr>
 *          <td>
 *          \p memb_fapl
 *          </td>
 *          <td>
 *          The default value is #H5P_DEFAULT for each element.
 *          </td>
 *          </tr>
 *          <tr>
 *          <td>
 *          \p memb_name
 *          </td>
 *          <td>
 *          The default string is \TText{%s-X.h5} where \c X is one of the following letters:
 *          - \c s for #H5FD_MEM_SUPER
 *          - \c b for #H5FD_MEM_BTREE
 *          - \c r for #H5FD_MEM_DRAW
 *          - \c g for #H5FD_MEM_GHEAP
 *          - \c l for #H5FD_MEM_LHEAP
 *          - \c o for #H5FD_MEM_OHDR
 *          </td>
 *          </tr>
 *          <tr>
 *          <td>
 *          \p memb_addr
 *          </td>
 *          <td>
 *          The default setting is that the address space is equally divided
 *          among all of the elements:
 *          - #H5FD_MEM_SUPER \TText{-> 0 * (HADDR_MAX/6)}
 *          - #H5FD_MEM_BTREE \TText{-> 1 * (HADDR_MAX/6)}
 *          - #H5FD_MEM_DRAW \TText{-> 2 * (HADDR_MAX/6)}
 *          - #H5FD_MEM_GHEAP \TText{-> 3 * (HADDR_MAX/6)}
 *          - #H5FD_MEM_LHEAP \TText{-> 4 * (HADDR_MAX/6)}
 *          - #H5FD_MEM_OHDR \TText{-> 5 * (HADDR_MAX/6)}
 *          </td>
 *          </tr>
 *          </table>
 *
 * \par Example:
 * The following code sample sets up a multi-file access property list that
 * partitions data into meta and raw files, each being one-half of the address:\n
 * \code
 * H5FD_mem_t mt, memb_map[H5FD_MEM_NTYPES];
 * hid_t memb_fapl[H5FD_MEM_NTYPES];
 * const char *memb[H5FD_MEM_NTYPES];
 * haddr_t memb_addr[H5FD_MEM_NTYPES];
 *
 * // The mapping...
 * for (mt=0; mt<H5FD_MEM_NTYPES; mt++) {
 *   memb_map[mt] = H5FD_MEM_SUPER;
 * }
 * memb_map[H5FD_MEM_DRAW] = H5FD_MEM_DRAW;
 *
 * // Member information
 * memb_fapl[H5FD_MEM_SUPER] = H5P_DEFAULT;
 * memb_name[H5FD_MEM_SUPER] = "%s.meta";
 * memb_addr[H5FD_MEM_SUPER] = 0;
 *
 * memb_fapl[H5FD_MEM_DRAW] = H5P_DEFAULT;
 * memb_name[H5FD_MEM_DRAW] = "%s.raw";
 * memb_addr[H5FD_MEM_DRAW] = HADDR_MAX/2;
 *
 * hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
 * H5Pset_fapl_multi(fapl, memb_map, memb_fapl,
 *                   memb_name, memb_addr, true);
 * \endcode
 *
 * \version 1.6.3 \p memb_name parameter type changed to \TText{const char* const*}.
 * \since 1.4.0
 */
H5_DLL herr_t H5Pset_fapl_multi(hid_t fapl_id, const H5FD_mem_t *memb_map, const hid_t *memb_fapl,
                                const char *const *memb_name, const haddr_t *memb_addr, hbool_t relax);

/**
 * \ingroup FAPL
 *
 * \brief Returns information about the multi-file access property list
 *
 * \fapl_id
 * \param[out] memb_map Maps memory usage types to other memory usage types
 * \param[out] memb_fapl Property list for each memory usage type
 * \param[out] memb_name Name generator for names of member files
 * \param[out] memb_addr The offsets within the virtual address space, from 0
 *           (zero) to #HADDR_MAX, at which each type of data storage begins
 * \param[out] relax Allows read-only access to incomplete file sets when \c true
 * \returns \herr_t
 *
 * \details H5Pget_fapl_multi() returns information about the multi-file access
 *          property list.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pget_fapl_multi(hid_t fapl_id, H5FD_mem_t *memb_map /*out*/, hid_t *memb_fapl /*out*/,
                                char **memb_name /*out*/, haddr_t *memb_addr /*out*/, hbool_t *relax /*out*/);

/**
 * \ingroup FAPL
 *
 * \brief Emulates the old split file driver
 *
 * \fapl_id{fapl}
 * \param[in] meta_ext Metadata filename extension
 * \param[in] meta_plist_id File access property list identifier for the metadata file
 * \param[in] raw_ext Raw data filename extension
 * \param[in] raw_plist_id
 * \returns \herr_t
 *
 * \details H5Pset_fapl_split() is a compatibility function that enables the
 *          multi-file driver to emulate the split driver from HDF5 Releases 1.0
 *          and 1.2. The split file driver stored metadata and raw data in
 *          separate files but provided no mechanism for separating types of
 *          metadata.
 *
 *          \p fapl is a file access property list identifier.
 *
 *          \p meta_ext is the filename extension for the metadata file. The
 *          extension is appended to the name passed to H5FDopen(), usually from
 *          H5Fcreate() or H5Fopen(), to form the name of the metadata file. If
 *          the string \TText{%s} is used in the extension, it works like the
 *          name generator as in H5Pset_fapl_multi().
 *
 *          \p meta_plist_id is the file access property list identifier for the
 *          metadata file.
 *
 *          \p raw_ext is the filename extension for the raw data file. The
 *          extension is appended to the name passed to H5FDopen(), usually from
 *          H5Fcreate() or H5Fopen(), to form the name of the raw data file. If
 *          the string \TText{%s} is used in the extension, it works like the
 *          name generator as in H5Pset_fapl_multi().
 *
 *          \p raw_plist_id is the file access property list identifier for the
 *          raw data file.
 *
 *          If a user wishes to check to see whether this driver is in use, the
 *          user must call H5Pget_driver() and compare the returned value to the
 *          string #H5FD_MULTI. A positive match will confirm that the multi
 *          driver is in use; HDF5 provides no mechanism to determine whether it
 *          was called as the special case invoked by H5Pset_fapl_split().
 *
 * \par Example:
 * \code
 * // Example 1: Both metadata and raw data files are in the same
 * //            directory. Use Station1-m.h5 and Station1-r.h5 as
 * //            the metadata and raw data files.
 * hid_t fapl, fid;
 * fapl = H5Pcreate(H5P_FILE_ACCESS);
 * H5Pset_fapl_split(fapl, "-m.h5", H5P_DEFAULT, "-r.h5", H5P_DEFAULT);
 * fid=H5Fcreate("Station1",H5F_ACC_TRUNC,H5P_DEFAULT,fapl);
 *
 * // Example 2: metadata and raw data files are in different
 * //            directories.  Use PointA-m.h5 and /pfs/PointA-r.h5 as
 * //            the metadata and raw data files.
 * hid_t fapl, fid;
 * fapl = H5Pcreate(H5P_FILE_ACCESS);
 * H5Pset_fapl_split(fapl, "-m.h5", H5P_DEFAULT, "/pfs/%s-r.h5", H5P_DEFAULT);
 * fid=H5Fcreate("PointA",H5F_ACC_TRUNC,H5P_DEFAULT,fapl);
 * \endcode
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pset_fapl_split(hid_t fapl, const char *meta_ext, hid_t meta_plist_id, const char *raw_ext,
                                hid_t raw_plist_id);
#ifdef __cplusplus
}
#endif

#endif
