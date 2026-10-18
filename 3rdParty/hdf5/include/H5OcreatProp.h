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

#ifndef H5ObjCreatPropList_H
#define H5ObjCreatPropList_H

namespace H5 {

/*! \class ObjCreatPropList
    \brief Class ObjCreatPropList inherits from PropList and provides
    wrappers for the HDF5 object create property list.
*/
//  Inheritance: PropList -> IdComponent
class H5_DLLCPP ObjCreatPropList : public PropList {
  public:
    ///\brief Default object creation property list.
    static const ObjCreatPropList &DEFAULT;

    // Creates a object creation property list.
    ObjCreatPropList();

    // Sets attribute storage phase change thresholds.
    void setAttrPhaseChange(unsigned max_compact = 8, unsigned min_dense = 6) const;

    // Gets attribute storage phase change thresholds.
    void getAttrPhaseChange(unsigned &max_compact, unsigned &min_dense) const;

    // Sets tracking and indexing of attribute creation order.
    void setAttrCrtOrder(unsigned crt_order_flags) const;

    // Gets tracking and indexing settings for attribute creation order.
    unsigned getAttrCrtOrder() const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("ObjCreatPropList");
    }

    // Copy constructor: same as the original ObjCreatPropList.
    ObjCreatPropList(const ObjCreatPropList &original);

    // Creates a copy of an existing object creation property list
    // using the property list id.
    ObjCreatPropList(const hid_t plist_id);

    // Noop destructor
    virtual ~ObjCreatPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static ObjCreatPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static ObjCreatPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of ObjCreatPropList
} // namespace H5

#endif // H5ObjCreatPropList_H
