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

#ifndef H5Zpublic_H
#define H5Zpublic_H

#include "H5public.h" /* Generic Functions                        */
/**
 * \brief Filter identifiers
 *
//! [FiltersIdTable]
<table>
<tr>
<th>Values for #H5Z_filter_t</th><th>Description</th>
</tr>
<tr>
<td><code>0-255</code></td>
<td>These values are reserved for filters predefined and
       registered by the HDF5 library and of use to the general public.</td>
</tr>
<tr>
<td><code>256-511</code></td>
<td>Filter values in this range are intended for testing only and can be
    temporarily used by any organization. No attempts are made to resolve
    numbering conflicts, as all definitions are temporary.</td>
</tr>
<tr>
<td><code>512-32,767</code></td>
<td>Filter values within this range are designated for filters managed by
    The HDF Group, but they are nominally requested, developed, and supported
    by third parties. Please contact the
    <a href="mailto:help@hdfgroup.org">HDF5 development team</a>
    to reserve a value or range of values for use by your filters.</td>
</tr>
<tr>
<td><code>32,768-65,535</code></td>
<td>Filter values in this range are designated for internal company use or
    application testing when assessing a feature. The HDF Group does not
    track or document the use of filters within this range.
</td>
</tr>
</table>
//! [FiltersIdTable]
 */

typedef int H5Z_filter_t;

/* Filter IDs */
/**
 * no filter
 */
#define H5Z_FILTER_ERROR (-1)
/**
 * reserved indefinitely
 */
#define H5Z_FILTER_NONE 0
/**
 * deflation like gzip
 */
#define H5Z_FILTER_DEFLATE 1
/**
 * shuffle the data
 */
#define H5Z_FILTER_SHUFFLE 2
/**
 * fletcher32 checksum of EDC
 */
#define H5Z_FILTER_FLETCHER32 3
/**
 * szip compression
 */
#define H5Z_FILTER_SZIP 4
/**
 * nbit compression
 */
#define H5Z_FILTER_NBIT 5
/**
 * scale+offset compression
 */
#define H5Z_FILTER_SCALEOFFSET 6
/**
 * filter ids below this value are reserved for library use
 */
#define H5Z_FILTER_RESERVED 256
/**
 * maximum filter id
 */
#define H5Z_FILTER_MAX 65535

/* General macros */
/**
 * Symbol to remove all filters in H5Premove_filter()
 */
#define H5Z_FILTER_ALL 0
/**
 * Maximum number of filters allowed in a pipeline
 *
 * \internal (should probably be allowed to be an unlimited amount, but
 *            currently each filter uses a bit in a 32-bit field, so the format
 *            would have to be changed to accommodate that)
 */
#define H5Z_MAX_NFILTERS 32

/* Flags for filter definition (stored) */
/**
 * definition flag mask
 */
#define H5Z_FLAG_DEFMASK 0x00ff
/**
 * filter is mandatory
 */
#define H5Z_FLAG_MANDATORY 0x0000
/**
 * filter is optional
 */
#define H5Z_FLAG_OPTIONAL 0x0001

/* Additional flags for filter invocation (not stored) */
/**
 * invocation flag mask
 */
#define H5Z_FLAG_INVMASK 0xff00
/**
 * reverse direction; read
 */
#define H5Z_FLAG_REVERSE 0x0100
/**
 * skip EDC filters for read
 */
#define H5Z_FLAG_SKIP_EDC 0x0200

/* Special parameters for szip compression
 *
 * These are aliases for similarly-named definitions in szlib.h, which we
 * can't include directly due to the duplication of various symbols with the
 * zlib.h header file.
 *
 * The flag values are set to the same values as in szlib.h. The following
 * symbols are internal and defined in H5Zprivate.h:
 *
 * - H5_SZIP_LSB_OPTION_MASK
 * - H5_SZIP_MSB_OPTION_MASK
 * - H5_SZIP_RAW_OPTION_MASK
 *
 * TODO: These symbols should probably be deprecated and moved to H5Zprivate.h
 *       in the next major release of the library since they are only used
 *       internally:
 *
 * - H5_SZIP_ALLOW_K13_OPTION_MASK
 * - H5_SZIP_CHIP_OPTION_MASK
 */
/**
 * \ingroup SZIP
 *
 * Used internally. Always added to the \p options_mask parameter of H5Pset_szip().
 */
#define H5_SZIP_ALLOW_K13_OPTION_MASK 1
/**
 * \ingroup SZIP
 *
 * Used internally. Always removed from the \p options_mask parameter of H5Pset_szip().
 */
#define H5_SZIP_CHIP_OPTION_MASK 2
/**
 * \ingroup SZIP
 *
 * Use the entropy coding method
 */
#define H5_SZIP_EC_OPTION_MASK 4
/**
 * \ingroup SZIP
 *
 * Use nearest neighbor preprocessing and then the entropy coding method
 */
#define H5_SZIP_NN_OPTION_MASK 32
/**
 * \ingroup SZIP
 *
 * The maximum number of pixels per block (see H5Pset_szip())
 */
#define H5_SZIP_MAX_PIXELS_PER_BLOCK 32

/* Macros for the shuffle filter */
/**
 * \ingroup SHUFFLE
 * Number of parameters that users can set for the shuffle filter
 */
#define H5Z_SHUFFLE_USER_NPARMS 0
/**
 * \ingroup SHUFFLE
 * Total number of parameters for the shuffle filter
 */
#define H5Z_SHUFFLE_TOTAL_NPARMS 1

/* Macros for the szip filter */
/**
 * \ingroup SZIP
 * Number of parameters that users can set for SZIP
 */
#define H5Z_SZIP_USER_NPARMS 2
/**
 * \ingroup SZIP
 * Total number of parameters for SZIP filter
 */
#define H5Z_SZIP_TOTAL_NPARMS 4
/**
 * \ingroup SZIP
 * "User" parameter for option mask
 */
#define H5Z_SZIP_PARM_MASK 0
/**
 * \ingroup SZIP
 * "User" parameter for pixels-per-block
 */
#define H5Z_SZIP_PARM_PPB 1
/**
 * \ingroup SZIP
 * "Local" parameter for bits-per-pixel
 */
#define H5Z_SZIP_PARM_BPP 2
/**
 * \ingroup SZIP
 * "Local" parameter for pixels-per-scanline
 */
#define H5Z_SZIP_PARM_PPS 3

/* Macros for the nbit filter */
/**
 * \ingroup NBIT
 * Number of parameters that users can set for the N-bit filter
 */
#define H5Z_NBIT_USER_NPARMS 0 /* Number of parameters that users can set */

/* Macros for the scale offset filter */
/**
 * \ingroup SCALEOFFSET
 * Number of parameters that users can set for the scale-offset filter
 */
#define H5Z_SCALEOFFSET_USER_NPARMS 2

/* Special parameters for ScaleOffset filter*/
/**
 * \ingroup SCALEOFFSET */
#define H5Z_SO_INT_MINBITS_DEFAULT 0
/**
 * \ingroup SCALEOFFSET */
typedef enum H5Z_SO_scale_type_t {
    H5Z_SO_FLOAT_DSCALE = 0,
    H5Z_SO_FLOAT_ESCALE = 1,
    H5Z_SO_INT          = 2
} H5Z_SO_scale_type_t;

/**
 * \ingroup FLETCHER32
 * Values to decide if EDC is enabled for reading data
 */
typedef enum H5Z_EDC_t {
    H5Z_ERROR_EDC   = -1, /**< error value */
    H5Z_DISABLE_EDC = 0,
    H5Z_ENABLE_EDC  = 1,
    H5Z_NO_EDC      = 2 /**< sentinel */
} H5Z_EDC_t;

/* Bit flags for H5Zget_filter_info */
#define H5Z_FILTER_CONFIG_ENCODE_ENABLED (0x0001)
#define H5Z_FILTER_CONFIG_DECODE_ENABLED (0x0002)

/**
 * Return values for filter callback function
 */
typedef enum H5Z_cb_return_t {
    H5Z_CB_ERROR = -1, /**< error value */
    H5Z_CB_FAIL  = 0,  /**< I/O should fail if filter fails. */
    H5Z_CB_CONT  = 1,  /**< I/O continues if filter fails.   */
    H5Z_CB_NO    = 2   /**< sentinel */
} H5Z_cb_return_t;

//! <!-- [H5Z_filter_func_t_snip] -->
/**
 *  Filter callback function definition
 */
typedef H5Z_cb_return_t (*H5Z_filter_func_t)(H5Z_filter_t filter, void *buf, size_t buf_size, void *op_data);
//! <!-- [H5Z_filter_func_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \ingroup H5Z
 *
 * \brief Determines whether a filter is available
 *
 * \param[in] id Filter identifier
 * \return \htri_t
 *
 * \details H5Zfilter_avail() determines whether the filter specified in \p id
 *          is available to the application.
 *
 * \since 1.6.0
 */
H5_DLL htri_t H5Zfilter_avail(H5Z_filter_t id);
/**
 * \ingroup H5Z
 *
 * \brief Retrieves information about a filter
 *
 * \param[in] filter Filter identifier
 * \param[out] filter_config_flags A bit field encoding the returned filter
 *                                 information
 * \return \herr_t
 *
 * \details H5Zget_filter_info() retrieves information about a filter. At
 *          present, this means that the function retrieves a filter's
 *          configuration flags, indicating whether the filter is configured to
 *          decode data, encode data, neither, or both.
 *
 *          If \p filter_config_flags is not set to NULL prior to the function
 *          call, the returned parameter contains a bit field specifying the
 *          available filter configuration. The configuration flag values can
 *          then be determined through a series of bitwise AND operations, as
 *          described below.
 *
 *          Valid filter configuration flags include the following:
 *          <table>
 *            <tr><td>#H5Z_FILTER_CONFIG_ENCODE_ENABLED</td>
 *                <td>Encoding is enabled for this filter</td></tr>
 *            <tr><td>#H5Z_FILTER_CONFIG_DECODE_ENABLED</td>
 *                <td>Decoding is enabled for this filter</td></tr>
 *          </table>
 *
 *          A bitwise AND of the returned \p filter_config_flags and a valid
 *          filter configuration flag will reveal whether the related
 *          configuration option is available. For example, if the value of
 *          \code
 *          H5Z_FILTER_CONFIG_ENCODE_ENABLED & filter_config_flags
 *          \endcode
 *          is true, i.e., greater than 0 (zero), the queried filter
 *          is configured to encode data; if the value is \c false, i.e., equal to
 *          0 (zero), the filter is not so configured.
 *
 *          If a filter is not encode-enabled, the corresponding \c H5Pset_*
 *          function will return an error if the filter is added to a dataset
 *          creation property list (which is required if the filter is to be
 *          used to encode that dataset). For example, if the
 *          #H5Z_FILTER_CONFIG_ENCODE_ENABLED flag is not returned for the SZIP
 *          filter, #H5Z_FILTER_SZIP, a call to H5Pset_szip() will fail.
 *
 *          If a filter is not decode-enabled, the application will not be able
 *          to read an existing file encoded with that filter.
 *
 *          This function should be called, and the returned \p
 *          filter_config_flags should be analyzed, before calling any other function,
 *          such as H5Pset_szip(), that might require a particular filter
 *          configuration.
 *
 * \since 1.6.0
 */
H5_DLL herr_t H5Zget_filter_info(H5Z_filter_t filter, unsigned int *filter_config_flags);

#ifdef __cplusplus
}
#endif

#endif /* _H5Zpublic_H */
