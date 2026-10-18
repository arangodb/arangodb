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

#ifndef H5Group_H
#define H5Group_H

namespace H5 {

/*! \class Group
    \brief Class Group represents an HDF5 group.
*/
//  Inheritance: CommonFG/H5Object -> H5Location -> IdComponent
class H5_DLLCPP Group : public H5Object, public CommonFG {
  public:
    // Close this group.
    virtual void close() override;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("Group");
    }

    // Throw group exception.
    virtual void throwException(const H5std_string &func_name, const H5std_string &msg) const override;

    // for CommonFG to get the file id.
    virtual hid_t getLocId() const override;

    // Creates a group by way of dereference.
    Group(const H5Location &loc, const void *ref, H5R_type_t ref_type = H5R_OBJECT,
          const PropList &plist = PropList::DEFAULT);
    // Removed in 1.10.1, because H5Location is baseclass
    //        Group(const Attribute& attr, const void* ref, H5R_type_t ref_type = H5R_OBJECT, const PropList&
    //        plist = PropList::DEFAULT);

    // Returns the number of objects in this group.
    hsize_t getNumObjs() const;

    // Opens an object within a group or a file, i.e., root group.
    hid_t getObjId(const char *name, const PropList &plist = PropList::DEFAULT) const;
    hid_t getObjId(const H5std_string &name, const PropList &plist = PropList::DEFAULT) const;

    // Closes an object opened by getObjId().
    void closeObjId(hid_t obj_id) const;

    // default constructor
    Group();

    // Copy constructor: same as the original Group.
    Group(const Group &original);

    // Gets the group id.
    virtual hid_t getId() const override;

    // Destructor
    virtual ~Group() override;

    // Copy assignment operator.
    Group &operator=(const Group &original);

    // Creates a copy of an existing group using its id.
    Group(const hid_t group_id);

  protected:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    // Sets the group id.
    virtual void p_setId(const hid_t new_id) override;
#endif // DOXYGEN_SHOULD_SKIP_THIS

  private:
    hid_t id; // HDF5 group id

}; // end of Group
} // namespace H5

#endif // H5Group_H
