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
 * This file contains public declarations for the H5T module.
 */
#ifndef H5Tpublic_H
#define H5Tpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Ipublic.h" /* Identifiers                              */

#define HOFFSET(S, M) (offsetof(S, M))

/**
 * These are the various classes of datatypes
 * internal If this goes over 16 types (0-15), the file format will need to
 *          change.
 */
//! <!-- [H5T_class_t_snip] -->
typedef enum H5T_class_t {
    H5T_NO_CLASS  = -1, /**< error                                   */
    H5T_INTEGER   = 0,  /**< integer types                           */
    H5T_FLOAT     = 1,  /**< floating-point types                    */
    H5T_TIME      = 2,  /**< date and time types                     */
    H5T_STRING    = 3,  /**< character string types                  */
    H5T_BITFIELD  = 4,  /**< bit field types                         */
    H5T_OPAQUE    = 5,  /**< opaque types                            */
    H5T_COMPOUND  = 6,  /**< compound types                          */
    H5T_REFERENCE = 7,  /**< reference types                         */
    H5T_ENUM      = 8,  /**< enumeration types                       */
    H5T_VLEN      = 9,  /**< variable-Length types                   */
    H5T_ARRAY     = 10, /**< array types                             */

    H5T_NCLASSES /**< sentinel: this must be last             */
} H5T_class_t;
//! <!-- [H5T_class_t_snip] -->

/**
 * Byte orders
 */
//! <!-- [H5T_order_t_snip] -->
typedef enum H5T_order_t {
    H5T_ORDER_ERROR = -1, /**< error                                   */
    H5T_ORDER_LE    = 0,  /**< little endian                           */
    H5T_ORDER_BE    = 1,  /**< big endian                              */
    H5T_ORDER_VAX   = 2,  /**< VAX mixed endian                        */
    H5T_ORDER_MIXED = 3,  /**< Compound type with mixed member orders  */
    H5T_ORDER_NONE  = 4   /**< no particular order (strings, bits,..)  */
    /*H5T_ORDER_NONE must be last */
} H5T_order_t;
//! <!-- [H5T_order_t_snip] -->

/**
 * Types of integer sign schemes
 */
//! <!-- [H5T_sign_t_snip] -->
typedef enum H5T_sign_t {
    H5T_SGN_ERROR = -1, /**< error                                   */
    H5T_SGN_NONE  = 0,  /**< this is an unsigned type                */
    H5T_SGN_2     = 1,  /**< two's complement                        */

    H5T_NSGN = 2 /** sentinel: this must be last!             */
} H5T_sign_t;
//! <!-- [H5T_sign_t_snip] -->

/**
 * Floating-point normalization schemes
 */
//! <!-- [H5T_norm_t_snip] -->
typedef enum H5T_norm_t {
    H5T_NORM_ERROR   = -1, /**< error                                   */
    H5T_NORM_IMPLIED = 0,  /**< msb of mantissa isn't stored, always 1  */
    H5T_NORM_MSBSET  = 1,  /**< msb of mantissa is always 1             */
    H5T_NORM_NONE    = 2   /**< not normalized                          */
    /*H5T_NORM_NONE must be last */
} H5T_norm_t;
//! <!-- [H5T_norm_t_snip] -->

/**
 * Character set to use for text strings.
 * \internal Do not change these values since they appear in HDF5 files!
 */
typedef enum H5T_cset_t {
    H5T_CSET_ERROR       = -1, /**< error                           */
    H5T_CSET_ASCII       = 0,  /**< US ASCII                        */
    H5T_CSET_UTF8        = 1,  /**< UTF-8 Unicode encoding		     */
    H5T_CSET_RESERVED_2  = 2,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_3  = 3,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_4  = 4,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_5  = 5,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_6  = 6,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_7  = 7,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_8  = 8,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_9  = 9,  /**< reserved for later use		     */
    H5T_CSET_RESERVED_10 = 10, /**< reserved for later use		     */
    H5T_CSET_RESERVED_11 = 11, /**< reserved for later use		     */
    H5T_CSET_RESERVED_12 = 12, /**< reserved for later use		     */
    H5T_CSET_RESERVED_13 = 13, /**< reserved for later use		     */
    H5T_CSET_RESERVED_14 = 14, /**< reserved for later use		     */
    H5T_CSET_RESERVED_15 = 15  /**< reserved for later use		     */
} H5T_cset_t;
#define H5T_NCSET H5T_CSET_RESERVED_2 /*Number of character sets actually defined  */

/**
 * Type of padding to use in character strings.
 * \internal  Do not change these values since they appear in HDF5 files!
 */
typedef enum H5T_str_t {
    H5T_STR_ERROR       = -1, /**< error                           */
    H5T_STR_NULLTERM    = 0,  /**< null terminate like in C        */
    H5T_STR_NULLPAD     = 1,  /**< pad with nulls                  */
    H5T_STR_SPACEPAD    = 2,  /**< pad with spaces like in Fortran */
    H5T_STR_RESERVED_3  = 3,  /**< reserved for later use		     */
    H5T_STR_RESERVED_4  = 4,  /**< reserved for later use		     */
    H5T_STR_RESERVED_5  = 5,  /**< reserved for later use		     */
    H5T_STR_RESERVED_6  = 6,  /**< reserved for later use		     */
    H5T_STR_RESERVED_7  = 7,  /**< reserved for later use		     */
    H5T_STR_RESERVED_8  = 8,  /**< reserved for later use		     */
    H5T_STR_RESERVED_9  = 9,  /**< reserved for later use		     */
    H5T_STR_RESERVED_10 = 10, /**< reserved for later use		     */
    H5T_STR_RESERVED_11 = 11, /**< reserved for later use		     */
    H5T_STR_RESERVED_12 = 12, /**< reserved for later use		     */
    H5T_STR_RESERVED_13 = 13, /**< reserved for later use		     */
    H5T_STR_RESERVED_14 = 14, /**< reserved for later use		     */
    H5T_STR_RESERVED_15 = 15  /**< reserved for later use		     */
} H5T_str_t;
#define H5T_NSTR H5T_STR_RESERVED_3 /*num H5T_str_t types actually defined	     */

/**
 * Type of padding to use in other atomic types
 */
//! <!-- [H5T_pad_t_snip] -->
typedef enum H5T_pad_t {
    H5T_PAD_ERROR      = -1, /**< error                           */
    H5T_PAD_ZERO       = 0,  /**< always set to zero              */
    H5T_PAD_ONE        = 1,  /**< always set to one               */
    H5T_PAD_BACKGROUND = 2,  /**< set to background value         */

    H5T_NPAD = 3 /**< sentinel: THIS MUST BE LAST     */
} H5T_pad_t;
//! <!-- [H5T_pad_t_snip] -->

/**
 * The order to retrieve atomic native datatype
 */
//! <!-- [H5T_direction_t_snip] -->
typedef enum H5T_direction_t {
    H5T_DIR_DEFAULT = 0, /**< default direction is ascending         */
    H5T_DIR_ASCEND  = 1, /**< in ascending order                     */
    H5T_DIR_DESCEND = 2  /**< in descending order                    */
} H5T_direction_t;
//! <!-- [H5T_direction_t_snip] -->

/**
 * The exception type passed into the conversion callback function
 */
typedef enum H5T_conv_except_t {
    H5T_CONV_EXCEPT_RANGE_HI = 0,
    /**< Source value is greater than destination's range */
    H5T_CONV_EXCEPT_RANGE_LOW = 1,
    /**< Source value is less than destination's range */
    H5T_CONV_EXCEPT_PRECISION = 2,
    /**< Source value loses precision in destination */
    H5T_CONV_EXCEPT_TRUNCATE = 3,
    /**< Source value is truncated in destination */
    H5T_CONV_EXCEPT_PINF = 4,
    /**< Source value is positive infinity */
    H5T_CONV_EXCEPT_NINF = 5,
    /**< Source value is negative infinity */
    H5T_CONV_EXCEPT_NAN = 6
    /**< Source value is \c NaN (not a number, including \c QNaN and \c SNaN) */
} H5T_conv_except_t;

/**
 * The return value from conversion callback function H5T_conv_except_func_t()
 */
typedef enum H5T_conv_ret_t {
    H5T_CONV_ABORT     = -1, /**< abort conversion                           */
    H5T_CONV_UNHANDLED = 0,  /**< callback function failed to handle the exception      */
    H5T_CONV_HANDLED   = 1   /**< callback function handled the exception successfully  */
} H5T_conv_ret_t;

/**
 * Variable Length Datatype struct in memory (This is only used for VL
 * sequences, not VL strings, which are stored in char *'s)
 */
typedef struct {
    size_t len; /**< Length of VL data (in base type units) */
    void  *p;   /**< Pointer to VL data */
} hvl_t;

/* Variable Length String information */
/**
 * Indicate that a string is variable length (null-terminated in C, instead of
 * fixed length)
 */
#define H5T_VARIABLE SIZE_MAX

/* Opaque information */
/**
 * Maximum length of an opaque tag
 * \internal This could be raised without too much difficulty
 */
#define H5T_OPAQUE_TAG_MAX 256

#ifdef __cplusplus
extern "C" {
#endif

//! <!-- [H5T_conv_except_func_t_snip] -->
/**
 * \brief Exception handler.
 *
 * \param[in] except_type The kind of exception that occurred
 * \param[in] src_id Source datatype identifier
 * \param[in] dst_id Destination datatype identifier
 * \param[in] src_buf Source data buffer
 * \param[in,out] dst_buf Destination data buffer
 * \param[in,out] user_data Callback context
 * \return Valid callback function return values are #H5T_CONV_ABORT,
 *          #H5T_CONV_UNHANDLED and #H5T_CONV_HANDLED.
 *
 * \details If an exception like overflow happens during conversion, this
 *          function is called if it's registered through H5Pset_type_conv_cb().
 *
 * \since 1.8.0
 */
typedef H5T_conv_ret_t (*H5T_conv_except_func_t)(H5T_conv_except_t except_type, hid_t src_id, hid_t dst_id,
                                                 void *src_buf, void *dst_buf, void *user_data);
//! <!-- [H5T_conv_except_func_t_snip] -->

/* When this header is included from a private header, don't make calls to H5open() */
#undef H5OPEN
#ifndef H5private_H
#define H5OPEN H5open(),
#else /* H5private_H */
#define H5OPEN
#endif /* H5private_H */

/*
 * The IEEE floating point types in various byte orders.
 */
/**
 * \ingroup PDTIEEE
 * 16-bit big-endian IEEE floating-point numbers
 */
#define H5T_IEEE_F16BE (H5OPEN H5T_IEEE_F16BE_g)
/**
 * \ingroup PDTIEEE
 * 16-bit little-endian IEEE floating-point numbers
 */
#define H5T_IEEE_F16LE (H5OPEN H5T_IEEE_F16LE_g)
/**
 * \ingroup PDTIEEE
 * 32-bit big-endian IEEE floating-point numbers
 */
#define H5T_IEEE_F32BE (H5OPEN H5T_IEEE_F32BE_g)
/**
 * \ingroup PDTIEEE
 * 32-bit little-endian IEEE floating-point numbers
 */
#define H5T_IEEE_F32LE (H5OPEN H5T_IEEE_F32LE_g)
/**
 * \ingroup PDTIEEE
 * 64-bit big-endian IEEE floating-point numbers
 */
#define H5T_IEEE_F64BE (H5OPEN H5T_IEEE_F64BE_g)
/**
 * \ingroup PDTIEEE
 * 64-bit little-endian IEEE floating-point numbers
 */
#define H5T_IEEE_F64LE (H5OPEN H5T_IEEE_F64LE_g)
H5_DLLVAR hid_t H5T_IEEE_F16BE_g;
H5_DLLVAR hid_t H5T_IEEE_F16LE_g;
H5_DLLVAR hid_t H5T_IEEE_F32BE_g;
H5_DLLVAR hid_t H5T_IEEE_F32LE_g;
H5_DLLVAR hid_t H5T_IEEE_F64BE_g;
H5_DLLVAR hid_t H5T_IEEE_F64LE_g;

/*
 * These are "standard" types.  For instance, signed (2's complement) and
 * unsigned integers of various sizes and byte orders.
 */
/**
 * \ingroup PDTSTD
 * 8-bit big-endian signed integers
 */
#define H5T_STD_I8BE (H5OPEN H5T_STD_I8BE_g)
/**
 * \ingroup PDTSTD
 * 8-bit little-endian signed integers
 */
#define H5T_STD_I8LE (H5OPEN H5T_STD_I8LE_g)
/**
 * \ingroup PDTSTD
 * 16-bit big-endian signed integers
 */
#define H5T_STD_I16BE (H5OPEN H5T_STD_I16BE_g)
/**
 * \ingroup PDTSTD
 * 16-bit little-endian signed integers
 */
#define H5T_STD_I16LE (H5OPEN H5T_STD_I16LE_g)
/**
 * \ingroup PDTSTD
 * 32-bit big-endian signed integers
 */
#define H5T_STD_I32BE (H5OPEN H5T_STD_I32BE_g)
/**
 * \ingroup PDTSTD
 * 32-bit little-endian signed integers
 */
#define H5T_STD_I32LE (H5OPEN H5T_STD_I32LE_g)
/**
 * \ingroup PDTSTD
 * 64-bit big-endian signed integers
 */
#define H5T_STD_I64BE (H5OPEN H5T_STD_I64BE_g)
/**
 * \ingroup PDTSTD
 * 64-bit little-endian signed integers
 */
#define H5T_STD_I64LE (H5OPEN H5T_STD_I64LE_g)
/**
 * \ingroup PDTSTD
 * 8-bit big-endian unsigned integers
 */
#define H5T_STD_U8BE (H5OPEN H5T_STD_U8BE_g)
/**
 * \ingroup PDTSTD
 * 8-bit little-endian unsigned integers
 */
#define H5T_STD_U8LE (H5OPEN H5T_STD_U8LE_g)
/**
 * \ingroup PDTSTD
 * 16-bit big-endian unsigned integers
 */
#define H5T_STD_U16BE (H5OPEN H5T_STD_U16BE_g)
/**
 * \ingroup PDTSTD
 * 16-bit little-endian unsigned integers
 */
#define H5T_STD_U16LE (H5OPEN H5T_STD_U16LE_g)
/**
 * \ingroup PDTSTD
 * 32-bit big-endian unsigned integers
 */
#define H5T_STD_U32BE (H5OPEN H5T_STD_U32BE_g)
/**
 * \ingroup PDTSTD
 * 32-bit little-endian unsigned integers
 */
#define H5T_STD_U32LE (H5OPEN H5T_STD_U32LE_g)
/**
 * \ingroup PDTSTD
 * 64-bit big-endian unsigned integers
 */
#define H5T_STD_U64BE (H5OPEN H5T_STD_U64BE_g)
/**
 * \ingroup PDTSTD
 * 64-bit little-endian unsigned integers
 */
#define H5T_STD_U64LE (H5OPEN H5T_STD_U64LE_g)
/**
 * \ingroup PDTSTD
 * 8-bit big-endian bitfield
 */
#define H5T_STD_B8BE (H5OPEN H5T_STD_B8BE_g)
/**
 * \ingroup PDTSTD
 * 8-bit little-endian bitfield
 */
#define H5T_STD_B8LE (H5OPEN H5T_STD_B8LE_g)
/**
 * \ingroup PDTSTD
 * 16-bit big-endian bitfield
 */
#define H5T_STD_B16BE (H5OPEN H5T_STD_B16BE_g)
/**
 * \ingroup PDTSTD
 * 16-bit little-endian bitfield
 */
#define H5T_STD_B16LE (H5OPEN H5T_STD_B16LE_g)
/**
 * \ingroup PDTSTD
 * 32-bit big-endian bitfield
 */
#define H5T_STD_B32BE (H5OPEN H5T_STD_B32BE_g)
/**
 * \ingroup PDTSTD
 * 32-bit little-endian bitfield
 */
#define H5T_STD_B32LE (H5OPEN H5T_STD_B32LE_g)
/**
 * \ingroup PDTSTD
 * 64-bit big-endian bitfield
 */
#define H5T_STD_B64BE (H5OPEN H5T_STD_B64BE_g)
/**
 * \ingroup PDTSTD
 * 64-bit little-endian bitfield
 */
#define H5T_STD_B64LE (H5OPEN H5T_STD_B64LE_g)
/**
 * \ingroup PDTSTD
 * Object reference
 */
#define H5T_STD_REF_OBJ (H5OPEN H5T_STD_REF_OBJ_g)
/**
 * \ingroup PDTSTD
 * Dataset region reference
 */
#define H5T_STD_REF_DSETREG (H5OPEN H5T_STD_REF_DSETREG_g)
/**
 * \ingroup PDTSTD
 * Generic reference
 */
#define H5T_STD_REF (H5OPEN H5T_STD_REF_g)
H5_DLLVAR hid_t H5T_STD_I8BE_g;
H5_DLLVAR hid_t H5T_STD_I8LE_g;
H5_DLLVAR hid_t H5T_STD_I16BE_g;
H5_DLLVAR hid_t H5T_STD_I16LE_g;
H5_DLLVAR hid_t H5T_STD_I32BE_g;
H5_DLLVAR hid_t H5T_STD_I32LE_g;
H5_DLLVAR hid_t H5T_STD_I64BE_g;
H5_DLLVAR hid_t H5T_STD_I64LE_g;
H5_DLLVAR hid_t H5T_STD_U8BE_g;
H5_DLLVAR hid_t H5T_STD_U8LE_g;
H5_DLLVAR hid_t H5T_STD_U16BE_g;
H5_DLLVAR hid_t H5T_STD_U16LE_g;
H5_DLLVAR hid_t H5T_STD_U32BE_g;
H5_DLLVAR hid_t H5T_STD_U32LE_g;
H5_DLLVAR hid_t H5T_STD_U64BE_g;
H5_DLLVAR hid_t H5T_STD_U64LE_g;
H5_DLLVAR hid_t H5T_STD_B8BE_g;
H5_DLLVAR hid_t H5T_STD_B8LE_g;
H5_DLLVAR hid_t H5T_STD_B16BE_g;
H5_DLLVAR hid_t H5T_STD_B16LE_g;
H5_DLLVAR hid_t H5T_STD_B32BE_g;
H5_DLLVAR hid_t H5T_STD_B32LE_g;
H5_DLLVAR hid_t H5T_STD_B64BE_g;
H5_DLLVAR hid_t H5T_STD_B64LE_g;
H5_DLLVAR hid_t H5T_STD_REF_OBJ_g;
H5_DLLVAR hid_t H5T_STD_REF_DSETREG_g;
H5_DLLVAR hid_t H5T_STD_REF_g;

/*
 * Types which are particular to Unix.
 */
/**
 * \ingroup PDTUNIX
 */
#define H5T_UNIX_D32BE (H5OPEN H5T_UNIX_D32BE_g)
/**
 * \ingroup PDTUNIX
 */
#define H5T_UNIX_D32LE (H5OPEN H5T_UNIX_D32LE_g)
/**
 * \ingroup PDTUNIX
 */
#define H5T_UNIX_D64BE (H5OPEN H5T_UNIX_D64BE_g)
/**
 * \ingroup PDTUNIX
 */
#define H5T_UNIX_D64LE (H5OPEN H5T_UNIX_D64LE_g)
H5_DLLVAR hid_t H5T_UNIX_D32BE_g;
H5_DLLVAR hid_t H5T_UNIX_D32LE_g;
H5_DLLVAR hid_t H5T_UNIX_D64BE_g;
H5_DLLVAR hid_t H5T_UNIX_D64LE_g;

/*
 * Types particular to the C language.  String types use `bytes' instead
 * of `bits' as their size.
 */
/**
 * \ingroup PDTS
 * String datatype in C (size defined in bytes rather than in bits)
 */
#define H5T_C_S1 (H5OPEN H5T_C_S1_g)
H5_DLLVAR hid_t H5T_C_S1_g;

/*
 * Types particular to Fortran.
 */
/**
 * \ingroup PDTS
 * String datatype in Fortran (as defined for the HDF5 C library)
 */
#define H5T_FORTRAN_S1 (H5OPEN H5T_FORTRAN_S1_g)
H5_DLLVAR hid_t H5T_FORTRAN_S1_g;

/*
 * These types are for Intel CPU's.  They are little endian with IEEE
 * floating point.
 */
/**
 * \ingroup PDTX86
 * 8-bit little-endian signed (2's complement) integers for Intel CPUs
 */
#define H5T_INTEL_I8 H5T_STD_I8LE
/**
 * \ingroup PDTX86
 * 16-bit little-endian signed (2's complement) integers for Intel CPUs
 */
#define H5T_INTEL_I16 H5T_STD_I16LE
/**
 * \ingroup PDTX86
 * 32-bit little-endian signed (2's complement) integers for Intel CPUs
 */
#define H5T_INTEL_I32 H5T_STD_I32LE
/**
 * \ingroup PDTX86
 * 64-bit little-endian signed (2's complement) integers for Intel CPUs
 */
#define H5T_INTEL_I64 H5T_STD_I64LE
/**
 * \ingroup PDTX86
 * 8-bit little-endian unsigned integers for Intel CPUs
 */
#define H5T_INTEL_U8 H5T_STD_U8LE
/**
 * \ingroup PDTX86
 * 16-bit little-endian unsigned integers for Intel CPUs
 */
#define H5T_INTEL_U16 H5T_STD_U16LE
/**
 * \ingroup PDTX86
 * 32-bit little-endian unsigned integers for Intel CPUs
 */
#define H5T_INTEL_U32 H5T_STD_U32LE
/**
 * \ingroup PDTX86
 * 64-bit little-endian unsigned integers for Intel CPUs
 */
#define H5T_INTEL_U64 H5T_STD_U64LE
/**
 * \ingroup PDTX86
 * 8-bit little-endian bitfield for Intel CPUs
 */
#define H5T_INTEL_B8 H5T_STD_B8LE
/**
 * \ingroup PDTX86
 * 16-bit little-endian bitfield for Intel CPUs
 */
#define H5T_INTEL_B16 H5T_STD_B16LE
/**
 * \ingroup PDTX86
 * 32-bit little-endian bitfield for Intel CPUs
 */
#define H5T_INTEL_B32 H5T_STD_B32LE
/**
 * \ingroup PDTX86
 * 64-bit little-endian bitfield for Intel CPUs
 */
#define H5T_INTEL_B64 H5T_STD_B64LE
/**
 * \ingroup PDTX86
 * 32-bit little-endian IEEE floating-point numbers for Intel CPUs
 */
#define H5T_INTEL_F32 H5T_IEEE_F32LE
/**
 * \ingroup PDTX86
 * 64-bit little-endian IEEE floating-point numbers for Intel CPUs
 */
#define H5T_INTEL_F64 H5T_IEEE_F64LE

/*
 * These types are for DEC Alpha CPU's.  They are little endian with IEEE
 * floating point.
 */
/**
 * \ingroup PDTALPHA
 * 8-bit little-endian signed (2's complement) integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_I8 H5T_STD_I8LE
/**
 * \ingroup PDTALPHA
 * 16-bit little-endian signed (2's complement) integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_I16 H5T_STD_I16LE
/**
 * \ingroup PDTALPHA
 * 32-bit little-endian signed (2's complement) integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_I32 H5T_STD_I32LE
/**
 * \ingroup PDTALPHA
 * 64-bit little-endian signed (2's complement) integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_I64 H5T_STD_I64LE
/**
 * \ingroup PDTALPHA
 * 8-bit little-endian unsigned integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_U8 H5T_STD_U8LE
/**
 * \ingroup PDTALPHA
 * 16-bit little-endian unsigned integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_U16 H5T_STD_U16LE
/**
 * \ingroup PDTALPHA
 * 32-bit little-endian unsigned integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_U32 H5T_STD_U32LE
/**
 * \ingroup PDTALPHA
 * 64-bit little-endian unsigned integers for DEC Alpha CPUs
 */
#define H5T_ALPHA_U64 H5T_STD_U64LE
/**
 * \ingroup PDTALPHA
 * 8-bit little-endian bitfield for DEC Alpha CPUs
 */
#define H5T_ALPHA_B8 H5T_STD_B8LE
/**
 * \ingroup PDTALPHA
 * 16-bit little-endian bitfield for DEC Alpha CPUs
 */
#define H5T_ALPHA_B16 H5T_STD_B16LE
/**
 * \ingroup PDTALPHA
 * 32-bit little-endian bitfield for DEC Alpha CPUs
 */
#define H5T_ALPHA_B32 H5T_STD_B32LE
/**
 * \ingroup PDTALPHA
 * 64-bit little-endian bitfield for DEC Alpha CPUs
 */
#define H5T_ALPHA_B64 H5T_STD_B64LE
/**
 * \ingroup PDTALPHA
 * 32-bit little-endian IEEE floating-point numbers for DEC Alpha CPUs
 */
#define H5T_ALPHA_F32 H5T_IEEE_F32LE
/**
 * \ingroup PDTALPHA
 * 64-bit little-endian IEEE floating-point numbers for DEC Alpha CPUs
 */
#define H5T_ALPHA_F64 H5T_IEEE_F64LE

/*
 * These types are for MIPS cpu's commonly used in SGI systems. They are big
 * endian with IEEE floating point.
 */
/**
 * \ingroup PDTMIPS
 * 8-bit big-endian signed (2's complement) integers for SGI MIPS CPUs
 */
#define H5T_MIPS_I8 H5T_STD_I8BE
/**
 * \ingroup PDTMIPS
 * 16-bit big-endian signed (2's complement) integers for SGI MIPS CPUs
 */
#define H5T_MIPS_I16 H5T_STD_I16BE
/**
 * \ingroup PDTMIPS
 * 32-bit big-endian signed (2's complement) integers for SGI MIPS CPUs
 */
#define H5T_MIPS_I32 H5T_STD_I32BE
/**
 * \ingroup PDTMIPS
 * 64-bit big-endian signed (2's complement) integers for SGI MIPS CPUs
 */
#define H5T_MIPS_I64 H5T_STD_I64BE
/**
 * \ingroup PDTMIPS
 * 8-bit big-endian unsigned integers for SGI MIPS CPUs
 */
#define H5T_MIPS_U8 H5T_STD_U8BE
/**
 * \ingroup PDTMIPS
 * 16-bit big-endian unsigned integers for SGI MIPS CPUs
 */
#define H5T_MIPS_U16 H5T_STD_U16BE
/**
 * \ingroup PDTMIPS
 * 32-bit big-endian unsigned integers for SGI MIPS CPUs
 */
#define H5T_MIPS_U32 H5T_STD_U32BE
/**
 * \ingroup PDTMIPS
 * 64-bit big-endian unsigned integers for SGI MIPS CPUs
 */
#define H5T_MIPS_U64 H5T_STD_U64BE
/**
 * \ingroup PDTMIPS
 * 8-bit big-endian bitfield for SGI MIPS CPUs
 */
#define H5T_MIPS_B8 H5T_STD_B8BE
/**
 * \ingroup PDTMIPS
 * 16-bit big-endian bitfield for SGI MIPS CPUs
 */
#define H5T_MIPS_B16 H5T_STD_B16BE
/**
 * \ingroup PDTMIPS
 * 32-bit big-endian bitfield for SGI MIPS CPUs
 */
#define H5T_MIPS_B32 H5T_STD_B32BE
/**
 * \ingroup PDTMIPS
 * 64-bit big-endian bitfield for SGI MIPS CPUs
 */
#define H5T_MIPS_B64 H5T_STD_B64BE
/**
 * \ingroup PDTMIPS
 * 32-bit big-endian IEEE floating-point numbers for MIPS CPUs
 */
#define H5T_MIPS_F32 H5T_IEEE_F32BE
/**
 * \ingroup PDTMIPS
 * 64-bit big-endian IEEE floating-point numbers for MIPS CPUs
 */
#define H5T_MIPS_F64 H5T_IEEE_F64BE

/*
 * The VAX floating point types (i.e. in VAX byte order)
 */
/**
 * \ingroup PDTALPHA
 * 32-bit VAX byte order floating-point numbers for OpenVMS on DEC Alpha CPUs
 */
#define H5T_VAX_F32 (H5OPEN H5T_VAX_F32_g)
/**
 * \ingroup PDTALPHA
 * 64-bit VAX byte order floating-point numbers for OpenVMS on DEC Alpha CPUs
 */
#define H5T_VAX_F64 (H5OPEN H5T_VAX_F64_g)
H5_DLLVAR hid_t H5T_VAX_F32_g;
H5_DLLVAR hid_t H5T_VAX_F64_g;

/*
 * The predefined native types for this platform. Instead of a class name,
 * precision and byte order as the last component, they have a C-like type
 * name.  If the type begins with `U' then it is the unsigned version of the
 * integer type; other integer types are signed.  The type LLONG corresponds
 * to C's `long long' and LDOUBLE is `long double' (these types might be the
 * same as `LONG' and `DOUBLE' respectively).
 */
/**
 * \ingroup PDTNAT
 * C-style \c char
 */
#define H5T_NATIVE_CHAR (CHAR_MIN ? H5T_NATIVE_SCHAR : H5T_NATIVE_UCHAR)
/**
 * \ingroup PDTNAT
 * C-style \TText{signed char}
 */
#define H5T_NATIVE_SCHAR (H5OPEN H5T_NATIVE_SCHAR_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{unsigned char}
 */
#define H5T_NATIVE_UCHAR (H5OPEN H5T_NATIVE_UCHAR_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{short}
 */
#define H5T_NATIVE_SHORT (H5OPEN H5T_NATIVE_SHORT_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{unsigned short}
 */
#define H5T_NATIVE_USHORT (H5OPEN H5T_NATIVE_USHORT_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{int}
 */
#define H5T_NATIVE_INT (H5OPEN H5T_NATIVE_INT_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{unsigned int}
 */
#define H5T_NATIVE_UINT (H5OPEN H5T_NATIVE_UINT_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{long}
 */
#define H5T_NATIVE_LONG (H5OPEN H5T_NATIVE_LONG_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{unsigned long}
 */
#define H5T_NATIVE_ULONG (H5OPEN H5T_NATIVE_ULONG_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{long long}
 */
#define H5T_NATIVE_LLONG (H5OPEN H5T_NATIVE_LLONG_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{unsigned long long}
 */
#define H5T_NATIVE_ULLONG (H5OPEN H5T_NATIVE_ULLONG_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{_Float16} (May be \TText{H5I_INVALID_HID} if platform doesn't support \TText{_Float16} type)
 */
#define H5T_NATIVE_FLOAT16 (H5OPEN H5T_NATIVE_FLOAT16_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{float}
 */
#define H5T_NATIVE_FLOAT (H5OPEN H5T_NATIVE_FLOAT_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{double}
 */
#define H5T_NATIVE_DOUBLE (H5OPEN H5T_NATIVE_DOUBLE_g)
/**
 * \ingroup PDTNAT
 * C-style \TText{long double}
 */
#define H5T_NATIVE_LDOUBLE (H5OPEN H5T_NATIVE_LDOUBLE_g)
/**
 * \ingroup PDTNAT
 * HDF5 8-bit bitfield based on native types
 */
#define H5T_NATIVE_B8 (H5OPEN H5T_NATIVE_B8_g)
/**
 * \ingroup PDTNAT
 * HDF5 16-bit bitfield based on native types
 */
#define H5T_NATIVE_B16 (H5OPEN H5T_NATIVE_B16_g)
/**
 * \ingroup PDTNAT
 * HDF5 32-bit bitfield based on native types
 */
#define H5T_NATIVE_B32 (H5OPEN H5T_NATIVE_B32_g)
/**
 * \ingroup PDTNAT
 * HDF5 64-bit bitfield based on native types
 */
#define H5T_NATIVE_B64 (H5OPEN H5T_NATIVE_B64_g)
/**
 * \ingroup PDTNAT
 * HDF5 opaque unit based on native types
 */
#define H5T_NATIVE_OPAQUE (H5OPEN H5T_NATIVE_OPAQUE_g)
/**
 * \ingroup PDTNAT
 * HDF5 address type based on native types
 */
#define H5T_NATIVE_HADDR (H5OPEN H5T_NATIVE_HADDR_g)
/**
 * \ingroup PDTNAT
 * HDF5 size type based on native types
 */
#define H5T_NATIVE_HSIZE (H5OPEN H5T_NATIVE_HSIZE_g)
/**
 * \ingroup PDTNAT
 * HDF5 signed size type based on native types
 */
#define H5T_NATIVE_HSSIZE (H5OPEN H5T_NATIVE_HSSIZE_g)
/**
 * \ingroup PDTNAT
 * HDF5 error code type based on native types
 */
#define H5T_NATIVE_HERR (H5OPEN H5T_NATIVE_HERR_g)
/**
 * \ingroup PDTNAT
 * HDF5 Boolean type based on native types
 */
#define H5T_NATIVE_HBOOL (H5OPEN H5T_NATIVE_HBOOL_g)
H5_DLLVAR hid_t H5T_NATIVE_SCHAR_g;
H5_DLLVAR hid_t H5T_NATIVE_UCHAR_g;
H5_DLLVAR hid_t H5T_NATIVE_SHORT_g;
H5_DLLVAR hid_t H5T_NATIVE_USHORT_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_g;
H5_DLLVAR hid_t H5T_NATIVE_LONG_g;
H5_DLLVAR hid_t H5T_NATIVE_ULONG_g;
H5_DLLVAR hid_t H5T_NATIVE_LLONG_g;
H5_DLLVAR hid_t H5T_NATIVE_ULLONG_g;
H5_DLLVAR hid_t H5T_NATIVE_FLOAT16_g;
H5_DLLVAR hid_t H5T_NATIVE_FLOAT_g;
H5_DLLVAR hid_t H5T_NATIVE_DOUBLE_g;
H5_DLLVAR hid_t H5T_NATIVE_LDOUBLE_g;
H5_DLLVAR hid_t H5T_NATIVE_B8_g;
H5_DLLVAR hid_t H5T_NATIVE_B16_g;
H5_DLLVAR hid_t H5T_NATIVE_B32_g;
H5_DLLVAR hid_t H5T_NATIVE_B64_g;
H5_DLLVAR hid_t H5T_NATIVE_OPAQUE_g;
H5_DLLVAR hid_t H5T_NATIVE_HADDR_g;
H5_DLLVAR hid_t H5T_NATIVE_HSIZE_g;
H5_DLLVAR hid_t H5T_NATIVE_HSSIZE_g;
H5_DLLVAR hid_t H5T_NATIVE_HERR_g;
H5_DLLVAR hid_t H5T_NATIVE_HBOOL_g;

/* C9x integer types */
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT8 (H5OPEN H5T_NATIVE_INT8_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT8 (H5OPEN H5T_NATIVE_UINT8_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_LEAST8 (H5OPEN H5T_NATIVE_INT_LEAST8_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_LEAST8 (H5OPEN H5T_NATIVE_UINT_LEAST8_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_FAST8 (H5OPEN H5T_NATIVE_INT_FAST8_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_FAST8 (H5OPEN H5T_NATIVE_UINT_FAST8_g)
H5_DLLVAR hid_t H5T_NATIVE_INT8_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT8_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_LEAST8_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_LEAST8_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_FAST8_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_FAST8_g;

/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT16 (H5OPEN H5T_NATIVE_INT16_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT16 (H5OPEN H5T_NATIVE_UINT16_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_LEAST16 (H5OPEN H5T_NATIVE_INT_LEAST16_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_LEAST16 (H5OPEN H5T_NATIVE_UINT_LEAST16_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_FAST16 (H5OPEN H5T_NATIVE_INT_FAST16_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_FAST16 (H5OPEN H5T_NATIVE_UINT_FAST16_g)
H5_DLLVAR hid_t H5T_NATIVE_INT16_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT16_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_LEAST16_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_LEAST16_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_FAST16_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_FAST16_g;

/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT32 (H5OPEN H5T_NATIVE_INT32_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT32 (H5OPEN H5T_NATIVE_UINT32_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_LEAST32 (H5OPEN H5T_NATIVE_INT_LEAST32_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_LEAST32 (H5OPEN H5T_NATIVE_UINT_LEAST32_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_FAST32 (H5OPEN H5T_NATIVE_INT_FAST32_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_FAST32 (H5OPEN H5T_NATIVE_UINT_FAST32_g)
H5_DLLVAR hid_t H5T_NATIVE_INT32_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT32_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_LEAST32_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_LEAST32_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_FAST32_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_FAST32_g;

/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT64 (H5OPEN H5T_NATIVE_INT64_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT64 (H5OPEN H5T_NATIVE_UINT64_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_LEAST64 (H5OPEN H5T_NATIVE_INT_LEAST64_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_LEAST64 (H5OPEN H5T_NATIVE_UINT_LEAST64_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_INT_FAST64 (H5OPEN H5T_NATIVE_INT_FAST64_g)
/**
 * \ingroup PDTC9x
 */
#define H5T_NATIVE_UINT_FAST64 (H5OPEN H5T_NATIVE_UINT_FAST64_g)
H5_DLLVAR hid_t H5T_NATIVE_INT64_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT64_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_LEAST64_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_LEAST64_g;
H5_DLLVAR hid_t H5T_NATIVE_INT_FAST64_g;
H5_DLLVAR hid_t H5T_NATIVE_UINT_FAST64_g;

/* Operations defined on all datatypes */
/**
 * \ingroup H5T
 *
 * \brief Creates a new datatype.
 *
 * \param[in] type Class of datatype to create
 * \param[in] size  Size, in bytes, of the datatype being created
 *
 * \return \hid_t{datatype}
 *
 * \details H5Tcreate() creates a new datatype of the specified class with the
 *          specified number of bytes. This function is used only with the
 *          following datatype classes:
 *          - #H5T_COMPOUND
 *          - #H5T_OPAQUE
 *          - #H5T_ENUM
 *          - #H5T_STRING
 *
 *          Other datatypes, including integer and floating-point datatypes,
 *          are typically created by using H5Tcopy() to copy and modify a
 *          predefined datatype.
 *
 *          When creating a variable-length string datatype, \p size must
 *          be #H5T_VARIABLE; see \ref_vlen_strings.
 *
 *          When creating a fixed-length string datatype, \p size will
 *          be the length of the string in bytes. The length of the
 *          string in characters will depend on i the encoding used; see
 *          #H5Pset_char_encoding.
 *
 *          ENUMs created with this function have a signed native integer
 *          base datatype.  Use H5Tenum_create() if a different integer base
 *          datatype is required.
 *
 *          The datatype identifier returned from this function should be
 *          released with H5Tclose or resource leaks will result.
 *
 * \see H5Tclose()
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Tcreate(H5T_class_t type, size_t size);
/**
 * \ingroup H5T
 *
 * \brief Copies an existing datatype.
 *
 * \type_id
 *
 * \return \hid_t{datatype}
 *
 * \details H5Tcopy() makes a copy of an existing datatype. The returned type
 *          is always transient and unlocked.
 *
 *          The \p type_id argument can be either a datatype identifier,
 *          a predefined datatype (defined in H5Tpublic.h), or a dataset
 *          identifier.  If \p type_id is a dataset identifier, this function
 *          returns a transient, modifiable datatype which is a copy of the
 *          dataset's datatype.
 *
 *          The returned datatype identifier should be released with H5Tclose()
 *          to prevent resource leaks.
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Tcopy(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Releases a datatype
 *
 * \type_id
 *
 * \return \herr_t
 *
 * \details H5Tclose() releases the datatype \p dtype_id. Further access
 *          through this datatype identifier is illegal. Failure to release
 *          a datatype with this call will result in resource leaks.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tclose(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Asynchronous version of H5Tclose().
 *
 * \since 1.12.0
 *
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Tclose_async(const char *app_file, const char *app_func, unsigned app_line, hid_t type_id,
                             hid_t es_id);
#else
H5_DLL herr_t H5Tclose_async(hid_t type_id, hid_t es_id);
#endif
/**
 * \ingroup H5T
 *
 * \brief Determines whether two datatype identifiers refer to the same datatype
 *
 * \type_id{type1_id}
 * \type_id{type2_id}
 *
 * \return \htri_t
 *
 * \details H5Tequal() determines whether two datatype identifiers refer to
 *          the same datatype.
 *
 * \since 1.0.0
 *
 */
H5_DLL htri_t H5Tequal(hid_t type1_id, hid_t type2_id);
/**
 * \ingroup H5T
 *
 * \brief Locks a datatype
 *
 * \type_id
 *
 * \return \herr_t
 *
 * \details H5Tlock() locks the datatype specified by the dtype_id identifier,
 *          making it read-only and non-destructible. This is normally done by
 *          the library for predefined datatypes so the application does not
 *          inadvertently change or delete a predefined type. Once a datatype
 *          is locked it can never be unlocked.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tlock(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Commits a transient datatype, linking it into the file and creating
 *        a new committed datatype
 *
 * \fg_loc_id
 * \param[in] name Name given to committed datatype
 * \type_id Identifier of datatype to be committed and, upon function's
 *          return, identifier for the committed datatype
 * \lcpl_id
 * \tcpl_id
 * \tapl_id
 *
 * \return \herr_t
 *
 * \details H5Tcommit2() saves a transient datatype as an immutable committed
 *          datatype in a file. The datatype specified by \p dtype_id is
 *          committed to the file with the name name at the location specified
 *          by \p loc_id and with the datatype creation and access property
 *          lists \p tcpl_id and \p tapl_id, respectively.
 *
 *          \p loc_id may be a file identifier, or a group identifier within
 *          that file. \p name may be either an absolute path in the file or
 *          a relative path from \p loc_id naming the newly-committed datatype.
 *
 *          The link creation property list, \p lcpl_id, governs creation of
 *          the link(s) by which the new committed datatype is accessed and
 *          the creation of any intermediate groups that may be missing.
 *
 *          Once committed, this datatype may be used to define the datatype
 *          of any other dataset or attribute in the file.
 *
 *          This function will not accept a datatype that cannot actually hold
 *          information. This currently includes compound datatypes with no
 *          fields and enumerated datatypes with no members.
 *
 *          Committed datatypes are sometimes referred to as named datatypes.
 *
 * \version 1.8.7 Function modified in this release to reject datatypes that
 *          will not accommodate actual data, such as a compound datatype
 *          with no fields or an enumerated datatype with no members.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Tcommit2(hid_t loc_id, const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id,
                         hid_t tapl_id);
/**
 * \ingroup H5T
 *
 * \brief Asynchronous version of H5Tcommit2().
 *
 * \since 1.12.0
 *
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Tcommit_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                              const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id,
                              hid_t es_id);
#else
H5_DLL herr_t H5Tcommit_async(hid_t loc_id, const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id,
                              hid_t tapl_id, hid_t es_id);
#endif
/**
 * --------------------------------------------------------------------------
 * \ingroup H5T
 *
 * \brief Opens a committed (named) datatype
 *
 * \fgdta_loc_id
 * \param[in] name Name of the datatype to open
 * \tapl_id
 *
 * \return \hid_t{datatype}
 *
 * \details H5Topen2() opens a committed datatype at the location specified
 *          by \p loc_id and returns an identifier for the datatype. \p
 *          loc_id is either a file or group identifier. The identifier should
 *          eventually be closed by calling H5Tclose()  to release resources.
 *
 *          The committed datatype is opened with the datatype access property
 *          list tapl_id.
 *
 * \since 1.8.0
 *
 */
H5_DLL hid_t H5Topen2(hid_t loc_id, const char *name, hid_t tapl_id);
/**
 * \ingroup H5T
 *
 * \brief Asynchronous version of H5Topen2().
 *
 * \since 1.12.0
 *
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Topen_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                           const char *name, hid_t tapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Topen_async(hid_t loc_id, const char *name, hid_t tapl_id, hid_t es_id);
#endif
/**
 * \ingroup H5T
 *
 * \brief Commits a transient datatype to a file, creating a newly named
 *        datatype, but does not link it into the file structure
 *
 * \fg_loc_id
 * \type_id
 * \tcpl_id
 * \tapl_id
 *
 * \return \herr_t
 *
 * \details H5Tcommit_anon() commits a transient datatype (not immutable)
 *          to a file, turning it into a named datatype with the specified
 *          creation and property lists. With default property lists,
 *          #H5P_DEFAULT, H5Tcommit_anon() provides similar functionality to
 *          that of H5Tcommit(), with the differences described below.
 *
 *          #H5P_DEFAULT can be passed in for the datatype creation property
 *          list identifier, \p tcpl_id.  The datatype access property list
 *          identifier, \p tapl_id, is provided for future functionality and
 *          is not used at this time. This parameter should always be passed
 *          as the value #H5P_DEFAULT.
 *
 *          Note that H5Tcommit_anon() does not link this newly-committed
 *          datatype into the file. After the H5Tcommit_anon() call, the
 *          datatype identifier \p type_id must be linked into the HDF5 file
 *          structure with H5Olink() or it will be deleted from the file when
 *          the file is closed.
 *
 *          The differences between this function and H5Tcommit() are as follows:
 *          \li H5Tcommit_anon() explicitly includes property lists,
 *              which provides for greater control of the creation process
 *              and of the properties of the new named datatype. H5Tcommit()
 *              always uses default properties.
 *          \li H5Tcommit_anon() neither provides the new named datatype's
 *              name nor links it into the HDF5 file structure; those actions
 *              must be performed separately through a call to H5Olink(),
 *              which offers greater control over linking.
 *
 *          This function will not accept a datatype that cannot actually
 *          hold data. This currently includes compound datatypes with no
 *          fields and enumerated datatypes with no members.
 *
 * \version 1.8.7 Function modified in this release to reject datatypes that
 *                will not accommodate actual data, such as a compound datatype
 *                with no fields or an enumerated datatype with no members.
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tcommit_anon(hid_t loc_id, hid_t type_id, hid_t tcpl_id, hid_t tapl_id);
/**
 * \ingroup H5T
 *
 * \brief Returns a copy of a datatype's creation property list
 *
 * \type_id
 *
 * \return \hid_t{datatype creation property list}
 *
 * \details H5Tget_create_plist() returns a property list identifier
 *          for the datatype creation property list associated with the datatype
 *          specified by \p type_id.
 *
 *          The creation property list identifier should be released with
 *          H5Pclose() to prevent memory leaks.
 *
 * \since 1.8.0
 *
 */
H5_DLL hid_t H5Tget_create_plist(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Determines whether a datatype is a committed type or a transient type
 *
 * \type_id
 *
 * \return \htri_t
 *
 * \details H5Tcommitted() queries a type to determine whether the type
 *          specified by the \p dtype_id identifier is a committed (formerly
 *          known as a \Emph{named}) type or a transient type. If this function returns
 *          a positive value, then the type is committed (that is, it has been
 *          committed, perhaps by some other application). Datasets which
 *          return committed datatypes with H5Dget_type() are able to share
 *          the datatype with other datasets in the same file.
 *
 * \version 1.8.0 Fortran API was added
 *
 * \since 1.0.0
 *
 */
H5_DLL htri_t H5Tcommitted(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Encodes a datatype object description into a binary buffer
 *
 * \param[in] obj_id Identifier of the object to be encoded
 * \param[in,out] buf Buffer for the object to be encoded into.
 * \param[in,out] nalloc IN: The size of the allocated buffer
 *                       OUT: The size of the buffer needed
 *
 * \return \herr_t
 *
 * \details H5Tencode() Given datatype identifier, H5Tencode() converts a
 *          datatype description into binary form in a buffer. Using this
 *          binary form in the buffer, a datatype object can be reconstructed
 *          using H5Tdecode() to return a new object handle (\ref hid_t) for
 *          this datatype.
 *
 *          If the provided buffer is NULL, only the size of buffer needed is
 *          returned through \p nalloc.
 *
 *          A preliminary H5Tencode() call can be made to find out the size
 *          of the buffer needed. This value is returned as \p nalloc. That
 *          value can then be assigned to \p nalloc for a second H5Tencode()
 *          call, which will retrieve the actual encoded object.
 *
 *          If the library finds that \p nalloc is not big enough for the
 *          object, it simply returns the size of the buffer needed through
 *          \p nalloc without encoding the provided buffer.
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tencode(hid_t obj_id, void *buf, size_t *nalloc);
/**
 * \ingroup H5T
 *
 * \brief Decodes a binary object description of datatype and returns a new
 *        object handle
 *
 * \param[in] buf Buffer for the datatype object to be decoded
 *
 * \return \hid_t{datatype}
 *
 * \details H5Tdecode() Given an object description of datatype in binary in a
 *          buffer, H5Tdecode() reconstructs the HDF5 datatype object and
 *          returns a new object handle for it. The binary description of
 *          the object is encoded by H5Tencode(). User is responsible for
 *          passing in the right buffer.
 *
 *          The datatype identifier returned by this function can be released
 *          with H5Tclose() when the identifier is no longer needed so that
 *          resource leaks will not develop.
 *
 * \since 1.2.0
 *
 */
H5_DLL hid_t H5Tdecode(const void *buf);
/**
 * \ingroup H5T
 *
 * \brief Flushes all buffers associated with a committed datatype to disk
 *
 * \type_id
 *
 * \return \herr_t
 *
 * \details H5Tflush() causes all buffers associated with a committed datatype
 *          \p type_id to be immediately flushed to disk without removing the
 *          data from the cache.
 *
 *          HDF5 does not possess full control over buffering.  H5Tflush()
 *          flushes the internal HDF5 buffers and then asks the operating
 *          system (the OS) to flush the system buffers for the open
 *          files. After that, the OS is responsible for ensuring that the
 *          data is actually flushed to disk.
 *
 * \return \herr_t
 *
 * \since 1.10.0
 *
 * \see     H5Dflush()
 *          H5Drefresh()
 *          H5Tflush()
 *          H5Grefresh()
 *          H5Oflush()
 *          H5Orefresh()
 *          H5Tflush()
 *          H5Trefresh()
 *          H5Fstart_swmr_write()
 *          H5Pget_append_flush()
 *          H5Pget_object_flush_cb()
 *          H5Pset_append_flush()
 *          H5Pset_object_flush_cb()
 *
 */
H5_DLL herr_t H5Tflush(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Refreshes all buffers associated with a committed datatype
 *
 * \type_id
 *
 * \return \herr_t
 *
 * \details H5Trefresh() causes all buffers associated with a committed
 *          datatype to be cleared and immediately re-loaded with updated
 *          contents from disk.
 *
 *          This function essentially closes the datatype, evicts all
 *          metadata associated with it from the cache, and then reopens the
 *          datatype. The reopened datatype is automatically re-registered
 *          with the same identifier.
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5Trefresh(hid_t type_id);

/* Operations defined on compound datatypes */
/**
 * \ingroup COMPOUND
 *
 * \brief Adds a new member to a compound datatype.
 *
 * \type_id{parent_id}
 * \param[in] name      Name of the field to insert
 * \param[in] offset    Offset in memory structure of the field to insert
 * \param[in] member_id  Datatype identifier of the field to insert
 *
 * \return \herr_t
 *
 * \details H5Tinsert() adds another member to the compound datatype, specified
 *          \p type_id.
 *
 *          The new member has a \p name which must be unique within the
 *          compound datatype. The \p offset argument defines the start of the
 *          member in an instance of the compound datatype, and \p member_id
 *          is the datatype identifier of the new member.
 *
 *          \note Members of a compound datatype do not have to be atomic
 *          datatypes; a compound datatype can have a member which is a
 *          compound datatype.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tinsert(hid_t parent_id, const char *name, size_t offset, hid_t member_id);
/**
 * \ingroup COMPOUND
 *
 * \brief Recursively removes padding from within a compound datatype
 *
 * \type_id
 *
 * \return \herr_t
 *
 * \details H5Tpack() recursively removes padding from within a compound
 *          datatype to make it more efficient (space-wise) to store that data.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tpack(hid_t type_id);

/* Operations defined on enumeration datatypes */
/**
 * \ingroup ENUM
 *
 * \brief Creates a new enumeration datatype
 *
 * \param[in] base_id Datatype identifier for the base datatype.  Must be an
 *            integer datatype
 *
 * \return \hid_t{enumeration datatype}
 *
 * \details H5Tenum_create() creates a new enumeration datatype based on the
 *          specified base datatype, dtype_id, which must be an integer datatype.
 *
 *          If a particular architecture datatype is required, a little endian
 *          or big endian datatype for example, use a native datatype as the
 *          base datatype and use H5Tconvert()  on values as they are read
 *          from or written to a dataset.
 *
 * \since 1.2.0
 *
 */
H5_DLL hid_t H5Tenum_create(hid_t base_id);
/**
 * \ingroup ENUM
 *
 * \brief Inserts a new enumeration datatype member
 *
 * \type_id{type}
 * \param[in] name  Name of the new member
 * \param[in] value Pointer to the value of the new member
 *
 * \return \herr_t
 *
 * \details H5Tenum_insert() inserts a new enumeration datatype member into an
 *          enumeration datatype.
 *
 *          \p type_id is the datatype identifier for the enumeration datatype,
 *          \p name is the name of the new member, and \p value points to the
 *           value of the new member.
 *
 *          \p name and \p value must both be unique within \p dtype_id.
 *
 *          \p value points to data which must be of the integer base datatype
 *          used when the enumeration datatype was created. If a particular
 *          architecture datatype is required, a little endian or big endian
 *          datatype for example, use a native datatype as the base datatype
 *          and use H5Tconvert() on values as they are read from or written
 *          to a dataset.
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tenum_insert(hid_t type, const char *name, const void *value);
/**
 * \ingroup ENUM
 *
 * \brief Returns the symbol name corresponding to a specified member of an
 *        enumeration datatype
 *
 * \type_id{type}
 * \param[in] value Value of the enumeration datatype
 * \param[out] name Buffer for output of the symbol name
 * \param[in] size Anticipated size of the symbol name, in bytes
 *
 * \return Returns a non-negative value if successful. Otherwise returns a
 *         negative value
 *
 * \details H5Tenum_nameof() finds the symbol name that corresponds to the
 *          specified \p value of the enumeration datatype \p type.
 *
 *          At most \p size characters of the symbol \p name are copied into
 *          the \p name buffer. If the entire symbol name and null terminator
 *          do not fit in the name buffer, then as many characters as possible
 *          are copied (not null terminated) and the function fails.
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tenum_nameof(hid_t type, const void *value, char *name /*out*/, size_t size);
/**
 * \ingroup ENUM
 *
 * \brief Returns the value corresponding to a specified member of an
 *        enumeration datatype
 *
 * \type_id{type}
 * \param[in] name Symbol name of the enumeration datatype
 * \param[out] value Buffer for the value of the enumeration datatype
 *
 * \return \herr_t
 *
 * \details H5Tenum_valueof() finds the value that corresponds to the
 *          specified name of the enumeration datatype \p dtype_id.
 *
 *          Values returned in \p value will be of the enumerated type's
 *          base type, that is, the datatype used by H5Tenum_create() when
 *          the enumerated type was created.
 *
 *          The \p value buffer must be at least large enough to hold a value
 *          of that base type. If the size is unknown, you can determine it
 *          with H5Tget_size().
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tenum_valueof(hid_t type, const char *name, void *value /*out*/);

/* Operations defined on variable-length datatypes */
/**
 * \ingroup VLEN
 *
 * \brief Creates a new variable-length array datatype
 *
 * \type_id{base_id}, the element type of the datatype to create
 *
 * \return \hid_t{variable-length datatype}
 *
 * \details H5Tvlen_create() creates a new one-dimensional array datatype of
 *          variable-length (VL) with the base datatype \p base_id.
 *
 *          This one-dimensional array often represents a data sequence of the
 *          base datatype, such as characters for character sequences or vertex
 *          coordinates for polygon lists. The base type specified for the VL
 *          datatype can be any HDF5 datatype, including another VL datatype, a
 *          compound datatype, or an atomic datatype.
 *
 *          When necessary, use H5Tget_super() to determine the base type of
 *          the VL datatype.
 *
 *          The datatype identifier returned from this function should be
 *          released with H5Tclose() or resource leaks will result. Under
 *          certain circumstances, H5Dvlen_reclaim() must also be used.
 *
 * \attention H5Tvlen_create() cannot be used to create a variable-length
 *            string datatype. H5Tvlen_create() called with a string or
 *            character base type creates a variable-length sequence of strings
 *            (a variable-length, 1-dimensional array), with each element of
 *            the array being of the string or character base type.\n
 *            To create a variable-length string datatype, see \ref_vlen_strings.
 *
 * \since 1.2.0
 *
 */
H5_DLL hid_t H5Tvlen_create(hid_t base_id);

/* Operations defined on array datatypes */
/**
 * \ingroup ARRAY
 *
 * \brief Creates an array datatype object
 *
 * \param[in] base_id Datatype identifier for the array base datatype
 * \param[in] ndims Rank of the array
 * \param[in] dim Size of each array dimension
 *
 * \return \hid_t{array datatype}
 *
 * \details H5Tarray_create2() creates a new array datatype object.\n\n
 *          \p base_id is the datatype of every element of the array, i.e.,
 *          of the number at each position in the array.
 *
 *          \p ndims is the number of dimensions and the size of each dimension
 *          is specified in the array \p dim. The value of \p rank is
 *          currently limited to #H5S_MAX_RANK and must be greater than 0
 *          (zero). All dimension sizes specified in \p dim must be greater
 *          than 0 (zero).
 *
 * \since 1.8.0
 *
 */
H5_DLL hid_t H5Tarray_create2(hid_t base_id, unsigned ndims, const hsize_t dim[/* ndims */]);
/**
 * \ingroup ARRAY
 *
 * \brief Returns the rank of an array datatype
 *
 * \type_id
 *
 * \return Returns the rank of the array if successful; otherwise returns a
 *         negative value.
 *
 * \details H5Tget_array_ndims() returns the rank, i.e., the number of
 *          dimensions, of an array datatype object.
 *
 * \since 1.2.0
 *
 */
H5_DLL int H5Tget_array_ndims(hid_t type_id);
/**
 * \ingroup ARRAY
 *
 * \brief Retrieves sizes of array dimensions
 *
 * \type_id
 * \param[out] dims Sizes of array dimensions
 *
 * \return Returns the non-negative number of dimensions of the array type
 *         if successful; otherwise returns a negative value.
 *
 * \details H5Tget_array_dims2() returns the sizes of the dimensions of the
 *          specified array datatype object in the array \p dims.
 *
 * \since 1.2.0
 *
 */
H5_DLL int H5Tget_array_dims2(hid_t type_id, hsize_t dims[]);

/* Operations defined on opaque datatypes */
/**
 * \ingroup OPAQUE
 *
 * \brief Tags an opaque datatype
 *
 * \type_id{type} of an opaque datatype
 * \param[in] tag Descriptive ASCII string with which the opaque datatype is
 *                to be tagged
 *
 * \return \herr_t
 *
 * \details H5Tset_tag() tags an opaque datatype \p type with a descriptive
 *          ASCII identifier, \p tag.
 *
 *          \p tag is intended to provide a concise description; the maximum
 *          size is hard-coded in the HDF5 library as 256 bytes
 *          (#H5T_OPAQUE_TAG_MAX).
 *
 * \version 1.6.5 The #H5T_OPAQUE_TAG_MAX macro constant, specifying the
 *                maximum size of an opaque datatype tag, was added in
 *                H5Tpublic.h.
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tset_tag(hid_t type, const char *tag);
/**
 * \ingroup OPAQUE
 *
 * \brief Gets the tag associated with an opaque datatype
 *
 * \type_id{type} of an opaque datatype
 *
 * \return Returns a pointer to an allocated string if successful; otherwise
 *         returns NULL.
 *
 * \details H5Tget_tag() returns the tag associated with the opaque datatype
 *         \p type.
 *
 * \attention The tag is returned via a pointer to an allocated string, which
 *            the caller must free.
 *
 * \since 1.2.0
 *
 */
H5_DLL char *H5Tget_tag(hid_t type);

/* Querying property values */
/**
 * \ingroup H5T
 *
 * \brief Returns the base datatype from which a datatype is derived
 *
 * \type_id{type}
 *
 * \return \hid_t{datatype}
 *
 * \details H5Tget_super() returns the base datatype from which the datatype
 *          \p type_id is derived.  In the case of an enumeration type, the
 *          return value is an integer type.
 *
 *          The datatype identifier returned by this function must be released
 *          with H5Tclose()  when the identifier is no longer needed so that
 *          resource leaks will not develop.
 *
 * \since 1.2.0
 *
 */
H5_DLL hid_t H5Tget_super(hid_t type);
/**
 * \ingroup H5T
 *
 * \brief Returns a datatype class
 *
 * \type_id
 *
 * \return Returns the datatype class if successful; otherwise #H5T_NO_CLASS.
 *
 * \details H5Tget_class() returns the class of the datatype \p type_id.
 *          Valid class identifiers, as defined in H5Tpublic.h, are:
 *          \snippet this H5T_class_t_snip
 *
 * \note The library returns #H5T_STRING for both fixed-length and
 *       variable-length strings.
 *
 * \note Unsupported datatype: The time datatype class, #H5T_TIME,
 *       is not supported. If #H5T_TIME is used, the resulting data will
 *       be readable and modifiable only on the originating computing
 *       platform; it will not be portable to other platforms.
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_class_t H5Tget_class(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Determines whether a datatype contains any datatypes of the given
 *        datatype class
 *
 * \type_id
 * \param[in] cls Datatype class
 *
 * \return \htri_t
 *
 * \details H5Tdetect_class() determines whether the datatype specified in
 *          \p type_id contains any datatypes of the datatype class specified
 *          in \p dtype_class.
 *
 *          This function is useful primarily in recursively examining all the
 *          fields and/or base types of compound, array, and variable-length
 *          datatypes.
 *
 *          Valid class identifiers, as defined in H5Tpublic.h, are:
 *          \snippet this H5T_class_t_snip
 *
 * \since 1.6.0
 *
 */
H5_DLL htri_t H5Tdetect_class(hid_t type_id, H5T_class_t cls);
/**
 * \ingroup H5T
 *
 * \brief Returns the size of a datatype
 *
 * \type_id
 *
 * \return Returns the size of the datatype in bytes if successful; otherwise,
 *         returns 0.
 *
 * \details H5Tget_size() returns the size of a datatype in bytes.
 *          \li For atomic datatypes, array datatypes, compound datatypes, and
 *          other datatypes of a constant size, the returned value is the
 *          size of the actual datatype in bytes.
 *          \li For variable-length string datatypes the returned value is
 *          the size of the pointer to the actual string, or \c sizeof(\c
 *          char \c *). This function does not return the size of actual
 *          variable-length string data.
 *          \li For variable-length sequence datatypes (see H5Tvlen_create()),
 *          the returned value is the size of the \p hvl_t struct, or \c
 *          sizeof(\p hvl_t). The \p hvl_t struct contains a pointer to the
 *          actual data and a size value.  This function does not return the
 *          size of actual variable-length sequence data.
 *
 * \see H5Tset_size()
 *
 * \since 1.0.0
 */
H5_DLL size_t H5Tget_size(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Returns the byte order of an atomic datatype
 *
 * \type_id
 *
 * \return Returns a byte order constant if successful; otherwise returns
 *         #H5T_ORDER_ERROR (-1)
 *
 * \details H5Tget_order() returns the byte order of an atomic datatype.
 *          Possible return values are:
 *          \snippet this H5T_order_t_snip
 *          Members of a compound datatype need not have the same byte
 *          order. If members of a compound datatype have more than one of
 *          little endian, big endian, or VAX byte order, H5Tget_order() will
 *          return #H5T_ORDER_MIXED for the compound datatype. A byte order of
 *          #H5T_ORDER_NONE will, however, be ignored; for example, if one or
 *          more members of a compound datatype have byte order #H5T_ORDER_NONE
 *          but all other members have byte order #H5T_ORDER_LE,  H5Tget_order()
 *          will return #H5T_ORDER_LE for the compound datatype.
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_order_t H5Tget_order(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Returns the precision of an atomic datatype
 *
 * \type_id
 *
 * \return Returns the number of significant bits if successful; otherwise 0
 *
 * \details H5Tget_precision() returns the precision of an atomic datatype
 *          (for example, integer or float) or a datatype whose base (parent)
 *          type is an atomic type (for example, array, enum and variable
 *          length). The precision is the number of significant bits which,
 *          unless padding is present, is 8 times larger than the value
 *          returned by H5Tget_size().
 *
 * \since 1.0.0
 *
 */
H5_DLL size_t H5Tget_precision(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the bit offset of the first significant bit
 *
 * \type_id
 *
 * \return Returns an offset value if successful; otherwise returns a
 *         negative value.
 *
 * \details H5Tget_offset() retrieves the bit offset of the first significant
 *          bit. The significant bits of an atomic datum can be offset from the
 *          beginning of the memory for that datum by an amount of padding. The
 *          'offset' property specifies the number of bits of padding that
 *          appear to the "right of" the value. That is, if we have a 32-bit
 *          datum with 16-bits of precision having the value 0x1122 then it
 *          will be laid out in memory as (from small byte address toward
 *          larger byte addresses):
 *          \code{.unparsed}
 *          0:  [ pad]  [0x11]  [0x22]  [ pad]
 *          1:  [ pad]  [0x22]  [0x11]  [ pad]
 *          2:  [0x11]  [ pad]  [ pad]  [0x22]
 *          3:  [0x22]  [ pad]  [ pad]  [0x11]
 *          \endcode
 *
 * \since 1.0.0
 *
 */
H5_DLL int H5Tget_offset(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the padding type of the least and most-significant bit padding
 *
 * \type_id
 * \param[out] lsb Buffer for the least-significant bit padding type
 * \param[out] msb Buffer for the most-significant bit padding type
 *
 * \return \herr_t
 *
 * \details H5Tget_pad() retrieves the padding type of the least and
 *          most-significant bit padding. Valid padding types are:
 *          \snippet this H5T_pad_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tget_pad(hid_t type_id, H5T_pad_t *lsb /*out*/, H5T_pad_t *msb /*out*/);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the sign type for an integer type
 *
 * \type_id
 *
 * \return Returns a valid sign type if successful; otherwise #H5T_SGN_ERROR (-1)
 *
 * \details H5Tget_sign() retrieves the sign type for an integer type.
 *          Valid types are:
 *          \snippet this H5T_sign_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_sign_t H5Tget_sign(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves floating point datatype bit field information
 *
 * \type_id
 * \param[out] spos Pointer to location to return floating-point sign bit
 * \param[out] epos Pointer to location to return exponent bit-position
 * \param[out] esize Pointer to location to return size of exponent in bits
 * \param[out] mpos Pointer to location to return mantissa bit-position
 * \param[out] msize Pointer to location to return size of mantissa in bits
 *
 * \return \herr_t
 *
 * \details H5Tget_fields() retrieves information about the locations of
 *          the various bit fields of a floating point datatype. The field
 *          positions are bit positions in the significant region of the
 *          datatype. Bits are numbered with the least significant bit number
 *          zero. Any (or even all) of the arguments can be null pointers.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tget_fields(hid_t type_id, size_t *spos /*out*/, size_t *epos /*out*/, size_t *esize /*out*/,
                            size_t *mpos /*out*/, size_t *msize /*out*/);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the exponent bias of a floating-point type
 *
 * \type_id
 *
 * \return Returns the bias if successful and 0, otherwise.
 *
 * \details H5Tget_ebias() retrieves the exponent bias of a floating-point type.
 *
 * \since 1.0.0
 *
 */
H5_DLL size_t H5Tget_ebias(hid_t type_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ATOM
 *
 * \brief Retrieves mantissa normalization of a floating-point datatype
 *
 * \type_id
 *
 * \return Returns a valid normalization type if successful; otherwise
 *         returns #H5T_NORM_ERROR (-1)
 *
 * \details H5Tget_norm() retrieves the mantissa normalization of a
 *          floating-point datatype. Valid normalization types are:
 *          \snippet this H5T_norm_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_norm_t H5Tget_norm(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the internal padding type for unused bits in floating-point
 *        datatypes
 *
 * \type_id
 *
 * \return Returns a valid padding type if successful; otherwise returns
 *         #H5T_PAD_ERROR (-1).
 *
 * \details H5Tget_inpad() retrieves the internal padding type for unused
 *          bits in floating-point datatypes. Valid padding types are:
 *          \snippet this H5T_pad_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_pad_t H5Tget_inpad(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the type of padding used for a string datatype
 *
 * \type_id
 *
 * \return Returns a valid string of the padding if successful; otherwise
 *         returns #H5T_STR_ERROR (-1)
 *
 * \details H5Tget_strpad() retrieves the type of padding used for a string
 *          datatype.
 *
 *          The string padding type is set with H5Tset_strpad().  Possible
 *          values returned are:
 * \str_pad_type
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_str_t H5Tget_strpad(hid_t type_id);
/**
 * \ingroup COMPENUM
 *
 * \brief Retrieves the number of elements in a compound or enumeration datatype
 *
 * \type_id
 *
 * \return Returns the number of elements if successful; otherwise returns a
 *         negative value.
 *
 * \details H5Tget_nmembers() retrieves the number of fields in a compound
 *          datatype or the number of members of an enumeration datatype.
 *
 * \since 1.0.0
 *
 */
H5_DLL int H5Tget_nmembers(hid_t type_id);
/**
 * \ingroup COMPENUM
 *
 * \brief Retrieves the name of a compound or enumeration datatype member
 *
 * \type_id
 * \param[in] membno Zero-based index of the field or element
 *
 * \return Returns a valid pointer to a string allocated with malloc() if
 *         successful; otherwise returns NULL.
 *
 * \details H5Tget_member_name() retrieves the name of a field of a compound
 *          datatype or an element of an enumeration datatype.
 *
 *          The index of the target field or element is specified in \p
 *          member_no. Compound datatype fields and enumeration datatype
 *          elements are stored in no particular order with index values of
 *          0 through N-1, where N is the value returned by H5Tget_nmembers().
 *
 *          The HDF5 library allocates a buffer to receive the name of
 *          the field. The caller must subsequently free the buffer with
 *          H5free_memory().
 *
 * \since 1.0.0
 *
 */
H5_DLL char *H5Tget_member_name(hid_t type_id, unsigned membno);
/**
 * \ingroup COMPENUM
 *
 * \brief Retrieves the index of a compound or enumeration datatype member
 *
 * \type_id
 * \param[in] name Name of the field or member
 *
 * \return \herr_t
 *
 * \details H5Tget_member_index() retrieves the index of a field of a compound
 *          datatype or an element of an enumeration datatype.
 *
 *          The name of the target field or element is specified by \p name.
 *
 *          Fields are stored in no particular order with index values of 0
 *          through N-1, where N is the value returned by H5Tget_nmembers() .
 *
 * \since 1.4.0
 *
 */
H5_DLL int H5Tget_member_index(hid_t type_id, const char *name);
/**
 * \ingroup COMPOUND
 *
 * \brief Retrieves the offset of a field of a compound datatype
 *
 * \type_id
 * \param[in] membno Zero-based index of the field or element
 *
 * \return Returns the byte offset of the field if successful; otherwise
 *         returns 0 (zero).
 *
 * \details H5Tget_member_offset() retrieves the byte offset of the beginning
 *          of a field within a compound datatype with respect to the beginning
 *          of the compound datatype datum.
 *
 *          Note that zero is a valid offset and that this function will fail
 *          only if a call to H5Tget_member_class() fails with the same arguments.
 *
 * \version 1.6.4 \p member_no parameter type changed to unsigned.
 *
 * \since 1.0.0
 *
 */
H5_DLL size_t H5Tget_member_offset(hid_t type_id, unsigned membno);
/**
 * \ingroup COMPOUND
 *
 * \brief Returns datatype class of compound datatype member
 *
 * \type_id
 * \param[in] membno Zero-based index of the field or element
 *
 * \return Returns the datatype class, a non-negative value, if successful;
 *         otherwise returns a negative value.
 *
 * \details Given a compound datatype, \p dtype_id, H5Tget_member_class()
 *          returns the datatype class of the member specified by \p member_no.
 *
 *          Valid class identifiers, as defined in H5Tpublic.h, are:
 *          \snippet this H5T_class_t_snip
 *
 * \since 1.4.0
 *
 */
H5_DLL H5T_class_t H5Tget_member_class(hid_t type_id, unsigned membno);
/**
 * \ingroup COMPOUND
 *
 * \brief Returns the datatype of the specified member
 *
 * \type_id
 * \param[in] membno Zero-based index of the field or element
 *
 * \return Returns the identifier of a copy of the datatype of the field if
 *         successful; otherwise returns a negative value.
 *
 * \details H5Tget_member_type() returns the datatype of the specified member.
 *          The caller should invoke H5Tclose() to release resources associated
 *          with the type.
 *
 * \version 1.6.4 \p membno parameter type changed to unsigned.
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Tget_member_type(hid_t type_id, unsigned membno);
/**
 * \ingroup ENUM
 *
 * \brief Returns the value of an enumeration datatype member
 *
 * \type_id
 * \param[in] membno Number of the enumeration datatype member
 * \param[out] value Buffer for the value of the enumeration datatype member
 *
 * \return \herr_t
 *
 * \details H5Tget_member_value() returns the value of the enumeration datatype
 *          member \p member_no.
 *
 *          The member value is returned in a user-supplied buffer pointed to
 *          by \p value. Values returned in \p value will be of the enumerated
 *          type's base type, that is, the datatype used by H5Tenum_create()
 *          when the enumerated type was created.
 *
 *          The value buffer must be at least large enough to hold a value
 *          of that base type. If the size is unknown, you can determine it
 *          with H5Tget_size().
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tget_member_value(hid_t type_id, unsigned membno, void *value /*out*/);
/**
 * \ingroup ATOM
 *
 * \brief Retrieves the character set type of a string datatype
 *
 * \type_id
 *
 * \return Returns a valid character set type if successful; otherwise
 *         #H5T_CSET_ERROR (-1).
 *
 * \details H5Tget_cset() retrieves the character set type of a string datatype.
 *          Valid character set types are:
 *          \csets
 *
 * \since 1.0.0
 *
 */
H5_DLL H5T_cset_t H5Tget_cset(hid_t type_id);
/**
 * \ingroup ATOM
 *
 * \brief Determines whether datatype is a variable-length string
 *
 * \type_id
 *
 * \return Returns:
 *         \li a positive value if the specified datatype is a variable-length
 *             string
 *         \li 0 if the specified datatype is not a variable-length string
 *         \li a negative value when the function fails
 *
 * \details H5Tis_variable_str() determines whether the datatype identified
 *          by \p dtype_id is a variable-length string.
 *
 *          This function can be used to distinguish between fixed and
 *          variable-length string datatypes.
 *
 * \since 1.6.0
 *
 */
H5_DLL htri_t H5Tis_variable_str(hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Returns the native datatype identifier of a specified datatype
 *
 * \type_id
 * \param[in] direction Direction of search
 *
 * \return \hid_t{native datatype}
 *
 * \details H5Tget_native_type() returns the equivalent native datatype
 *          identifier for the datatype specified by \p type_id.
 *
 *          H5Tget_native_type() is designed primarily to facilitate the use of
 *          the H5Dread() function, for which users otherwise must undertake a
 *          multi-step process to determine the native datatype of a dataset
 *          prior to reading it into memory. This function can be used for
 *          the following purposes:
 *
 *          \li To determine the native datatype of an atomic datatype
 *          \li To determine the base datatype of an array, enumerated, or
 *              variable-length datatype
 *          \li To determine the native atomic datatypes of the individual
 *              components of a compound datatype
 *
 *          For example, if \p type_id is a compound datatype, the returned
 *          datatype identifier will be for a similar compound datatype with
 *          each element converted to the corresponding native datatype;
 *          nested compound datatypes will be unwound. If \p type_id is an
 *          array, the returned datatype identifier will be for the native
 *          datatype of a single array element.
 *
 *          H5Tget_native_type() selects the first matching native datatype
 *          from the following list:
 *
 *          \li #H5T_NATIVE_CHAR
 *          \li #H5T_NATIVE_SHORT
 *          \li #H5T_NATIVE_INT
 *          \li #H5T_NATIVE_LONG
 *          \li #H5T_NATIVE_LLONG
 *
 *          \li #H5T_NATIVE_UCHAR
 *          \li #H5T_NATIVE_USHORT
 *          \li #H5T_NATIVE_UINT
 *          \li #H5T_NATIVE_ULONG
 *          \li #H5T_NATIVE_ULLONG
 *
 *          \li #H5T_NATIVE_FLOAT16 (if available)
 *          \li #H5T_NATIVE_FLOAT
 *          \li #H5T_NATIVE_DOUBLE
 *          \li #H5T_NATIVE_LDOUBLE
 *
 *          \li #H5T_NATIVE_B8
 *          \li #H5T_NATIVE_B16
 *          \li #H5T_NATIVE_B32
 *          \li #H5T_NATIVE_B64
 *
 *          The direction parameter indicates the order in which the library
 *          searches for a native datatype match. Valid values for direction
 *          are as follows:
 *          \snippet this H5T_direction_t_snip
 *
 *          H5Tget_native_type() is designed primarily for use with integer,
 *          floating point, and bitfield datatypes. String, time, opaque, and
 *          reference datatypes are returned as a copy of dtype_id. See above
 *          for compound, array, enumerated, and variable-length datatypes.
 *
 *          The identifier returned by H5Tget_native_type() should eventually
 *          be closed by calling H5Tclose() to release resources.
 *
 *          \note Please note that a datatype is actually an object
 *          identifier or handle returned from opening the datatype. It
 *          is not persistent, and its value can be different from one HDF5
 *          session to the next.
 *
 *          \note H5Tequal() can be used to compare datatypes.
 *
 *          \note HDF5 High Level APIs that may also be of interest are: H5LTdtype_to_text()
 *                creates a text description of a datatype. H5LTtext_to_dtype() creates an
 *                HDF5 datatype given a text description.
 *
 * \since 1.6.0
 *
 */
H5_DLL hid_t H5Tget_native_type(hid_t type_id, H5T_direction_t direction);

/* Setting property values */
/**
 * \ingroup H5T
 *
 * \brief Sets size for a datatype.
 *
 * \type_id
 * \param[in] size New datatype size in bytes or #H5T_VARIABLE
 *
 * \return \herr_t
 *
 * \details H5Tset_size() sets the total size, \p size, in bytes, for a
 *          datatype.
 *
 *          \p size must have a positive value unless it is passed in as
 *          #H5T_VARIABLE and the datatype is a string datatype.
 *
 *          \li Numeric datatypes: If the datatype is atomic and the size
 *          is decreased so that significant bits of the datatype extend
 *          beyond the edge of the new size, then the offset property of the
 *          datatype is decreased toward zero.  If the offset becomes zero
 *          and the significant bits of the datatype still hang over the edge
 *          of the new size, then the number of significant bits is decreased.
 *
 *          \li String or character datatypes: The size set for a string
 *          datatype should include space for the null-terminator character,
 *          otherwise it will not be stored on (or retrieved from) the
 *          disk. Adjusting the size of a string automatically sets the
 *          precision to \p 8*size.
 *
 *          \li Variable-length string datatypes: If \p dtype_id is a
 *          variable-length string, size must normally be set to #H5T_VARIABLE.
 *          See \ref_vlen_strings.
 *
 *          \li Compound datatypes: This function may be used to increase or
 *          decrease the size of a compound datatype, but the function will
 *          fail if the new size is too small to accommodate all member fields.
 *
 *          \li Ineligible datatypes: This function cannot be used with
 *          enumerated datatypes (#H5T_ENUM), array datatypes (#H5T_ARRAY),
 *          variable-length array datatypes (#H5T_VLEN), or reference datatypes
 *          (#H5T_REFERENCE).
 *
 * \see H5Tget_size()
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_size(hid_t type_id, size_t size);
/**
 * \ingroup ATOM
 *
 * \brief Sets the byte order of a datatype
 *
 * \type_id
 * \param[in] order Byte order constant
 *
 * \return \herr_t
 *
 * \details H5Tset_order() sets the byte order of a datatype.\n
 *          Byte order can currently be set to any of the following:
 *          \snippet this H5T_order_t_snip
 *          #H5T_ORDER_MIXED (3) is a valid value for order only when
 *          returned by the function H5Tget_order(); it cannot be set with
 *          H5Tset_order().
 *
 *          #H5T_ORDER_NONE (4) is a valid value for order, but it has no
 *          effect. It is valid only for fixed-length strings and object and
 *          region references and specifies “no particular order.”
 *
 *          The byte order of a derived datatype is initially the same as
 *          that of the parent type, but can be changed with H5Tset_order().
 *
 *          This function cannot be used with a datatype after it has been
 *          committed.
 *
 * \note    Special considerations:
 *          \li ENUM datatypes: Byte order must be set before any member on
 *              an ENUM is defined.
 *          \li Compound datatypes: Byte order is set individually on each member
 *              of a compound datatype; members of a compound datatype need not
 *              have the same byte order.
 *          \li Opaque datatypes: Byte order can be set but has no effect.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_order(hid_t type_id, H5T_order_t order);
/**
 * \ingroup ATOM
 *
 * \brief Sets the precision of an atomic datatype
 *
 * \type_id
 * \param[in] prec Number of bits of precision for datatype
 *
 * \return \herr_t
 *
 * \details H5Tset_precision() sets the precision of an atomic datatype. The
 *          precision is the number of significant bits which, unless
 *          padding is present, is 8 times larger than the value returned
 *          by H5Tget_size().
 *
 *          If the precision is increased then the offset is decreased and
 *          then the size is increased to insure that significant bits do not
 *          "hang over" the edge of the datatype.
 *
 *          Changing the precision of an #H5T_STRING automatically changes
 *          the size as well. The precision must be a multiple of 8.
 *
 *          When decreasing the precision of a floating point type, set the
 *          locations and sizes of the sign, mantissa, and exponent fields
 *          first.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_precision(hid_t type_id, size_t prec);
/**
 * \ingroup ATOM
 *
 * \brief Sets the bit offset of the first significant bit
 *
 * \type_id
 * \param[in] offset Offset of first significant bit
 *
 * \return \herr_t
 *
 * \details H5Tset_offset() sets the bit offset of the first significant
 *          bit. The significant bits of an atomic datum can be offset from
 *          the beginning of the memory for that datum by an amount of
 *          padding. The offset property specifies the number of bits of
 *          padding that appear “to the right of” the value. That is,
 *          if we have a 32-bit datum with 16-bits of precision having the
 *          value 0x1122, then it will be laid out in memory as (from small
 *          byte address toward larger byte addresses):
 *          \code{.unparsed}
 *          0:  [ pad]  [0x11]  [0x22]  [ pad]
 *          1:  [ pad]  [0x22]  [0x11]  [ pad]
 *          2:  [0x11]  [ pad]  [ pad]  [0x22]
 *          3:  [0x22]  [ pad]  [ pad]  [0x11]
 *          \endcode
 *          If the offset is incremented then the total size is incremented
 *          also if necessary to prevent significant bits of the value from
 *          hanging over the edge of the datatype.
 *
 *          The offset of an #H5T_STRING cannot be set to anything but zero.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_offset(hid_t type_id, size_t offset);
/**
 * \ingroup ATOM
 *
 * \brief Sets the least and most-significant bits padding types
 *
 * \type_id
 * \param[in] lsb Padding type for least-significant bits
 * \param[in] msb Padding type for most-significant bits
 *
 * \return \herr_t
 *
 * \details H5Tset_pad() sets the least and most-significant bits padding types.
 *          Available values are:
 * \padding_type
 *
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tset_pad(hid_t type_id, H5T_pad_t lsb, H5T_pad_t msb);
/**
 * \ingroup ATOM
 *
 * \brief Sets the sign property for an integer type
 *
 * \type_id
 * \param[in] sign Sign type
 *
 * \return \herr_t
 *
 * \details H5Tset_sign() sets the sign property for an integer type:
 * \sign_prop
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_sign(hid_t type_id, H5T_sign_t sign);
/**
 * \ingroup ATOM
 *
 * \brief Sets locations and sizes of floating point bit fields
 *
 * \type_id
 * \param[in] spos Sign position, i.e., the bit offset of the floating-point
 *                 sign bit
 * \param[in] epos Exponent bit position
 * \param[in] esize Size of exponent in bits
 * \param[in] mpos Mantissa bit position
 * \param[in] msize Size of mantissa in bits
 *
 * \return \herr_t
 *
 * \details H5Tset_fields() sets the locations and sizes of the various
 *          floating-point bit fields. The field positions are bit positions
 *          in the significant region of the datatype. Bits are numbered with
 *          the least significant bit number zero.
 *
 *          Fields are not allowed to extend beyond the number of bits of
 *          precision, nor are they allowed to overlap with one another.
 *
 * \note The size and precision of, as well as any offset for, a floating-point
 *       datatype should generally be set appropriately before calling
 *       H5Tset_fields(). Otherwise, H5Tset_fields() may fail when checking that
 *       the values make sense for the datatype. However, if the precision of a
 *       floating-point datatype will be decreased during its creation with a call
 *       to H5Tset_precision(), then H5Tset_fields() should instead be called
 *       first to set appropriate values for \p spos, \p epos, \p esize, \p mpos
 *       and \p msize before reducing the precision of the datatype with
 *       H5Tset_precision(). This is of particular concern if another floating-point
 *       datatype was copied as a starting point.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_fields(hid_t type_id, size_t spos, size_t epos, size_t esize, size_t mpos, size_t msize);
/**
 * \ingroup ATOM
 *
 * \brief Sets the exponent bias of a floating-point type
 *
 * \type_id
 * \param[in] ebias Exponent bias value
 *
 * \return \herr_t
 *
 * \details H5Tset_ebias() sets the exponent bias of a floating-point type.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_ebias(hid_t type_id, size_t ebias);
/**
 * \ingroup ATOM
 *
 * \brief Sets the mantissa normalization of a floating-point datatype
 *
 * \type_id
 * \param[in] norm Mantissa normalization type
 *
 * \return \herr_t
 *
 * \details H5Tset_norm() sets the mantissa normalization of a floating-point
 *          datatype. Valid normalization types are:
 *          \snippet this H5T_norm_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_norm(hid_t type_id, H5T_norm_t norm);
/**
 * \ingroup ATOM
 *
 * \brief Fills unused internal floating-point bits
 *
 * \type_id
 * \param[in] pad Padding type
 *
 * \return \herr_t
 *
 * \details H5Tset_inpad() If any internal bits of a floating point-type are
 *          unused (that is, those significant bits which are not part of the
 *          sign, exponent, or mantissa), then H5Tset_inpad()  will be filled
 *          according to the value of the padding value property inpad. Valid
 *          padding types are:
 *          \snippet this H5T_pad_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_inpad(hid_t type_id, H5T_pad_t pad);
/**
 * \ingroup ATOM
 *
 * \brief Sets character set to be used in a string or character datatype
 *
 * \type_id
 * \param[in] cset Character set type
 *
 * \return \herr_t
 *
 * \details H5Tset_cset() sets the character set to be used in a dataset with
 *          a string or character datatype.
 *
 *          Valid values for cset include the following:
 *          \csets
 *          For example, if the character set for the datatype \p type_id is set
 *          to #H5T_CSET_UTF8, string or character data of datatype dtype_id
 *          will be encoded using the UTF-8 Unicode character set.
 *
 *          ASCII and UTF-8 Unicode are the only currently supported character
 *          encodings. Extended ASCII encodings (for example, ISO 8859) are
 *          not supported. This encoding policy is not enforced by the HDF5
 *          library. Using encodings other than ASCII and UTF-8 can lead to
 *          compatibility and usability problems.
 *
 *          Note that H5Tset_cset()  sets the character set for a character or
 *          string datatype while H5Pset_char_encoding()  sets the character
 *          set used for an HDF5 link or attribute name.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_cset(hid_t type_id, H5T_cset_t cset);
/**
 * \ingroup ATOM
 *
 * \brief Defines the type of padding used for character strings
 *
 * \type_id
 * \param[in] strpad String padding type
 *
 * \return \herr_t
 *
 * \details H5Tset_strpad() defines the type of padding used for a string
 *          datatype.
 *
 *          The method used to store character strings differs with the
 *          programming language.  C usually null terminates strings while
 *          Fortran left-justifies and space-pads strings.
 *
 *          Valid values of \p strpad are as follows:
 *          \str_pad_type
 *          When converting from a longer string to a shorter string, the
 *          behavior is as follows. If the shorter string is #H5T_STR_NULLPAD
 *          or #H5T_STR_SPACEPAD, then the string is simply truncated. If
 *          the short string is #H5T_STR_NULLTERM, it is truncated and a null
 *          terminator is appended.
 *
 *          When converting from a shorter string to a longer string, the
 *          longer string is padded on the end by appending nulls or spaces.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tset_strpad(hid_t type_id, H5T_str_t strpad);

/**
 * --------------------------------------------------------------------------
 * \ingroup CONV
 *
 * \brief Converts data from one specified datatype to another
 *
 * \type_id{src_id} of source datatype
 * \type_id{dst_id} of destination datatype
 * \param[in] nelmts Size of array \p buf
 * \param[in,out] buf Array containing pre- and post-conversion values
 * \param[in] background Optional background buffer
 * \dxpl_id{plist_id}
 *
 * \return \herr_t
 *
 * \details H5Tconvert() converts \p nelmts elements from a source datatype,
 *          specified by \p src_id, to a destination datatype, \p dst_id. The
 *          source elements are packed in \p buf and on return the destination
 *          elements will be packed in \p buf. That is, the conversion is
 *          performed in place.
 *
 *          The optional background buffer is for use with compound datatypes.
 *          It is an array of \p nelmts values for the destination datatype
 *          which can then be merged with the converted values to recreate the
 *          compound datatype. For instance, background might be an array of
 *          structs with the \c a and \c b fields already initialized and the
 *          conversion of buf supplies the \c c and \c d field values.
 *
 *          The parameter \p plist_id contains the dataset transfer property list
 *          identifier which is passed to the conversion functions. As of
 *          Release 1.2, this parameter is only used to pass along the
 *          variable-length datatype custom allocation information.
 *
 * \note H5Tconvert() will not resize the buffer \p buf; it must be large
 *       enough to hold the larger of the input and output data.
 *
 * \version 1.6.3 \p nelmts parameter type changed to size_t.
 * \version 1.4.0 \p nelmts parameter type changed to hsize_t.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Tconvert(hid_t src_id, hid_t dst_id, size_t nelmts, void *buf, void *background,
                         hid_t plist_id);
/**
 * \ingroup VLEN
 *
 * \brief Reclaims the variable length (VL) datatype memory buffers
 *
 * \type_id
 * \space_id
 * \dxpl_id{plist_id} used to create the buffer
 * \param[in] buf Pointer to the buffer to be reclaimed
 *
 * \return \herr_t
 *
 * \details H5Treclaim() reclaims memory buffers created to store VL datatypes.
 *          It only frees the variable length data in the selection defined in
 *          the dataspace specified by \p space_id. The dataset transfer
 *          property list \p plist_id is required to find the correct
 *          allocation and/or free methods for the variable-length data in the
 *          buffer.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Treclaim(hid_t type_id, hid_t space_id, hid_t plist_id, void *buf);

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */

/* API Wrappers for async routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5T_MODULE
#define H5Tcommit_async(...) H5Tcommit_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Topen_async(...)   H5Topen_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Tclose_async(...)  H5Tclose_async(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 * be passed in by language wrapper or library layer on top of HDF5. */
#define H5Tcommit_async_wrap H5_NO_EXPAND(H5Tcommit_async)
#define H5Topen_async_wrap   H5_NO_EXPAND(H5Topen_async)
#define H5Tclose_async_wrap  H5_NO_EXPAND(H5Tclose_async)
#endif /* H5T_MODULE */

#ifndef H5_NO_DEPRECATED_SYMBOLS

/* Macros */

/* Typedefs */

/* Function prototypes */
/**
 * \ingroup H5T
 *
 * \brief Commits a transient datatype to a file, creating a newly named datatype
 *
 * \fg_loc_id
 * \param[in] name Name given to committed datatype
 * \param[in] type_id Identifier of datatype to be committed
 *
 * \return \herr_t
 *
 * \deprecated This function has been renamed from H5Tcommit() and is
 *             deprecated in favor of the macro #H5Tcommit or the function
 *             H5Tcommit2().
 *
 * \details H5Tcommit1() commits the transient datatype (not immutable) to
 *          a file, turning it into a named datatype.
 *
 *          The datatype \p dtype_id is committed as a named datatype at the
 *          location \p loc_id, which is either a file or group identifier,
 *          with the name \p name.
 *
 *          \p name can be a relative path based at \p loc_id or an absolute
 *          path from the root of the file. Use of this function requires
 *          that any intermediate groups specified in the path already exist.
 *
 *          As is the case for any object in a group, the length of the name
 *          of a named datatype is not limited.
 *
 *          See H5Tcommit_anon() for a discussion of the differences between
 *          H5Tcommit() and H5Tcommit_anon().
 *
 *          This function will not accept a datatype that cannot actually
 *          hold data. This currently includes compound datatypes with no
 *          fields and enumerated datatypes with no members.
 *
 * \version 1.8.7 Function modified in this release to reject datatypes that
 *          will not accommodate actual data, such as a compound datatype with
 *          no fields or an enumerated datatype with no members.
 * \version 1.8.0 C function H5Tcommit() renamed to H5Tcommit1() and deprecated
 *          in this release.
 * \since 1.2.0
 *
 */
H5_DLL herr_t H5Tcommit1(hid_t loc_id, const char *name, hid_t type_id);
/**
 * \ingroup H5T
 *
 * \brief Opens a named datatype
 *
 * \fg_loc_id
 * \param[in] name A datatype name, defined within the specified file or group
 *
 * \return \herr_t
 *
 * \deprecated This function has been renamed from H5Topen() and is
 *             deprecated in favor of the macro #H5Topen or the function
 *             H5Topen2().
 *
 * \details H5Topen1() opens a named datatype at the location specified by
 *          \p loc_id and returns an identifier for the datatype. \p loc_id
 *          can be either a file or group identifier. The identifier should
 *          eventually be closed by calling H5Tclose()  to release resources.
 *
 * \version 1.8.0 Function H5Topen() renamed to H5Topen1() and deprecated in
 *          this release.
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Topen1(hid_t loc_id, const char *name);
/**
 * \ingroup ARRAY
 *
 * \brief Creates an array datatype object
 *
 * \param[in] base_id Datatype identifier for the array base datatype
 * \param[in] ndims Rank of the array
 * \param[in] dim Size of each array dimension
 * \param[in] perm Dimension permutation   (Currently not implemented.)
 *
 * \return \hid_t{array datatype}
 *
 * \deprecated This function has been renamed from H5Tarray_create() and is
 *             deprecated in favor of the macro #H5Tarray_create or the function
 *             H5Tarray_create2().
 *
 * \details H5Tarray_create1() creates a new array datatype object.\n\n
 *          \p base_id is the datatype of every element of the array, i.e.,
 *          of the number at each position in the array.
 *
 *          \p rank is the number of dimensions and the size of each dimension
 *          is specified in the array dims. The value of rank is currently
 *          limited to #H5S_MAX_RANK and must be greater than 0 (zero). All
 *          dimension sizes specified in dims must be greater than 0 (zero).
 *
 *          The array \p perm is designed to contain the dimension permutation,
 *          i.e. C versus FORTRAN array order.   (The parameter perm is
 *          currently unused and is not yet implemented.)
 *
 * \version 1.8.0 Function H5Tarray_create() renamed to H5Tarray_create1()
 *          and deprecated in this release.
 *
 * \since 1.4.0
 *
 */
H5_DLL hid_t H5Tarray_create1(hid_t base_id, int ndims, const hsize_t dim[/* ndims */],
                              const int perm[/* ndims */]);
/**
 * \ingroup ARRAY
 *
 * \brief Retrieves sizes of array dimensions
 *
 * \type_id
 * \param[out] dims Sizes of array dimensions
 * \param[out] perm Dimension permutations (This parameter is not used.)
 *
 * \return Returns the non-negative number of dimensions of the array type
 *         if successful; otherwise, returns a negative value.
 *
 * \deprecated This function has been renamed from H5Tget_array_dims() and is
 *             deprecated in favor of the macro #H5Tget_array_dims or the
 *             function H5Tget_array_dims2().
 *
 * \details H5Tget_array_dims1() returns the sizes of the dimensions and
 *          the dimension permutations of the specified array datatype object.
 *
 *          The sizes of the dimensions are returned in the array \p dims.
 *
 * \version 1.8.0 Function H5Tarray_create() renamed to H5Tarray_create1()
 *          and deprecated in this release.
 *
 * \since 1.4.0
 *
 */
H5_DLL int H5Tget_array_dims1(hid_t type_id, hsize_t dims[], int perm[]);

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#ifdef __cplusplus
}
#endif
#endif /* H5Tpublic_H */
