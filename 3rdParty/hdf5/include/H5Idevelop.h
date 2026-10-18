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
 * This file contains public declarations for the H5I (ID management) developer
 *      support routines.
 */

#ifndef H5Idevelop_H
#define H5Idevelop_H

/* Include package's public header */
#include "H5Ipublic.h" /* ID management */

/*****************/
/* Public Macros */
/*****************/

/*******************/
/* Public Typedefs */
/*******************/

/**
 * The type of the realize_cb callback for H5Iregister_future
 */
//! <!-- [H5I_future_realize_func_t_snip] -->
typedef herr_t (*H5I_future_realize_func_t)(void *future_object, hid_t *actual_object_id);
//! <!-- [H5I_future_realize_func_t_snip] -->

/**
 * The type of the discard_cb callback for H5Iregister_future
 */
//! <!-- [H5I_future_discard_func_t_snip] -->
typedef herr_t (*H5I_future_discard_func_t)(void *future_object);
//! <!-- [H5I_future_discard_func_t_snip] -->

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
 * \ingroup H5I
 *
 * \brief Registers a "future" object under a type and returns an ID for it
 *
 * \param[in] type The identifier of the type of the new ID
 * \param[in] object Pointer to "future" object for which a new ID is created
 * \param[in] realize_cb Function pointer to realize a future object
 * \param[in] discard_cb Function pointer to destroy a future object
 *
 * \return \hid_t{object}
 *
 * \details H5Iregister_future() creates and returns a new ID for a "future" object.
 *          Future objects are a special kind of object and represent a
 *          placeholder for an object that has not yet been created or opened.
 *          The \p realize_cb will be invoked by the HDF5 library to 'realize'
 *          the future object as an actual object.  A call to H5Iobject_verify()
 *          will invoke the \p realize_cb callback and if it successfully
 *          returns, will return the actual object, not the future object.
 *
 * \details The \p type parameter is the identifier for the ID type to which
 *          this new future ID will belong. This identifier may have been created
 *          by a call to H5Iregister_type() or may be one of the HDF5 pre-defined
 *          ID classes (e.g. H5I_FILE, H5I_GROUP, H5I_DATASPACE, etc).
 *
 * \details The \p object parameter is a pointer to the memory which the new ID
 *          will be a reference to. This pointer will be stored by the library,
 *          but will not be returned to a call to H5Iobject_verify() until the
 *          \p realize_cb callback has returned the actual pointer for the object.
 *
 *          A  NULL value for \p object is allowed.
 *
 * \details The \p realize_cb parameter is a function pointer that will be
 *          invoked by the HDF5 library to convert a future object into an
 *          actual object.   The \p realize_cb function may be invoked by
 *          H5Iobject_verify() to return the actual object for a user-defined
 *          ID class (i.e. an ID class registered with H5Iregister_type()) or
 *          internally by the HDF5 library in order to use or get information
 *          from an HDF5 pre-defined ID type.  For example, the \p realize_cb
 *          for a future dataspace object will be called during the process
 *          of returning information from H5Sget_simple_extent_dims().
 *
 *          Note that although the \p realize_cb routine returns
 *          an ID (as a parameter) for the actual object, the HDF5 library
 *          will swap the actual object in that ID for the future object in
 *          the future ID.  This ensures that the ID value for the object
 *          doesn't change for the user when the object is realized.
 *
 *          Note that the \p realize_cb callback could receive a NULL value
 *          for a future object pointer, if one was used when H5Iregister_future()
 *          was initially called.  This is permitted as a means of allowing
 *          the \p realize_cb to act as a generator of new objects, without
 *          requiring creation of unnecessary future objects.
 *
 *          It is an error to pass NULL for \p realize_cb.
 *
 * \details The \p discard_cb parameter is a function pointer that will be
 *          invoked by the HDF5 library to destroy a future object.  This
 *          callback will always be invoked for _every_ future object, whether
 *          the \p realize_cb is invoked on it or not.  It's possible that
 *          the \p discard_cb is invoked on a future object without the
 *          \p realize_cb being invoked, e.g. when a future ID is closed without
 *          requiring the future object to be realized into an actual one.
 *
 *          Note that the \p discard_cb callback could receive a NULL value
 *          for a future object pointer, if one was used when H5Iregister_future()
 *          was initially called.
 *
 *          It is an error to pass NULL for \p discard_cb.
 *
 * \note The H5Iregister_future() function is primarily targeted at VOL connector
 *          authors and is _not_ designed for general-purpose application use.
 *
 * \since 1.14.0
 *
 */
H5_DLL hid_t H5Iregister_future(H5I_type_t type, const void *object, H5I_future_realize_func_t realize_cb,
                                H5I_future_discard_func_t discard_cb);

#ifdef __cplusplus
}
#endif

#endif /* H5Idevelop_H */
