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

#ifndef H5DSetAccPropList_H
#define H5DSetAccPropList_H

namespace H5 {

/*! \class DSetAccPropList
    \brief Class DSetAccPropList inherits from LinkAccPropList and provides
    wrappers for the HDF5 dataset access property functions.
*/
//  Inheritance: LinkAccPropList -> PropList -> IdComponent
class H5_DLLCPP DSetAccPropList : public LinkAccPropList {
  public:
    ///\brief Default dataset creation property list.
    static const DSetAccPropList &DEFAULT;

    // Creates a dataset creation property list.
    DSetAccPropList();

    // Sets the raw data chunk cache parameters.
    void setChunkCache(size_t rdcc_nslots, size_t rdcc_nbytes, double rdcc_w0) const;

    // Retrieves the raw data chunk cache parameters.
    void getChunkCache(size_t &rdcc_nslots, size_t &rdcc_nbytes, double &rdcc_w0) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("DSetAccPropList");
    }

    // Copy constructor - same as the original DSetAccPropList.
    DSetAccPropList(const DSetAccPropList &orig);

    // Creates a copy of an existing dataset creation property list
    // using the property list id.
    DSetAccPropList(const hid_t plist_id);

    // Noop destructor.
    virtual ~DSetAccPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static DSetAccPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static DSetAccPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of DSetAccPropList
} // namespace H5

#endif // H5DSetAccPropList_H
