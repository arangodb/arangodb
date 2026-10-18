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

#ifndef H5DOpublic_H
#define H5DOpublic_H

#ifdef __cplusplus
extern "C" {
#endif

/** \page H5DO_UG HDF5 High Level Optimizations
 * Since version 1.10.3 these functions are deprecated in favor of #H5Dwrite_chunk and #H5Dread_chunk.
 *
 * \section sec_hldo_direct_chunk Direct Chunk Write Function
 * When a user application has a chunked dataset and is trying to write a single chunk of data with
 * #H5Dwrite, the data goes through several steps inside the HDF5 library. The library first examines the
 * hyperslab selection. Then it converts the data from the datatype in memory to the datatype in the file if
 * they are different. Finally, the library processes the data in the filter pipeline. Starting with
 * the 1.8.11 release, a new high-level C function called #H5DOwrite_chunk becomes available. It writes a data
 * chunk directly to the file bypassing the library’s hyperslab selection, data conversion, and filter
 * pipeline processes. In other words, if an application can pre-process the data, then the application can
 * use #H5DOwrite_chunk to write the data much faster.
 *
 * #H5DOwrite_chunk was developed in response to a client request. The client builds X-ray pixel
 * detectors for use at synchrotron light sources. These detectors can produce data at the rate of tens of
 * gigabytes per second. Before transferring the data over their network, the detectors compress the data
 * by a factor of 10 or more. The modular architecture of the detectors can scale up its data stream in
 * parallel and maps well to current parallel computing and storage systems.
 * See the \ref_rfc20121114 for the original proposal.
 *
 * \subsection subsec_hldo_direct_chunk_using Using the Direct Chunk Write Function
 * Basically, the #H5DOwrite_chunk function takes a pre-processed data chunk (buf) and its size
 * (data_size) and writes to the chunk location (offset) in the dataset ( dset_id).
 *
 * The function prototype is shown below:
 * \code
 * herr_t H5DOwrite_chunk(
 *                        hid_t       dset_id,     // the dataset
 *                        hid_t       dxpl_id,     // data transfer property list
 *                        uint32_t    filter_mask, // indicates which filters are used
 *                        hsize_t*    offset,      // position of the chunk
 *                        size_t      data_size,   // size of the actual data
 *                        const void* buf          // buffer with data to be written
 *                        )
 * \endcode
 *
 * Below is a simple example showing how to use the function:
 * <em>Example 1. Using H5DOwrite_chunk</em>
 * \code
 *     hsize_t offset[2] = {4, 4};
 *     uint32_t filter_mask = 0;
 *     size_t nbytes = 40;
 *     if(H5DOwrite_chunk(dset_id, dxpl, filter_mask, offset, nbytes, data_buf) < 0)
 *         goto error;
 * \endcode
 *
 * In the example above, the dataset is 8x8 elements of int. Each chunk is 4x4. The offset of the first
 * element of the chunk to be written is 4 and 4. In the diagram below, the shaded chunk is the data to be
 * written. The function is writing a pre-compressed data chunk of 40 bytes (assumed) to the dataset. The
 * zero value of the filter mask means that all filters have been applied to the pre-processed data.
 *
 * <table>
 * <tr>
 * <td>
 * \image html DOChunks_fig1.png "Figure 1. Illustration of the chunk to be written"
 * </td>
 * </tr>
 * </table>
 *
 * The complete code example at the end of this topic shows how to set the value of the filter mask to
 * indicate a filter being skipped. The corresponding bit in the filter mask is turned on when a filter is
 * skipped. For example, if the second filter is skipped, the second bit of the filter mask should be turned
 * on. For more information, see the #H5DOwrite_chunk entry in the \ref RM.
 *
 * \subsection subsec_hldo_direct_chunk_design The Design
 * The following diagram shows how the function #H5DOwrite_chunk bypasses hyperslab selection, data
 * conversion, and filter pipeline inside the HDF5 library.
 *
 * <table>
 * <tr>
 * <td>
 * \image html DOChunks_fig2.png "Figure 2. Diagram for H5DOwrite_chunk"
 * </td>
 * </tr>
 * </table>
 *
 * \subsection subsec_hldo_direct_chunk_perf Performance
 * The table below describes the results of performance benchmark tests run by HDF developers. It shows
 * that using the new function #H5DOwrite_chunk to write pre-compressed data is much faster than using
 * the #H5Dwrite function to compress and write the same data with the filter pipeline. Measurements
 * involving #H5Dwrite include compression time in the filter pipeline. Since the data is already
 * compressed before #H5DOwrite_chunk is called, use of #H5DOwrite_chunk to write compressed data
 * avoids the performance bottleneck in the HDF5 filter pipeline.
 *
 * The test was run on a Linux 2.6.18 / 64-bit Intel x86_64 machine. The dataset contained 100 chunks.
 * Only one chunk was written to the file per write call. The number of writes was 100. The time
 * measurement was for the entire dataset with the Unix system function gettimeofday. Writing the
 * entire dataset with one write call took almost the same amount of time as writing chunk by chunk. In
 * order to force the system to flush the data to the file, the O_SYNC flag was used to open the file.
 *
 * <em>Table 1. Performance result for H5DOwrite_chunk in the high-level library</em>
 * <table>
 * <tr>
 * <td>Dataset size (MB)</td><td span='2'>95.37</td><td span='2'>762.94</td><td span='2'>2288.82</td>
 * </tr>
 * <tr>
 * <td>Size after compression (MB)</td><td span='2'>64.14</td><td span='2'>512.94</td><td
 * span='2'>1538.81</td>
 * </tr>
 * <tr>
 * <td>Dataset dimensionality</td><td span='2'>100x1000x250</td><td span='2'>100x2000x1000</td><td
 * span='2'>100x2000x3000</td>
 * </tr>
 * <tr>
 * <td>Chunk dimensionality</td><td span='2'>1000x250</td><td span='2'>2000x1000</td><td
 * span='2'>2000x3000</td>
 * </tr>
 * <tr>
 * <td>Datatype</td><td span='2'>4-byte integer</td><td span='2'>4-byte integer</td><td span='2'>4-byte
 * integer</td>
 * </tr>
 * <tr>
 * <th>IO speed is in MB/s and Time is in second
 * (s).</th><th>speed1</th><th>time2</th><th>speed</th><th>time</th><th>speed</th><th>time</th>
 * </tr>
 * <tr>
 * <td>H5Dwrite writes without compression
 * filter</td><td>77.27</td><td>1.23</td><td>97.02</td><td>7.86</td><td>91.77</td><td>24.94</td>
 * </tr>
 * <tr>
 * <td>H5DOwrite_chunk writes uncompressed
 * data</td><td>79</td><td>1.21</td><td>95.71</td><td>7.97</td><td>89.17</td><td>25.67</td>
 * </tr>
 * <tr>
 * <td>H5Dwrite writes with compression
 * filter</td><td>2.68</td><td>35.59</td><td>2.67</td><td>285.75</td><td>2.67</td><td>857.24</td>
 * </tr>
 * <tr>
 * <td>H5DOwrite_chunk writes compressed
 * data</td><td>77.19</td><td>0.83</td><td>78.56</td><td>6.53</td><td>96.28</td><td>15.98</td>
 * </tr>
 * <tr>
 * <td>Unix writes compressed data to Unix
 * file</td><td>76.49</td><td>0.84</td><td>95</td><td>5.4</td><td>98.59</td><td>15.61</td>
 * </tr>
 * </table>
 *
 * \subsection subsec_hldo_direct_chunk_caution A Word of Caution
 * Since #H5DOwrite_chunk writes data chunks directly in a file, developers must be careful when using it.
 * The function bypasses hyperslab selection, the conversion of data from one datatype to another, and
 * the filter pipeline to write the chunk. Developers should have experience with these processes before
 * they use this function.
 *
 * \subsection subsec_hldo_direct_chunk_example A Complete Code Example
 * The following is an example of using #H5DOwrite_chunk to write an entire dataset by chunk.
 * \code
 *     #include <zlib.h>
 *     #include <math.h>
 *     #define DEFLATE_SIZE_ADJUST(s) (ceil(((double)(s))*1.001)+12)
 *
 *     size_t       buf_size     = CHUNK_NX*CHUNK_NY*sizeof(int);
 *     const Bytef *z_src        = (const Bytef*)(direct_buf);
 *     Bytef       *z_dst;         // destination buffer
 *     uLongf       z_dst_nbytes = (uLongf)DEFLATE_SIZE_ADJUST(buf_size);
 *     uLong        z_src_nbytes = (uLong)buf_size;
 *     int          aggression   = 9; // Compression aggression setting
 *     uint32_t     filter_mask  = 0;
 *     size_t       buf_size     = CHUNK_NX*CHUNK_NY*sizeof(int);
 *
 *     // Create the data space
 *     if((dataspace = H5Screate_simple(RANK, dims, maxdims)) < 0)
 *         goto error;
 *     // Create a new file
 *     if((file = H5Fcreate(FILE_NAME5, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
 *         goto error;
 *     // Modify dataset creation properties, i.e. enable chunking and compression
 *     if((cparms = H5Pcreate(H5P_DATASET_CREATE)) < 0)
 *         goto error;
 *     if((status = H5Pset_chunk( cparms, RANK, chunk_dims)) < 0)
 *         goto error;
 *     if((status = H5Pset_deflate( cparms, aggression)) < 0)
 *         goto error;
 *     // Create a new dataset within the file using cparms creation properties
 *     if((dset_id = H5Dcreate2(file, DATASETNAME, H5T_NATIVE_INT, dataspace, H5P_DEFAULT,cparms,
 * H5P_DEFAULT)) < 0) goto error;
 *     // Initialize data for one chunk
 *     for(i = n = 0; i < CHUNK_NX; i++)
 *         for(j = 0; j < CHUNK_NY; j++)
 *             direct_buf[i][j] = n++;
 *     // Allocate output (compressed) buffer
 *     outbuf = malloc(z_dst_nbytes);
 *     z_dst = (Bytef *)outbuf;
 *     // Perform compression from the source to the destination buffer
 *     ret = compress2(z_dst, &z_dst_nbytes, z_src, z_src_nbytes, aggression);
 *     // Check for various zlib errors
 *     if(Z_BUF_ERROR == ret) {
 *         fprintf(stderr, "overflow");
 *         goto error;
 *     } else if(Z_MEM_ERROR == ret) {
 *         fprintf(stderr, "deflate memory error");
 *         goto error;
 *     } else if(Z_OK != ret) {
 *         fprintf(stderr, "other deflate error");
 *         goto error;
 *     }
 *     // Write the compressed chunk data repeatedly to cover all the chunks in the dataset, using the direct
 * write function. for(i=0; i<NX/CHUNK_NX; i++) { for(j=0; j<NY/CHUNK_NY; j++) { status =
 * H5DOwrite_chunk(dset_id, H5P_DEFAULT, filter_mask, offset, z_dst_nbytes, outbuf); offset[1] += CHUNK_NY;
 *         }
 *         offset[0] += CHUNK_NX;
 *         offset[1] = 0;
 *     }
 *     // Overwrite the first chunk with uncompressed data. Set the filter mask to indicate the compression
 * filter is skipped filter_mask = 0x00000001; offset[0] = offset[1] = 0; if(H5DOwrite_chunk(dset_id,
 * H5P_DEFAULT, filter_mask, offset, buf_size, direct_buf) < 0) goto error;
 *     // Read the entire dataset back for data verification converting ints to longs
 *     if(H5Dread(dataset, H5T_NATIVE_LONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, outbuf_long) < 0)
 *     goto error;
 *     // Data verification here
 *     ...
 * \endcode
 */

/**\defgroup H5DO HDF5 Optimizations APIs (H5DO)
 *
 * <em>Bypassing default HDF5 behavior in order to optimize for specific
 * use cases (H5DO)</em>
 *
 * HDF5 functions described is this section are implemented in the HDF5 High-level
 * library as optimized functions. These functions generally require careful setup
 * and testing as they enable an application to bypass portions of the HDF5
 * library's I/O pipeline for performance purposes.
 *
 * These functions are distributed in the standard HDF5 distribution and are
 * available any time the HDF5 High-level library is available.
 *
 * - \ref H5DOappend
 *    \n Appends data to a dataset along a specified dimension.
 * - \ref H5DOread_chunk
 *   \n Reads a raw data chunk directly from a dataset in a file into a buffer (DEPRECATED)
 * - \ref H5DOwrite_chunk
 *   \n  Writes a raw data chunk from a buffer directly to a dataset in a file (DEPRECATED)
 *
 */

/*-------------------------------------------------------------------------
 *
 * "Optimized dataset" routines.
 *
 *-------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DO
 *
 * \brief Appends data to a dataset along a specified dimension.
 *
 * \param[in] dset_id   Dataset identifier
 * \param[in] dxpl_id   Dataset transfer property list identifier
 * \param[in] axis      Dataset Dimension (0-based) for the append
 * \param[in] extension Number of elements to append for the
 *                      axis-th dimension
 * \param[in] memtype   The memory datatype identifier
 * \param[in] buf       Buffer with data for the append
 *
 * \return \herr_t
 *
 * \details The H5DOappend() routine extends a dataset by \p extension
 *          number of elements along a dimension specified by a
 *          dimension \p axis and writes \p buf of elements to the
 *          dataset. Dimension \p axis is 0-based. Elements’ type
 *          is described by \p memtype.
 *
 *          This routine combines calling H5Dset_extent(),
 *          H5Sselect_hyperslab(), and H5Dwrite() into a single routine
 *          that simplifies application development for the common case
 *          of appending elements to an existing dataset.
 *
 *          For a multi-dimensional dataset, appending to one dimension
 *          will write a contiguous hyperslab over the other dimensions.
 *          For example, if a 3-D dataset has dimension sizes (3, 5, 8),
 *          extending the 0th dimension (currently of size 3) by 3 will
 *          append 3*5*8 = 120 elements (which must be pointed to by the
 *          \p buffer parameter) to the dataset, making its final
 *          dimension sizes (6, 5, 8).
 *
 *          If a dataset has more than one unlimited dimension, any of
 *          those dimensions may be appended to, although only along
 *          one dimension per call to H5DOappend().
 *
 * \since   1.10.0
 *
 */
H5_HLDLL herr_t H5DOappend(hid_t dset_id, hid_t dxpl_id, unsigned axis, size_t extension, hid_t memtype,
                           const void *buf);

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/* Compatibility wrappers for functionality moved to H5D */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DO
 *
 * \brief Writes a raw data chunk from a buffer directly to a dataset in a file.
 *
 * \param[in] dset_id       Identifier for the dataset to write to
 * \param[in] dxpl_id       Transfer property list identifier for
 *                          this I/O operation
 * \param[in] filters       Mask for identifying the filters in use
 * \param[in] offset        Logical position of the chunk's first element
 *                          in the dataspace
 * \param[in] data_size     Size of the actual data to be written in bytes
 * \param[in] buf           Buffer containing data to be written to the chunk
 *
 * \return \herr_t
 *
 * \deprecated This function was deprecated in favor of the function
 *             H5Dwrite_chunk() of HDF5-1.10.3.
 *             The functionality of H5DOwrite_chunk() was moved
 *             to H5Dwrite_chunk().
 * \deprecated For compatibility, this API call has been left as a stub which
 *             simply calls H5Dwrite_chunk(). New code should use H5Dwrite_chunk().
 *
 * \details The H5DOwrite_chunk() writes a raw data chunk as specified by its
 *          logical \p offset in a chunked dataset \p dset_id from the application
 *          memory buffer \p buf to the dataset in the file. Typically, the data
 *          in \p buf is preprocessed in memory by a custom transformation, such as
 *          compression. The chunk will bypass the library's internal data
 *          transfer pipeline, including filters, and will be written directly to the file.
 *
 *          \p dxpl_id is a data transfer property list identifier.
 *
 *          \p filters is a mask providing a record of which filters are used
 *          with the chunk. The default value of the mask is zero (\c 0),
 *          indicating that all enabled filters are applied. A filter is skipped
 *          if the bit corresponding to the filter's position in the pipeline
 *          (<tt>0 ≤ position < 32</tt>) is turned on. This mask is saved
 *          with the chunk in the file.
 *
 *          \p offset is an array specifying the logical position of the first
 *          element of the chunk in the dataset's dataspace. The length of the
 *          offset array must equal the number of dimensions, or rank, of the
 *          dataspace. The values in \p offset must not exceed the dimension limits
 *          and must specify a point that falls on a dataset chunk boundary.
 *
 *          \p data_size is the size in bytes of the chunk, representing the number of
 *          bytes to be read from the buffer \p buf. If the data chunk has been
 *          precompressed, \p data_size should be the size of the compressed data.
 *
 *          \p buf is the memory buffer containing data to be written to the chunk in the file.
 *
 * \attention   Exercise caution when using H5DOread_chunk() and H5DOwrite_chunk(),
 *              as they read and write data chunks directly in a file.
 *              H5DOwrite_chunk() bypasses hyperslab selection, the conversion of data
 *              from one datatype to another, and the filter pipeline to write the chunk.
 *              Developers should have experience with these processes before
 *              using this function. Please see
 *              <a href="https://\DOCURL/advanced_topics/UsingDirectChunkWrite.pdf">
 *              Using the Direct Chunk Write Function</a>
 *              for more information.
 *
 * \note    H5DOread_chunk() and H5DOwrite_chunk() are not
 *          supported under parallel and do not support variable length types.
 *
 * \par Example
 * The following code illustrates the use of H5DOwrite_chunk to write
 * an entire dataset, chunk by chunk:
 * \snippet H5DO_examples.c H5DOwrite
 *
 * \version 1.10.3  Function deprecated in favor of H5Dwrite_chunk.
 *
 * \since   1.8.11
 */
H5_HLDLL herr_t H5DOwrite_chunk(hid_t dset_id, hid_t dxpl_id, uint32_t filters, const hsize_t *offset,
                                size_t data_size, const void *buf);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DO
 *
 * \brief Reads a raw data chunk directly from a dataset in a file into a buffer.
 *
 * \param[in] dset_id           Identifier for the dataset to be read
 * \param[in] dxpl_id           Transfer property list identifier for
 *                              this I/O operation
 * \param[in] offset            Logical position of the chunk's first
                                element in the dataspace
 * \param[in,out] filters       Mask for identifying the filters used
 *                              with the chunk
 * \param[in] buf               Buffer containing the chunk read from
 *                              the dataset
 *
 * \return \herr_t
 *
 * \deprecated This function was deprecated in favor of the function
 *             H5Dread_chunk() as of HDF5-1.10.3.
 *             In HDF5 1.10.3, the functionality of H5DOread_chunk()
 *             was moved to H5Dread_chunk().
 * \deprecated For compatibility, this API call has been left as a stub which
 *             simply calls H5Dread_chunk().  New code should use H5Dread_chunk().
 *
 * \details The H5DOread_chunk() reads a raw data chunk as specified
 *          by its logical \p offset in a chunked dataset \p dset_id
 *          from the dataset in the file into the application memory
 *          buffer \p buf. The data in \p buf is read directly from the file
 *          bypassing the library's internal data transfer pipeline,
 *          including filters.
 *
 *          \p dxpl_id is a data transfer property list identifier.
 *
 *          The mask \p filters indicates which filters are used with the
 *          chunk when written. A zero value indicates that all enabled filters
 *          are applied on the chunk. A filter is skipped if the bit corresponding
 *          to the filter's position in the pipeline
 *          (<tt>0 ≤ position < 32</tt>) is turned on.
 *
 *          \p offset is an array specifying the logical position of the first
 *          element of the chunk in the dataset's dataspace. The length of the
 *          offset array must equal the number of dimensions, or rank, of the
 *          dataspace. The values in \p offset must not exceed the dimension
 *          limits and must specify a point that falls on a dataset chunk boundary.
 *
 *          \p buf is the memory buffer containing the chunk read from the dataset
 *          in the file.
 *
 * \par Example
 * The following code illustrates the use of H5DOread_chunk()
 * to read a chunk from a dataset:
 * \snippet H5DO_examples.c H5DOread
 *
 * \version 1.10.3  Function deprecated in favor of H5Dread_chunk.
 *
 * \since   1.10.2, 1.8.19
 */
H5_HLDLL herr_t H5DOread_chunk(hid_t dset_id, hid_t dxpl_id, const hsize_t *offset, uint32_t *filters /*out*/,
                               void *buf /*out*/);

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#ifdef __cplusplus
}
#endif

#endif
