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

#ifndef H5EnumType_H
#define H5EnumType_H

namespace H5 {

/*! \class EnumType
    \brief EnumType is a derivative of a DataType and operates on HDF5
    enum datatypes.
*/
//  Inheritance: DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP EnumType : public DataType {

  public:
    // Creates an empty enumeration datatype based on a native signed
    // integer type, whose size is given by size.
    EnumType(size_t size);

    // Gets the enum datatype of the specified dataset
    EnumType(const DataSet &dataset); // H5Dget_type

    // Creates a new enum datatype based on an integer datatype
    EnumType(const IntType &data_type); // H5Tenum_create

    // Constructors that open an enum datatype, given a location.
    EnumType(const H5Location &loc, const char *name);
    EnumType(const H5Location &loc, const H5std_string &name);

    // Returns an EnumType object via DataType* by decoding the
    // binary object description of this type.
    virtual DataType *decode() const override;

    // Returns the number of members in this enumeration datatype.
    int getNmembers() const;

    // Returns the index of a member in this enumeration data type.
    int getMemberIndex(const char *name) const;
    int getMemberIndex(const H5std_string &name) const;

    // Returns the value of an enumeration datatype member
    void getMemberValue(unsigned memb_no, void *value) const;

    // Inserts a new member to this enumeration type.
    void insert(const char *name, void *value) const;
    void insert(const H5std_string &name, void *value) const;

    // Returns the symbol name corresponding to a specified member
    // of this enumeration datatype.
    H5std_string nameOf(void *value, size_t size) const;

    // Returns the value corresponding to a specified member of this
    // enumeration datatype.
    void valueOf(const char *name, void *value) const;
    void valueOf(const H5std_string &name, void *value) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("EnumType");
    }

    // Default constructor
    EnumType();

    // Creates an enumeration datatype using an existing id
    EnumType(const hid_t existing_id);

    // Copy constructor: same as the original EnumType.
    EnumType(const EnumType &original);

    virtual ~EnumType() override = default;

}; // end of EnumType
} // namespace H5

#endif // H5EnumType_H
