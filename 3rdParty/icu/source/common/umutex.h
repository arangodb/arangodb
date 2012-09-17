/*
**********************************************************************
*   Copyright (C) 1997-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File UMUTEX.H
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97  aliu        Creation.
*   04/07/99  srl         rewrite - C interface, multiple mutices
*   05/13/99  stephen     Changed to umutex (from cmutex)
******************************************************************************
*/

#ifndef UMUTEX_H
#define UMUTEX_H

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "putilimp.h"

#if defined(_MSC_VER) && _MSC_VER >= 1500
# include <intrin.h>
#endif

#if U_PLATFORM_IS_DARWIN_BASED
#if defined(__STRICT_ANSI__)
#define UPRV_REMAP_INLINE
#define inline
#endif
#include <libkern/OSAtomic.h>
#define USE_MAC_OS_ATOMIC_INCREMENT 1
#if defined(UPRV_REMAP_INLINE)
#undef inline
#undef UPRV_REMAP_INLINE
#endif
#endif

/*
 * If we do not compile with dynamic_annotations.h then define
 * empty annotation macros.
 *  See http://code.google.com/p/data-race-test/wiki/DynamicAnnotations
 */
#ifndef ANNOTATE_HAPPENS_BEFORE
# define ANNOTATE_HAPPENS_BEFORE(obj)
# define ANNOTATE_HAPPENS_AFTER(obj)
# define ANNOTATE_UNPROTECTED_READ(x) (x)
#endif

#ifndef UMTX_FULL_BARRIER
# if !ICU_USE_THREADS
#  define UMTX_FULL_BARRIER
# elif U_HAVE_GCC_ATOMICS
#  define UMTX_FULL_BARRIER __sync_synchronize();
# elif defined(_MSC_VER) && _MSC_VER >= 1500
    /* From MSVC intrin.h. Use _ReadWriteBarrier() only on MSVC 9 and higher. */
#  define UMTX_FULL_BARRIER _ReadWriteBarrier();
# elif U_PLATFORM_IS_DARWIN_BASED
#  define UMTX_FULL_BARRIER OSMemoryBarrier();
# else
#  define UMTX_FULL_BARRIER \
    { \
        umtx_lock(NULL); \
        umtx_unlock(NULL); \
    }
# endif
#endif

#ifndef UMTX_ACQUIRE_BARRIER
# define UMTX_ACQUIRE_BARRIER UMTX_FULL_BARRIER
#endif

#ifndef UMTX_RELEASE_BARRIER
# define UMTX_RELEASE_BARRIER UMTX_FULL_BARRIER
#endif

/**
 * \def UMTX_CHECK
 * Encapsulates a safe check of an expression
 * for use with double-checked lazy inititialization.
 * Either memory barriers or mutexes are required, to prevent both the hardware
 * and the compiler from reordering operations across the check.
 * The expression must involve only a  _single_ variable, typically
 *    a possibly null pointer or a boolean that indicates whether some service
 *    is initialized or not.
 * The setting of the variable involved in the test must be the last step of
 *    the initialization process.
 *
 * @internal
 */
#define UMTX_CHECK(pMutex, expression, result) \
    { \
        (result)=(expression); \
        UMTX_ACQUIRE_BARRIER; \
    }
/*
 * TODO: Replace all uses of UMTX_CHECK and surrounding code
 * with SimpleSingleton or TriStateSingleton, and remove UMTX_CHECK.
 */

/*
 * Code within ICU that accesses shared static or global data should
 * instantiate a Mutex object while doing so.  The unnamed global mutex
 * is used throughout ICU, so keep locking short and sweet.
 *
 * For example:
 *
 * void Function(int arg1, int arg2)
 * {
 *   static Object* foo;     // Shared read-write object
 *   umtx_lock(NULL);        // Lock the ICU global mutex
 *   foo->Method();
 *   umtx_unlock(NULL);
 * }
 *
 * an alternative C++ mutex API is defined in the file common/mutex.h
 */

/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_CAPI void U_EXPORT2 umtx_lock   ( UMTX* mutex ); 

/* Unlock a mutex. Pass in NULL if you want the single global
   mutex. 
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_CAPI void U_EXPORT2 umtx_unlock ( UMTX* mutex );

/* Initialize a mutex. Use it this way:
   umtx_init( &aMutex ); 
 * ICU Mutexes do not need explicit initialization before use.  Use of this
 *   function is not necessary.
 * Initialization of an already initialized mutex has no effect, and is safe to do.
 * Initialization of mutexes is thread safe.  Two threads can concurrently 
 *   initialize the same mutex without causing problems.
 * @param mutex The given mutex to be initialized
 */
U_CAPI void U_EXPORT2 umtx_init   ( UMTX* mutex );

/* Destroy a mutex. This will free the resources of a mutex.
 * Use it this way:
 *   umtx_destroy( &aMutex ); 
 * Destroying an already destroyed mutex has no effect, and causes no problems.
 * This function is not thread safe.  Two threads must not attempt to concurrently
 *   destroy the same mutex.
 * @param mutex The given mutex to be destroyed.
 */
U_CAPI void U_EXPORT2 umtx_destroy( UMTX *mutex );

/*
 * Atomic Increment and Decrement of an int32_t value.
 *
 * Return Values:
 *   If the result of the operation is zero, the return zero.
 *   If the result of the operation is not zero, the sign of returned value
 *      is the same as the sign of the result, but the returned value itself may
 *      be different from the result of the operation.
 */
U_CAPI int32_t U_EXPORT2 umtx_atomic_inc(int32_t *);
U_CAPI int32_t U_EXPORT2 umtx_atomic_dec(int32_t *);

#endif /*_CMUTEX*/
/*eof*/
