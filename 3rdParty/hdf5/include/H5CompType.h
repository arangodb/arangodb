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

#ifndef H5CompType_H
#define H5CompType_H

namespace H5 {

/*! \class CompType
    \brief CompType is a derivative of a DataType and operates on HDF5
    compound datatypes.
*/
//  Inheritance: DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP CompType : public DataType {
  public:
    // Default constructor
    CompType();

    // Creates a compound datatype using an existing id
    CompType(const hid_t existing_id);

    // Creates a new compound datatype, given the type's size
    CompType(size_t size); // H5Tcreate

    // Gets the compound datatype of the specified dataset
    CompType(const DataSet &dataset); // H5Dget_type

    // Copy constructor - same as the original CompType.
    CompType(const CompType &original);

    // Constructors that open a compound datatype, given a location.
    CompType(const H5Location &loc, const char *name);
    CompType(const H5Location &loc, const H5std_string &name);

    // Returns a CompType object via DataType* by decoding the binary
    // object description of this type.
    virtual DataType *decode() const override;

    // Returns the type class of the specified member of this compound
    // datatype.  It provides to the user a way of knowing what type
    // to create another datatype of the same class
    H5T_class_t getMemberClass(unsigned member_num) const;

    // Returns the index of a member in this compound data type.
    int getMemberIndex(const char *name) const;
    int getMemberIndex(const H5std_string &name) const;

    // Returns the offset of a member of this compound datatype.
    size_t getMemberOffset(unsigned memb_no) const;

    // Returns the name of a member of this compound datatype.
    H5std_string getMemberName(unsigned member_num) const;

    // Returns the generic datatype of the specified member in
    // this compound datatype.
    DataType getMemberDataType(unsigned member_num) const;

    // Returns the array datatype of the specified member in
    // this compound datatype.
    ArrayType getMemberArrayType(unsigned member_num) const;

    // Returns the compound datatype of the specified member in
    // this compound datatype.
    CompType getMemberCompType(unsigned member_num) const;

    // Returns the enumeration datatype of the specified member in
    // this compound datatype.
    EnumType getMemberEnumType(unsigned member_num) const;

    // Returns the integer datatype of the specified member in
    // this compound datatype.
    IntType getMemberIntType(unsigned member_num) const;

    // Returns the floating-point datatype of the specified member in
    // this compound datatype.
    FloatType getMemberFloatType(unsigned member_num) const;

    // Returns the string datatype of the specified member in
    // this compound datatype.
    StrType getMemberStrType(unsigned member_num) const;

    // Returns the variable length datatype of the specified member in
    // this compound datatype.
    VarLenType getMemberVarLenType(unsigned member_num) const;

    // Returns the number of members in this compound datatype.
    int getNmembers() const;

    // Adds a new member to this compound datatype.
    void insertMember(const H5std_string &name, size_t offset, const DataType &new_member) const;

    // Recursively removes padding from within this compound datatype.
    void pack() const;

    // Sets the total size for this compound datatype.
    void setSize(size_t size) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("CompType");
    }

    // Noop destructor.
    virtual ~CompType() override = default;

  private:
    // Contains common code that is used by the member functions
    // getMemberXxxType
    hid_t p_get_member_type(unsigned member_num) const;

}; // end of CompType
} // namespace H5

#endif // H5CompType_H
