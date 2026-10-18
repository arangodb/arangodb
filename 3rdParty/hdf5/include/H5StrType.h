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

#ifndef H5StrType_H
#define H5StrType_H

namespace H5 {

/*! \class StrType
    \brief StrType is a derivative of a DataType and operates on HDF5
    string datatype.
*/
//  Inheritance: AtomType -> DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP StrType : public AtomType {
  public:
    // Creates a string type using a predefined type
    StrType(const PredType &pred_type);

    // Creates a string type with specified length - may be obsolete
    StrType(const PredType &pred_type, const size_t &size);

    // Creates a string type with specified length
    StrType(const int dummy, const size_t &size);

    // Gets the string datatype of the specified dataset
    StrType(const DataSet &dataset);

    // Constructors that open an HDF5 string datatype, given a location.
    StrType(const H5Location &loc, const char *name);
    StrType(const H5Location &loc, const H5std_string &name);

    // Returns an StrType object via DataType* by decoding the
    // binary object description of this type.
    virtual DataType *decode() const override;

    // Retrieves the character set type of this string datatype.
    H5T_cset_t getCset() const;

    // Sets character set to be used.
    void setCset(H5T_cset_t cset) const;

    // Retrieves the string padding method for this string datatype.
    H5T_str_t getStrpad() const;

    // Defines the storage mechanism for character strings.
    void setStrpad(H5T_str_t strpad) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("StrType");
    }

    // default constructor
    StrType();

    // Creates a string datatype using an existing id
    StrType(const hid_t existing_id);

    // Copy constructor: same as the original StrType.
    StrType(const StrType &original);

    // Noop destructor.
    virtual ~StrType() override = default;

}; // end of StrType
} // namespace H5

#endif // H5StrType_H
