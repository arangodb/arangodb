// C++ informative line for the emacs editor: -*- C++ -*-
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

#ifndef H5ArrayType_H
#define H5ArrayType_H

namespace H5 {

/*! \class ArrayType
    \brief Class ArrayType inherits from DataType and provides wrappers for
     the HDF5's Array Datatypes.
*/
// Inheritance: DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP ArrayType : public DataType {
  public:
    // Constructor that creates a new array data type based on the
    // specified base type.
    ArrayType(const DataType &base_type, int ndims, const hsize_t *dims);

    // Assignment operator
    ArrayType &operator=(const ArrayType &rhs);

    // Constructors that open an array datatype, given a location.
    ArrayType(const H5Location &loc, const char *name);
    ArrayType(const H5Location &loc, const H5std_string &name);

    // Returns an ArrayType object via DataType* by decoding the
    // binary object description of this type.
    virtual DataType *decode() const override;

    // Returns the number of dimensions of this array datatype.
    int getArrayNDims() const;
    // int getArrayNDims(); // removed 1.8.18 and 1.10.1

    // Returns the sizes of dimensions of this array datatype.
    int getArrayDims(hsize_t *dims) const;
    // int getArrayDims(hsize_t* dims); // removed 1.8.18 and 1.10.1

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("ArrayType");
    }

    // Copy constructor: same as the original ArrayType.
    ArrayType(const ArrayType &original);

    // Constructor that takes an existing id
    ArrayType(const hid_t existing_id);

    // Noop destructor
    virtual ~ArrayType() override = default;

    // Default constructor
    ArrayType();

}; // end of ArrayType
} // namespace H5

#endif // H5ArrayType_H
