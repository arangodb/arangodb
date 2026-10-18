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

#ifndef H5LinkAccPropList_H
#define H5LinkAccPropList_H

namespace H5 {

/*! \class LinkAccPropList
    \brief Class LinkAccPropList inherits from PropList and provides
    wrappers for the HDF5 link access property list.
*/
// Inheritance: PropList -> IdComponent
class H5_DLLCPP LinkAccPropList : public PropList {
  public:
    ///\brief Default link access property list.
    static const LinkAccPropList &DEFAULT;

    // Creates a link access property list.
    LinkAccPropList();

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("LinkAccPropList");
    }

    // Copy constructor: same as the original LinkAccPropList.
    LinkAccPropList(const LinkAccPropList &original);

    // Creates a copy of an existing link access property list
    // using the property list id.
    LinkAccPropList(const hid_t plist_id);

    // Sets the number of soft or user-defined links that can be
    // traversed before a failure occurs.
    void setNumLinks(size_t nlinks) const;

    // Gets the number of soft or user-defined link traversals allowed
    size_t getNumLinks() const;

    // Noop destructor
    virtual ~LinkAccPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static LinkAccPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static LinkAccPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of LinkAccPropList
} // namespace H5

#endif // H5LinkAccPropList_H
