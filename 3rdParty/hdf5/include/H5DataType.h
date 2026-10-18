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

#ifndef H5DataType_H
#define H5DataType_H

namespace H5 {

/*! \class DataType
    \brief Class DataType provides generic operations on HDF5 datatypes.

    DataType inherits from H5Object because a named datatype is an HDF5
    object and is a base class of ArrayType, AtomType, CompType, EnumType,
    and VarLenType.
*/
//  Inheritance: DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP DataType : public H5Object {
  public:
    // Creates a datatype given its class and size
    DataType(const H5T_class_t type_class, size_t size);

    // Copy constructor - same as the original DataType.
    DataType(const DataType &original);

    // Creates a copy of a predefined type
    DataType(const PredType &pred_type);

    // Constructors to open a generic named datatype at a given location.
    DataType(const H5Location &loc, const char *name);
    DataType(const H5Location &loc, const H5std_string &name);

    // Creates a datatype by way of dereference.
    DataType(const H5Location &loc, const void *ref, H5R_type_t ref_type = H5R_OBJECT,
             const PropList &plist = PropList::DEFAULT);
    //        DataType(const Attribute& attr, const void* ref, H5R_type_t ref_type = H5R_OBJECT, const
    //        PropList& plist = PropList::DEFAULT);

    // Closes this datatype.
    virtual void close() override;

    // Copies an existing datatype to this datatype object.
    void copy(const DataType &like_type);

    // Copies the datatype of dset to this datatype object.
    void copy(const DataSet &dset);

    // Returns a DataType instance by decoding the binary object
    // description of this datatype.
    virtual DataType *decode() const;

    // Creates a binary object description of this datatype.
    void encode();

    // Returns the datatype class identifier.
    H5T_class_t getClass() const;

    // Commits a transient datatype to a file; this datatype becomes
    // a named datatype which can be accessed from the location.
    void commit(const H5Location &loc, const char *name);
    void commit(const H5Location &loc, const H5std_string &name);

    // These two overloaded functions are kept for backward compatibility
    // only; they missed the const - removed from 1.8.18 and 1.10.1
    // void commit(H5Location& loc, const char* name);
    // void commit(H5Location& loc, const H5std_string& name);

    // Determines whether this datatype is a named datatype or
    // a transient datatype.
    bool committed() const;

    // Finds a conversion function that can handle the conversion
    // this datatype to the given datatype, dest.
    H5T_conv_t find(const DataType &dest, H5T_cdata_t **pcdata) const;

    // Converts data from between specified datatypes.
    void convert(const DataType &dest, size_t nelmts, void *buf, void *background,
                 const PropList &plist = PropList::DEFAULT) const;

    // Assignment operator
    DataType &operator=(const DataType &rhs);

    // Determines whether two datatypes are the same.
    bool operator==(const DataType &compared_type) const;

    // Determines whether two datatypes are not the same.
    bool operator!=(const DataType &compared_type) const;

    // Locks a datatype.
    void lock() const;

    // Returns the size of a datatype.
    size_t getSize() const;

    // Returns the base datatype from which a datatype is derived.
    // Note: not quite right for specific types yet???
    DataType getSuper() const;

    // Registers a conversion function.
    void registerFunc(H5T_pers_t pers, const char *name, const DataType &dest, H5T_conv_t func) const;
    void registerFunc(H5T_pers_t pers, const H5std_string &name, const DataType &dest, H5T_conv_t func) const;

    // Removes a conversion function from all conversion paths.
    void unregister(H5T_pers_t pers, const char *name, const DataType &dest, H5T_conv_t func) const;
    void unregister(H5T_pers_t pers, const H5std_string &name, const DataType &dest, H5T_conv_t func) const;

    // Tags an opaque datatype.
    void setTag(const char *tag) const;
    void setTag(const H5std_string &tag) const;

    // Gets the tag associated with an opaque datatype.
    H5std_string getTag() const;

    // Checks whether this datatype contains (or is) a certain type class.
    bool        detectClass(H5T_class_t cls) const;
    static bool detectClass(const PredType &pred_type, H5T_class_t cls);

    // Checks whether this datatype is a variable-length string.
    bool isVariableStr() const;

    // Returns a copy of the creation property list of a datatype.
    PropList getCreatePlist() const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("DataType");
    }

    // Creates a copy of an existing DataType using its id
    DataType(const hid_t type_id);

    // Default constructor
    DataType();

    // Determines whether this datatype has a binary object description.
    bool hasBinaryDesc() const;

    // Gets the datatype id.
    virtual hid_t getId() const override;

    // Destructor: properly terminates access to this datatype.
    virtual ~DataType() override;

  protected:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    hid_t id; // HDF5 datatype id

    // Returns an id of a type by decoding the binary object
    // description of this datatype.
    hid_t p_decode() const;

    // Sets the datatype id.
    virtual void p_setId(const hid_t new_id) override;

    // Opens a datatype and returns the id.
    hid_t p_opentype(const H5Location &loc, const char *dtype_name) const;

#endif // DOXYGEN_SHOULD_SKIP_THIS

  private:
    // Buffer for binary object description of this datatype, allocated
    // in DataType::encode and used in DataType::decode
    unsigned char *encoded_buf;
    size_t         buf_size;

    // Friend function to set DataType id.  For library use only.
    friend void f_DataType_setId(DataType *dtype, hid_t new_id);

    void p_commit(hid_t loc_id, const char *name);

}; // end of DataType
} // namespace H5

#endif // H5DataType_H
