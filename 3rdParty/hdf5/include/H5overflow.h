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

/* Generated automatically by bin/make_overflow -- do not edit */
/* Add new types to H5overflow.txt file */


#ifndef H5overflow_H
#define H5overflow_H


/* Each type in this file is tested for assignment to the other types,
 *      and range checks are defined for bad assignments at run-time.
 */

/* Assignment checks for unsigned */

/* src: unsigned, dst: int8_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_INT8_T
    #define ASSIGN_unsigned_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_INT8_T
    #define ASSIGN_unsigned_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_INT8_T */
    #define ASSIGN_unsigned_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: int8_t */

/* src: unsigned, dst: int */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_INT
    #define ASSIGN_unsigned_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_INT
    #define ASSIGN_unsigned_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_INT */
    #define ASSIGN_unsigned_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: int */

/* src: unsigned, dst: long */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_LONG
    #define ASSIGN_unsigned_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_LONG
    #define ASSIGN_unsigned_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_LONG */
    #define ASSIGN_unsigned_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: long */

/* src: unsigned, dst: int64_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_INT64_T
    #define ASSIGN_unsigned_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_INT64_T
    #define ASSIGN_unsigned_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_INT64_T */
    #define ASSIGN_unsigned_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: int64_t */

/* src: unsigned, dst: uint8_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_UINT8_T
    #define ASSIGN_unsigned_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_UINT8_T
    #define ASSIGN_unsigned_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_UINT8_T */
    #define ASSIGN_unsigned_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: uint8_t */

/* src: unsigned, dst: uint16_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_UINT16_T
    #define ASSIGN_unsigned_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_UINT16_T
    #define ASSIGN_unsigned_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_UINT16_T */
    #define ASSIGN_unsigned_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: uint16_t */

/* src: unsigned, dst: uint32_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_UINT32_T
    #define ASSIGN_unsigned_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_UINT32_T
    #define ASSIGN_unsigned_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_UINT32_T */
    #define ASSIGN_unsigned_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: uint32_t */

/* src: unsigned, dst: uint64_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_UINT64_T
    #define ASSIGN_unsigned_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_UINT64_T
    #define ASSIGN_unsigned_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_UINT64_T */
    #define ASSIGN_unsigned_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: uint64_t */

/* src: unsigned, dst: ptrdiff_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_unsigned_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_unsigned_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_unsigned_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: ptrdiff_t */

/* src: unsigned, dst: size_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_SIZE_T
    #define ASSIGN_unsigned_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_SIZE_T
    #define ASSIGN_unsigned_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_SIZE_T */
    #define ASSIGN_unsigned_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: size_t */

/* src: unsigned, dst: ssize_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_SSIZE_T
    #define ASSIGN_unsigned_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_SSIZE_T
    #define ASSIGN_unsigned_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_unsigned_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: ssize_t */

/* src: unsigned, dst: haddr_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_HADDR_T
    #define ASSIGN_unsigned_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_HADDR_T
    #define ASSIGN_unsigned_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_HADDR_T */
    #define ASSIGN_unsigned_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: haddr_t */

/* src: unsigned, dst: hsize_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_HSIZE_T
    #define ASSIGN_unsigned_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_HSIZE_T
    #define ASSIGN_unsigned_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_unsigned_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: hsize_t */

/* src: unsigned, dst: hssize_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_unsigned_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_unsigned_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_unsigned_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: hssize_t */

/* src: unsigned, dst: h5_stat_size_t */
#if H5_SIZEOF_UNSIGNED < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_unsigned_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UNSIGNED > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_unsigned_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UNSIGNED == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_unsigned_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: unsigned dst: h5_stat_size_t */


/* Assignment checks for int8_t */

/* src: int8_t, dst: unsigned */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_int8_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_int8_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_int8_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: unsigned */

/* src: int8_t, dst: int */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_INT
    #define ASSIGN_int8_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_INT
    #define ASSIGN_int8_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_INT */
    #define ASSIGN_int8_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: int */

/* src: int8_t, dst: long */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_LONG
    #define ASSIGN_int8_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_LONG
    #define ASSIGN_int8_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_LONG */
    #define ASSIGN_int8_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: long */

/* src: int8_t, dst: int64_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_INT64_T
    #define ASSIGN_int8_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_INT64_T
    #define ASSIGN_int8_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_int8_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: int64_t */

/* src: int8_t, dst: uint8_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_int8_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_int8_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_int8_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: uint8_t */

/* src: int8_t, dst: uint16_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_int8_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_int8_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_int8_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: uint16_t */

/* src: int8_t, dst: uint32_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_int8_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_int8_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_int8_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: uint32_t */

/* src: int8_t, dst: uint64_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_int8_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_int8_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_int8_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: uint64_t */

/* src: int8_t, dst: ptrdiff_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_int8_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_int8_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_int8_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: ptrdiff_t */

/* src: int8_t, dst: size_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_int8_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_int8_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_int8_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: size_t */

/* src: int8_t, dst: ssize_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_int8_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_int8_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_int8_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: ssize_t */

/* src: int8_t, dst: haddr_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_int8_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_int8_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_int8_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: haddr_t */

/* src: int8_t, dst: hsize_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_int8_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_int8_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_int8_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: hsize_t */

/* src: int8_t, dst: hssize_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_int8_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_int8_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_int8_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: hssize_t */

/* src: int8_t, dst: h5_stat_size_t */
#if H5_SIZEOF_INT8_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_int8_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT8_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_int8_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT8_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_int8_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int8_t dst: h5_stat_size_t */


/* Assignment checks for int */

/* src: int, dst: unsigned */
#if H5_SIZEOF_INT < H5_SIZEOF_UNSIGNED
    #define ASSIGN_int_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_UNSIGNED
    #define ASSIGN_int_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_int_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: unsigned */

/* src: int, dst: int8_t */
#if H5_SIZEOF_INT < H5_SIZEOF_INT8_T
    #define ASSIGN_int_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_INT8_T
    #define ASSIGN_int_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_INT8_T */
    #define ASSIGN_int_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: int8_t */

/* src: int, dst: long */
#if H5_SIZEOF_INT < H5_SIZEOF_LONG
    #define ASSIGN_int_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_LONG
    #define ASSIGN_int_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_LONG */
    #define ASSIGN_int_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: long */

/* src: int, dst: int64_t */
#if H5_SIZEOF_INT < H5_SIZEOF_INT64_T
    #define ASSIGN_int_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_INT64_T
    #define ASSIGN_int_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_INT64_T */
    #define ASSIGN_int_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: int64_t */

/* src: int, dst: uint8_t */
#if H5_SIZEOF_INT < H5_SIZEOF_UINT8_T
    #define ASSIGN_int_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_UINT8_T
    #define ASSIGN_int_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_UINT8_T */
    #define ASSIGN_int_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: uint8_t */

/* src: int, dst: uint16_t */
#if H5_SIZEOF_INT < H5_SIZEOF_UINT16_T
    #define ASSIGN_int_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_UINT16_T
    #define ASSIGN_int_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_UINT16_T */
    #define ASSIGN_int_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: uint16_t */

/* src: int, dst: uint32_t */
#if H5_SIZEOF_INT < H5_SIZEOF_UINT32_T
    #define ASSIGN_int_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_UINT32_T
    #define ASSIGN_int_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_UINT32_T */
    #define ASSIGN_int_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: uint32_t */

/* src: int, dst: uint64_t */
#if H5_SIZEOF_INT < H5_SIZEOF_UINT64_T
    #define ASSIGN_int_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_UINT64_T
    #define ASSIGN_int_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_UINT64_T */
    #define ASSIGN_int_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: uint64_t */

/* src: int, dst: ptrdiff_t */
#if H5_SIZEOF_INT < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_int_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_int_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_int_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: ptrdiff_t */

/* src: int, dst: size_t */
#if H5_SIZEOF_INT < H5_SIZEOF_SIZE_T
    #define ASSIGN_int_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_SIZE_T
    #define ASSIGN_int_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_SIZE_T */
    #define ASSIGN_int_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: size_t */

/* src: int, dst: ssize_t */
#if H5_SIZEOF_INT < H5_SIZEOF_SSIZE_T
    #define ASSIGN_int_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_SSIZE_T
    #define ASSIGN_int_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_int_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: ssize_t */

/* src: int, dst: haddr_t */
#if H5_SIZEOF_INT < H5_SIZEOF_HADDR_T
    #define ASSIGN_int_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_HADDR_T
    #define ASSIGN_int_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_HADDR_T */
    #define ASSIGN_int_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: haddr_t */

/* src: int, dst: hsize_t */
#if H5_SIZEOF_INT < H5_SIZEOF_HSIZE_T
    #define ASSIGN_int_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_HSIZE_T
    #define ASSIGN_int_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_int_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: hsize_t */

/* src: int, dst: hssize_t */
#if H5_SIZEOF_INT < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_int_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_int_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_int_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: hssize_t */

/* src: int, dst: h5_stat_size_t */
#if H5_SIZEOF_INT < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_int_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_int_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_int_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int dst: h5_stat_size_t */


/* Assignment checks for long */

/* src: long, dst: unsigned */
#if H5_SIZEOF_LONG < H5_SIZEOF_UNSIGNED
    #define ASSIGN_long_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_UNSIGNED
    #define ASSIGN_long_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_long_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: unsigned */

/* src: long, dst: int8_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_INT8_T
    #define ASSIGN_long_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_INT8_T
    #define ASSIGN_long_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_INT8_T */
    #define ASSIGN_long_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: int8_t */

/* src: long, dst: int */
#if H5_SIZEOF_LONG < H5_SIZEOF_INT
    #define ASSIGN_long_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_INT
    #define ASSIGN_long_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_INT */
    #define ASSIGN_long_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: int */

/* src: long, dst: int64_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_INT64_T
    #define ASSIGN_long_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_INT64_T
    #define ASSIGN_long_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_INT64_T */
    #define ASSIGN_long_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: int64_t */

/* src: long, dst: uint8_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_UINT8_T
    #define ASSIGN_long_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_UINT8_T
    #define ASSIGN_long_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_UINT8_T */
    #define ASSIGN_long_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: uint8_t */

/* src: long, dst: uint16_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_UINT16_T
    #define ASSIGN_long_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_UINT16_T
    #define ASSIGN_long_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_UINT16_T */
    #define ASSIGN_long_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: uint16_t */

/* src: long, dst: uint32_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_UINT32_T
    #define ASSIGN_long_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_UINT32_T
    #define ASSIGN_long_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_UINT32_T */
    #define ASSIGN_long_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: uint32_t */

/* src: long, dst: uint64_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_UINT64_T
    #define ASSIGN_long_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_UINT64_T
    #define ASSIGN_long_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_UINT64_T */
    #define ASSIGN_long_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: uint64_t */

/* src: long, dst: ptrdiff_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_long_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_long_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_long_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: ptrdiff_t */

/* src: long, dst: size_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_SIZE_T
    #define ASSIGN_long_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_SIZE_T
    #define ASSIGN_long_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_SIZE_T */
    #define ASSIGN_long_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: size_t */

/* src: long, dst: ssize_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_SSIZE_T
    #define ASSIGN_long_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_SSIZE_T
    #define ASSIGN_long_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_long_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: ssize_t */

/* src: long, dst: haddr_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_HADDR_T
    #define ASSIGN_long_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_HADDR_T
    #define ASSIGN_long_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_HADDR_T */
    #define ASSIGN_long_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: haddr_t */

/* src: long, dst: hsize_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_HSIZE_T
    #define ASSIGN_long_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_HSIZE_T
    #define ASSIGN_long_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_long_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: hsize_t */

/* src: long, dst: hssize_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_long_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_long_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_long_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: hssize_t */

/* src: long, dst: h5_stat_size_t */
#if H5_SIZEOF_LONG < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_long_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_LONG > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_long_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_LONG == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_long_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: long dst: h5_stat_size_t */


/* Assignment checks for int64_t */

/* src: int64_t, dst: unsigned */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_int64_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_int64_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_int64_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: unsigned */

/* src: int64_t, dst: int8_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_INT8_T
    #define ASSIGN_int64_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_INT8_T
    #define ASSIGN_int64_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_int64_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: int8_t */

/* src: int64_t, dst: int */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_INT
    #define ASSIGN_int64_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_INT
    #define ASSIGN_int64_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_INT */
    #define ASSIGN_int64_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: int */

/* src: int64_t, dst: long */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_LONG
    #define ASSIGN_int64_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_LONG
    #define ASSIGN_int64_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_LONG */
    #define ASSIGN_int64_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: long */

/* src: int64_t, dst: uint8_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_int64_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_int64_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_int64_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: uint8_t */

/* src: int64_t, dst: uint16_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_int64_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_int64_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_int64_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: uint16_t */

/* src: int64_t, dst: uint32_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_int64_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_int64_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_int64_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: uint32_t */

/* src: int64_t, dst: uint64_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_int64_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_int64_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_int64_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: uint64_t */

/* src: int64_t, dst: ptrdiff_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_int64_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_int64_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_int64_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: ptrdiff_t */

/* src: int64_t, dst: size_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_int64_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_int64_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_int64_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: size_t */

/* src: int64_t, dst: ssize_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_int64_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_int64_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_int64_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: ssize_t */

/* src: int64_t, dst: haddr_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_int64_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_int64_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_int64_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: haddr_t */

/* src: int64_t, dst: hsize_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_int64_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_int64_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_int64_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: hsize_t */

/* src: int64_t, dst: hssize_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_int64_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_int64_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_int64_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: hssize_t */

/* src: int64_t, dst: h5_stat_size_t */
#if H5_SIZEOF_INT64_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_int64_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_INT64_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_int64_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_INT64_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_int64_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: int64_t dst: h5_stat_size_t */


/* Assignment checks for uint8_t */

/* src: uint8_t, dst: unsigned */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint8_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint8_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_uint8_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: unsigned */

/* src: uint8_t, dst: int8_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_INT8_T
    #define ASSIGN_uint8_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_INT8_T
    #define ASSIGN_uint8_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_uint8_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: int8_t */

/* src: uint8_t, dst: int */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_INT
    #define ASSIGN_uint8_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_INT
    #define ASSIGN_uint8_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_INT */
    #define ASSIGN_uint8_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: int */

/* src: uint8_t, dst: long */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_LONG
    #define ASSIGN_uint8_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_LONG
    #define ASSIGN_uint8_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_LONG */
    #define ASSIGN_uint8_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: long */

/* src: uint8_t, dst: int64_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_INT64_T
    #define ASSIGN_uint8_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_INT64_T
    #define ASSIGN_uint8_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_uint8_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: int64_t */

/* src: uint8_t, dst: uint16_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_uint8_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_uint8_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_uint8_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: uint16_t */

/* src: uint8_t, dst: uint32_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_uint8_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_uint8_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_uint8_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: uint32_t */

/* src: uint8_t, dst: uint64_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_uint8_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_uint8_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_uint8_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: uint64_t */

/* src: uint8_t, dst: ptrdiff_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint8_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint8_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_uint8_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: ptrdiff_t */

/* src: uint8_t, dst: size_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_uint8_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_uint8_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_uint8_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: size_t */

/* src: uint8_t, dst: ssize_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint8_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint8_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_uint8_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: ssize_t */

/* src: uint8_t, dst: haddr_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_uint8_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_uint8_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_uint8_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: haddr_t */

/* src: uint8_t, dst: hsize_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint8_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint8_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_uint8_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: hsize_t */

/* src: uint8_t, dst: hssize_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint8_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint8_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_uint8_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: hssize_t */

/* src: uint8_t, dst: h5_stat_size_t */
#if H5_SIZEOF_UINT8_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint8_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT8_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint8_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT8_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_uint8_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint8_t dst: h5_stat_size_t */


/* Assignment checks for uint16_t */

/* src: uint16_t, dst: unsigned */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint16_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint16_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_uint16_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: unsigned */

/* src: uint16_t, dst: int8_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_INT8_T
    #define ASSIGN_uint16_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_INT8_T
    #define ASSIGN_uint16_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_uint16_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: int8_t */

/* src: uint16_t, dst: int */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_INT
    #define ASSIGN_uint16_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_INT
    #define ASSIGN_uint16_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_INT */
    #define ASSIGN_uint16_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: int */

/* src: uint16_t, dst: long */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_LONG
    #define ASSIGN_uint16_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_LONG
    #define ASSIGN_uint16_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_LONG */
    #define ASSIGN_uint16_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: long */

/* src: uint16_t, dst: int64_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_INT64_T
    #define ASSIGN_uint16_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_INT64_T
    #define ASSIGN_uint16_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_uint16_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: int64_t */

/* src: uint16_t, dst: uint8_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_uint16_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_uint16_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_uint16_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: uint8_t */

/* src: uint16_t, dst: uint32_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_uint16_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_uint16_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_uint16_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: uint32_t */

/* src: uint16_t, dst: uint64_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_uint16_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_uint16_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_uint16_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: uint64_t */

/* src: uint16_t, dst: ptrdiff_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint16_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint16_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_uint16_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: ptrdiff_t */

/* src: uint16_t, dst: size_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_uint16_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_uint16_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_uint16_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: size_t */

/* src: uint16_t, dst: ssize_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint16_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint16_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_uint16_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: ssize_t */

/* src: uint16_t, dst: haddr_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_uint16_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_uint16_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_uint16_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: haddr_t */

/* src: uint16_t, dst: hsize_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint16_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint16_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_uint16_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: hsize_t */

/* src: uint16_t, dst: hssize_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint16_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint16_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_uint16_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: hssize_t */

/* src: uint16_t, dst: h5_stat_size_t */
#if H5_SIZEOF_UINT16_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint16_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT16_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint16_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT16_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_uint16_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint16_t dst: h5_stat_size_t */


/* Assignment checks for uint32_t */

/* src: uint32_t, dst: unsigned */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint32_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint32_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_uint32_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: unsigned */

/* src: uint32_t, dst: int8_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_INT8_T
    #define ASSIGN_uint32_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_INT8_T
    #define ASSIGN_uint32_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_uint32_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: int8_t */

/* src: uint32_t, dst: int */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_INT
    #define ASSIGN_uint32_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_INT
    #define ASSIGN_uint32_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_INT */
    #define ASSIGN_uint32_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: int */

/* src: uint32_t, dst: long */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_LONG
    #define ASSIGN_uint32_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_LONG
    #define ASSIGN_uint32_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_LONG */
    #define ASSIGN_uint32_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: long */

/* src: uint32_t, dst: int64_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_INT64_T
    #define ASSIGN_uint32_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_INT64_T
    #define ASSIGN_uint32_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_uint32_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: int64_t */

/* src: uint32_t, dst: uint8_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_uint32_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_uint32_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_uint32_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: uint8_t */

/* src: uint32_t, dst: uint16_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_uint32_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_uint32_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_uint32_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: uint16_t */

/* src: uint32_t, dst: uint64_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_uint32_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_uint32_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_uint32_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: uint64_t */

/* src: uint32_t, dst: ptrdiff_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint32_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint32_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_uint32_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: ptrdiff_t */

/* src: uint32_t, dst: size_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_uint32_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_uint32_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_uint32_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: size_t */

/* src: uint32_t, dst: ssize_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint32_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint32_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_uint32_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: ssize_t */

/* src: uint32_t, dst: haddr_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_uint32_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_uint32_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_uint32_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: haddr_t */

/* src: uint32_t, dst: hsize_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint32_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint32_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_uint32_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: hsize_t */

/* src: uint32_t, dst: hssize_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint32_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint32_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_uint32_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: hssize_t */

/* src: uint32_t, dst: h5_stat_size_t */
#if H5_SIZEOF_UINT32_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint32_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT32_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint32_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT32_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_uint32_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint32_t dst: h5_stat_size_t */


/* Assignment checks for uint64_t */

/* src: uint64_t, dst: unsigned */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint64_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_uint64_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_uint64_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: unsigned */

/* src: uint64_t, dst: int8_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_INT8_T
    #define ASSIGN_uint64_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_INT8_T
    #define ASSIGN_uint64_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_uint64_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: int8_t */

/* src: uint64_t, dst: int */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_INT
    #define ASSIGN_uint64_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_INT
    #define ASSIGN_uint64_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_INT */
    #define ASSIGN_uint64_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: int */

/* src: uint64_t, dst: long */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_LONG
    #define ASSIGN_uint64_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_LONG
    #define ASSIGN_uint64_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_LONG */
    #define ASSIGN_uint64_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: long */

/* src: uint64_t, dst: int64_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_INT64_T
    #define ASSIGN_uint64_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_INT64_T
    #define ASSIGN_uint64_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_uint64_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: int64_t */

/* src: uint64_t, dst: uint8_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_uint64_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_uint64_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_uint64_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: uint8_t */

/* src: uint64_t, dst: uint16_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_uint64_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_uint64_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_uint64_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: uint16_t */

/* src: uint64_t, dst: uint32_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_uint64_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_uint64_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_uint64_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: uint32_t */

/* src: uint64_t, dst: ptrdiff_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint64_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_uint64_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_uint64_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: ptrdiff_t */

/* src: uint64_t, dst: size_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_uint64_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_uint64_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_uint64_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: size_t */

/* src: uint64_t, dst: ssize_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint64_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_uint64_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_uint64_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: ssize_t */

/* src: uint64_t, dst: haddr_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_uint64_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_uint64_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_uint64_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: haddr_t */

/* src: uint64_t, dst: hsize_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint64_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_uint64_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_uint64_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: hsize_t */

/* src: uint64_t, dst: hssize_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint64_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_uint64_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_uint64_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: hssize_t */

/* src: uint64_t, dst: h5_stat_size_t */
#if H5_SIZEOF_UINT64_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint64_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_UINT64_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_uint64_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_UINT64_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_uint64_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: uint64_t dst: h5_stat_size_t */


/* Assignment checks for ptrdiff_t */

/* src: ptrdiff_t, dst: unsigned */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_ptrdiff_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_ptrdiff_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_ptrdiff_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: unsigned */

/* src: ptrdiff_t, dst: int8_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_INT8_T
    #define ASSIGN_ptrdiff_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_INT8_T
    #define ASSIGN_ptrdiff_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_ptrdiff_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: int8_t */

/* src: ptrdiff_t, dst: int */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_INT
    #define ASSIGN_ptrdiff_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_INT
    #define ASSIGN_ptrdiff_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_INT */
    #define ASSIGN_ptrdiff_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: int */

/* src: ptrdiff_t, dst: long */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_LONG
    #define ASSIGN_ptrdiff_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_LONG
    #define ASSIGN_ptrdiff_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_LONG */
    #define ASSIGN_ptrdiff_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: long */

/* src: ptrdiff_t, dst: int64_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_INT64_T
    #define ASSIGN_ptrdiff_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_INT64_T
    #define ASSIGN_ptrdiff_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_ptrdiff_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: int64_t */

/* src: ptrdiff_t, dst: uint8_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_ptrdiff_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_ptrdiff_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_ptrdiff_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: uint8_t */

/* src: ptrdiff_t, dst: uint16_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_ptrdiff_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_ptrdiff_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_ptrdiff_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: uint16_t */

/* src: ptrdiff_t, dst: uint32_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_ptrdiff_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_ptrdiff_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_ptrdiff_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: uint32_t */

/* src: ptrdiff_t, dst: uint64_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_ptrdiff_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_ptrdiff_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_ptrdiff_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: uint64_t */

/* src: ptrdiff_t, dst: size_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_ptrdiff_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_ptrdiff_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_ptrdiff_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: size_t */

/* src: ptrdiff_t, dst: ssize_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_ptrdiff_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_ptrdiff_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_ptrdiff_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: ssize_t */

/* src: ptrdiff_t, dst: haddr_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_ptrdiff_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_ptrdiff_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_ptrdiff_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: haddr_t */

/* src: ptrdiff_t, dst: hsize_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_ptrdiff_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_ptrdiff_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_ptrdiff_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: hsize_t */

/* src: ptrdiff_t, dst: hssize_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_ptrdiff_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_ptrdiff_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_ptrdiff_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: hssize_t */

/* src: ptrdiff_t, dst: h5_stat_size_t */
#if H5_SIZEOF_PTRDIFF_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_ptrdiff_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_PTRDIFF_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_ptrdiff_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_PTRDIFF_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_ptrdiff_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ptrdiff_t dst: h5_stat_size_t */


/* Assignment checks for size_t */

/* src: size_t, dst: unsigned */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_size_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_size_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_size_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: unsigned */

/* src: size_t, dst: int8_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_INT8_T
    #define ASSIGN_size_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_INT8_T
    #define ASSIGN_size_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_size_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: int8_t */

/* src: size_t, dst: int */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_INT
    #define ASSIGN_size_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_INT
    #define ASSIGN_size_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_INT */
    #define ASSIGN_size_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: int */

/* src: size_t, dst: long */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_LONG
    #define ASSIGN_size_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_LONG
    #define ASSIGN_size_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_LONG */
    #define ASSIGN_size_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: long */

/* src: size_t, dst: int64_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_INT64_T
    #define ASSIGN_size_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_INT64_T
    #define ASSIGN_size_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_size_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: int64_t */

/* src: size_t, dst: uint8_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_size_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_size_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_size_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: uint8_t */

/* src: size_t, dst: uint16_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_size_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_size_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_size_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: uint16_t */

/* src: size_t, dst: uint32_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_size_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_size_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_size_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: uint32_t */

/* src: size_t, dst: uint64_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_size_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_size_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_size_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: uint64_t */

/* src: size_t, dst: ptrdiff_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_size_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_size_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_size_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: ptrdiff_t */

/* src: size_t, dst: ssize_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_size_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_size_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_size_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: ssize_t */

/* src: size_t, dst: haddr_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_size_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_size_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_size_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: haddr_t */

/* src: size_t, dst: hsize_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_size_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_size_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_size_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: hsize_t */

/* src: size_t, dst: hssize_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_size_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_size_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_size_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: hssize_t */

/* src: size_t, dst: h5_stat_size_t */
#if H5_SIZEOF_SIZE_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_size_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SIZE_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_size_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SIZE_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_size_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: size_t dst: h5_stat_size_t */


/* Assignment checks for ssize_t */

/* src: ssize_t, dst: unsigned */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_ssize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_ssize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_ssize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: unsigned */

/* src: ssize_t, dst: int8_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_INT8_T
    #define ASSIGN_ssize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_INT8_T
    #define ASSIGN_ssize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_ssize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: int8_t */

/* src: ssize_t, dst: int */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_INT
    #define ASSIGN_ssize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_INT
    #define ASSIGN_ssize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_INT */
    #define ASSIGN_ssize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: int */

/* src: ssize_t, dst: long */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_LONG
    #define ASSIGN_ssize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_LONG
    #define ASSIGN_ssize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_LONG */
    #define ASSIGN_ssize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: long */

/* src: ssize_t, dst: int64_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_INT64_T
    #define ASSIGN_ssize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_INT64_T
    #define ASSIGN_ssize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_ssize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: int64_t */

/* src: ssize_t, dst: uint8_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_ssize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_ssize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_ssize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: uint8_t */

/* src: ssize_t, dst: uint16_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_ssize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_ssize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_ssize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: uint16_t */

/* src: ssize_t, dst: uint32_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_ssize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_ssize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_ssize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: uint32_t */

/* src: ssize_t, dst: uint64_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_ssize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_ssize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_ssize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: uint64_t */

/* src: ssize_t, dst: ptrdiff_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_ssize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_ssize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_ssize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: ptrdiff_t */

/* src: ssize_t, dst: size_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_ssize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_ssize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_ssize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: size_t */

/* src: ssize_t, dst: haddr_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_ssize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_ssize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_ssize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: haddr_t */

/* src: ssize_t, dst: hsize_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_ssize_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_ssize_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_ssize_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: hsize_t */

/* src: ssize_t, dst: hssize_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_ssize_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_ssize_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_ssize_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: hssize_t */

/* src: ssize_t, dst: h5_stat_size_t */
#if H5_SIZEOF_SSIZE_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_ssize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_SSIZE_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_ssize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_SSIZE_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_ssize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: ssize_t dst: h5_stat_size_t */


/* Assignment checks for haddr_t */

/* src: haddr_t, dst: unsigned */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_haddr_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_haddr_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_haddr_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: unsigned */

/* src: haddr_t, dst: int8_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_INT8_T
    #define ASSIGN_haddr_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_INT8_T
    #define ASSIGN_haddr_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_haddr_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: int8_t */

/* src: haddr_t, dst: int */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_INT
    #define ASSIGN_haddr_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_INT
    #define ASSIGN_haddr_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_INT */
    #define ASSIGN_haddr_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: int */

/* src: haddr_t, dst: long */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_LONG
    #define ASSIGN_haddr_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_LONG
    #define ASSIGN_haddr_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_LONG */
    #define ASSIGN_haddr_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: long */

/* src: haddr_t, dst: int64_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_INT64_T
    #define ASSIGN_haddr_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_INT64_T
    #define ASSIGN_haddr_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_haddr_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: int64_t */

/* src: haddr_t, dst: uint8_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_haddr_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_haddr_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_haddr_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: uint8_t */

/* src: haddr_t, dst: uint16_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_haddr_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_haddr_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_haddr_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: uint16_t */

/* src: haddr_t, dst: uint32_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_haddr_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_haddr_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_haddr_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: uint32_t */

/* src: haddr_t, dst: uint64_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_haddr_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_haddr_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_haddr_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: uint64_t */

/* src: haddr_t, dst: ptrdiff_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_haddr_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_haddr_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_haddr_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: ptrdiff_t */

/* src: haddr_t, dst: size_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_haddr_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_haddr_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_haddr_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: size_t */

/* src: haddr_t, dst: ssize_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_haddr_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_haddr_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_haddr_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: ssize_t */

/* src: haddr_t, dst: hsize_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_haddr_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_haddr_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_haddr_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: hsize_t */

/* src: haddr_t, dst: hssize_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_haddr_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_haddr_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_haddr_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: hssize_t */

/* src: haddr_t, dst: h5_stat_size_t */
#if H5_SIZEOF_HADDR_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_haddr_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HADDR_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_haddr_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HADDR_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_haddr_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: haddr_t dst: h5_stat_size_t */


/* Assignment checks for hsize_t */

/* src: hsize_t, dst: unsigned */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_hsize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_hsize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_hsize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: unsigned */

/* src: hsize_t, dst: int8_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_INT8_T
    #define ASSIGN_hsize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_INT8_T
    #define ASSIGN_hsize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_hsize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: int8_t */

/* src: hsize_t, dst: int */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_INT
    #define ASSIGN_hsize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_INT
    #define ASSIGN_hsize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_INT */
    #define ASSIGN_hsize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: int */

/* src: hsize_t, dst: long */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_LONG
    #define ASSIGN_hsize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_LONG
    #define ASSIGN_hsize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_LONG */
    #define ASSIGN_hsize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: long */

/* src: hsize_t, dst: int64_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_INT64_T
    #define ASSIGN_hsize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_INT64_T
    #define ASSIGN_hsize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_hsize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: int64_t */

/* src: hsize_t, dst: uint8_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_hsize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_hsize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_hsize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: uint8_t */

/* src: hsize_t, dst: uint16_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_hsize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_hsize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_hsize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: uint16_t */

/* src: hsize_t, dst: uint32_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_hsize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_hsize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_hsize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: uint32_t */

/* src: hsize_t, dst: uint64_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_hsize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_hsize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_hsize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: uint64_t */

/* src: hsize_t, dst: ptrdiff_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_hsize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_hsize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_hsize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: ptrdiff_t */

/* src: hsize_t, dst: size_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_hsize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_hsize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_hsize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: size_t */

/* src: hsize_t, dst: ssize_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_hsize_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_hsize_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_hsize_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: ssize_t */

/* src: hsize_t, dst: haddr_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_hsize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_hsize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_hsize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: haddr_t */

/* src: hsize_t, dst: hssize_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_hsize_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_hsize_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_hsize_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: hssize_t */

/* src: hsize_t, dst: h5_stat_size_t */
#if H5_SIZEOF_HSIZE_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_hsize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSIZE_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_hsize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSIZE_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_hsize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hsize_t dst: h5_stat_size_t */


/* Assignment checks for hssize_t */

/* src: hssize_t, dst: unsigned */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_hssize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_hssize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_hssize_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: unsigned */

/* src: hssize_t, dst: int8_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_INT8_T
    #define ASSIGN_hssize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_INT8_T
    #define ASSIGN_hssize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_hssize_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: int8_t */

/* src: hssize_t, dst: int */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_INT
    #define ASSIGN_hssize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_INT
    #define ASSIGN_hssize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_INT */
    #define ASSIGN_hssize_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: int */

/* src: hssize_t, dst: long */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_LONG
    #define ASSIGN_hssize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_LONG
    #define ASSIGN_hssize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_LONG */
    #define ASSIGN_hssize_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: long */

/* src: hssize_t, dst: int64_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_INT64_T
    #define ASSIGN_hssize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_INT64_T
    #define ASSIGN_hssize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_hssize_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: int64_t */

/* src: hssize_t, dst: uint8_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_hssize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_hssize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_hssize_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: uint8_t */

/* src: hssize_t, dst: uint16_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_hssize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_hssize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_hssize_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: uint16_t */

/* src: hssize_t, dst: uint32_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_hssize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_hssize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_hssize_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: uint32_t */

/* src: hssize_t, dst: uint64_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_hssize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_hssize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_hssize_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: uint64_t */

/* src: hssize_t, dst: ptrdiff_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_hssize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_hssize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_hssize_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: ptrdiff_t */

/* src: hssize_t, dst: size_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_hssize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_hssize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_hssize_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: size_t */

/* src: hssize_t, dst: ssize_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_hssize_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_hssize_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_hssize_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: ssize_t */

/* src: hssize_t, dst: haddr_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_hssize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_hssize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_hssize_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: haddr_t */

/* src: hssize_t, dst: hsize_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_hssize_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_hssize_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_hssize_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: hsize_t */

/* src: hssize_t, dst: h5_stat_size_t */
#if H5_SIZEOF_HSSIZE_T < H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_hssize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_HSSIZE_T > H5_SIZEOF_H5_STAT_SIZE_T
    #define ASSIGN_hssize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_HSSIZE_T == H5_SIZEOF_H5_STAT_SIZE_T */
    #define ASSIGN_hssize_t_TO_h5_stat_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SIGNED_TO_UNSIGNED(dst, dsttype, src, srctype)
#endif /* src: hssize_t dst: h5_stat_size_t */


/* Assignment checks for h5_stat_size_t */

/* src: h5_stat_size_t, dst: unsigned */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_UNSIGNED
    #define ASSIGN_h5_stat_size_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_UNSIGNED
    #define ASSIGN_h5_stat_size_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_UNSIGNED */
    #define ASSIGN_h5_stat_size_t_TO_unsigned(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: unsigned */

/* src: h5_stat_size_t, dst: int8_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_INT8_T
    #define ASSIGN_h5_stat_size_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_INT8_T
    #define ASSIGN_h5_stat_size_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_INT8_T */
    #define ASSIGN_h5_stat_size_t_TO_int8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: int8_t */

/* src: h5_stat_size_t, dst: int */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_INT
    #define ASSIGN_h5_stat_size_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_INT
    #define ASSIGN_h5_stat_size_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_INT */
    #define ASSIGN_h5_stat_size_t_TO_int(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: int */

/* src: h5_stat_size_t, dst: long */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_LONG
    #define ASSIGN_h5_stat_size_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_LONG
    #define ASSIGN_h5_stat_size_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_LONG */
    #define ASSIGN_h5_stat_size_t_TO_long(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: long */

/* src: h5_stat_size_t, dst: int64_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_INT64_T
    #define ASSIGN_h5_stat_size_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_INT64_T
    #define ASSIGN_h5_stat_size_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_INT64_T */
    #define ASSIGN_h5_stat_size_t_TO_int64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: int64_t */

/* src: h5_stat_size_t, dst: uint8_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_UINT8_T
    #define ASSIGN_h5_stat_size_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_UINT8_T
    #define ASSIGN_h5_stat_size_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_UINT8_T */
    #define ASSIGN_h5_stat_size_t_TO_uint8_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: uint8_t */

/* src: h5_stat_size_t, dst: uint16_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_UINT16_T
    #define ASSIGN_h5_stat_size_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_UINT16_T
    #define ASSIGN_h5_stat_size_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_UINT16_T */
    #define ASSIGN_h5_stat_size_t_TO_uint16_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: uint16_t */

/* src: h5_stat_size_t, dst: uint32_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_UINT32_T
    #define ASSIGN_h5_stat_size_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_UINT32_T
    #define ASSIGN_h5_stat_size_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_UINT32_T */
    #define ASSIGN_h5_stat_size_t_TO_uint32_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: uint32_t */

/* src: h5_stat_size_t, dst: uint64_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_UINT64_T
    #define ASSIGN_h5_stat_size_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_UINT64_T
    #define ASSIGN_h5_stat_size_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_UINT64_T */
    #define ASSIGN_h5_stat_size_t_TO_uint64_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: uint64_t */

/* src: h5_stat_size_t, dst: ptrdiff_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_h5_stat_size_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_PTRDIFF_T
    #define ASSIGN_h5_stat_size_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_PTRDIFF_T */
    #define ASSIGN_h5_stat_size_t_TO_ptrdiff_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: ptrdiff_t */

/* src: h5_stat_size_t, dst: size_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_SIZE_T
    #define ASSIGN_h5_stat_size_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_SIZE_T
    #define ASSIGN_h5_stat_size_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_SIZE_T */
    #define ASSIGN_h5_stat_size_t_TO_size_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: size_t */

/* src: h5_stat_size_t, dst: ssize_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_SSIZE_T
    #define ASSIGN_h5_stat_size_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_SSIZE_T
    #define ASSIGN_h5_stat_size_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_SSIZE_T */
    #define ASSIGN_h5_stat_size_t_TO_ssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: ssize_t */

/* src: h5_stat_size_t, dst: haddr_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_HADDR_T
    #define ASSIGN_h5_stat_size_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_HADDR_T
    #define ASSIGN_h5_stat_size_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_HADDR_T */
    #define ASSIGN_h5_stat_size_t_TO_haddr_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: haddr_t */

/* src: h5_stat_size_t, dst: hsize_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_HSIZE_T
    #define ASSIGN_h5_stat_size_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_HSIZE_T
    #define ASSIGN_h5_stat_size_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_HSIZE_T */
    #define ASSIGN_h5_stat_size_t_TO_hsize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_SAME_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: hsize_t */

/* src: h5_stat_size_t, dst: hssize_t */
#if H5_SIZEOF_H5_STAT_SIZE_T < H5_SIZEOF_HSSIZE_T
    #define ASSIGN_h5_stat_size_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_LARGER_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#elif H5_SIZEOF_H5_STAT_SIZE_T > H5_SIZEOF_HSSIZE_T
    #define ASSIGN_h5_stat_size_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SMALLER_SIZE(dst, dsttype, src, srctype)
#else /* H5_SIZEOF_H5_STAT_SIZE_T == H5_SIZEOF_HSSIZE_T */
    #define ASSIGN_h5_stat_size_t_TO_hssize_t(dst, dsttype, src, srctype) \
        ASSIGN_TO_SAME_SIZE_UNSIGNED_TO_SIGNED(dst, dsttype, src, srctype)
#endif /* src: h5_stat_size_t dst: hssize_t */

#endif /* H5overflow_H */

