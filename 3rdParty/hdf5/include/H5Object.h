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

#ifndef H5Object_H
#define H5Object_H

namespace H5 {

/*! \class H5Object
    \brief Class H5Object is a bridge between H5Location and DataSet, DataType,
     and Group.

    Modification:
        Sept 18, 2012: Added class H5Location in between IdComponent and
                H5Object.  An H5File now inherits from H5Location.  All HDF5
                wrappers in H5Object are moved up to H5Location.  H5Object
                is left mostly empty for future wrappers that are only for
                group, dataset, and named datatype.  Note that the reason for
                adding H5Location instead of simply moving H5File to be under
                H5Object is H5File is not an HDF5 object, and renaming H5Object
                to H5Location will risk breaking user applications.
                -BMR
        Apr 2, 2014: Added wrapper getObjName for H5Iget_name
        Sep 21, 2016: Rearranging classes (HDFFV-9920) moved H5A wrappers back
                into H5Object.  This way, C functions that takes attribute id
                can be in H5Location and those that cannot take attribute id
                can be in H5Object.
*/
// Inheritance: H5Location -> IdComponent

// Define the operator function pointer for H5Aiterate().
typedef void (*attr_operator_t)(H5Object &loc, const H5std_string attr_name, void *operator_data);

// Define the operator function pointer for H5Ovisit3().
typedef int (*visit_operator_t)(H5Object &obj, const H5std_string attr_name, const H5O_info2_t *oinfo,
                                void *operator_data);

// User data for attribute iteration
class UserData4Aiterate {
  public:
    attr_operator_t op;
    void           *opData;
    H5Object       *location; // Consider changing to H5Location
};

// User data for visit iteration
class UserData4Visit {
  public:
    visit_operator_t op;
    void            *opData;
    H5Object        *obj;
};

class H5_DLLCPP H5Object : public H5Location {
  public:
    // Creates an attribute for the specified object
    // PropList is currently not used, so always be default.
    Attribute createAttribute(const char *name, const DataType &type, const DataSpace &space,
                              const PropList &create_plist = PropList::DEFAULT) const;
    Attribute createAttribute(const H5std_string &name, const DataType &type, const DataSpace &space,
                              const PropList &create_plist = PropList::DEFAULT) const;

    // Given its name, opens the attribute that belongs to an object at
    // this location.
    Attribute openAttribute(const char *name) const;
    Attribute openAttribute(const H5std_string &name) const;

    // Given its index, opens the attribute that belongs to an object at
    // this location.
    Attribute openAttribute(const unsigned int idx) const;

    // Iterate user's function over the attributes of this object.
    int iterateAttrs(attr_operator_t user_op, unsigned *idx = NULL, void *op_data = NULL);

    // Recursively visit elements reachable from this object.
    void visit(H5_index_t idx_type, H5_iter_order_t order, visit_operator_t user_op, void *op_data,
               unsigned int fields);

    // Returns the object header version of an object
    unsigned objVersion() const;

    // Determines the number of attributes belong to this object.
    int getNumAttrs() const;

    // Checks whether the named attribute exists for this object.
    bool attrExists(const char *name) const;
    bool attrExists(const H5std_string &name) const;

    // Renames the named attribute to a new name.
    void renameAttr(const char *oldname, const char *newname) const;
    void renameAttr(const H5std_string &oldname, const H5std_string &newname) const;

    // Removes the named attribute from this object.
    void removeAttr(const char *name) const;
    void removeAttr(const H5std_string &name) const;

    // Returns an identifier.
    virtual hid_t getId() const override = 0;

    // Gets the name of this HDF5 object, i.e., Group, DataSet, or
    // DataType.
    ssize_t      getObjName(char *obj_name, size_t buf_size = 0) const;
    ssize_t      getObjName(H5std_string &obj_name, size_t len = 0) const;
    H5std_string getObjName() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

  protected:
    // Default constructor
    H5Object();

    // Sets the identifier of this object to a new value. - this one
    // doesn't increment reference count
    virtual void p_setId(const hid_t new_id) override = 0;

    // Noop destructor.
    virtual ~H5Object() override = default;

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of H5Object
} // namespace H5

#endif // H5Object_H
