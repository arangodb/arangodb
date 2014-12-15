/*
**********************************************************************
*   Copyright (C) 2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File cmutex.h
*
*     Minimal plain C declarations for ICU mutex functions.
*     This header provides a transition path for plain C files that 
*     formerly included mutex.h, which is now a C++ only header.
*
*     This header should not be used for new code.
*
*     C++ files should include umutex.h, not this header.
*
*/

#ifndef __CMUTEX_H__
#define __CMUTEX_H__

typedef struct UMutex UMutex;


/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_INTERNAL void U_EXPORT2 umtx_lock(UMutex* mutex); 

/* Unlock a mutex.
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_INTERNAL void U_EXPORT2 umtx_unlock (UMutex* mutex);

#endif

