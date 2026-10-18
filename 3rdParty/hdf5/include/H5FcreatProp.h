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

#ifndef H5FileCreatPropList_H
#define H5FileCreatPropList_H

namespace H5 {

/*! \class FileCreatPropList
    \brief Class FileCreatPropList inherits from PropList and provides
    wrappers for the HDF5 file create property list.
*/
//  Inheritance: PropList -> IdComponent
class H5_DLLCPP FileCreatPropList : public PropList {
  public:
    ///\brief Default file creation property list.
    static const FileCreatPropList &DEFAULT;

    // Creates a file create property list.
    FileCreatPropList();

#ifndef H5_NO_DEPRECATED_SYMBOLS
    // Retrieves version information for various parts of a file.
    void getVersion(unsigned &super, unsigned &freelist, unsigned &stab, unsigned &shhdr) const;
#endif /* H5_NO_DEPRECATED_SYMBOLS */

    // Sets the userblock size field of a file creation property list.
    void setUserblock(hsize_t size) const;

    // Gets the size of a user block in this file creation property list.
    hsize_t getUserblock() const;

    // Retrieves the size-of address and size quantities stored in a
    // file according to this file creation property list.
    void getSizes(size_t &sizeof_addr, size_t &sizeof_size) const;

    // Sets file size-of addresses and sizes.
    void setSizes(size_t sizeof_addr = 4, size_t sizeof_size = 4) const;

    // Retrieves the size of the symbol table B-tree 1/2 rank and the
    // symbol table leaf node 1/2 size.
    void getSymk(unsigned &int_nodes_k, unsigned &leaf_nodes_k) const;

    // Sets the size of parameters used to control the symbol table nodes.
    void setSymk(unsigned int_nodes_k, unsigned leaf_nodes_k) const;

    // Returns the 1/2 rank of an indexed storage B-tree.
    unsigned getIstorek() const;

    // Sets the size of parameter used to control the B-trees for
    // indexing chunked datasets.
    void setIstorek(unsigned ik) const;

    // Sets the strategy and the threshold value that the library will
    // will employ in managing file space.
    void setFileSpaceStrategy(H5F_fspace_strategy_t strategy, hbool_t persist, hsize_t threshold) const;

    // Returns the strategy that the library uses in managing file space.
    void getFileSpaceStrategy(H5F_fspace_strategy_t &strategy, hbool_t &persist, hsize_t &threshold) const;

    // Sets the file space page size for paged aggregation.
    void setFileSpacePagesize(hsize_t fsp_psize) const;

    // Returns the threshold value that the library uses in tracking free
    // space sections.
    hsize_t getFileSpacePagesize() const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("FileCreatPropList");
    }

    // Copy constructor: same as the original FileCreatPropList.
    FileCreatPropList(const FileCreatPropList &orig);

    // Creates a copy of an existing file create property list
    // using the property list id.
    FileCreatPropList(const hid_t plist_id);

    // Noop destructor
    virtual ~FileCreatPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static FileCreatPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static FileCreatPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of FileCreatPropList
} // namespace H5

#endif // H5FileCreatPropList_H
