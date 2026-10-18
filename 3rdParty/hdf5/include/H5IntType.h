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

#ifndef H5IntType_H
#define H5IntType_H

namespace H5 {

/*! \class IntType
    \brief IntType is a derivative of a DataType and operates on HDF5
    integer datatype.
*/
//  Inheritance: AtomType -> DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP IntType : public AtomType {
  public:
    // Creates an integer type using a predefined type
    IntType(const PredType &pred_type);

    // Gets the integer datatype of the specified dataset
    IntType(const DataSet &dataset);

    // Constructors that open an HDF5 integer datatype, given a location.
    IntType(const H5Location &loc, const char *name);
    IntType(const H5Location &loc, const H5std_string &name);

    // Returns an IntType object via DataType* by decoding the
    // binary object description of this type.
    virtual DataType *decode() const override;

    // Retrieves the sign type for an integer type
    H5T_sign_t getSign() const;

    // Sets the sign property for an integer type.
    void setSign(H5T_sign_t sign) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("IntType");
    }

    // Default constructor
    IntType();

    // Creates a integer datatype using an existing id
    IntType(const hid_t existing_id);

    // Copy constructor: same as the original IntType.
    IntType(const IntType &original);

    // Noop destructor.
    virtual ~IntType() override = default;

}; // end of IntType
} // namespace H5

#endif // H5IntType_H
