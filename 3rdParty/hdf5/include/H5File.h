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

#ifndef H5File_H
#define H5File_H

namespace H5 {

/*! \class H5File
    \brief Class H5File represents an HDF5 file and inherits from class Group
    as file is a root group.
*/
//  Inheritance: Group -> CommonFG/H5Object -> H5Location -> IdComponent
class H5_DLLCPP H5File : public Group {
  public:
    // Creates or opens an HDF5 file.
    H5File(const char *name, unsigned int flags,
           const FileCreatPropList &create_plist = FileCreatPropList::DEFAULT,
           const FileAccPropList   &access_plist = FileAccPropList::DEFAULT);
    H5File(const H5std_string &name, unsigned int flags,
           const FileCreatPropList &create_plist = FileCreatPropList::DEFAULT,
           const FileAccPropList   &access_plist = FileAccPropList::DEFAULT);
    H5File(const char *name, unsigned int flags, const FileAccPropList &access_plist);
    H5File(const H5std_string &name, unsigned int flags, const FileAccPropList &access_plist);

    // Open the file
    void openFile(const H5std_string &name, unsigned int flags,
                  const FileAccPropList &access_plist = FileAccPropList::DEFAULT);
    void openFile(const char *name, unsigned int flags,
                  const FileAccPropList &access_plist = FileAccPropList::DEFAULT);

    // Close this file.
    virtual void close() override;

    // Gets a copy of the access property list of this file.
    FileAccPropList getAccessPlist() const;

    // Gets a copy of the creation property list of this file.
    FileCreatPropList getCreatePlist() const;

    // Gets general information about this file.
    void getFileInfo(H5F_info2_t &file_info) const;

    // Returns the amount of free space in the file.
    hssize_t getFreeSpace() const;

    // Returns the number of opened object IDs (files, datasets, groups
    // and datatypes) in the same file.
    ssize_t getObjCount(unsigned types = H5F_OBJ_ALL) const;

    // Retrieves a list of opened object IDs (files, datasets, groups
    // and datatypes) in the same file.
    void getObjIDs(unsigned types, size_t max_objs, hid_t *oid_list) const;

    // Returns the pointer to the file handle of the low-level file driver.
    void getVFDHandle(void **file_handle) const;
    void getVFDHandle(const FileAccPropList &fapl, void **file_handle) const;
    // void getVFDHandle(FileAccPropList& fapl, void **file_handle) const; // removed from 1.8.18 and 1.10.1

    // Returns the file size of the HDF5 file.
    hsize_t getFileSize() const;

    // Returns the 'file number' of the HDF5 file.
    unsigned long getFileNum() const;

    // Determines if a file, specified by its name, is in HDF5 format
    static bool isHdf5(const char *name);
    static bool isHdf5(const H5std_string &name);

    // Determines if a file, specified by its name, can be accessed as HDF5
    static bool isAccessible(const char            *name,
                             const FileAccPropList &access_plist = FileAccPropList::DEFAULT);
    static bool isAccessible(const H5std_string    &name,
                             const FileAccPropList &access_plist = FileAccPropList::DEFAULT);

    // Reopens this file.
    void reOpen(); // added for better name

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    void reopen(); // obsolete in favor of reOpen()

    // Creates an H5File using an existing file id.  Not recommended
    // in applications.
    H5File(hid_t existing_id);

#endif // DOXYGEN_SHOULD_SKIP_THIS

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("H5File");
    }

    // Throw file exception.
    virtual void throwException(const H5std_string &func_name, const H5std_string &msg) const override;

    // For CommonFG to get the file id.
    virtual hid_t getLocId() const override;

    // Default constructor
    H5File();

    // Copy constructor: same as the original H5File.
    H5File(const H5File &original);

    // Gets the HDF5 file id.
    virtual hid_t getId() const override;

    // H5File destructor.
    virtual ~H5File() override;

  protected:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    // Sets the HDF5 file id.
    virtual void p_setId(const hid_t new_id) override;
#endif // DOXYGEN_SHOULD_SKIP_THIS

  private:
    hid_t id; // HDF5 file id

    // This function is private and contains common code between the
    // constructors taking a string or a char*
    void p_get_file(const char *name, unsigned int flags, const FileCreatPropList &create_plist,
                    const FileAccPropList &access_plist);

}; // end of H5File
} // namespace H5

#endif // H5File_H
