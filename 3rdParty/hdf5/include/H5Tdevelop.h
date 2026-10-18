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
 * This file contains public declarations for the H5T (datatype) developer
 *      support routines.
 */

#ifndef H5Tdevelop_H
#define H5Tdevelop_H

/* Include package's public header */
#include "H5Tpublic.h"

/*****************/
/* Public Macros */
/*****************/

/*******************/
/* Public Typedefs */
/*******************/

/**
 * Commands sent to conversion functions
 */
typedef enum H5T_cmd_t {
    H5T_CONV_INIT = 0, /**< query and/or initialize private data	     */
    H5T_CONV_CONV = 1, /**< convert data from source to dest datatype */
    H5T_CONV_FREE = 2  /**< function is being removed from path	     */
} H5T_cmd_t;

/**
 * How is the `bkg' buffer used by the conversion function?
 */
typedef enum H5T_bkg_t {
    H5T_BKG_NO   = 0, /**< background buffer is not needed, send NULL */
    H5T_BKG_TEMP = 1, /**< bkg buffer used as temp storage only       */
    H5T_BKG_YES  = 2  /**< init bkg buf with data before conversion   */
} H5T_bkg_t;

/**
 * Type conversion client data
 */
//! <!-- [H5T_cdata_t_snip] -->
typedef struct H5T_cdata_t {
    H5T_cmd_t command;  /**< what should the conversion function do?    */
    H5T_bkg_t need_bkg; /**< is the background buffer needed?	     */
    hbool_t   recalc;   /**< recalculate private data		     */
    void     *priv;     /**< private data				     */
} H5T_cdata_t;
//! <!-- [H5T_cdata_t_snip] -->

/**
 * Conversion function persistence
 */
typedef enum H5T_pers_t {
    H5T_PERS_DONTCARE = -1, /**< wild card				     */
    H5T_PERS_HARD     = 0,  /**< hard conversion function		     */
    H5T_PERS_SOFT     = 1   /**< soft conversion function		     */
} H5T_pers_t;

/**
 * All datatype conversion functions are...
 */
//! <!-- [H5T_conv_t_snip] -->
typedef herr_t (*H5T_conv_t)(hid_t src_id, hid_t dst_id, H5T_cdata_t *cdata, size_t nelmts, size_t buf_stride,
                             size_t bkg_stride, void *buf, void *bkg, hid_t dset_xfer_plist);
//! <!-- [H5T_conv_t_snip] -->

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \ingroup CONV
 *
 * \brief Registers a datatype conversion function
 *
 * \param[in] pers Conversion function type
 * \param[in] name Name displayed in diagnostic output
 * \type_id{src_id} of source datatype
 * \type_id{dst_id} of destination datatype
 * \param[in] func Function to convert between source and destination datatypes
 *
 * \return \herr_t
 *
 * \details H5Tregister() registers a hard or soft conversion function for a
 *          datatype conversion path. The parameter \p pers indicates whether a
 *          conversion function is hard (#H5T_PERS_HARD) or soft
 *          (#H5T_PERS_SOFT). User-defined functions employing compiler casting
 *          are designated as \Emph{hard}; other user-defined conversion
 *          functions registered with the HDF5 library (with H5Tregister() )
 *          are designated as \Emph{soft}. The HDF5 library also has its own
 *          hard and soft conversion functions.
 *
 *          A conversion path can have only one hard function. When type is
 *          #H5T_PERS_HARD, \p func replaces any previous hard function.
 *
 *          When type is #H5T_PERS_SOFT, H5Tregister() adds the function to the
 *          end of the master soft list and replaces the soft function in all
 *          applicable existing conversion paths. Soft functions are used when
 *          determining which conversion function is appropriate for this path.
 *
 *          The \p name is used only for debugging and should be a short
 *          identifier for the function.
 *
 *          The path is specified by the source and destination datatypes \p
 *          src_id and \p dst_id. For soft conversion functions, only the class
 *          of these types is important.
 *
 *          The type of the conversion function pointer is declared as:
 *          \snippet this H5T_conv_t_snip
 *
 *          The \ref H5T_cdata_t \c struct is declared as:
 *          \snippet this H5T_cdata_t_snip
 *
 * \since 1.6.3 The following change occurred in the \ref H5T_conv_t function:
 *              the \c nelmts parameter type changed to size_t.
 *
 */
H5_DLL herr_t H5Tregister(H5T_pers_t pers, const char *name, hid_t src_id, hid_t dst_id, H5T_conv_t func);
/**
 * \ingroup CONV
 *
 * \brief Removes a conversion function
 *
 * \param[in] pers Conversion function type
 * \param[in] name Name displayed in diagnostic output
 * \type_id{src_id} of source datatype
 * \type_id{dst_id} of destination datatype
 * \param[in] func Function to convert between source and destination datatypes
 *
 * \return \herr_t
 *
 * \details H5Tunregister() removes a conversion function matching criteria
 *          such as soft or hard conversion, source and destination types, and
 *          the conversion function.
 *
 *          If a user is trying to remove a conversion function he registered,
 *          all parameters can be used. If he is trying to remove a library's
 *          default conversion function, there is no guarantee the \p name and
 *          \p func parameters will match the user's chosen values. Passing in
 *          some values may cause this function to fail. A good practice is to
 *          pass in NULL as their values.
 *
 *          All parameters are optional. The missing parameters will be used to
 *          generalize the search criteria.
 *
 *          The conversion function pointer type declaration is described in
 *          H5Tregister().
 *
 * \version 1.6.3 The following change occurred in the \ref H5T_conv_t function:
 *                the \c nelmts parameter type changed to size_t.
 *
 */
H5_DLL herr_t H5Tunregister(H5T_pers_t pers, const char *name, hid_t src_id, hid_t dst_id, H5T_conv_t func);
/**
 * \ingroup CONV
 *
 * \brief Finds a conversion function
 *
 * \type_id{src_id} of source datatype
 * \type_id{dst_id} of destination datatype
 * \param[out] pcdata Pointer to type conversion data
 *
 * \return Returns a pointer to a suitable conversion function if successful.
 *         Otherwise returns NULL.
 *
 * \details H5Tfind() finds a conversion function that can handle a conversion
 *          from type \p src_id to type \p dst_id. The \p pcdata argument is a
 *          pointer to a pointer to type conversion data which was created and
 *          initialized by the soft type conversion function of this path when
 *          the conversion function was installed on the path.
 *
 */
H5_DLL H5T_conv_t H5Tfind(hid_t src_id, hid_t dst_id, H5T_cdata_t **pcdata);
/**
 * \ingroup CONV
 *
 * \brief Check whether the library's default conversion is hard conversion
 *
 * \type_id{src_id} of source datatype
 * \type_id{dst_id} of destination datatype
 *
 * \return \htri_t
 *
 * \details H5Tcompiler_conv() determines whether the library's conversion
 *          function from type \p src_id to type \p dst_id is a compiler (hard)
 *          conversion or not. A compiler conversion uses compiler's casting; a
 *          library (soft) conversion uses the library's own conversion
 *          function.
 *
 * \since 1.8.0
 *
 */
H5_DLL htri_t H5Tcompiler_conv(hid_t src_id, hid_t dst_id);

#ifdef __cplusplus
}
#endif

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#endif /* H5Tdevelop_H */
