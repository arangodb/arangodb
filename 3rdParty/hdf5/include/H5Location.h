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

#ifndef H5Location_H
#define H5Location_H

#include "H5Classes.h" // contains forward class declarations

namespace H5 {

/*! \class H5Location
    \brief H5Location is an abstract base class, added in version 1.8.12.

    It provides a collection of wrappers for the C functions that take a
    location identifier to specify the HDF5 object.  The location identifier
    can be either file, group, dataset, attribute, or named datatype.
    Wrappers for H5A functions stay in H5Object.
*/
// Inheritance: IdComponent
class H5_DLLCPP H5Location : public IdComponent {
  public:
    // Checks if a link of a given name exists in a location
    bool nameExists(const char *name, const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    bool nameExists(const H5std_string &name, const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

    // Checks if a link of a given name exists in a location
    // Deprecated in favor of nameExists for better name.
    bool exists(const char *name, const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    bool exists(const H5std_string &name, const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

    // Flushes all buffers associated with this location to disk.
    void flush(H5F_scope_t scope) const;

    // Gets the name of the file, specified by this location.
    H5std_string getFileName() const;

#ifndef H5_NO_DEPRECATED_SYMBOLS
    // Retrieves the type of object that an object reference points to.
    H5G_obj_t getObjType(void *ref, H5R_type_t ref_type = H5R_OBJECT) const;
#endif /* H5_NO_DEPRECATED_SYMBOLS */

    // Retrieves the type of object that an object reference points to.
    H5O_type_t getRefObjType(void *ref, H5R_type_t ref_type = H5R_OBJECT) const;
    // Note: getRefObjType deprecates getObjType, but getObjType's name is
    // misleading, so getRefObjType is used in the new function instead.

    // Sets the comment for an HDF5 object specified by its name.
    void setComment(const char *name, const char *comment) const;
    void setComment(const H5std_string &name, const H5std_string &comment) const;
    void setComment(const char *comment) const;
    void setComment(const H5std_string &comment) const;

    // Retrieves comment for the HDF5 object specified by its name.
    ssize_t      getComment(const char *name, size_t buf_size, char *comment) const;
    H5std_string getComment(const char *name, size_t buf_size = 0) const;
    H5std_string getComment(const H5std_string &name, size_t buf_size = 0) const;

    // Removes the comment for the HDF5 object specified by its name.
    void removeComment(const char *name) const;
    void removeComment(const H5std_string &name) const;

    // Creates a reference to a named object or to a dataset region
    // in this object.
    void reference(void *ref, const char *name, H5R_type_t ref_type = H5R_OBJECT) const;
    void reference(void *ref, const H5std_string &name, H5R_type_t ref_type = H5R_OBJECT) const;
    void reference(void *ref, const char *name, const DataSpace &dataspace,
                   H5R_type_t ref_type = H5R_DATASET_REGION) const;
    void reference(void *ref, const H5std_string &name, const DataSpace &dataspace,
                   H5R_type_t ref_type = H5R_DATASET_REGION) const;

    // Open a referenced object whose location is specified by either
    // a file, an HDF5 object, or an attribute.
    void dereference(const H5Location &loc, const void *ref, H5R_type_t ref_type = H5R_OBJECT,
                     const PropList &plist = PropList::DEFAULT);
    // Removed in 1.10.1, because H5Location is baseclass
    // void dereference(const Attribute& attr, const void* ref, H5R_type_t ref_type = H5R_OBJECT, const
    // PropList& plist = PropList::DEFAULT);

    // Retrieves a dataspace with the region pointed to selected.
    DataSpace getRegion(void *ref, H5R_type_t ref_type = H5R_DATASET_REGION) const;

    // Create a new group with using link create property list.
    Group createGroup(const char *name, const LinkCreatPropList &lcpl) const;
    Group createGroup(const H5std_string &name, const LinkCreatPropList &lcpl) const;

    // From CommonFG
    // Creates a new group at this location which can be a file
    // or another group.
    Group createGroup(const char *name, size_t size_hint = 0) const;
    Group createGroup(const H5std_string &name, size_t size_hint = 0) const;

    // Opens an existing group in a location which can be a file
    // or another group.
    Group openGroup(const char *name) const;
    Group openGroup(const H5std_string &name) const;

    // Creates a new dataset in this location.
    DataSet createDataSet(const char *name, const DataType &data_type, const DataSpace &data_space,
                          const DSetCreatPropList &create_plist = DSetCreatPropList::DEFAULT,
                          const DSetAccPropList   &dapl         = DSetAccPropList::DEFAULT,
                          const LinkCreatPropList &lcpl         = LinkCreatPropList::DEFAULT) const;
    DataSet createDataSet(const H5std_string &name, const DataType &data_type, const DataSpace &data_space,
                          const DSetCreatPropList &create_plist = DSetCreatPropList::DEFAULT,
                          const DSetAccPropList   &dapl         = DSetAccPropList::DEFAULT,
                          const LinkCreatPropList &lcpl         = LinkCreatPropList::DEFAULT) const;

    // Deprecated to add LinkCreatPropList and DSetAccPropList - 1.10.3
    // DataSet createDataSet(const char* name, const DataType& data_type, const DataSpace& data_space, const
    // DSetCreatPropList& create_plist = DSetCreatPropList::DEFAULT) const; DataSet createDataSet(const
    // H5std_string& name, const DataType& data_type, const DataSpace& data_space, const DSetCreatPropList&
    // create_plist = DSetCreatPropList::DEFAULT) const;

    // Opens an existing dataset at this location.
    // DSetAccPropList is added - 1.10.3
    DataSet openDataSet(const char *name, const DSetAccPropList &dapl = DSetAccPropList::DEFAULT) const;
    DataSet openDataSet(const H5std_string    &name,
                        const DSetAccPropList &dapl = DSetAccPropList::DEFAULT) const;

    H5L_info2_t getLinkInfo(const char            *link_name,
                            const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    H5L_info2_t getLinkInfo(const H5std_string    &link_name,
                            const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

    // Returns the value of a symbolic link.
    H5std_string getLinkval(const char *link_name, size_t size = 0) const;
    H5std_string getLinkval(const H5std_string &link_name, size_t size = 0) const;

    // Returns the number of objects in this group.
    // Deprecated - moved to H5::Group in 1.10.2.
    hsize_t getNumObjs() const;

    // Retrieves the name of an object in this group, given the
    // object's index.
    H5std_string getObjnameByIdx(hsize_t idx) const;
    ssize_t      getObjnameByIdx(hsize_t idx, char *name, size_t size) const;
    ssize_t      getObjnameByIdx(hsize_t idx, H5std_string &name, size_t size) const;

    // Retrieves the type of an object in this file or group, given the
    // object's name
    H5O_type_t childObjType(const H5std_string &objname) const;
    H5O_type_t childObjType(const char *objname) const;
    H5O_type_t childObjType(hsize_t index, H5_index_t index_type = H5_INDEX_NAME,
                            H5_iter_order_t order = H5_ITER_INC, const char *objname = ".") const;

    // Returns the object header version of an object in this file or group,
    // given the object's name.
    unsigned childObjVersion(const char *objname) const;
    unsigned childObjVersion(const H5std_string &objname) const;

    // Retrieves information about an HDF5 object.
    void getObjinfo(H5O_info2_t &objinfo, unsigned fields = H5O_INFO_BASIC) const;

    // Retrieves information about an HDF5 object, given its name.
    void getObjinfo(const char *name, H5O_info2_t &objinfo, unsigned fields = H5O_INFO_BASIC,
                    const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    void getObjinfo(const H5std_string &name, H5O_info2_t &objinfo, unsigned fields = H5O_INFO_BASIC,
                    const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

    // Retrieves information about an HDF5 object, given its index.
    void getObjinfo(const char *grp_name, H5_index_t idx_type, H5_iter_order_t order, hsize_t idx,
                    H5O_info2_t &objinfo, unsigned fields = H5O_INFO_BASIC,
                    const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    void getObjinfo(const H5std_string &grp_name, H5_index_t idx_type, H5_iter_order_t order, hsize_t idx,
                    H5O_info2_t &objinfo, unsigned fields = H5O_INFO_BASIC,
                    const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

    // Retrieves native native information about an HDF5 object.
    void getNativeObjinfo(H5O_native_info_t &objinfo, unsigned fields = H5O_NATIVE_INFO_HDR) const;

    // Retrieves native information about an HDF5 object, given its name.
    void getNativeObjinfo(const char *name, H5O_native_info_t &objinfo, unsigned fields = H5O_NATIVE_INFO_HDR,
                          const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    void getNativeObjinfo(const H5std_string &name, H5O_native_info_t &objinfo,
                          unsigned               fields = H5O_NATIVE_INFO_HDR,
                          const LinkAccPropList &lapl   = LinkAccPropList::DEFAULT) const;

    // Retrieves native information about an HDF5 object, given its index.
    void getNativeObjinfo(const char *grp_name, H5_index_t idx_type, H5_iter_order_t order, hsize_t idx,
                          H5O_native_info_t &objinfo, unsigned fields = H5O_NATIVE_INFO_HDR,
                          const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    void getNativeObjinfo(const H5std_string &grp_name, H5_index_t idx_type, H5_iter_order_t order,
                          hsize_t idx, H5O_native_info_t &objinfo, unsigned fields = H5O_NATIVE_INFO_HDR,
                          const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

#ifndef H5_NO_DEPRECATED_SYMBOLS
    // Returns the type of an object in this group, given the
    // object's index.
    H5G_obj_t getObjTypeByIdx(hsize_t idx) const;
    H5G_obj_t getObjTypeByIdx(hsize_t idx, char *type_name) const;
    H5G_obj_t getObjTypeByIdx(hsize_t idx, H5std_string &type_name) const;

    // Returns information about an HDF5 object, given by its name,
    // at this location. - Deprecated
    void getObjinfo(const char *name, hbool_t follow_link, H5G_stat_t &statbuf) const;
    void getObjinfo(const H5std_string &name, hbool_t follow_link, H5G_stat_t &statbuf) const;
    void getObjinfo(const char *name, H5G_stat_t &statbuf) const;
    void getObjinfo(const H5std_string &name, H5G_stat_t &statbuf) const;

    // Iterates over the elements of this group - not implemented in
    // C++ style yet.
    int iterateElems(const char *name, int *idx, H5G_iterate_t op, void *op_data);
    int iterateElems(const H5std_string &name, int *idx, H5G_iterate_t op, void *op_data);
#endif /* H5_NO_DEPRECATED_SYMBOLS */

    // Creates a soft link from link_name to target_name.
    void link(const char *target_name, const char *link_name,
              const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
              const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void link(const H5std_string &target_name, const H5std_string &link_name,
              const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
              const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Creates a hard link from new_name to curr_name.
    void link(const char *curr_name, const Group &new_loc, const char *new_name,
              const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
              const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void link(const H5std_string &curr_name, const Group &new_loc, const H5std_string &new_name,
              const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
              const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Creates a hard link from new_name to curr_name in same location.
    void link(const char *curr_name, const hid_t same_loc, const char *new_name,
              const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
              const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void link(const H5std_string &curr_name, const hid_t same_loc, const H5std_string &new_name,
              const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
              const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Creates a link of the specified type from new_name to current_name;
    // both names are interpreted relative to the specified location id.
    // Deprecated due to inadequate functionality.
    void link(H5L_type_t link_type, const char *curr_name, const char *new_name) const;
    void link(H5L_type_t link_type, const H5std_string &curr_name, const H5std_string &new_name) const;

    // Removes the specified link from this location.
    void unlink(const char *link_name, const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;
    void unlink(const H5std_string &link_name, const LinkAccPropList &lapl = LinkAccPropList::DEFAULT) const;

    // Mounts the file 'child' onto this location.
    void mount(const char *name, const H5File &child, const PropList &plist) const;
    void mount(const H5std_string &name, const H5File &child, const PropList &plist) const;

    // Unmounts the file named 'name' from this parent location.
    void unmount(const char *name) const;
    void unmount(const H5std_string &name) const;

    // Copies a link from a group to another.
    void copyLink(const char *src_name, const Group &dst, const char *dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void copyLink(const H5std_string &src_name, const Group &dst, const H5std_string &dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Makes a copy of a link in the same group.
    void copyLink(const char *src_name, const char *dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void copyLink(const H5std_string &src_name, const H5std_string &dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Renames a link in this group and moves to a new location.
    void moveLink(const char *src_name, const Group &dst, const char *dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void moveLink(const H5std_string &src_name, const Group &dst, const H5std_string &dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Renames a link in this group.
    void moveLink(const char *src_name, const char *dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;
    void moveLink(const H5std_string &src_name, const H5std_string &dst_name,
                  const LinkCreatPropList &lcpl = LinkCreatPropList::DEFAULT,
                  const LinkAccPropList   &lapl = LinkAccPropList::DEFAULT) const;

    // Renames an object at this location.
    // Deprecated due to inadequate functionality.
    void move(const char *src, const char *dst) const;
    void move(const H5std_string &src, const H5std_string &dst) const;

    // end From CommonFG

    /// For subclasses, H5File and Group, to throw appropriate exception.
    virtual void throwException(const H5std_string &func_name, const H5std_string &msg) const;

    // Default constructor
    H5Location();

  protected:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    // *** Deprecation warning ***
    // The following two constructors are no longer appropriate after the
    // data member "id" had been moved to the sub-classes.
    // The copy constructor is a noop and is removed in 1.8.15 and the
    // other will be removed from 1.10 release, and then from 1.8 if its
    // removal does not raise any problems in two 1.10 releases.

    // Creates a copy of an existing object giving the location id.
    // H5Location(const hid_t loc_id);

    // Creates a reference to an HDF5 object or a dataset region.
    void p_reference(void *ref, const char *name, hid_t space_id, H5R_type_t ref_type) const;

    // Dereferences a ref into an HDF5 id.
    hid_t p_dereference(hid_t loc_id, const void *ref, H5R_type_t ref_type, const PropList &plist,
                        const char *from_func);

#ifndef H5_NO_DEPRECATED_SYMBOLS
    // Retrieves the type of object that an object reference points to.
    H5G_obj_t p_get_obj_type(void *ref, H5R_type_t ref_type) const;
#endif /* H5_NO_DEPRECATED_SYMBOLS */

    // Retrieves the type of object that an object reference points to.
    H5O_type_t p_get_ref_obj_type(void *ref, H5R_type_t ref_type) const;

    // Sets the identifier of this object to a new value. - this one
    // doesn't increment reference count
    // virtual void p_setId(const hid_t new_id);

#endif // DOXYGEN_SHOULD_SKIP_THIS

    // Noop destructor.
    virtual ~H5Location() override = default;

}; // end of H5Location
} // namespace H5

#endif // H5Location_H
