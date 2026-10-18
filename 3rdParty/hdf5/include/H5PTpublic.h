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

#ifndef H5PTpublic_H
#define H5PTpublic_H

#ifdef __cplusplus
extern "C" {
#endif

/** \page H5PT_UG HDF5 High Level Packet Table
 * @todo Under Construction
 */

/**\defgroup H5PT HDF5 Packet Table APIs (H5PT)
 *
 * <em>Creating and manipulating HDF5 datasets to support append-
 * and read-only operations on table data (H5PT)</em>
 *
 * The HDF5 Packet Table API is designed to allow records to be
 * appended to and read from a table. Packet Table datasets are
 * chunked, allowing them to grow as needed.
 *
 * The Packet Table API, with the H5PT prefix, is not to be confused with
 * the H5TB Table API (H5TB prefix). The H5TB APIs are stateless
 * (H5TB Tables do not need to be opened or closed) but H5PT Packet Tables
 * require less performance overhead. Also, H5TB Tables support insertions
 * and deletions, while H5PT Packet Tables support only append operations.
 * H5TB functions should not be called on tables created with the
 * H5PT API, or vice versa.
 *
 * Packet Tables are datasets in an HDF5 file, so while their contents
 * should not be changed outside of the H5PT API calls, the datatypes of
 * Packet Tables can be queried using \ref H5Dget_type.
 * Packet Tables can also be given attributes using the normal HDF5 APIs.
 *
 * \note \Bold{Programming hints:}
 * \note The following line includes the HDF5 Packet Table package, H5PT,
 *       in C applications:
 *       \code #include "hdf5_hl.h" \endcode
 *       Without this include, an application will not have access to
 *       these functions.
 *
 * - \ref H5PTappend
 *   \n Appends packets to the end of a packet table.
 * - \ref H5PTclose
 *   \n Closes an open packet table.
 * - \ref H5PTcreate
 *   \n Creates a packet table to store fixed-length
 *      or variable-length packets.
 * - \ref H5PTcreate_fl
 *   \n Creates a packet table to store fixed-length packets.
 * - \ref H5PTcreate_index
 *   \n Resets a packet table's index to the first packet.
 * - \ref H5PTfree_vlen_buff
 *   \n Releases memory allocated in the process of
 *      reading variable-length packets.
 * - \ref H5PTget_dataset
 *   \n Returns the backend dataset of this packet table.
 * - \ref H5PTget_index
 *   \n Gets the current record index for a packet table
 * - \ref H5PTget_next
 *   \n Reads packets from a packet table starting at the
 *      current index.
 * - \ref H5PTget_num_packets
 *   \n Returns the number of packets in a packet table.
 * - \ref H5PTget_type
 *   \n Returns the backend datatype of this packet table.
 * - \ref H5PTis_valid
 *   \n Determines whether an identifier points to a packet table.
 * - \ref H5PTis_varlen
 *   \n Determines whether a packet table contains variable-length
 *      or fixed-length packets.
 * - \ref H5PTopen
 *   \n Opens an existing packet table.
 * - \ref H5PTread_packets
 *   \n Reads a number of packets from a packet table.
 * - \ref H5PTset_index
 *   \n Sets a packet table's index.
 *
 */

/*-------------------------------------------------------------------------
 * Create/Open/Close functions
 *-------------------------------------------------------------------------
 */
/* NOTE: H5PTcreate is replacing H5PTcreate_fl for better name due to the
   removal of H5PTcreate_vl.  H5PTcreate_fl may be retired in 1.8.19. */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief   Creates a packet table to store fixed-length or
 *          variable-length packets.
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the packet table to create
 * \param[in] dtype_id      The datatype of the packet
 * \param[in] chunk_size    The size in number of table entries per chunk
 * \param[in] plist_id      Identifier of the property list. Can be used to
 *                          specify the compression of the packet table.
 *
 * \return Returns an identifier for the new packet table or
 *         #H5I_INVALID_HID on error.
 *
 * \details The H5PTcreate() creates and opens a packet table named
 *          \p dset_name attached to the object specified by the
 *          identifier \p loc_id. The created packet table should be closed
 *          with H5PTclose(), eventually.
 *
 *          The datatype, \p dtype_id, may specify any datatype, including
 *          variable-length data.  If \p dtype_id specifies a compound
 *          datatype, one or more fields in that compound type may be
 *          variable-length.
 *
 *          \p chunk_size is the size in number of table entries per chunk.
 *          Packet table datasets use HDF5 chunked storage
 *          to allow them to grow. This value allows the user
 *          to set the size of a chunk. The chunk size affects
 *          performance.
 *
 * \since   1.10.0 and 1.8.17
 *
 */
H5_HLDLL hid_t H5PTcreate(hid_t loc_id, const char *dset_name, hid_t dtype_id, hsize_t chunk_size,
                          hid_t plist_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Opens an existing packet table.
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the packet table to open
 *
 * \return Returns an identifier for the packet table or
 *         #H5I_INVALID_HID on error.
 *
 * \details H5PTopen() opens an existing packet table in the file or group
 *          specified by \p loc_id. \p dset_name is the name of the packet
 *          table and is used to identify it in the file. This function is
 *          used to open both fixed-length packet tables and variable-length
 *          packet tables. The packet table should later be closed with
 *          H5PTclose().
 *
 */
H5_HLDLL hid_t H5PTopen(hid_t loc_id, const char *dset_name);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Closes an open packet table.
 *
 * \param[in] table_id  Identifier of packet table to be closed
 *
 * \return \herr_t
 *
 * \details The H5PTclose() ends access to a packet table specified
 *          by \p table_id.
 *
 */
H5_HLDLL herr_t H5PTclose(hid_t table_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Creates a packet table to store fixed-length packets
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to create
 * \param[in] dtype_id      The datatype of a packet.
 * \param[in] chunk_size    The size in number of table entries per
 *                          chunk.
 * \param[in] compression   The compression level;
 *                          a value of 0 through 9.
 *
 * \return Returns an identifier for the packet table or
 *         #H5I_INVALID_HID on error.
 *
 * \deprecated This function was deprecated in favor of the function
 *             H5PTcreate().
 *
 * \details The H5PTcreate_fl() creates and opens a packet table
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id. It should be closed
 *          with H5PTclose().
 *
 *          The datatype, \p dtype_id, may specify any datatype,
 *          including variable-length data. If \p dtype_id specifies a
 *          compound datatype, one or more fields in that compound type
 *          may be variable-length.
 *
 *          \p chunk_size is the size in number of table entries per chunk.
 *          Packet table datasets use HDF5 chunked storage
 *          to allow them to grow. This value allows the user
 *          to set the size of a chunk. The chunk size affects
 *          performance.
 *
 *          \p compression is the compression level, a value of 0 through 9.
 *          Level 0 is faster but offers the least compression;
 *          level 9 is slower but offers maximum compression.
 *          A setting of -1 indicates that no compression is desired.
 *
 */
/* This function may be removed from the packet table in release 1.8.19. */
H5_HLDLL hid_t H5PTcreate_fl(hid_t loc_id, const char *dset_name, hid_t dtype_id, hsize_t chunk_size,
                             int compression);

/*-------------------------------------------------------------------------
 * Write functions
 *-------------------------------------------------------------------------
 */
/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Appends packets to the end of a packet table.
 *
 * \param[in] table_id  Identifier of packet table to which
 *                      packets should be appended
 * \param[in] nrecords  Number of packets to be appended
 * \param[in] data      Buffer holding data to write
 *
 * \return \herr_t
 *
 * \details The H5PTappend() writes \p nrecords packets to the end of a
 *          packet table specified by \p table_id. \p data is a buffer
 *          containing the data to be written. For a packet table holding
 *          fixed-length packets, this data should be in the packet
 *          table's datatype. For a variable-length packet table,
 *          the data should be in the form of #hvl_t structs.
 *
 */
H5_HLDLL herr_t H5PTappend(hid_t table_id, size_t nrecords, const void *data);

/*-------------------------------------------------------------------------
 * Read functions
 *-------------------------------------------------------------------------
 */
/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Reads packets from a packet table starting at the current index.
 *
 * \param[in] table_id  Identifier of packet table to read from
 * \param[in] nrecords  Number of packets to be read
 * \param[out] data     Buffer into which to read data
 *
 * \return \herr_t
 *
 * \details The H5PTget_next() reads \p nrecords packets starting with
 *          the "current index" from a packet table specified by \p table_id.
 *          The packet table's index is set and reset with H5PTset_index()
 *          and H5PTcreate_index(). \p data is a buffer into which the
 *          data should be read.
 *
 *          For a packet table holding variable-length records, the data
 *          returned in the buffer will be in form of a #hvl_t struct
 *          containing the length of the data and a pointer to it in memory.
 *          The memory used by this data must be freed using H5PTfree_vlen_buff().
 *
 */
H5_HLDLL herr_t H5PTget_next(hid_t table_id, size_t nrecords, void *data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Reads a number of packets from a packet table.
 *
 * \param[in] table_id  Identifier of packet table to read from
 * \param[in] start     Packet to start reading from
 * \param[in] nrecords  Number of packets to be read
 * \param[out] data     Buffer into which to read data.
 *
 * \return \herr_t
 *
 * \details The H5PTread_packets() reads \p nrecords packets starting at
 *          packet number \p start from a packet table specified by
 *          \p table_id. \p data is a buffer into which the data should
 *          be read.
 *
 *          For a packet table holding variable-length records, the data
 *          returned in the buffer will be in form of #hvl_t structs,
 *          each containing the length of the data and a pointer to it in
 *          memory. The memory used by this data must be freed using
 *          H5PTfree_vlen_buff().
 *
 */
H5_HLDLL herr_t H5PTread_packets(hid_t table_id, hsize_t start, size_t nrecords, void *data);

/*-------------------------------------------------------------------------
 * Inquiry functions
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Returns the number of packets in a packet table.
 *
 * \param[in] table_id  Identifier of packet table to query
 * \param[out] nrecords Number of packets in packet table
 *
 * \return \herr_t
 *
 * \details The H5PTget_num_packets() returns by reference the number
 *          of packets in a packet table specified by \p table_id.
 *
 */
H5_HLDLL herr_t H5PTget_num_packets(hid_t table_id, hsize_t *nrecords);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Determines whether an identifier points to a packet table.
 *
 * \param[in] table_id  Identifier to query
 *
 * \return Returns a non-negative value if \p table_id is
 *         a valid packet table, otherwise returns a negative value.
 *
 * \details The H5PTis_valid() returns a non-negative value if
 *          \p table_id corresponds to an open packet table,
 *          and returns a negative value otherwise.
 *
 */
H5_HLDLL herr_t H5PTis_valid(hid_t table_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Determines whether a packet table contains
 *        variable-length or fixed-length packets.
 *
 * \param[in] table_id  Packet table to query
 *
 * \return Returns 1 for a variable-length packet table,
 *         0 for fixed-length, or a negative value on error.
 *
 * \details The H5PTis_varlen() returns 1 (TRUE) if \p table_id is
 *          a packet table containing variable-length records.
 *          It returns 0 (FALSE) if \p table_id is a packet table
 *          containing fixed-length records. If \p table_id is not a
 *          packet table, a negative value is returned.
 *
 * \version 1.10.0 and 1.8.17 Function re-introduced.
 *                            Function had been removed in 1.8.3.
 *
 */
H5_HLDLL herr_t H5PTis_varlen(hid_t table_id);

/*-------------------------------------------------------------------------
 *
 * Accessor functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Returns the backend dataset of this packet table.
 *
 * \param[in] table_id  Identifier of the packet table
 *
 * \return Returns a dataset identifier or H5I_INVALID_HID on error.
 *
 * \details The H5PTget_dataset() returns the identifier of the dataset
 *          storing the packet table \p table_id. This dataset identifier
 *          will be closed by H5PTclose().
 *
 * \since 1.10.0 and 1.8.17
 *
 */
H5_HLDLL hid_t H5PTget_dataset(hid_t table_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Returns the backend datatype of this packet table.
 *
 * \param[in] table_id  Identifier of the packet table
 *
 * \return Returns a datatype identifier or H5I_INVALID_HID on error.
 *
 * \details The H5PTget_type() returns the identifier of the datatype
 *          used by the packet table \p table_id. This datatype
 *          identifier will be closed by H5PTclose().
 *
 * \since 1.10.0 and 1.8.17
 *
 */
H5_HLDLL hid_t H5PTget_type(hid_t table_id);

/*-------------------------------------------------------------------------
 *
 * Packet Table "current index" functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Resets a packet table's index to the first packet.
 *
 * \param[in] table_id  Identifier of packet table whose index
 *                      should be initialized.
 *
 * \return \herr_t
 *
 * \details Each packet table keeps an index of the "current" packet
 *          so that \c get_next can iterate through the packets in order.
 *          H5PTcreate_index() initializes a packet table's index, and
 *          should be called before using \c get_next. The index must be
 *          initialized every time a packet table is created or opened;
 *          this information is lost when the packet table is closed.
 *
 */
H5_HLDLL herr_t H5PTcreate_index(hid_t table_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Sets a packet table's index.
 *
 * \param[in] table_id  Identifier of packet table whose index is to be set
 * \param[in] pt_index  The packet to which the index should point
 *
 * \return \herr_t
 *
 * \details Each packet table keeps an index of the "current" packet
 *          so that \c get_next can iterate through the packets in order.
 *          H5PTset_index() sets this index to point to a user-specified
 *          packet (the packets are zero-indexed).
 *
 */
H5_HLDLL herr_t H5PTset_index(hid_t table_id, hsize_t pt_index);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Gets the current record index for a packet table.
 *
 * \param[in] table_id  Table identifier
 * \param[out] pt_index Current record index
 *
 * \return \herr_t
 *
 * \details The H5PTget_index() returns the current record index
 *          \p pt_index for the table identified by \p table_id.
 *
 * \since 1.8.0
 *
 */
H5_HLDLL herr_t H5PTget_index(hid_t table_id, hsize_t *pt_index);

/*-------------------------------------------------------------------------
 *
 * Memory Management functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5PT
 *
 * \brief Releases memory allocated in the process of reading
 *        variable-length packets.
 *
 * \param[in] table_id  Packet table whose memory should be freed.
 * \param[in] bufflen   Size of \p buff
 * \param[in] buff      Buffer that was used to read in variable-length
 *                      packets
 *
 * \return \herr_t
 *
 * \details When variable-length packets are read, memory is automatically
 *          allocated to hold them, and must be freed. H5PTfree_vlen_buff()
 *          frees this memory, and should be called whenever packets are
 *          read from a variable-length packet table.
 *
 * \version 1.10.0 and 1.8.17 Function re-introduced.
 *                            Function had been removed in 1.8.3.
 *
 */
H5_HLDLL herr_t H5PTfree_vlen_buff(hid_t table_id, size_t bufflen, void *buff);

#ifdef __cplusplus
}
#endif

#endif
