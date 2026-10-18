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
 * Purpose:	The public header file for the MPI-I/O (mpio) virtual file driver (VFD)
 */
#ifndef H5FDmpio_H
#define H5FDmpio_H

#ifdef H5_HAVE_PARALLEL

/** Initializer for the mpio VFD */
#define H5FD_MPIO (H5FDperform_init(H5FD_mpio_init))

#else

/** Initializer for the mpio VFD (disabled) */
#define H5FD_MPIO (H5I_INVALID_HID)

#endif

#ifdef H5_HAVE_PARALLEL

#if defined(H5F_DEBUG) && !defined(H5FDmpio_DEBUG)
/** Turn mpio VFD debugging on (requires H5F_DEBUG) */
#define H5FDmpio_DEBUG
#endif

/* Global var whose value comes from environment variable */
/* (Defined in H5FDmpio.c) */
H5_DLLVAR hbool_t H5FD_mpi_opt_types_g;

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the mpio VFD
 */
H5_DLL hid_t H5FD_mpio_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Stores MPI IO communicator information to the file access property list
 *
 * \fapl_id
 * \param[in] comm MPI communicator
 * \param[in] info MPI info object
 * \returns \herr_t
 *
 * \details H5Pset_fapl_mpio() stores the user-supplied MPI IO parameters \p
 *          comm, for communicator, and \p info, for information, in the file
 *          access property list \p fapl_id. That property list can then be used
 *          to create and/or open a file.
 *
 *          H5Pset_fapl_mpio() is available only in the parallel HDF5 library
 *          and is not a collective function.
 *
 *          \p comm is the MPI communicator to be used for file open, as defined
 *          in \c MPI_File_open of MPI. This function makes a duplicate of the
 *          communicator, so modifications to \p comm after this function call
 *          returns have no effect on the file access property list.
 *
 *          \p info is the MPI Info object to be used for file open, as defined
 *          in MPI_File_open() of MPI. This function makes a duplicate copy of
 *          the Info object, so modifications to the Info object after this
 *          function call returns will have no effect on the file access
 *          property list.
 *
 *          If the file access property list already contains previously-set
 *          communicator and Info values, those values will be replaced and the
 *          old communicator and Info object will be freed.
 *
 * \note Raw dataset chunk caching is not currently supported when using this
 *       file driver in read/write mode. All calls to H5Dread() and H5Dwrite()
 *       will access the disk directly, and H5Pset_cache() and
 *       H5Pset_chunk_cache() will have no effect on performance.\n
 *       Raw dataset chunk caching is supported when this driver is used in
 *       read-only mode.
 *
 * \version 1.4.5 Handling of the MPI Communicator and Info object changed at
 *          this release. A duplicate of each of these is now stored in the property
 *          list instead of pointers to each.
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pset_fapl_mpio(hid_t fapl_id, MPI_Comm comm, MPI_Info info);

/**
 * \ingroup FAPL
 *
 * \brief Returns MPI IO communicator information
 *
 * \fapl_id
 * \param[out] comm MPI communicator
 * \param[out] info MPI info object
 * \returns \herr_t
 *
 * \details If the file access property list is set to the #H5FD_MPIO driver,
 *          H5Pget_fapl_mpio() returns duplicates of the stored MPI communicator
 *          and Info object through the \p comm and \p info pointers, if those
 *          values are non-null.
 *
 *          Since the MPI communicator and Info object are duplicates of the
 *          stored information, future modifications to the access property list
 *          will not affect them. It is the responsibility of the application to
 *          free these objects.
 *
 * \version 1.4.5 Handling of the MPI Communicator and Info object changed at
 *          this release. A duplicate of each of these is now stored in the
 *          property list instead of pointers to each.
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pget_fapl_mpio(hid_t fapl_id, MPI_Comm *comm /*out*/, MPI_Info *info /*out*/);

/**
 * \ingroup DXPL
 *
 * \brief Sets data transfer mode
 *
 * \dxpl_id
 * \param[in] xfer_mode Transfer mode
 * \returns \herr_t
 *
 * \details H5Pset_dxpl_mpio() sets the data transfer property list \p dxpl_id
 *          to use transfer mode \p xfer_mode. The property list can then be
 *          used to control the I/O transfer mode during data I/O operations.
 *
 *          Valid transfer modes are #H5FD_MPIO_INDEPENDENT (default) and
 *          #H5FD_MPIO_COLLECTIVE.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pset_dxpl_mpio(hid_t dxpl_id, H5FD_mpio_xfer_t xfer_mode);

/**
 * \ingroup DXPL
 *
 * \brief Returns the data transfer mode
 *
 * \dxpl_id
 * \param[out] xfer_mode Transfer mode
 * \returns \herr_t
 *
 * \details H5Pget_dxpl_mpio() queries the data transfer mode currently set in
 *          the data transfer property list \p dxpl_id.
 *
 *          Upon return, \p xfer_mode contains the data transfer mode, if it is
 *          non-null.
 *
 *          H5Pget_dxpl_mpio() is not a collective function.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pget_dxpl_mpio(hid_t dxpl_id, H5FD_mpio_xfer_t *xfer_mode /*out*/);

/**
 * \ingroup DXPL
 *
 * \brief Sets low-level data transfer mode
 *
 * \dxpl_id
 * \param[in] opt_mode Transfer mode
 * \returns \herr_t
 *
 * \details H5Pset_dxpl_mpio_collective_opt() sets the data transfer property
 *          list \p dxpl_id to use transfer mode \p opt_mode when performing
 *          I/O. This allows the application to specify collective I/O at the
 *          HDF5 interface level (with the H5Pset_dxpl_mpio() API routine),
 *          while controlling whether the actual I/O is performed collectively
 *          (e.g., via MPI_File_write_at_all) or independently (e.g., via
 *          MPI_File_write_at). If the collectivity setting at the HDF5
 *          interface level (set via H5Pset_dxpl_mpio()) is not set to
 *          H5FD_MPIO_COLLECTIVE, this setting will be ignored.
 *
 *          Valid transfer modes are #H5FD_MPIO_COLLECTIVE_IO (default) and
 *          #H5FD_MPIO_INDIVIDUAL_IO.
 *
 * \since 1.4.0
 *
 */
H5_DLL herr_t H5Pset_dxpl_mpio_collective_opt(hid_t dxpl_id, H5FD_mpio_collective_opt_t opt_mode);

/**
 * \ingroup DXPL
 *
 * \brief Sets a flag specifying linked-chunk I/O or multi-chunk I/O
 *
 * \dxpl_id
 * \param[in] opt_mode Transfer mode
 * \returns \herr_t
 *
 * \details H5Pset_dxpl_mpio_chunk_opt() specifies whether I/O is to be
 *          performed as linked-chunk I/O or as multi-chunk I/O. This function
 *          overrides the HDF5 library's internal algorithm for determining
 *          which mechanism to use.
 *
 *          When an application uses collective I/O with chunked storage, the
 *          HDF5 library normally uses an internal algorithm to determine
 *          whether that I/O activity should be conducted as one linked-chunk
 *          I/O or as multi-chunk I/O. H5Pset_dxpl_mpio_chunk_opt() is provided
 *          so that an application can override the library's algorithm in
 *          circumstances where the library might lack the information needed to
 *          make an optimal decision.
 *
 *          H5Pset_dxpl_mpio_chunk_opt() works by setting one of the following
 *          flags in the parameter \p opt_mode:
 *          - #H5FD_MPIO_CHUNK_ONE_IO -	Do one-link chunked I/O
 *          - #H5FD_MPIO_CHUNK_MULTI_IO - Do multi-chunked I/O
 *
 *          This function works by setting a corresponding property in the
 *          dataset transfer property list \p dxpl_id.
 *
 *          The library performs I/O in the specified manner unless it
 *          determines that the low-level MPI IO package does not support the
 *          requested behavior; in such cases, the HDF5 library will internally
 *          use independent I/O.
 *
 *          Use of this function is optional.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Pset_dxpl_mpio_chunk_opt(hid_t dxpl_id, H5FD_mpio_chunk_opt_t opt_mode);

/**
 * \ingroup DXPL
 *
 * \brief Sets a numeric threshold for linked-chunk I/O
 *
 * \dxpl_id
 * \param[in] num_chunk_per_proc
 * \returns \herr_t
 *
 * \details H5Pset_dxpl_mpio_chunk_opt_num() sets a numeric threshold for the
 *          use of linked-chunk I/O.
 *
 *          The library will calculate the average number of chunks selected by
 *          each process when doing collective access with chunked storage. If
 *          the number is greater than the threshold set in \p
 *          num_chunk_per_proc, the library will use linked-chunk I/O;
 *          otherwise, a separate I/O process will be invoked for each chunk
 *          (multi-chunk I/O).
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Pset_dxpl_mpio_chunk_opt_num(hid_t dxpl_id, unsigned num_chunk_per_proc);

/**
 * \ingroup DXPL
 *
 * \brief Sets a ratio threshold for collective I/O
 *
 * \dxpl_id
 * \param[in] percent_num_proc_per_chunk
 * \returns \herr_t
 *
 * \details H5Pset_dxpl_mpio_chunk_opt_ratio() sets a threshold for the use of
 *          collective I/O based on the ratio of processes with collective
 *          access to a dataset with chunked storage. The decision whether to
 *          use collective I/O is made on a per-chunk basis.
 *
 *          The library will calculate the percentage of the total number of
 *          processes, the ratio, that hold selections in each chunk. If that
 *          percentage is greater than the threshold set in \p
 *          percent_proc_per_chunk, the library will do collective I/O for this
 *          chunk; otherwise, independent I/O will be done for the chunk.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Pset_dxpl_mpio_chunk_opt_ratio(hid_t dxpl_id, unsigned percent_num_proc_per_chunk);
#ifdef __cplusplus
}
#endif

#endif /* H5_HAVE_PARALLEL */

#endif
