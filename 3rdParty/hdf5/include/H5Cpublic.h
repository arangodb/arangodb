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

/*-------------------------------------------------------------------------
 *
 * Created:     H5Cpublic.h
 *
 * Purpose:     Public header file for cache functions
 *
 *-------------------------------------------------------------------------
 */
#ifndef H5Cpublic_H
#define H5Cpublic_H

#include "H5public.h" /* Generic Functions                        */

enum H5C_cache_incr_mode {
    H5C_incr__off,
    /**<Automatic cache size increase is disabled, and the remaining increment fields are ignored.*/

    H5C_incr__threshold
    /**<Automatic cache size increase is enabled using the hit rate threshold algorithm.*/
};

enum H5C_cache_flash_incr_mode {
    H5C_flash_incr__off,
    /**<Flash cache size increase is disabled.*/

    H5C_flash_incr__add_space
    /**<Flash cache size increase is enabled using the add space algorithm.*/
};

enum H5C_cache_decr_mode {
    H5C_decr__off,
    /**<Automatic cache size decrease is disabled.*/

    H5C_decr__threshold,
    /**<Automatic cache size decrease is enabled  using the hit rate threshold algorithm.*/

    H5C_decr__age_out,
    /**<Automatic cache size decrease is enabled using the ageout algorithm. */

    H5C_decr__age_out_with_threshold
    /**<Automatic cache size decrease is enabled using the ageout with hit rate threshold algorithm.*/
};

#endif
