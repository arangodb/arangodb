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

#ifndef H5LinkCreatPropList_H
#define H5LinkCreatPropList_H

namespace H5 {

/*! \class LinkCreatPropList
    \brief Class LinkCreatPropList inherits from PropList and provides
    wrappers for the HDF5 link creation property list.
*/
// Inheritance: PropList -> IdComponent
class H5_DLLCPP LinkCreatPropList : public PropList {
  public:
    ///\brief Default link creation property list.
    static const LinkCreatPropList &DEFAULT;

    // Creates a link creation property list.
    LinkCreatPropList();

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("LinkCreatPropList");
    }

    // Copy constructor: same as the original LinkCreatPropList.
    LinkCreatPropList(const LinkCreatPropList &original);

    // Creates a copy of an existing link creation property list
    // using the property list id.
    LinkCreatPropList(const hid_t plist_id);

    // Specifies in property list whether to create missing
    // intermediate groups
    void setCreateIntermediateGroup(bool crt_intmd_group) const;

    // Determines whether property is set to enable creating missing
    // intermediate groups
    bool getCreateIntermediateGroup() const;

    // Sets the character encoding of the string.
    void setCharEncoding(H5T_cset_t encoding) const;

    // Gets the character encoding of the string.
    H5T_cset_t getCharEncoding() const;

    // Noop destructor
    virtual ~LinkCreatPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static LinkCreatPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static LinkCreatPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of LinkCreatPropList
} // namespace H5

#endif // H5LinkCreatPropList_H
