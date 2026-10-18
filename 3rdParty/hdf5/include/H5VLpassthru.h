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
 * Purpose:	The public header file for the pass-through VOL connector.
 */

#ifndef H5VLpassthru_H
#define H5VLpassthru_H

/* Public headers needed by this file */
#include "H5VLpublic.h" /* Virtual Object Layer                 */

/* Identifier for the pass-through VOL connector */
#define H5VL_PASSTHRU (H5VL_pass_through_register())

/* Characteristics of the pass-through VOL connector */
#define H5VL_PASSTHRU_NAME    "pass_through"
#define H5VL_PASSTHRU_VALUE   1 /* VOL connector ID */
#define H5VL_PASSTHRU_VERSION 0

/* Pass-through VOL connector info */
typedef struct H5VL_pass_through_info_t {
    hid_t under_vol_id;   /* VOL ID for under VOL */
    void *under_vol_info; /* VOL info for under VOL */
} H5VL_pass_through_info_t;

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL hid_t H5VL_pass_through_register(void);

#ifdef __cplusplus
}
#endif

#endif /* H5VLpassthru_H */
