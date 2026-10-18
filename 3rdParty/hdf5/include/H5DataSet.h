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

#ifndef H5DataSet_H
#define H5DataSet_H

namespace H5 {

/*! \class DataSet
    \brief Class DataSet operates on HDF5 datasets.

    An datasets has many characteristics similar to an attribute, thus both
    Attribute and DataSet are derivatives of AbstractDs.  DataSet also
    inherits from H5Object because a dataset is an HDF5 object.
*/
//  Inheritance: multiple H5Object/AbstractDs -> H5Location -> IdComponent
class H5_DLLCPP DataSet : public H5Object, public AbstractDs {
  public:
    // Close this dataset.
    virtual void close() override;

    // Extends the dataset with unlimited dimension.
    void extend(const hsize_t *size) const;

    // Fills a selection in memory with a value
    void fillMemBuf(const void *fill, const DataType &fill_type, void *buf, const DataType &buf_type,
                    const DataSpace &space) const;
    // void fillMemBuf(const void *fill, DataType& fill_type, void *buf, DataType& buf_type, DataSpace&
    // space); // removed from 1.8.18 and 1.10.1

    // Fills a selection in memory with zero
    void fillMemBuf(void *buf, const DataType &buf_type, const DataSpace &space) const;
    // void fillMemBuf(void *buf, DataType& buf_type, DataSpace& space); // removed from 1.8.18 and 1.10.1

    // Gets the creation property list of this dataset.
    DSetCreatPropList getCreatePlist() const;

    // Gets the access property list of this dataset.
    DSetAccPropList getAccessPlist() const;

    // Returns the address of this dataset in the file.
    haddr_t getOffset() const;

    // Gets the dataspace of this dataset.
    virtual DataSpace getSpace() const override;

    // Determines whether space has been allocated for a dataset.
    void getSpaceStatus(H5D_space_status_t &status) const;

    // Returns the amount of storage size required for this dataset.
    virtual hsize_t getStorageSize() const override;

    // Returns the in memory size of this attribute's data.
    virtual size_t getInMemDataSize() const override;

    // Returns the number of bytes required to store VL data.
    hsize_t getVlenBufSize(const DataType &type, const DataSpace &space) const;
    // hsize_t getVlenBufSize(DataType& type, DataSpace& space) const; // removed from 1.8.18 and 1.10.1

    // Reclaims VL datatype memory buffers.
    static void vlenReclaim(const DataType &type, const DataSpace &space,
                            const DSetMemXferPropList &xfer_plist, void *buf);
    static void vlenReclaim(void *buf, const DataType &type, const DataSpace &space = DataSpace::ALL,
                            const DSetMemXferPropList &xfer_plist = DSetMemXferPropList::DEFAULT);

    // Reads the data of this dataset and stores it in the provided buffer.
    // The memory and file dataspaces and the transferring property list
    // can be defaults.
    void read(void *buf, const DataType &mem_type, const DataSpace &mem_space = DataSpace::ALL,
              const DataSpace           &file_space = DataSpace::ALL,
              const DSetMemXferPropList &xfer_plist = DSetMemXferPropList::DEFAULT) const;
    void read(H5std_string &buf, const DataType &mem_type, const DataSpace &mem_space = DataSpace::ALL,
              const DataSpace           &file_space = DataSpace::ALL,
              const DSetMemXferPropList &xfer_plist = DSetMemXferPropList::DEFAULT) const;

    // Writes the buffered data to this dataset.
    // The memory and file dataspaces and the transferring property list
    // can be defaults.
    void write(const void *buf, const DataType &mem_type, const DataSpace &mem_space = DataSpace::ALL,
               const DataSpace           &file_space = DataSpace::ALL,
               const DSetMemXferPropList &xfer_plist = DSetMemXferPropList::DEFAULT) const;
    void write(const H5std_string &buf, const DataType &mem_type, const DataSpace &mem_space = DataSpace::ALL,
               const DataSpace           &file_space = DataSpace::ALL,
               const DSetMemXferPropList &xfer_plist = DSetMemXferPropList::DEFAULT) const;

    // Iterates the selected elements in the specified dataspace - not implemented in C++ style yet
    int iterateElems(void *buf, const DataType &type, const DataSpace &space, H5D_operator_t op,
                     void *op_data = NULL);

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("DataSet");
    }

    // Creates a dataset by way of dereference.
    DataSet(const H5Location &loc, const void *ref, H5R_type_t ref_type = H5R_OBJECT,
            const PropList &plist = PropList::DEFAULT);
    DataSet(const Attribute &attr, const void *ref, H5R_type_t ref_type = H5R_OBJECT,
            const PropList &plist = PropList::DEFAULT);

    // Default constructor.
    DataSet();

    // Copy constructor - same as the original DataSet.
    DataSet(const DataSet &original);

    // Assignment operator
    DataSet &operator=(const DataSet &original);

    // Creates a copy of an existing DataSet using its id.
    DataSet(const hid_t existing_id);

    // Gets the dataset id.
    virtual hid_t getId() const override;

    // Destructor: properly terminates access to this dataset.
    virtual ~DataSet() override;

  protected:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    // Sets the dataset id.
    virtual void p_setId(const hid_t new_id) override;
#endif // DOXYGEN_SHOULD_SKIP_THIS

  private:
    hid_t id; // HDF5 dataset id

    // This function contains the common code that is used by
    // getTypeClass and various API functions getXxxType
    // defined in AbstractDs for generic datatype and specific
    // sub-types
    virtual hid_t p_get_type() const override;

    // Reads variable or fixed len strings from this dataset.
    void p_read_fixed_len(const hid_t mem_type_id, const hid_t mem_space_id, const hid_t file_space_id,
                          const hid_t xfer_plist_id, H5std_string &strg) const;
    void p_read_variable_len(const hid_t mem_type_id, const hid_t mem_space_id, const hid_t file_space_id,
                             const hid_t xfer_plist_id, H5std_string &strg) const;

    // Friend function to set DataSet id.  For library use only.
    friend void f_DataSet_setId(DataSet *dset, hid_t new_id);

}; // end of DataSet
} // namespace H5

#endif // H5DataSet_H
