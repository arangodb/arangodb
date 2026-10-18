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

#ifndef H5FileAccPropList_H
#define H5FileAccPropList_H

namespace H5 {

/*! \class FileAccPropList
    \brief Class FileAccPropList inherits from PropList and provides
    wrappers for the HDF5 file access property list.
*/
//  Inheritance: PropList -> IdComponent
class H5_DLLCPP FileAccPropList : public PropList {
  public:
    ///\brief Default file access property list.
    static const FileAccPropList &DEFAULT;

    // Creates a file access property list.
    FileAccPropList();

    // Modifies this property list to use the H5FD_STDIO driver
    void setStdio() const;

    // Set file driver for this property list
    void setDriver(hid_t new_driver_id, const void *new_driver_info) const;

    // Returns a low-level file driver identifier.
    hid_t getDriver() const;

    // Sets offset for family driver.
    void setFamilyOffset(hsize_t offset) const;

    // Gets offset for family driver.
    hsize_t getFamilyOffset() const;

    // Modifies this file access property list to use the sec2 driver.
    void setSec2() const;

    // Modifies this file access property list to use the H5FD_CORE
    // driver.
    void setCore(size_t increment, hbool_t backing_store) const;

    // Queries H5FD_CORE driver properties.
    void getCore(size_t &increment, hbool_t &backing_store) const;

    // Sets this file access properties list to the family driver.
    void setFamily(hsize_t memb_size, const FileAccPropList &memb_plist) const;

    // Returns information about the family file access property list.
    void            getFamily(hsize_t &memb_size, FileAccPropList &memb_plist) const;
    FileAccPropList getFamily(hsize_t &memb_size) const;

    // Emulates the old split file driver,
    void setSplit(const FileAccPropList &meta_plist, const FileAccPropList &raw_plist,
                  const char *meta_ext = ".meta", const char *raw_ext = ".raw") const;
    void setSplit(const FileAccPropList &meta_plist, const FileAccPropList &raw_plist,
                  const H5std_string &meta_ext = ".meta", const H5std_string &raw_ext = ".raw") const;

    // Sets the maximum size of the data sieve buffer.
    void setSieveBufSize(size_t bufsize) const;

    // Returns the current settings for the data sieve buffer size
    // property
    size_t getSieveBufSize() const;

    // Sets the minimum size of metadata block allocations.
    void setMetaBlockSize(hsize_t &block_size) const;

    // Returns the current metadata block size setting.
    hsize_t getMetaBlockSize() const;

    // Modifies this file access property list to use the logging driver.
    void setLog(const char *logfile, unsigned flags, size_t buf_size) const;
    void setLog(const H5std_string &logfile, unsigned flags, size_t buf_size) const;

    // Sets alignment properties of this file access property list
    void setAlignment(hsize_t threshold = 1, hsize_t alignment = 1) const;

    // Retrieves the current settings for alignment properties from
    // this property list.
    void getAlignment(hsize_t &threshold, hsize_t &alignment) const;

    // Sets data type for multi driver.
    void setMultiType(H5FD_mem_t dtype) const;

    // Returns the data type property for MULTI driver.
    H5FD_mem_t getMultiType() const;

    // Sets the meta data cache and raw data chunk cache parameters.
    void setCache(int mdc_nelmts, size_t rdcc_nelmts, size_t rdcc_nbytes, double rdcc_w0) const;

    // Queries the meta data cache and raw data chunk cache parameters.
    void getCache(int &mdc_nelmts, size_t &rdcc_nelmts, size_t &rdcc_nbytes, double &rdcc_w0) const;

    // Sets the degree for the file close behavior.
    void setFcloseDegree(H5F_close_degree_t degree) const;

    // Returns the degree for the file close behavior.
    H5F_close_degree_t getFcloseDegree() const;

    // Sets file access property list to use the H5FD_DIRECT driver.
    void setFileAccDirect(size_t boundary, size_t block_size, size_t cbuf_size) const;

    // Retrieves information about the direct file access property list.
    void getFileAccDirect(size_t &boundary, size_t &block_size, size_t &cbuf_size) const;

    // Sets garbage collecting references flag.
    void setGcReferences(unsigned gc_ref = 0) const;

    // Returns garbage collecting references setting.
    unsigned getGcReferences() const;

    // Sets file locking parameters.
    void setFileLocking(hbool_t use_file_locking, hbool_t ignore_when_disabled) const;

    // Gets file locking parameters.
    void getFileLocking(hbool_t &use_file_locking, hbool_t &ignore_when_disabled) const;

    // Sets bounds on versions of library format to be used when creating
    // or writing objects.
    void setLibverBounds(H5F_libver_t libver_low, H5F_libver_t libver_high) const;

    // Gets the current settings for the library version format bounds.
    void getLibverBounds(H5F_libver_t &libver_low, H5F_libver_t &libver_high) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("FileAccPropList");
    }

    // Copy constructor: same as the original FileAccPropList.
    FileAccPropList(const FileAccPropList &original);

    // Creates a copy of an existing file access property list
    // using the property list id.
    FileAccPropList(const hid_t plist_id);

    // Noop destructor
    virtual ~FileAccPropList() override = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the global constant, should only be used by the library
    static void deleteConstants();

  private:
    static FileAccPropList *DEFAULT_;

    // Creates the global constant, should only be used by the library
    static FileAccPropList *getConstant();

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of FileAccPropList
} // namespace H5

#endif // H5FileAccPropList_H
