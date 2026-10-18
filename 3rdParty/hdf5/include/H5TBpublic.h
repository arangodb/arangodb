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

#ifndef H5TBpublic_H
#define H5TBpublic_H

#ifdef __cplusplus
extern "C" {
#endif

/** \page H5TB_UG HDF5 High Level Table
 * @todo Under Construction
 */

/**\defgroup H5TB HDF5 Table APIs (H5TB)
 *
 * <em>Creating and manipulating HDF5 datasets intended to be
 * interpreted as tables (H5TB)</em>
 *
 * The HDF5 Table API defines a standard storage for HDF5 datasets
 * that are intended to be interpreted as tables. A table is defined
 * as a collection of records whose values are stored in fixed-length
 * fields. All records have the same structure, and all values in
 * each field have the same data type.
 *
 * \note \Bold{Programming hints:}
 * \note To use any of these functions or subroutines,
 *       you must first include the relevant include file (C) or
 *       module (Fortran) in your application.
 * \note The following line includes the HDF5 Table package, H5TB,
 *       in C applications:
 *       \code #include "hdf5_hl.h" \endcode
 * \note To include the H5TB module in Fortran applications specify:
 *       \code use h5tb \endcode
 *       Fortran applications must also include \ref H5open before
 *       any HDF5 calls to initialize global variables and \ref H5close
 *       after all HDF5 calls to close the Fortran interface.
 *
 * <table>
 * <tr valign="top"><td style="border: none;">
 *
 * - Creation
 *   - \ref H5TBmake_table
 * - Storage
 *   - \ref H5TBappend_records (No Fortran)
 *   - \ref H5TBwrite_records (No Fortran)
 *   - \ref H5TBwrite_fields_name
 *   - \ref H5TBwrite_fields_index
 *
 * - Modification
 *   - \ref H5TBdelete_record (No Fortran)
 *   - \ref H5TBinsert_record (No Fortran)
 *   - \ref H5TBadd_records_from (No Fortran)
 *   - \ref H5TBcombine_tables (No Fortran)
 *   - \ref H5TBinsert_field
 *   - \ref H5TBdelete_field
 *
 * </td><td style="border: none;">
 *
 * - Retrieval
 *   - \ref H5TBread_table
 *   - \ref H5TBread_records (No Fortran)
 *   - \ref H5TBread_fields_name
 *   - \ref H5TBread_fields_index
 *
 * - Query
 *   - \ref H5TBget_table_info
 *   - \ref H5TBget_field_info
 *
 * - Query Table Attributes
 *   - \ref H5TBAget_fill
 *   - \ref H5TBAget_title
 *
 * </td></tr>
 * </table>
 *
 */

/*-------------------------------------------------------------------------
 *
 * Create functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Creates and writes a table
 *
 * \param[in] table_title   The title of the table
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to create
 * \param[in] nfields       The number of fields
 * \param[in] nrecords      The number of records
 * \param[in] type_size     The size in bytes of the structure
 *                          associated with the table;
 *                          This value is obtained with \c sizeof().
 * \param[in] field_names   An array containing the names of
 *                          the fields
 * \param[in] field_offset  An array containing the offsets of
 *                          the fields
 * \param[in] field_types   An array containing the type of
 *                          the fields
 * \param[in] chunk_size    The chunk size
 * \param[in] fill_data     Fill values data
 * \param[in] compress      Flag that turns compression on or off
 * \param[in] buf           Buffer with data to be written to the table
 *
 * \return \herr_t
 *
 * \details H5TBmake_table() creates and writes a dataset named
 *          \p dset_name attached to the object specified by the
 *          identifier loc_id.
 *
 */
H5_HLDLL herr_t H5TBmake_table(const char *table_title, hid_t loc_id, const char *dset_name, hsize_t nfields,
                               hsize_t nrecords, size_t type_size, const char *field_names[],
                               const size_t *field_offset, const hid_t *field_types, hsize_t chunk_size,
                               void *fill_data, int compress, const void *buf);

/*-------------------------------------------------------------------------
 *
 * Write functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Adds records to the end of the table
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to overwrite
 * \param[in] nrecords      The number of records to append
 * \param[in] type_size     The size of the structure type,
 *                          as calculated by \c sizeof().
 * \param[in] field_offset  An array containing the offsets of
 *                          the fields. These offsets can be
 *                          calculated with the #HOFFSET macro
 * \param[in] dst_sizes     An array containing the sizes of
 *                          the fields
 * \param[in] buf           Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBappend_records() adds records to the end of the table
 *          named \p dset_name attached to the object specified by the
 *          identifier \p loc_id. The dataset is extended to hold the
 *          new records.
 *
 */
H5_HLDLL herr_t H5TBappend_records(hid_t loc_id, const char *dset_name, hsize_t nrecords, size_t type_size,
                                   const size_t *field_offset, const size_t *dst_sizes, const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Overwrites records
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset to overwrite
 * \param[in] start         The zero index record to start writing
 * \param[in] nrecords      The number of records to write
 * \param[in] type_size     The size of the structure type, as
 *                          calculated by \c sizeof().
 * \param[in] field_offset  An array containing the offsets of
 *                          the fields.  These offsets can be
 *                          calculated with the #HOFFSET macro
 * \param[in] dst_sizes     An array containing the sizes of
 *                          the fields
 * \param[in] buf           Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBwrite_records() overwrites records starting at the zero
 *          index position start of the table named \p dset_name attached
 *          to the object specified by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBwrite_records(hid_t loc_id, const char *dset_name, hsize_t start, hsize_t nrecords,
                                  size_t type_size, const size_t *field_offset, const size_t *dst_sizes,
                                  const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Overwrites fields
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset to overwrite
 * \param[in] field_names   The names of the fields to write
 * \param[in] start         The zero index record to start writing
 * \param[in] nrecords      The number of records to write
 * \param[in] type_size     The size of the structure type, as
 *                          calculated by \c sizeof().
 * \param[in] field_offset  An array containing the offsets of
 *                          the fields.  These offsets can be
 *                          calculated with the #HOFFSET macro
 * \param[in] dst_sizes     An array containing the sizes of
 *                          the fields
 * \param[in] buf           Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBwrite_fields_name() overwrites one or several fields
 *          specified by \p field_names with data in \p buf from a
 *          dataset named \p dset_name attached to the object specified
 *          by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBwrite_fields_name(hid_t loc_id, const char *dset_name, const char *field_names,
                                      hsize_t start, hsize_t nrecords, size_t type_size,
                                      const size_t *field_offset, const size_t *dst_sizes, const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Overwrites fields
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to overwrite
 * \param[in] nfields       The number of fields to overwrite.
 *                          This parameter is also the size of the
 *                          \p field_index array.
 * \param[in] field_index   The indexes of the fields to write
 * \param[in] start         The zero based index record to start writing
 * \param[in] nrecords      The number of records to write
 * \param[in] type_size     The size of the structure type, as
 *                          calculated by \c sizeof().
 * \param[in] field_offset  An array containing the offsets of
 *                          the fields.  These offsets can be
 *                          calculated with the #HOFFSET macro
 * \param[in] dst_sizes     An array containing the sizes of
 *                          the fields
 * \param[in] buf           Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBwrite_fields_index() overwrites one or several fields
 *          specified by \p field_index with a buffer \p buf from a
 *          dataset named \p dset_name attached to the object
 *          specified by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBwrite_fields_index(hid_t loc_id, const char *dset_name, hsize_t nfields,
                                       const int *field_index, hsize_t start, hsize_t nrecords,
                                       size_t type_size, const size_t *field_offset, const size_t *dst_sizes,
                                       const void *buf);

/*-------------------------------------------------------------------------
 *
 * Read functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Reads a table
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset to read
 * \param[in] dst_size     The size of the structure type,
 *                          as calculated by \c sizeof()
 * \param[in] dst_offset    An array containing the offsets of
 *                          the fields.  These offsets can be
 *                          calculated with the #HOFFSET macro
 * \param[in] dst_sizes     An array containing the sizes of
 *                          the fields.  These sizes can be
 *                          calculated with the sizeof() macro.
 * \param[in] dst_buf       Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBread_table() reads a table named
 *          \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBread_table(hid_t loc_id, const char *dset_name, size_t dst_size, const size_t *dst_offset,
                               const size_t *dst_sizes, void *dst_buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 * \brief Reads one or several fields. The fields are identified by name.
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset to read
 * \param[in] field_names   An array containing the names of the
 *                          fields to read
 * \param[in] start         The start record to read from
 * \param[in] nrecords      The number of records to read
 * \param[in] type_size     The size in bytes of the structure associated
 *                          with the table
 *                          (This value is obtained with \c sizeof().)
 * \param[in] field_offset  An array containing the offsets of the fields
 * \param[in] dst_sizes     An array containing the size in bytes of
 *                          the fields
 * \param[out] buf          Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBread_fields_name() reads the fields identified
 *          by \p field_names from a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBread_fields_name(hid_t loc_id, const char *dset_name, const char *field_names,
                                     hsize_t start, hsize_t nrecords, size_t type_size,
                                     const size_t *field_offset, const size_t *dst_sizes, void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Reads one or several fields. The fields are identified by index.
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to read
 * \param[in] nfields       The number of fields to read
 *                          (This parameter is also the size of the
 *                          \p field_index array.)
 *                          fields to read
 * \param[in] field_index   The indexes of the fields to read
 * \param[in] start         The start record to read from
 * \param[in] nrecords      The number of records to read
 * \param[in] type_size     The size in bytes of the structure associated
 *                          with the table
 *                          (This value is obtained with \c sizeof())
 * \param[in] field_offset  An array containing the offsets of the fields
 * \param[in] dst_sizes     An array containing the size in bytes of
 *                          the fields
 * \param[out] buf          Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBread_fields_index() reads the fields identified
 *          by \p field_index from a dataset named \p dset_name attached
 *          to the object specified by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBread_fields_index(hid_t loc_id, const char *dset_name, hsize_t nfields,
                                      const int *field_index, hsize_t start, hsize_t nrecords,
                                      size_t type_size, const size_t *field_offset, const size_t *dst_sizes,
                                      void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Reads records
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to read
 * \param[in] start         The start record to read from
 * \param[in] nrecords      The number of records to read
 * \param[in] type_size     The size of the structure type,
 *                          as calculated by \c sizeof()
 * \param[in] dst_offset    An array containing the offsets of the
 *                          fields.  These offsets can be calculated
 *                          with the #HOFFSET macro
 * \param[in] dst_sizes     An array containing the size in bytes of
 *                          the fields
 * \param[out] buf          Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBread_records() reads some records identified from a dataset
 *          named \p dset_name attached to the object specified by the
 *          identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBread_records(hid_t loc_id, const char *dset_name, hsize_t start, hsize_t nrecords,
                                 size_t type_size, const size_t *dst_offset, const size_t *dst_sizes,
                                 void *buf);

/*-------------------------------------------------------------------------
 *
 * Inquiry functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Gets the table dimensions
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset to read
 * \param[out] nfields      The number of fields
 * \param[out] nrecords     The number of records
 *
 * \return \herr_t
 *
 * \details H5TBget_table_info() retrieves the table dimensions from a
 *          dataset named \p dset_name attached to the object specified
 *          by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBget_table_info(hid_t loc_id, const char *dset_name, hsize_t *nfields, hsize_t *nrecords);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Gets information about a table
 *
 * \fg_loc_id
 * \param[in] dset_name         The name of the dataset to read
 * \param[out] field_names      An array containing the names of the fields
 * \param[out] field_sizes      An array containing the size of the fields
 * \param[out] field_offsets    An array containing the offsets of the fields
 * \param[out] type_size        The size of the HDF5 datatype associated
 *                              with the table.  (More specifically,
 *                              the size in bytes of the HDF5 compound
 *                              datatype used to define a row, or record,
 *                              in the table)
 *
 * \return \herr_t
 *
 * \details H5TBget_field_info() gets information about a dataset
 *          named \p dset_name attached to the object specified
 *          by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5TBget_field_info(hid_t loc_id, const char *dset_name, char *field_names[],
                                   size_t *field_sizes, size_t *field_offsets, size_t *type_size);

/*-------------------------------------------------------------------------
 *
 * Manipulation functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Delete records
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset
 * \param[in] start        The start record to delete from
 * \param[in] nrecords     The number of records to delete
 *
 * \return \herr_t
 *
 * \details H5TBdelete_record() deletes nrecords number of records starting
 *          from \p start from the middle of the table \p dset_name
 *          ("pulling up" all the records after it).
 *
 */
H5_HLDLL herr_t H5TBdelete_record(hid_t loc_id, const char *dset_name, hsize_t start, hsize_t nrecords);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Insert records
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset
 * \param[in] start         The position to insert
 * \param[in] nrecords      The number of records to insert
 * \param[in] dst_size      The size in bytes of the structure
 *                          associated with the table
 * \param[in] dst_offset    An array containing the offsets of the
 *                          fields
 * \param[in] dst_sizes     An array containing the size in bytes of
 *                          the fields
 * \param[in] buf           Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBinsert_record() inserts records into the middle of the table
 *          ("pushing down" all the records after it)
 *
 */
H5_HLDLL herr_t H5TBinsert_record(hid_t loc_id, const char *dset_name, hsize_t start, hsize_t nrecords,
                                  size_t dst_size, const size_t *dst_offset, const size_t *dst_sizes,
                                  void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Add records from first table to second table
 *
 * \fg_loc_id
 * \param[in] dset_name1    The name of the dataset to read the records
 * \param[in] start1        The position to read the records from the
 *                          first table
 * \param[in] nrecords      The number of records to read from the first
 *                          table
 * \param[in] dset_name2    The name of the dataset to write the records
 * \param[in] start2        The position to write the records on the
 *                          second table
 *
 * \return \herr_t
 *
 * \details H5TBadd_records_from() adds records from a dataset named
 *          \p dset_name1 to a dataset named \p dset_name2. Both tables
 *          are attached to the object specified by the identifier loc_id.
 *
 */
H5_HLDLL herr_t H5TBadd_records_from(hid_t loc_id, const char *dset_name1, hsize_t start1, hsize_t nrecords,
                                     const char *dset_name2, hsize_t start2);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Combines records from two tables into a third
 *
 * \param[in] loc_id1       Identifier of the file or group in which
 *                          the first table is located
 * \param[in] dset_name1    The name of the first table to combine
 * \param[in] loc_id2       Identifier of the file or group in which
 *                          the second table is located
 * \param[in] dset_name2    The name of the second table to combine
 * \param[in] dset_name3    The name of the new table
 *
 * \return \herr_t
 *
 * \details H5TBcombine_tables() combines records from two datasets named
 *          \p dset_name1 and \p dset_name2, to a new table named
 *          \p dset_name3. These tables can be located on different files,
 *          identified by \p loc_id1 and \p loc_id2 (identifiers obtained
 *          with H5Fcreate()). They can also be located on the same file.
 *          In this case one uses the same identifier for both parameters
 *          \p loc_id1 and \p loc_id2. If two files are used, the third
 *          table is written in the first file.
 *
 */
H5_HLDLL herr_t H5TBcombine_tables(hid_t loc_id1, const char *dset_name1, hid_t loc_id2,
                                   const char *dset_name2, const char *dset_name3);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Insert a new field into a table
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the table
 * \param[in] field_name    The name of the field to insert
 * \param[in] field_type    The data type of the field
 * \param[in] position      The zero based index position where to
 *                          insert the field
 * \param[in] fill_data     Fill value data for the field. This parameter
 *                          can be NULL
 * \param[in] buf           Buffer with data
 *
 * \return \herr_t
 *
 * \details H5TBinsert_field() inserts a new field named \p field_name into
 *          the table \p dset_name. Note: this function requires the table
 *          to be re-created and rewritten in its entirety, and this can result
 *          in some unused space in the file, and can also take a great deal of
 *          time if the table is large.
 *
 */
H5_HLDLL herr_t H5TBinsert_field(hid_t loc_id, const char *dset_name, const char *field_name,
                                 hid_t field_type, hsize_t position, const void *fill_data, const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Deletes a field from a table
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the table
 * \param[in] field_name    The name of the field to delete
 *
 * \return \herr_t
 *
 * \details H5TBdelete_field() deletes a field named \p field_name from the
 *          table \p dset_name. Note: this function requires the table to be
 *          re-created and rewritten in its entirety, and this can result in
 *          some unused space in the file, and can also take a great deal of
 *          time if the table is large.
 *
 */
H5_HLDLL herr_t H5TBdelete_field(hid_t loc_id, const char *dset_name, const char *field_name);

/*-------------------------------------------------------------------------
 *
 * Table attribute functions
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Reads a table's title
 *
 * \fg_loc_id
 * \param[out] table_title  Buffer for title name
 *
 * \return \herr_t
 *
 * \details H5TBget_title() returns the title of the table identified
 *          by \p loc_id in a buffer \p table_title.
 *
 */
H5_HLDLL herr_t H5TBAget_title(hid_t loc_id, char *table_title);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5TB
 *
 *
 * \brief Reads the table attribute fill values
 *
 * \fg_loc_id
 * \param[in] dset_name Name of table
 * \param[in] dset_id   Table identifier
 * \param[out] dst_buf Buffer of fill values for table fields
 *
 * \return
 * \return A return value of 1 indicates that a fill value is present.
 * \return A return value of 0 indicates a fill value is not present.
 * \return A return value <0 indicates an error.
 *
 * \details H5TBget_fill() reads the table attribute fill values into
 *          the buffer \p dst_buf for the table specified by \p dset_id
 *          and \p dset_name located in \p loc_id.
 *
 * \par Example
 * \include H5TBAget_fill.c
 *
 */
H5_HLDLL htri_t H5TBAget_fill(hid_t loc_id, const char *dset_name, hid_t dset_id, unsigned char *dst_buf);

#ifdef __cplusplus
}
#endif

#endif
