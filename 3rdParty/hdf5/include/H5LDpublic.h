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

#ifndef H5LDpublic_H
#define H5LDpublic_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Retrieves the current dimension sizes of a dataset.
 *
 * \param[in] did       The dataset identifier
 * \param[out] cur_dims The current dimension sizes of the dataset
 *
 * \return \herr_t
 *
 * \details H5LDget_dset_dims() retrieves the current dimension sizes
 *          for the dataset \p did through the parameter \p cur_dims.
 *          It will return failure if \p cur_dims is NULL.
 *
 * \note See Also:
 * \note Dataset Watch functions (used with h5watch):
 *       - H5LDget_dset_dims()
 *       - H5LDget_dset_elmts()
 *       - H5LDget_dset_type_size()
 *
 * \par Example:
 * See the example code in H5LDget_dset_elmts() for usage of this routine.
 *
 * \since 1.10.0
 *
 */
H5_HLDLL herr_t H5LDget_dset_dims(hid_t did, hsize_t *cur_dims);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Returns the size in bytes of the dataset's datatype
 *
 * \param[in] did       The dataset identifier
 * \param[in] fields    The pointer to a comma-separated list of fields for a compound datatype
 *
 * \return If successful, returns the size in bytes of the
 *         dataset's datatype. Otherwise, returns 0.
 *
 * \details H5LDget_dset_type_size() allows the user to find out the datatype
 *          size for the dataset associated with \p did. If the
 *          parameter \p fields is NULL, this routine just returns the size
 *          of the dataset's datatype. If the dataset has a compound datatype
 *          and \p fields is non-NULL, this routine returns the size of the
 *          datatype(s) for the selected fields specified in \p fields.
 *          Note that ’,’ is the separator for the fields of a compound
 *          datatype while ’.’ (dot) is the separator for a nested field.
 *          Use a backslash ( \ ) to escape characters in field names that
 *          conflict with these two separators.
 *
 * \note See Also:
 * \note Dataset Watch functions (used with h5watch):
 *       - H5LDget_dset_dims()
 *       - H5LDget_dset_elmts()
 *       - H5LDget_dset_type_size()
 *
 * \par Example:
 * See the example code in H5LDget_dset_elmts() for usage of this routine.
 *
 * \since 1.10.0
 *
 */
H5_HLDLL size_t H5LDget_dset_type_size(hid_t did, const char *fields);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Retrieves selected data from the dataset
 *
 * \param[in] did       The dataset identifier
 * \param[in] prev_dims The previous dimension size of the dataset
 * \param[in] cur_dims  The current dimension sizes of the dataset
 * \param[in] fields    A string containing a comma-separated list
 *                      of fields for a compound datatype
 * \param[out] buf      Buffer for storing data retrieved from the
 *                      dataset
 *
 * \return \herr_t
 *
 * \details H5LDget_dset_dims() retrieves selected data of the dataset
 *          \p did and stores the data in the parameter \p buf.
 *          The difference between the parameters \p prev_dims and
 *          \p cur_dims indicates the dimension sizes of the data to be
 *          selected from the dataset. Note that \p cur_dims must have
 *          at least one dimension whose size is greater than the
 *          corresponding dimension in \p prev_dims. Users can
 *          determine the size of buf by multiplying the datatype
 *          size of the dataset by the number of selected elements.
 *
 *          If the parameter \p fields is NULL, this routine returns
 *          data for the selected elements of the dataset. If \p fields
 *          is not NULL and the dataset has a compound datatype, \p fields
 *          is a string containing a comma-separated list of fields.
 *          Each name in \p fields specifies a field in the compound
 *          datatype, and this routine returns data of the selected fields
 *          for the dataset's selected elements. Note that ’,’ is the
 *          separator for the fields of a compound datatype while
 *          ’.’ is the separator for a nested field. Use backslash to
 *          escape characters in field names that conflict with these
 *          two separators.
 *
 * \note See Also:
 * \note Dataset Watch functions (used with h5watch):
 *       - H5LDget_dset_dims()
 *       - H5LDget_dset_elmts()
 *       - H5LDget_dset_type_size()
 *
 * \par Examples:
 *
 * For the first example, \c DSET1 is a two-dimensional chunked dataset with atomic type defined below:
 * \snippet H5LDget_dset_elmts.c first_declare
 *
 * The following coding sample illustrates the reading of
 * data elements appended to the dataset \c DSET1:
 * \snippet H5LDget_dset_elmts.c first_reading
 *
 * The output buffer will contain data elements selected from
 * \c DSET1 as follows:
 * \snippet H5LDget_dset_elmts.c first_output
 *
 * For the second example, DSET2 is a one-dimensional chunked dataset
 * with compound type defined below:
 * \snippet H5LDget_dset_elmts.c second_declare
 *
 * The following coding sample illustrates the reading of data elements
 * appended to the dataset \c DSET2 with compound datatype.
 * This example selects only 2 fields: the fourth field \c d and a
 * subfield of the sixth field \c s2.c:
 * \snippet H5LDget_dset_elmts.c second_reading
 *
 * The output buffer will contain data for \c d and \c s2.c
 * selected from \c DSET2 as follows:
 * \snippet H5LDget_dset_elmts.c second_output
 *
 * \since 1.10.0
 *
 */
H5_HLDLL herr_t H5LDget_dset_elmts(hid_t did, const hsize_t *prev_dims, const hsize_t *cur_dims,
                                   const char *fields, void *buf);

#ifdef __cplusplus
}
#endif

#endif /* H5LDpublic_H */
