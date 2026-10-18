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
 * This file contains public declarations for the H5ES (event set) developer
 *      support routines.
 */

#ifndef H5ESdevelop_H
#define H5ESdevelop_H

/* Include package's public header */
#include "H5ESpublic.h"

/*****************/
/* Public Macros */
/*****************/

/*******************/
/* Public Typedefs */
/*******************/

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL herr_t H5ESinsert_request(hid_t es_id, hid_t connector_id, void *request);
H5_DLL herr_t H5ESget_requests(hid_t es_id, H5_iter_order_t order, hid_t *connector_ids, void **requests,
                               size_t array_len, size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* H5ESdevelop_H */
