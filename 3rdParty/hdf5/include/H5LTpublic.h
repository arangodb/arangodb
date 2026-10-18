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

#ifndef H5LTpublic_H
#define H5LTpublic_H

/* Flag definitions for H5LTopen_file_image() */
#define H5LT_FILE_IMAGE_OPEN_RW   0x0001 /* Open image for read-write */
#define H5LT_FILE_IMAGE_DONT_COPY 0x0002 /* The HDF5 lib won't copy   */
/* user supplied image buffer. The same image is open with the core driver.  */
#define H5LT_FILE_IMAGE_DONT_RELEASE 0x0004 /* The HDF5 lib won't        */
/* deallocate user supplied image buffer. The user application is responsible */
/* for doing so.                                                             */
#define H5LT_FILE_IMAGE_ALL 0x0007

typedef enum H5LT_lang_t {
    H5LT_LANG_ERR = -1, /*this is the first*/
    H5LT_DDL      = 0,  /*for DDL*/
    H5LT_C        = 1,  /*for C*/
    H5LT_FORTRAN  = 2,  /*for Fortran*/
    H5LT_NO_LANG  = 3   /*this is the last*/
} H5LT_lang_t;

#ifdef __cplusplus
extern "C" {
#endif

/** \page H5LT_UG HDF5 High Level Lite
 * @todo Under Construction
 */

/**\defgroup H5LT HDF5 Lite APIs (H5LT,H5LD)
 * <em>Functions used to simplify creating and manipulating datasets,
 * attributes and other features (H5LT, H5LD)</em>
 *
 * The HDF5 Lite API consists of higher-level functions which do
 * more operations per call than the basic HDF5 interface.
 * The purpose is to wrap intuitive functions around certain sets
 * of features in the existing APIs.
 * It has the following sets of functions listed below.
 *
 * \note \Bold{Programming hints:}
 * \note To use any of these functions or subroutines,
 *       you must first include the relevant include file (C) or
 *       module (Fortran) in your application.
 * \note The following line includes the HDF5 Lite package, H5LT,
 *       in C applications:
 *       \code #include "hdf5_hl.h" \endcode
 * \note This line includes the H5LT module in Fortran applications:
 *       \code use h5lt \endcode
 *
 * <table>
 * <tr valign="top"><td style="border: none;">
 *
 * - Dataset Functions
 *   - Make dataset functions
 *      - \ref H5LTmake_dataset
 *      - \ref H5LTmake_dataset_char
 *      - \ref H5LTmake_dataset_short
 *      - \ref H5LTmake_dataset_int
 *      - \ref H5LTmake_dataset_long
 *      - \ref H5LTmake_dataset_float
 *      - \ref H5LTmake_dataset_double
 *      - \ref H5LTmake_dataset_string
 *
 *   - Read dataset functions
 *      - \ref H5LTread_dataset
 *      - \ref H5LTread_dataset_char
 *      - \ref H5LTread_dataset_short
 *      - \ref H5LTread_dataset_int
 *      - \ref H5LTread_dataset_long
 *      - \ref H5LTread_dataset_float
 *      - \ref H5LTread_dataset_double
 *      - \ref H5LTread_dataset_string
 *
 *   - Query dataset functions
 *      - \ref H5LTfind_dataset
 *      - \ref H5LTget_dataset_ndims
 *      - \ref H5LTget_dataset_info
 *
 *   - Dataset watch functions
 *      - \ref H5LDget_dset_dims
 *      - \ref H5LDget_dset_elmts
 *      - \ref H5LDget_dset_type_size
 *
 * </td><td style="border: none;">
 *
 * - Attribute Functions
 *   - Set attribute functions
 *      - \ref H5LTset_attribute_string
 *      - \ref H5LTset_attribute_char
 *      - \ref H5LTset_attribute_uchar
 *      - \ref H5LTset_attribute_short
 *      - \ref H5LTset_attribute_ushort
 *      - \ref H5LTset_attribute_int
 *      - \ref H5LTset_attribute_uint
 *      - \ref H5LTset_attribute_long
 *      - \ref H5LTset_attribute_long_long
 *      - \ref H5LTset_attribute_ulong
 *      - \ref H5LTset_attribute_ullong
 *      - \ref H5LTset_attribute_float
 *      - \ref H5LTset_attribute_double
 *      - <code>H5LTset_attribute_f</code> (fortran ONLY)
 *
 *   - Get attribute functions
 *      - \ref H5LTget_attribute
 *      - \ref H5LTget_attribute_string
 *      - \ref H5LTget_attribute_char
 *      - \ref H5LTget_attribute_uchar
 *      - \ref H5LTget_attribute_short
 *      - \ref H5LTget_attribute_ushort
 *      - \ref H5LTget_attribute_int
 *      - \ref H5LTget_attribute_uint
 *      - \ref H5LTget_attribute_long
 *      - \ref H5LTget_attribute_long_long
 *      - \ref H5LTget_attribute_ulong
 *      - \ref H5LTget_attribute_ullong
 *      - \ref H5LTget_attribute_float
 *      - \ref H5LTget_attribute_double
 *
 *   - Query attribute functions
 *      - \ref H5LTfind_attribute
 *      - \ref H5LTget_attribute_info
 *      - \ref H5LTget_attribute_ndims
 *
 * </td><td style="border: none;">
 *
 * - Datatype Functions
 *   - Datatype translation functions
 *      - \ref H5LTtext_to_dtype
 *      - \ref H5LTdtype_to_text
 *
 * - File image function
 *   - Open file image function
 *      - \ref H5LTopen_file_image
 *
 * - Path and object function
 *   - Query path and object function
 *      - \ref H5LTpath_valid
 *
 * </td></tr>
 * </table>
 *
 */

/*-------------------------------------------------------------------------
 *
 * Make dataset functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset of a type \p type_id.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] type_id   Identifier of the datatype to use when creating the dataset
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset() creates and writes a dataset named
 *          \p dset_name attached to the object specified by the
 *          identifier \p loc_id.
 *
 *          The parameter \p type_id can be any valid HDF5 Prdefined \ref PDTNAT;
 *          For example, setting \p type_id to #H5T_NATIVE_INT will result in a dataset
 *          of <em>signed \e integer datatype</em>.
 *
 * \version 1.10.0 Fortran 2003 subroutine added to accept a C address of the data buffer.
 * \version 1.8.7 Fortran subroutine modified in this release to accommodate arrays
 *                with more than three dimensions.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                 hid_t type_id, const void *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_char() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be \e character, #H5T_NATIVE_CHAR.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_char(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                      const char *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_short() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be <em>short signed integer</em>,
 *          #H5T_NATIVE_SHORT.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_short(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                       const short *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_int() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be <em>native signed integer</em>,
 *          #H5T_NATIVE_INT.
 *
 * \version Fortran subroutine modified in this release to accommodate
 *          arrays with more than three dimensions.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_int(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                     const int *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_long() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be <em>long signed integer</em>,
 *          #H5T_NATIVE_LONG.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_long(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                      const long *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_float() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be <em>native floating point</em>,
 *          #H5T_NATIVE_FLOAT.
 *
 * \version 1.8.7 Fortran subroutine modified in this release to accommodate
 *                arrays with more than three dimensions.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_float(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                       const float *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset.
 *
 * \fg_loc_id
 * \param[in] dset_name The Name of the dataset to create
 * \param[in] rank      Number of dimensions of dataspace
 * \param[in] dims      An array of the size of each dimension
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_double() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be
 *          <em>native floating-point double</em>, #H5T_NATIVE_DOUBLE.
 *
 * \version 1.8.7 Fortran subroutine modified in this release to accommodate
 *                arrays with more than three dimensions.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_double(hid_t loc_id, const char *dset_name, int rank, const hsize_t *dims,
                                        const double *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a dataset with string datatype.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to create
 * \param[in] buf       Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5LTmake_dataset_string() creates and writes a dataset
 *          named \p dset_name attached to the object specified by
 *          the identifier \p loc_id.
 *
 *          The dataset's datatype will be <em>C string</em>, #H5T_C_S1.
 *
 */
H5_HLDLL herr_t H5LTmake_dataset_string(hid_t loc_id, const char *dset_name, const char *buf);

/*-------------------------------------------------------------------------
 *
 * Read dataset functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[in] type_id   Identifier of the datatype to use when reading
 *                      the dataset
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *
 * \version 1.10.0  Fortran 2003 subroutine added to accept a C
 *                  address of the data buffer.
 * \version 1.8.7   Fortran subroutine modified in this release to
 *                  accommodate arrays with more than three dimensions.
 *
 */
H5_HLDLL herr_t H5LTread_dataset(hid_t loc_id, const char *dset_name, hid_t type_id, void *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_char() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_NATIVE_CHAR.
 *
 */
H5_HLDLL herr_t H5LTread_dataset_char(hid_t loc_id, const char *dset_name, char *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_short() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_NATIVE_SHORT.
 *
 */
H5_HLDLL herr_t H5LTread_dataset_short(hid_t loc_id, const char *dset_name, short *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_int() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_NATIVE_INT.
 *
 * \version 1.8.7 Fortran subroutine modified in this release to
 *                accommodate arrays with more than three dimensions.
 *
 */
H5_HLDLL herr_t H5LTread_dataset_int(hid_t loc_id, const char *dset_name, int *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_long() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_NATIVE_LONG.
 *
 */
H5_HLDLL herr_t H5LTread_dataset_long(hid_t loc_id, const char *dset_name, long *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_float() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_NATIVE_FLOAT.
 *
 * \version 1.8.7 Fortran subroutine modified in this release to
 *                accommodate arrays with more than three dimensions.
 */
H5_HLDLL herr_t H5LTread_dataset_float(hid_t loc_id, const char *dset_name, float *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buffer   Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_double() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_NATIVE_DOUBLE.
 *
 * \version 1.8.7 Fortran subroutine modified in this release to
 *                accommodate arrays with more than three dimensions.
 */
H5_HLDLL herr_t H5LTread_dataset_double(hid_t loc_id, const char *dset_name, double *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a dataset from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to read
 * \param[out] buf      Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTread_dataset_string() reads a dataset named \p dset_name
 *          attached to the object specified by the identifier \p loc_id.
 *          The HDF5 datatype is #H5T_C_S1.
 *
 */
H5_HLDLL herr_t H5LTread_dataset_string(hid_t loc_id, const char *dset_name, char *buf);

/*-------------------------------------------------------------------------
 *
 * Query dataset functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Gets the dimensionality of a dataset
 *
 * \param[in]   loc_id      Identifier of the object to
 *                          locate the dataset within
 * \param[in]   dset_name   The dataset name
 * \param[out]  rank        The dimensionality of the dataset
 *
 * \return \herr_t
 *
 * \details H5LTget_dataset_ndims() gets the dimensionality of a dataset
 *          named \p dset_name exists attached to the object \p loc_id.
 *
 */
H5_HLDLL herr_t H5LTget_dataset_ndims(hid_t loc_id, const char *dset_name, int *rank);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Retrieves information about a dataset
 *
 * \param[in]   loc_id      Identifier of the object to locate
 *                          the dataset within
 * \param[in]   dset_name   The dataset name
 * \param[out]  dims        The dimensions of the dataset
 * \param[out]  type_class  The class identifier. #H5T_class_t is defined in
 *                          H5Tpublic.h. See H5Tget_class() for a list
 *                          of class types.
 * \param[out]  type_size   The size of the datatype in bytes
 *
 * \return \herr_t
 *
 * \details H5LTget_dataset_info() retrieves information about a dataset
 *          named \p dset_name attached to the object \p loc_id.
 *
 */
H5_HLDLL herr_t H5LTget_dataset_info(hid_t loc_id, const char *dset_name, hsize_t *dims,
                                     H5T_class_t *type_class, size_t *type_size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Determines whether a dataset exists.
 *
 * \param[in]   loc_id  Identifier of the group containing the dataset
 * \param[in]   name    Dataset name
 *
 * \return \htri_t
 *
 * \details H5LTfind_dataset() determines whether a dataset named
 *          \p name exists in the group specified by \p loc_id.
 *
 *          \p loc_id must be a group identifier and \p name must
 *          specify a dataset that is a member of that group.
 *
 */
H5_HLDLL herr_t H5LTfind_dataset(hid_t loc_id, const char *name);

/*-------------------------------------------------------------------------
 *
 * Set attribute functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes a string attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   attr_data   Buffer with data to be written to the attribute
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_string() creates and writes a string attribute
 *          named \p attr_name and attaches it to the object specified by
 *          the name \p obj_name. If the attribute already exists,
 *          it is overwritten.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_string(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         const char *attr_data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_char() creates and writes a numerical attribute
 *          named \p attr_name and attaches it to the object specified by the
 *          name \p obj_name. The attribute has a dimensionality of 1.
 *          The HDF5 datatype of the attribute is #H5T_NATIVE_CHAR.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_char(hid_t loc_id, const char *obj_name, const char *attr_name,
                                       const char *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_uchar() creates and writes a numerical attribute
 *          named \p attr_name and attaches it to the object specified by the
 *          name \p obj_name. The attribute has a dimensionality of 1.
 *          The HDF5 datatype of the attribute is #H5T_NATIVE_UCHAR.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_uchar(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        const unsigned char *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_short() creates and writes a numerical attribute
 *          named \p attr_name and attaches it to the object specified by the
 *          name \p obj_name. The attribute has a dimensionality of 1.
 *          The HDF5 datatype of the attribute is #H5T_NATIVE_SHORT.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_short(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        const short *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_ushort() creates and writes a numerical attribute
 *          named \p attr_name and attaches it to the object specified by the
 *          name \p obj_name. The attribute has a dimensionality of 1.
 *          The HDF5 datatype of the attribute is #H5T_NATIVE_USHORT.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_ushort(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         const unsigned short *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_int() creates and writes a numerical integer
 *          attribute named \p attr_name and attaches it to the object
 *          specified by the name \p obj_name. The attribute has a
 *          dimensionality of 1.  The HDF5 datatype of the attribute
 *          is #H5T_NATIVE_INT.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_int(hid_t loc_id, const char *obj_name, const char *attr_name,
                                      const int *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_uint() creates and writes a numerical integer
 *          attribute named \p attr_name and attaches it to the object specified
 *          by the name \p obj_name. The attribute has a dimensionality of 1.
 *          The HDF5 datatype of the attribute is #H5T_NATIVE_UINT.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_uint(hid_t loc_id, const char *obj_name, const char *attr_name,
                                       const unsigned int *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_long() creates and writes a numerical
 *          attribute named \p attr_name and attaches it to the object
 *          specified by the name \p obj_name. The attribute has a
 *          dimensionality of 1.  The HDF5 datatype of the attribute
 *          is #H5T_NATIVE_LONG.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_long(hid_t loc_id, const char *obj_name, const char *attr_name,
                                       const long *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Location of the object to which the attribute
 *                          is to be attached
 * \param[in]   obj_name    That object's name
 * \param[in]   attr_name   Attribute name
 * \param[in]   buffer      Attribute value
 * \param[in]   size        Attribute size
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_long_long() creates and writes a numerical
 *          attribute named \p attr_name and attaches it to the object
 *          specified by the name \p obj_name.
 *
 *          The attribute has a dimensionality of 1 and its HDF5 datatype
 *          is #H5T_NATIVE_LLONG.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_long_long(hid_t loc_id, const char *obj_name, const char *attr_name,
                                            const long long *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_ulong() creates and writes a numerical
 *          attribute named \p attr_name and attaches it to the object
 *          specified by the name \p obj_name. The attribute has a
 *          dimensionality of 1.  The HDF5 datatype of the attribute
 *          is #H5T_NATIVE_ULONG.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_ulong(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        const unsigned long *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Location of the object to which the attribute
 *                          is to be attached
 * \param[in]   obj_name    That object's name
 * \param[in]   attr_name   Attribute name
 * \param[in]   buffer      Attribute value
 * \param[in]   size        Attribute size
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_ullong() creates and writes a numerical
 *          attribute named \p attr_name and attaches it to the object
 *          specified by the name \p obj_name.
 *
 *          The attribute has a dimensionality of 1 and its HDF5 datatype
 *          is #H5T_NATIVE_ULLONG.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_ullong(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         const unsigned long long *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_float() creates and writes a numerical
 *          floating point attribute named \p attr_name and attaches
 *          it to the object specified by the name \p obj_name.
 *          The attribute has a dimensionality of 1.  The HDF5 datatype
 *          of the attribute is #H5T_NATIVE_FLOAT.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_float(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        const float *buffer, size_t size);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates and writes an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to create the attribute within
 * \param[in]   obj_name    The name of the object to attach the attribute
 * \param[in]   attr_name   The attribute name
 * \param[in]   buffer      Buffer with data to be written to the attribute
 * \param[in]   size        The size of the 1D array (one in the case of a
 *                          scalar attribute). This value is used by
 *                          H5Screate_simple() to create the dataspace.
 *
 * \return \herr_t
 *
 * \details H5LTset_attribute_double() creates and writes a numerical
 *          attribute named \p attr_name and attaches
 *          it to the object specified by the name \p obj_name.
 *          The attribute has a dimensionality of 1.  The HDF5 datatype
 *          of the attribute is #H5T_NATIVE_DOUBLE.
 *
 */
H5_HLDLL herr_t H5LTset_attribute_double(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         const double *buffer, size_t size);

/*-------------------------------------------------------------------------
 *
 * Get attribute functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[in]   mem_type_id Identifier of the memory datatype
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute() reads an attribute named
 *          \p attr_name with the memory type \p mem_type_id.
 *
 */
H5_HLDLL herr_t H5LTget_attribute(hid_t loc_id, const char *obj_name, const char *attr_name,
                                  hid_t mem_type_id, void *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_string() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The datatype is a string.
 *
 * \version 1.8.9 The content of the buffer returned by the Fortran
 *                subroutine has changed in this release:\n
 *                If the returned buffer requires padding,
 *                h5ltget_attribute_string_f() now employs space
 *                padding; this buffer was previously returned with a C NULL terminator.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_string(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         char *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_char() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The datatype of the attribute
 *          is #H5T_NATIVE_CHAR.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_char(hid_t loc_id, const char *obj_name, const char *attr_name, char *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_uchar() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_UCHAR
 *
 */
H5_HLDLL herr_t H5LTget_attribute_uchar(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        unsigned char *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_short() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_SHORT
 *
 */
H5_HLDLL herr_t H5LTget_attribute_short(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        short *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_ushort() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_USHORT.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_ushort(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         unsigned short *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_int() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_INT.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_int(hid_t loc_id, const char *obj_name, const char *attr_name, int *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_uint() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_INT.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_uint(hid_t loc_id, const char *obj_name, const char *attr_name,
                                       unsigned int *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_long() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_LONG.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_long(hid_t loc_id, const char *obj_name, const char *attr_name, long *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads a \e long \e long attribute.
 *
 * \param[in]   loc_id      Location of the object to which
 *                          the attribute is attached
 * \param[in]   obj_name    That object's name
 * \param[in]   attr_name   Attribute name
 * \param[out]  data        Attribute value
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_long_long() reads the attribute
 *          specified by \p loc_id and \p obj_name.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_long_long(hid_t loc_id, const char *obj_name, const char *attr_name,
                                            long long *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_ulong() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_ULONG.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_ulong(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        unsigned long *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_ullong() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_ULLONG.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_ullong(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         unsigned long long *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_float() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_FLOAT.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_float(hid_t loc_id, const char *obj_name, const char *attr_name,
                                        float *data);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Reads an attribute from disk.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  data        Buffer with data
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_double() reads an attribute named
 *          \p attr_name that is attached to the object specified
 *          by the name \p obj_name.  The HDF5 datatype of the
 *          attribute is #H5T_NATIVE_DOUBLE.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_double(hid_t loc_id, const char *obj_name, const char *attr_name,
                                         double *data);

/*-------------------------------------------------------------------------
 *
 * Query attribute functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Gets the dimensionality of an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  rank        The dimensionality of the attribute
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_ndims() gets the dimensionality of an attribute
 *          named \p attr_name that is attached to the object specified
 *          by the name \p obj_name.
 *
 */
H5_HLDLL herr_t H5LTget_attribute_ndims(hid_t loc_id, const char *obj_name, const char *attr_name, int *rank);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Gets information about an attribute.
 *
 * \param[in]   loc_id      Identifier of the object (dataset or group)
 *                          to read the attribute from
 * \param[in]   obj_name    The name of the object that the attribute is
 *                          attached to
 * \param[in]   attr_name   The attribute name
 * \param[out]  dims        The dimensions of the attribute
 * \param[out]  type_class  The class identifier. #H5T_class_t is
 *                          defined in H5Tpublic.h. For a list of valid class
 *                          types see: H5Tget_class().
 * \param[out]  type_size   The size of the datatype in bytes
 *
 * \return \herr_t
 *
 * \details H5LTget_attribute_info() gets information about an attribute
 *          named \p attr_name attached to the object specified by
 *          the name \p obj_name.
 *
 * \par Example
 * \snippet H5LT_examples.c get_attribute_info
 *
 */
H5_HLDLL herr_t H5LTget_attribute_info(hid_t loc_id, const char *obj_name, const char *attr_name,
                                       hsize_t *dims, H5T_class_t *type_class, size_t *type_size);

/*-------------------------------------------------------------------------
 *
 * General functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates an HDF5 datatype given a text description.
 *
 * \param[in] text      A character string containing a DDL
 *                      definition of the datatype to be created
 * \param[in] lang_type The language used to describe the datatype.
 *                      The only currently supported language is
 *                      #H5LT_DDL.
 *
 * \return  Returns the datatype identifier(non-negative) if successful;
 *          otherwise returns a negative value.
 *
 * \details Given a text description of a datatype, this function creates
 *          an HDF5 datatype and returns the datatype identifier.
 *          The text description of the datatype has to comply with the
 *          \p lang_type definition of HDF5 datatypes.
 *          Currently, only the DDL(#H5LT_DDL) is supported.
 *          The complete DDL definition of HDF5 datatypes can be found in
 *          the specifications chapter of the
 *          <a href="https://\DOXURL/_s_p_e_c.html">
 *          HDF5 User's Guide</a>.
 *
 * \par Example
 * An example of DDL definition of \c enum type is shown as follows.
 * \snippet H5LT_examples.c enum
 *
 */
H5_HLDLL hid_t H5LTtext_to_dtype(const char *text, H5LT_lang_t lang_type);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Creates a text description of an HDF5 datatype.
 *
 * \param[in] dtype     Identifier of the datatype to be converted
 * \param[out] str      Buffer for the text description of the datatype
 * \param[in] lang_type The language used to describe the datatype.
 *                      The currently supported language is #H5LT_DDL.
 * \param[out] len      The size of buffer needed to store the text description
 *
 * \return  \herr_t
 *
 * \details Given an HDF5 datatype identifier, this function creates
 *          a description of this datatype in \p lang_type language format.
 *          A preliminary H5LTdtype_to_text() call can be made to determine
 *          the size of the buffer needed with a NULL passed in for \p str.
 *          This value is returned as \p len. That value can then be assigned
 *          to len for a second H5Ttype_to_text() call, which will
 *          retrieve the actual text description for the datatype.
 *
 *          If \p len is not big enough for the description, the text
 *          description will be truncated to fit in the buffer.
 *
 *          Currently only DDL (#H5LT_DDL) is supported for \p lang_type.
 *          The complete DDL definition of HDF5 data types can be found in
 *          the specifications chapter of the
 *          <a href="https://\DOXURL/_s_p_e_c.html">
 *          HDF5 User's Guide</a>.
 *
 * \par Example
 * An example of DDL definition of \c enum type is shown as follows.
 * \snippet H5LT_examples.c enum
 *
 */
H5_HLDLL herr_t H5LTdtype_to_text(hid_t dtype, char *str, H5LT_lang_t lang_type, size_t *len);

/*-------------------------------------------------------------------------
 *
 * Utility functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Determines whether an attribute exists.
 *
 * \param[in] loc_id    Identifier of the object to which the attribute
 *                      is expected to be attached
 * \param[in] name      Attribute name
 *
 * \return  \htri_t
 *
 * \details H5LTfind_attribute() determines whether an attribute named
 *          \p name exists attached to the object specified
 *          by \p loc_id.
 *
 *          \p loc_id must be an object identifier and \p name
 *          must specify an attribute that is expected to be attached
 *          to that object.
 *
 */
H5_HLDLL herr_t H5LTfind_attribute(hid_t loc_id, const char *name);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Determines whether an HDF5 path is valid and, optionally,
 *        whether the path resolves to an HDF5 object.
 *
 * \param[in] loc_id                Identifier of an object in the file
 * \param[in] path                  The path to the object to check;
 *                                  links in \p path may be of any type.
 * \param[in] check_object_valid    If TRUE, determine whether the final
 *                                  component of \p path resolves to
 *                                  an object; if FALSE, do not check.
 *
 * \return  Upon success:
 * \return  If \p check_object_valid is set to \c FALSE:
 * \return  Returns \c TRUE if the path is valid;
 *          otherwise returns \c FALSE.
 * \return  If \p check_object_valid is set to \c TRUE:
 * \return  Returns \c TRUE if the path is valid and
 *          resolves to an HDF5 object;
 *          otherwise returns \c FALSE.
 *
 * \return  Upon error, returns a negative value.
 *
 * \details H5LTpath_valid() checks the validity of \p path relative
 *          to the identifier of an object, \p loc_id. Optionally,
 *          \p check_object_valid can be set to determine whether the
 *          final component of \p path resolves to an HDF5 object;
 *          if not, the final component is a dangling link.
 *
 *          The meaning of the function's return value depends on the
 *          value of \p check_object_valid:
 *
 *          If \p check_object_valid is set to \c FALSE, H5LTpath_valid()
 *          will check all links in \p path to verify that they exist.
 *          If all the links in \p path exist, the function will
 *          return \c TRUE; otherwise the function will return \c FALSE.
 *
 *          If \p check_object_valid is set to \c TRUE,
 *          H5LTpath_valid() will first check the links in \p path,
 *          as described above. If all the links exist,
 *          \p check_object_valid will then determine whether the final
 *          component of \p path resolves to an actual HDF5 object.
 *          H5LTpath_valid() will return \c TRUE if all the links in
 *          \p path exist and the final component resolves to an
 *          actual object; otherwise, it will return \c FALSE.
 *
 *          \p path can be any one of the following:
 *
 *          - An absolute path, which starts with a slash (\c /)
 *            indicating the file's root group, followed by the members
 *          - A relative path with respect to \p loc_id
 *          - A dot (\c .), if \p loc_id is the object identifier for
 *            the object itself.
 *
 *          If \p path is an absolute path, then \p loc_id can be an
 *          identifier for any object in the file as it is used only to
 *          identify the file. If \p path is a relative path, then
 *          \p loc_id must be a file or a group identifier.
 *
 * \note
 * <b>Note on Behavior Change:</b>
 * The behavior of  H5LTpath_valid() was changed in the 1.10.0 release
 * in the case where the root group, “/”, is the value of path.
 * This change is described below:
 *     - Let \p loc_id denote a valid HDF5 file identifier, and let
 *       \p check_object_valid be set to true or false.
 *       A call to  H5LTpath_valid() with arguments \p loc_id, “/”,
 *       and \p check_object_valid returns a positive value;
 *       in other words, H5LTpath_valid(loc_id, "/", check_object_valid)
 *       returns a positive value.
 *       In HDF5 version 1.8.16, this function returns 0.
 *     - Let ‘root’ denote a valid HDF5 group identifier that refers
 *       to the root group of an HDF5 file, and let \p check_object_valid
 *       be set to true or false.
 *       A call to H5LTpath_valid() with arguments ‘root’, “/”, and
 *       \p check_object_valid returns a positive value;
 *       in other words, H5LTpath_valid(root, "/", check_object_valid)
 *       returns a positive value.
 *       In HDF5 version 1.8.16, this function returns 0.
 *
 * \version 1.10.0 Function behavior changed in this release.
 *                 See the “Note on Behavior Change” section above.
 *
 */
H5_HLDLL htri_t H5LTpath_valid(hid_t loc_id, const char *path, hbool_t check_object_valid);

/*-------------------------------------------------------------------------
 *
 * File image operations functions
 *
 *-------------------------------------------------------------------------
 */

/**
 *-------------------------------------------------------------------------
 * \ingroup H5LT
 *
 * \brief Opens an HDF5 file image in memory.
 *
 * \param[in] buf_ptr   A pointer to the supplied initial image
 * \param[in] buf_size  Size of the supplied buffer
 * \param[in] flags     Flags specifying whether to open the image
 *                      read-only or read/write, whether HDF5 is to
 *                      take control of the buffer, and instruction
 *                      regarding releasing the buffer.
 *
 * \return  Returns a file identifier if successful;
 *          otherwise returns a negative value.
 * \warning \Bold{Failure Modes:}
 * \warning H5LTopen_file_image() will fail if either \p buf_ptr is NULL
 *          or \p buf_size equals 0 (zero).
 *
 *
 * \details H5LTopen_file_image() opens the HDF5 file image that is
 *          located in system memory at the address indicated by
 *          \p buf_ptr of size \p buf_size.
 *          H5LTopen_file_image() opens a file image with the
 *          Core driver, #H5FD_CORE.
 *
 *          A value of NULL for \p buf_ptr is invalid and will
 *          cause the function to fail.
 *
 *          A value of 0 for \p buf_size is invalid and will cause
 *          the function to fail.
 *
 *          The flags passed in \p flags specify whether to open the image
 *          read-only or read/write, whether HDF5 is to take control of the
 *          buffer, and instruction regarding releasing the buffer.
 *          Valid values are:
 *          - #H5LT_FILE_IMAGE_OPEN_RW
 *            - Specifies opening the file image in read/write mode.
 *            - Default without this flag: File image will be opened read-only.
 *
 *          - #H5LT_FILE_IMAGE_DONT_COPY
 *            - Specifies to not copy the provided file image buffer;
 *              the buffer will be used directly. HDF5 will release the
 *              file image when finished.
 *            - Default without this flag: Copy the file image buffer and
 *              open the copied file image.
 *
 *          - #H5LT_FILE_IMAGE_DONT_RELEASE
 *            - Specifies that HDF5 is not to release the buffer when
 *              the file opened with H5LTopen_file_image() is closed;
 *              releasing the buffer will be left to the application.
 *            - Default without this flag: HDF5 will automatically
 *              release the file image buffer after the file image is
 *              closed.  This flag is valid only when used with
 *              #H5LT_FILE_IMAGE_DONT_COPY.
 *
 * \note      **Motivation:**
 * \note      H5LTopen_file_image() and other elements of HDF5
 *            are used to load an image of an HDF5 file into system memory
 *            and open that image as a regular HDF5 file. An application can
 *            then use the file without the overhead of disk I/O.
 *
 * \note      **Recommended Reading:**
 * \note      This function is part of the file image operations feature set.
 *            It is highly recommended to study the guide
 *            <a href="https://\RFCURL/HDF5FileImageOperations.pdf">
 *            HDF5 File Image Operations</a> before using this feature set.\n
 *            See the “See Also” section below for links to other elements of
 *            HDF5 file image operations.
 *
 * \todo There is no "See Also" section???
 *
 * \since 1.8.9
 */
H5_HLDLL hid_t H5LTopen_file_image(void *buf_ptr, size_t buf_size, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif
