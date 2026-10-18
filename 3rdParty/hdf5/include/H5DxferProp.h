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

#ifndef H5DSetMemXferPropList_H
#define H5DSetMemXferPropList_H

namespace H5 {

/*! \class DSetMemXferPropList
    \brief Class DSetCreatPropList inherits from PropList and provides
    wrappers for the HDF5 dataset memory and transfer property list.
*/
//  Inheritance: PropList -> IdComponent
class H5_DLLCPP DSetMemXferPropList : public PropList {
  public:
    ///\brief Default dataset memory and transfer property list.
    static const DSetMemXferPropList &DEFAULT;

    // Creates a dataset memory and transfer property list.
    DSetMemXferPropList();

    // Creates a dataset transform property list.
    DSetMemXferPropList(const char *expression);

    // Sets type conversion and background buffers.
    void setBuffer(size_t size, void *tconv, void *bkg) const;

    // Reads buffer settings.
    size_t getBuffer(void **tconv, void **bkg) const;

    // Sets B-tree split ratios for a dataset transfer property list.
    void setBtreeRatios(double left, double middle, double right) const;

    // Gets B-tree split ratios for a dataset transfer property list.
    void getBtreeRatios(double &left, double &middle, double &right) const;

    // Sets data transform expression.
    void setDataTransform(const char *expression) const;
    void setDataTransform(const H5std_string &expression) const;

    // Gets data transform expression.
    ssize_t      getDataTransform(char *exp, size_t buf_size = 0) const;
    H5std_string getDataTransform() const;

    // Sets the dataset transfer property list status to TRUE or FALSE.
    void setPreserve(bool status) const;

    // Checks status of the dataset transfer property list.
    bool getPreserve() const;

    // Sets an exception handling callback for datatype conversion.
    void setTypeConvCB(H5T_conv_except_func_t op, void *user_data) const;

    // Gets the exception handling callback for datatype conversion.
    void getTypeConvCB(H5T_conv_except_func_t *op, void **user_data) const;

    // Sets the memory manager for variable-length datatype
    // allocation in H5Dread and H5Treclaim.
    void setVlenMemManager(H5MM_allocate_t alloc, void *alloc_info, H5MM_free_t free, void *free_info) const;

    // alloc and free are set to NULL, indicating that system
    // malloc and free are to be used.
    void setVlenMemManager() const;

    // Gets the memory manager for variable-length datatype
    // allocation in H5Dread and H5Treclaim.
    void getVlenMemManager(H5MM_allocate_t &alloc, void **alloc_info, H5MM_free_t &free,
                           void **free_info) const;

    // Sets the size of a contiguous block reserved for small data.
    void setSmallDataBlockSize(hsize_t size) const;

    // Returns the current small data block size setting.
    hsize_t getSmallDataBlockSize() const;

    // Sets number of I/O vectors to be read/written in hyperslab I/O.
    void setHyperVectorSize(size_t vector_size) const;

    // Returns the number of I/O vectors to be read/written in
    // hyperslab I/O.
    size_t getHyperVectorSize() const;

    // Enables or disables error-detecting for a dataset reading
    // process.
    void setEDCCheck(H5Z_EDC_t check) const;

    // Determines whether error-detection is enabled for dataset reads.
    H5Z_EDC_t getEDCCheck() const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("DSetMemXferPropList");
    }

    // Copy constructor - same as the original DSetMemXferPropList.
    DSetMemXferPropList(const DSetMemXferPropList &orig);

    // Creates a copy of an existing dataset memory and transfer
    // property list using the property list id.
    DSetMemXferPropList(const hid_t plist_id);

    // Noop destructor
    virtual ~DSetMemXferPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static DSetMemXferPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static DSetMemXferPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of DSetMemXferPropList
} // namespace H5

#endif // H5DSetMemXferPropList_H
