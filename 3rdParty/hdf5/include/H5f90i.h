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

#ifndef H5f90i_H
#define H5f90i_H

/*
 * Include generated header.  This header defines integer types,
 * so this file only needs to define _fcd.
 */
#include "H5f90i_gen.h"

/* Define _fcd.  These are the same on every system but UNICOS.
 */
#define _fcdtocp(desc) (desc)

#if (defined(UNICOS) || defined(_UNICOS)) && !defined(__crayx1)

#include <fortran.h>

/*typedef char*              _fcd;*/

#else

typedef char *_fcd;

#endif

#endif /* H5f90i_H */
