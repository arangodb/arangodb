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

/* Packet Table wrapper classes
 *
 * Wraps the H5PT Packet Table C functions in C++ objects
 *
 * Nat Furrer and James Laird
 * February 2004
 */

#ifndef H5PTWRAP_H
#define H5PTWRAP_H

/* Public HDF5 header */
#include "hdf5.h"

#include "H5PTpublic.h"
#include "H5api_adpt.h"

class H5_HLCPPDLL PacketTable {
  public:
    /* Null constructor
     * Sets table_id to H5I_INVALID_HID
     */
    PacketTable();

    /* "Open" Constructor
     * Opens an existing packet table, which can contain either fixed-length or
     * variable-length packets.
     */
    PacketTable(hid_t fileID, const char *name);

    /* "Open" Constructor - will be deprecated because of char* name */
    PacketTable(hid_t fileID, char *name);

    /* Destructor
     * Cleans up the packet table
     */
    virtual ~PacketTable();

    /* IsValid
     * Returns true if this packet table is valid, false otherwise.
     * Use this after the constructor to ensure HDF did not have
     * any trouble making or opening the packet table.
     */
    bool IsValid() const;

    /* IsVariableLength
     * Return 1 if this packet table uses variable-length datatype,
     * return 0 if it is Fixed Length.  Returns -1 if the table is
     * invalid (not open).
     */
    int IsVariableLength() const;

    /* ResetIndex
     * Sets the "current packet" index to point to the first packet in the
     * packet table
     */
    void ResetIndex() const;

    /* SetIndex
     * Sets the current packet to point to the packet specified by index.
     * Returns 0 on success, negative on failure (if index is out of bounds)
     */
    int SetIndex(hsize_t index) const;

    /* GetIndex
     * Returns the position of the current packet.
     * On failure, returns 0 and error is set to negative.
     */
    hsize_t GetIndex(int &error) const;

    /* GetPacketCount
     * Returns the number of packets in the packet table.  Error
     * is set to 0 on success.  On failure, returns 0 and
     * error is set to negative.
     */
    hsize_t GetPacketCount(int &error) const;

    hsize_t
    GetPacketCount()
    {
        int ignoreError;
        return GetPacketCount(ignoreError);
    }

    /* GetTableId
     * Returns the identifier of the packet table.
     */
    hid_t GetTableId() const;

    /* GetDatatype
     * Returns the datatype identifier used by the packet table, on success,
     * or FAIL, on failure.
     * Note: it is best to avoid using this identifier in applications, unless
     * the desired functionality cannot be performed via the packet table ID.
     */
    hid_t GetDatatype() const;

    /* GetDataset
     * Returns the dataset identifier associated with the packet table, on
     * success, or FAIL, on failure.
     * Note: it is best to avoid using this identifier in applications, unless
     * the desired functionality cannot be performed via the packet table ID.
     */
    hid_t GetDataset() const;

    /* FreeBuff
     * Frees the buffers created when variable-length packets are read.
     * Takes the number of hvl_t structs to be freed and a pointer to their
     * location in memory.
     * Returns 0 on success, negative on error.
     */
    int FreeBuff(size_t numStructs, hvl_t *buffer) const;

  protected:
    hid_t table_id;
};

class H5_HLCPPDLL FL_PacketTable : virtual public PacketTable {
  public:
    /* Constructor
     * Creates a packet table to store either fixed- or variable-length packets.
     * Takes the ID of the file the packet table will be created in, the ID of
     * the property list to specify compression, the name of the packet table,
     * the ID of the datatype, and the size of a memory chunk used in chunking.
     */
    FL_PacketTable(hid_t fileID, const char *name, hid_t dtypeID, hsize_t chunkSize = 0,
                   hid_t plistID = H5P_DEFAULT);

    /* Constructors - deprecated
     * Creates a packet table in which to store fixed length packets.
     * Takes the ID of the file the packet table will be created in, the name of
     * the packet table, the ID of the datatype of the set, the size
     * of a memory chunk used in chunking, and the desired compression level
     * (0-9, or -1 for no compression).
     * Note: these overloaded constructors will be deprecated in favor of the
     * constructor above.
     */
    FL_PacketTable(hid_t fileID, hid_t plist_id, const char *name, hid_t dtypeID, hsize_t chunkSize);
    FL_PacketTable(hid_t fileID, char *name, hid_t dtypeID, hsize_t chunkSize, int compression = 0);

    /* "Open" Constructor
     * Opens an existing fixed-length packet table.
     * Fails if the packet table specified is variable-length.
     */
    FL_PacketTable(hid_t fileID, const char *name);

    /* "Open" Constructor - will be deprecated because of char* name */
    FL_PacketTable(hid_t fileID, char *name);

    /* Destructor
     * Cleans up the packet table
     */
    ~FL_PacketTable() override
    {
    }

    /* AppendPacket
     * Adds a single packet to the packet table.  Takes a pointer
     * to the location of the data in memory.
     * Returns 0 on success, negative on failure
     */
    int AppendPacket(void *data);

    /* AppendPackets (multiple packets)
     * Adds multiple packets to the packet table.  Takes the number of packets
     * to be added and a pointer to their location in memory.
     * Returns 0 on success, -1 on failure.
     */
    int AppendPackets(size_t numPackets, void *data);

    /* GetPacket (indexed)
     * Gets a single packet from the packet table.  Takes the index
     * of the packet (with 0 being the first packet) and a pointer
     * to memory where the data should be stored.
     * Returns 0 on success, negative on failure
     */
    int GetPacket(hsize_t index, void *data);

    /* GetPackets (multiple packets)
     * Gets multiple packets at once, all packets between
     * startIndex and endIndex inclusive.  Also takes a pointer to
     * the memory where these packets should be stored.
     * Returns 0 on success, negative on failure.
     */
    int GetPackets(hsize_t startIndex, hsize_t endIndex, void *data);

    /* GetNextPacket (single packet)
     * Gets the next packet in the packet table.  Takes a pointer to
     * memory where the packet should be stored.
     * Returns 0 on success, negative on failure.  Index
     * is not advanced to the next packet on failure.
     */
    int GetNextPacket(void *data);

    /* GetNextPackets (multiple packets)
     * Gets the next numPackets packets in the packet table.  Takes a
     * pointer to memory where these packets should be stored.
     * Returns 0 on success, negative on failure.  Index
     * is not advanced on failure.
     */
    int GetNextPackets(size_t numPackets, void *data);
};

/* Removed "#ifdef VLPT_REMOVED" block.  03/08/2016, -BMR */

#endif /* H5PTWRAP_H */
