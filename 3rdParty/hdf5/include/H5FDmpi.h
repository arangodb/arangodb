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

/*
 * Purpose:	The public header file for common items for all MPI VFL drivers
 */
#ifndef H5FDmpi_H
#define H5FDmpi_H

/***** Macros for One linked collective IO case. *****/
/* The default value to do one linked collective IO for all chunks.
   If the average number of chunks per process is greater than this value,
      the library will create an MPI derived datatype to link all chunks to do collective IO.
      The user can set this value through an API. */

#define H5D_ONE_LINK_CHUNK_IO_THRESHOLD 0
/***** Macros for multi-chunk collective IO case. *****/
/* The default value of the threshold to do collective IO for this chunk.
   If the average percentage of processes per chunk is greater than the default value,
   collective IO is done for this chunk.
*/

#define H5D_MULTI_CHUNK_IO_COL_THRESHOLD 60
/**
 * Type of I/O for data transfer properties
 */
typedef enum H5FD_mpio_xfer_t {
    H5FD_MPIO_INDEPENDENT = 0, /**< Use independent I/O access */
    H5FD_MPIO_COLLECTIVE       /**< Use collective I/O access */
} H5FD_mpio_xfer_t;

/* Type of chunked dataset I/O */
typedef enum H5FD_mpio_chunk_opt_t {
    H5FD_MPIO_CHUNK_DEFAULT = 0,
    H5FD_MPIO_CHUNK_ONE_IO, /*zero is the default*/
    H5FD_MPIO_CHUNK_MULTI_IO
} H5FD_mpio_chunk_opt_t;

/* Type of collective I/O */
typedef enum H5FD_mpio_collective_opt_t {
    H5FD_MPIO_COLLECTIVE_IO = 0,
    H5FD_MPIO_INDIVIDUAL_IO /*zero is the default*/
} H5FD_mpio_collective_opt_t;

/* Include all the MPI VFL headers */
#include "H5FDmpio.h" /* MPI I/O file driver			*/

#endif /* H5FDmpi_H */
