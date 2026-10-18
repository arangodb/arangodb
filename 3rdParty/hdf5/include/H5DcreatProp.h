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

#ifndef H5DSCreatPropList_H
#define H5DSCreatPropList_H

namespace H5 {

// Class forwarding
class DataType;
class DataSpace;

/*! \class DSetCreatPropList
    \brief Class DSetCreatPropList inherits from ObjCreatPropList and provides
    wrappers for the HDF5 dataset creation property functions.
*/
//  Inheritance: ObjCreatPropList -> PropList -> IdComponent
class H5_DLLCPP DSetCreatPropList : public ObjCreatPropList {
  public:
    ///\brief Default dataset creation property list.
    static const DSetCreatPropList &DEFAULT;

    // Creates a dataset creation property list.
    DSetCreatPropList();

    // Queries whether all the filters set in this property list are
    // available currently.
    bool allFiltersAvail() const;

    // Get space allocation time for this property.
    H5D_alloc_time_t getAllocTime() const;

    // Set space allocation time for dataset during creation.
    void setAllocTime(H5D_alloc_time_t alloc_time) const;

    // Retrieves the size of the chunks used to store a chunked layout dataset.
    int getChunk(int max_ndims, hsize_t *dim) const;

    // Sets the size of the chunks used to store a chunked layout dataset.
    void setChunk(int ndims, const hsize_t *dim) const;

    // Returns information about an external file.
    void getExternal(unsigned idx, size_t name_size, char *name, off_t &offset, hsize_t &size) const;

    // Returns the number of external files for a dataset.
    int getExternalCount() const;

    // Gets fill value writing time.
    H5D_fill_time_t getFillTime() const;

    // Sets fill value writing time for dataset.
    void setFillTime(H5D_fill_time_t fill_time) const;

    // Retrieves a dataset fill value.
    void getFillValue(const DataType &fvalue_type, void *value) const;

    // Sets a dataset fill value.
    void setFillValue(const DataType &fvalue_type, const void *value) const;

    // Returns information about a filter in a pipeline.
    H5Z_filter_t getFilter(int filter_number, unsigned int &flags, size_t &cd_nelmts, unsigned int *cd_values,
                           size_t namelen, char name[], unsigned int &filter_config) const;

    // Returns information about a filter in a pipeline given the filter id.
    void getFilterById(H5Z_filter_t filter_id, unsigned int &flags, size_t &cd_nelmts,
                       unsigned int *cd_values, size_t namelen, char name[],
                       unsigned int &filter_config) const;

    // Gets the layout of the raw data storage of the data that uses this
    // property list.
    H5D_layout_t getLayout() const;

    // Sets the type of storage used to store the raw data for the
    // dataset that uses this property list.
    void setLayout(H5D_layout_t layout) const;

    // Returns the number of filters in the pipeline.
    int getNfilters() const;

    // Checks if fill value has been defined for this property.
    H5D_fill_value_t isFillValueDefined() const;

    // Modifies the specified filter.
    void modifyFilter(H5Z_filter_t filter_id, unsigned int flags, size_t cd_nelmts,
                      const unsigned int cd_values[]) const;

    // Remove one or all filters from the filter pipeline.
    void removeFilter(H5Z_filter_t filter_id) const;

    // Sets compression method and compression level.
    void setDeflate(int level) const;

    // Adds an external file to the list of external files.
    void setExternal(const char *name, off_t offset, hsize_t size) const;

    // Adds a filter to the filter pipeline.
    void setFilter(H5Z_filter_t filter, unsigned int flags = 0, size_t cd_nelmts = 0,
                   const unsigned int cd_values[] = NULL) const;

    // Sets Fletcher32 checksum of EDC for this property list.
    void setFletcher32() const;

    // Sets method of the shuffle filter.
    void setShuffle() const;

    // Sets SZIP compression method.
    void setSzip(unsigned int options_mask, unsigned int pixels_per_block) const;

    // Sets N-bit compression method.
    void setNbit() const;

    // Maps elements of a virtual dataset to elements of the source dataset.
    void setVirtual(const DataSpace &vspace, const char *src_fname, const char *src_dsname,
                    const DataSpace &sspace) const;
    void setVirtual(const DataSpace &vspace, const H5std_string src_fname, const H5std_string src_dsname,
                    const DataSpace &sspace) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("DSetCreatPropList");
    }

    // Copy constructor - same as the original DSetCreatPropList.
    DSetCreatPropList(const DSetCreatPropList &orig);

    // Creates a copy of an existing dataset creation property list
    // using the property list id.
    DSetCreatPropList(const hid_t plist_id);

    // Noop destructor.
    virtual ~DSetCreatPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static DSetCreatPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static DSetCreatPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of DSetCreatPropList
} // namespace H5

#endif // H5DSCreatPropList_H
