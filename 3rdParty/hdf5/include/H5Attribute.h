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

#ifndef H5Attribute_H
#define H5Attribute_H

namespace H5 {

/*! \class Attribute
    \brief Class Attribute operates on HDF5 attributes.

    An attribute has many characteristics similar to a dataset, thus both
    Attribute and DataSet are derivatives of AbstractDs.  Attribute also
    inherits from H5Location because an attribute can be used to specify
    a location.
*/
//  Inheritance: multiple H5Location/AbstractDs -> IdComponent
class H5_DLLCPP Attribute : public AbstractDs, public H5Location {
  public:
    // Copy constructor: same as the original Attribute.
    Attribute(const Attribute &original);

    // Default constructor
    Attribute();

    // Creates a copy of an existing attribute using the attribute id
    Attribute(const hid_t attr_id);

    // Closes this attribute.
    virtual void close() override;

    // Gets the name of this attribute.
    ssize_t      getName(char *attr_name, size_t buf_size = 0) const;
    H5std_string getName(size_t len) const;
    H5std_string getName() const;
    ssize_t      getName(H5std_string &attr_name, size_t len = 0) const;
    // The overloaded function below is replaced by the one above and it
    // is kept for backward compatibility purpose.
    ssize_t getName(size_t buf_size, H5std_string &attr_name) const;

    // Gets a copy of the dataspace for this attribute.
    virtual DataSpace getSpace() const override;

    // Returns the amount of storage size required for this attribute.
    virtual hsize_t getStorageSize() const override;

    // Returns the in memory size of this attribute's data.
    virtual size_t getInMemDataSize() const override;

    // Reads data from this attribute.
    void read(const DataType &mem_type, void *buf) const;
    void read(const DataType &mem_type, H5std_string &strg) const;

    // Writes data to this attribute.
    void write(const DataType &mem_type, const void *buf) const;
    void write(const DataType &mem_type, const H5std_string &strg) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("Attribute");
    }

    // Gets the attribute id.
    virtual hid_t getId() const override;

    // Destructor: properly terminates access to this attribute.
    virtual ~Attribute() override;

    // Copy assignment operator.
    Attribute &operator=(const Attribute &original);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  protected:
    // Sets the attribute id.
    virtual void p_setId(const hid_t new_id) override;
#endif // DOXYGEN_SHOULD_SKIP_THIS

  private:
    hid_t id; // HDF5 attribute id

    // This function contains the common code that is used by
    // getTypeClass and various API functions getXxxType
    // defined in AbstractDs for generic datatype and specific
    // sub-types
    virtual hid_t p_get_type() const override;

    // Reads variable or fixed len strings from this attribute.
    void p_read_variable_len(const DataType &mem_type, H5std_string &strg) const;
    void p_read_fixed_len(const DataType &mem_type, H5std_string &strg) const;

    // Friend function to set Attribute id.  For library use only.
    friend void f_Attribute_setId(Attribute *attr, hid_t new_id);

}; // end of Attribute
} // namespace H5

#endif // H5Attribute_H
