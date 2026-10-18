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
 * This file contains function prototypes for each exported function in
 * the H5I module.
 */
#ifndef H5Ipublic_H
#define H5Ipublic_H

#include "H5public.h" /* Generic Functions                        */

/**
 * Library type values.
 * \internal Library type values.  Start with `1' instead of `0' because it
 *           makes the tracing output look better when hid_t values are large
 *           numbers. Change the TYPE_BITS in H5I.c if the MAXID gets larger
 *           than 32 (an assertion will fail otherwise).
 *
 *           When adding types here, add a section to the 'misc19' test in
 *           test/tmisc.c to verify that the H5I{inc|dec|get}_ref() routines
 *           work correctly with it. \endinternal
 */
//! <!-- [H5I_type_t_snip] -->
typedef enum H5I_type_t {
    H5I_UNINIT = (-2),  /**< uninitialized type                        */
    H5I_BADID  = (-1),  /**< invalid Type                              */
    H5I_FILE   = 1,     /**< type ID for File objects                  */
    H5I_GROUP,          /**< type ID for Group objects                 */
    H5I_DATATYPE,       /**< type ID for Datatype objects              */
    H5I_DATASPACE,      /**< type ID for Dataspace objects             */
    H5I_DATASET,        /**< type ID for Dataset objects               */
    H5I_MAP,            /**< type ID for Map objects                   */
    H5I_ATTR,           /**< type ID for Attribute objects             */
    H5I_VFL,            /**< type ID for virtual file layer            */
    H5I_VOL,            /**< type ID for virtual object layer          */
    H5I_GENPROP_CLS,    /**< type ID for generic property list classes */
    H5I_GENPROP_LST,    /**< type ID for generic property lists        */
    H5I_ERROR_CLASS,    /**< type ID for error classes                 */
    H5I_ERROR_MSG,      /**< type ID for error messages                */
    H5I_ERROR_STACK,    /**< type ID for error stacks                  */
    H5I_SPACE_SEL_ITER, /**< type ID for dataspace selection iterator  */
    H5I_EVENTSET,       /**< type ID for event sets                    */
    H5I_NTYPES          /**< number of library types, MUST BE LAST!    */
} H5I_type_t;
//! <!-- [H5I_type_t_snip] -->

/**
 * Type of IDs to return to users
 */
typedef int64_t hid_t;

#define PRIdHID PRId64
#define PRIxHID PRIx64
#define PRIXHID PRIX64
#define PRIoHID PRIo64

/**
 * The size of identifiers
 */
#define H5_SIZEOF_HID_T H5_SIZEOF_INT64_T

/**
 * An invalid object ID. This is also negative for error return.
 */
#define H5I_INVALID_HID (-1)

/**
 * A function for freeing objects. This function will be called with a pointer
 * to the object and a pointer to a pointer to the asynchronous request object.
 * The function should free the object and return non-negative to indicate that
 * the object can be removed from the ID type. If the function returns negative
 * (failure) then the object will remain in the ID type. For asynchronous
 * operations and handling the request parameter, see the HDF5 user guide and
 * VOL connector author guide.
 */
typedef herr_t (*H5I_free_t)(void *obj, void **request);

/**
 * The type of a function to compare objects & keys
 */
//! <!-- [H5I_search_func_t_snip] -->
typedef int (*H5I_search_func_t)(void *obj, hid_t id, void *key);
//! <!-- [H5I_search_func_t_snip] -->

/**
 * The type of H5Iiterate() callback functions
 */
//! <!-- [H5I_iterate_func_t_snip] -->
typedef herr_t (*H5I_iterate_func_t)(hid_t id, void *udata);
//! <!-- [H5I_iterate_func_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/* Public API functions */

/**
 * \ingroup H5IUD
 *
 * \brief Registers an object under a type and returns an ID for it
 *
 * \param[in] type The identifier of the type of the new ID
 * \param[in] object Pointer to object for which a new ID is created
 *
 * \return \hid_t{object}
 *
 * \details H5Iregister() creates and returns a new ID for an object.
 *
 * \details The \p type parameter is the identifier for the ID type to which
 *          this new ID will belong. This identifier must have been created by
 *          a call to H5Iregister_type().
 *
 * \details The \p object parameter is a pointer to the memory which the new ID
 *          will be a reference to. This pointer will be stored by the library
 *          and returned via a call to H5Iobject_verify().
 *
 * \since 1.8.0
 *
 */
H5_DLL hid_t H5Iregister(H5I_type_t type, const void *object);
/**
 * \ingroup H5IUD
 *
 * \brief Returns the object referenced by an ID
 *
 * \param[in] id ID to be dereferenced
 * \param[in] type The identifier type

 *
 * \return Pointer to the object referenced by \p id on success, NULL on failure.
 *
 * \details H5Iobject_verify() returns a pointer to the memory referenced by id
 *          after verifying that \p id is of type \p type. This function is
 *          analogous to dereferencing a pointer in C with type checking.
 *
 * \note H5Iobject_verify() does not change the ID it is called on in any way
 *       (as opposed to H5Iremove_verify(), which removes the ID from its
 *       type's hash table).
 *
 * \see H5Iregister()
 *
 * \since 1.8.0
 *
 */
H5_DLL void *H5Iobject_verify(hid_t id, H5I_type_t type);
/**
 * \ingroup H5IUD
 *
 * \brief Removes an ID from its type
 *
 * \param[in] id The ID to be removed from its type
 * \param[in] type The identifier type

 *
 * \return Returns a pointer to the memory referred to by \p id on success,
 *         NULL on failure.
 *
 * \details H5Iremove_verify() first ensures that \p id belongs to \p type.
 *          If so, it removes \p id from its type and returns the pointer
 *          to the memory it referred to. This pointer is the same pointer that
 *          was placed in storage by H5Iregister(). If id does not belong to
 *          \p type, then NULL is returned.
 *
 *          The \p id parameter is the ID which is to be removed from its type.
 *
 *          The \p type parameter is the identifier for the ID type which \p id
 *          is supposed to belong to. This identifier must have been created by
 *          a call to H5Iregister_type().
 *
 * \note This function does NOT deallocate the memory that \p id refers to.
 *       The pointer returned by H5Iregister() must be deallocated by the user
 *       to avoid memory leaks.
 *
 * \since 1.8.0
 *
 */
H5_DLL void *H5Iremove_verify(hid_t id, H5I_type_t type);
/**
 * \ingroup H5I
 *
 * \brief Retrieves the type of an object
 *
 * \obj_id{id}
 *
 * \return Returns the object type if successful; otherwise #H5I_BADID.
 *
 * \details H5Iget_type() retrieves the type of the object identified by
 *          \p id. If no valid type can be determined or the identifier submitted is
 *          invalid, the function returns #H5I_BADID.
 *
 *          This function is of particular use in determining the type of
 *          object closing function (H5Dclose(), H5Gclose(), etc.) to call
 *          after a call to H5Rdereference().
 *
 * \note Note that this function returns only the type of object that \p id
 *       would identify if it were valid; it does not determine whether \p id
 *       is valid identifier. Validity can be determined with a call to
 *       H5Iis_valid().
 *
 * \since 1.0.0
 *
 */
H5_DLL H5I_type_t H5Iget_type(hid_t id);
/**
 * \ingroup H5I
 *
 * \brief Retrieves an identifier for the file containing the specified object
 *
 * \obj_id{id}
 *
 * \return \hid_t{file}
 *
 * \details H5Iget_file_id() returns the identifier of the file associated with
 *          the object referenced by \p id.
 *
 * \note Note that the HDF5 library permits an application to close a file
 *       while objects within the file remain open. If the file containing the
 *       object \p id is still open, H5Iget_file_id() will retrieve the
 *       existing file identifier. If there is no existing file identifier for
 *       the file, i.e., the file has been closed, H5Iget_file_id() will reopen
 *       the file and return a new file identifier. In either case, the file
 *       identifier must eventually be released using H5Fclose().
 *
 * \since 1.6.3
 *
 */
H5_DLL hid_t H5Iget_file_id(hid_t id);
/**
 * \ingroup H5I
 *
 * \brief Retrieves a name of an object based on the object identifier
 *
 * \obj_id{id}
 * \param[out] name A buffer for the name associated with the identifier
 * \param[in]  size The size, in bytes, of the \p name buffer. Must be the
 *                  size of the object name in bytes plus 1 for a NULL
 *                  terminator
 *
 * \return ssize_t
 *
 * \details H5Iget_name() retrieves a name for the object identified by \p id.
 *
 * \details Up to size characters of the name are returned in \p name;
 *          additional characters, if any, are not returned to the user
 *          application.
 *
 *          \details_namelen{object,H5Iget_name}
 *
 *          If the object identified by \p id is an attribute, as determined
 *          via H5Iget_type(), H5Iget_name() retrieves the name of the object
 *          to which that attribute is attached. To retrieve the name of the
 *          attribute itself, use H5Aget_name().
 *
 *          If there is no name associated with the object identifier or if the
 *          name is NULL, H5Iget_name() returns 0 (zero).
 *
 * \note Note that an object in an HDF5 file may have multiple paths if there
 *       are multiple links pointing to it. This function may return any one of
 *       these paths. When possible, H5Iget_name() returns the path with which
 *       the object was opened.
 *
 * \since 1.6.0
 *
 */
H5_DLL ssize_t H5Iget_name(hid_t id, char *name /*out*/, size_t size);
/**
 * \ingroup H5I
 *
 * \brief Increments the reference count for an object
 *
 * \obj_id{id}
 *
 * \return Returns a non-negative reference count of the object ID after
 *         incrementing it if successful; otherwise a negative value is
 *         returned.
 *
 * \details H5Iinc_ref() increments the reference count of the object
 *          identified by \p id.
 *
 *          The reference count for an object ID is attached to the information
 *          about an object in memory and has no relation to the number of
 *          links to an object on disk.
 *
 *          The reference count for a newly created object will be 1. Reference
 *          counts for objects may be explicitly modified with this function or
 *          with H5Idec_ref(). When an object ID's reference count reaches
 *          zero, the object will be closed. Calling an object ID's \c close
 *          function decrements the reference count for the ID which normally
 *          closes the object, but if the reference count for the ID has been
 *          incremented with this function, the object will only be closed when
 *          the reference count reaches zero with further calls to H5Idec_ref()
 *          or the object ID's \c close function.
 *
 *          If the object ID was created by a collective parallel call (such as
 *          H5Dcreate(), H5Gopen(), etc.), the reference count should be
 *          modified by all the processes which have copies of the ID.
 *          Generally this means that group, dataset, attribute, file and named
 *          datatype IDs should be modified by all the processes and that all
 *          other types of IDs are safe to modify by individual processes.
 *
 *          This function is of particular value when an application is
 *          maintaining multiple copies of an object ID. The object ID can be
 *          incremented when a copy is made. Each copy of the ID can then be
 *          safely closed or decremented and the HDF5 object will be closed
 *          when the reference count for that that object drops to zero.
 *
 * \since 1.6.3
 *
 */
H5_DLL int H5Iinc_ref(hid_t id);
/**
 * \ingroup H5I
 *
 * \brief Decrements the reference count for an object
 *
 * \obj_id{id}
 *
 * \return Returns a non-negative reference count of the object ID after
 *         decrementing it, if successful; otherwise a negative value is
 *         returned.
 *
 * \details H5Idec_ref() decrements the reference count of the object
 *          identified by \p id.
 *
 *          The reference count for an object ID is attached to the information
 *          about an object in memory and has no relation to the number of
 *          links to an object on disk.
 *
 *          The reference count for a newly created object will be 1. Reference
 *          counts for objects may be explicitly modified with this function or
 *          with H5Iinc_ref(). When an object identifier's reference count
 *          reaches zero, the object will be closed. Calling an object
 *          identifier's \c close function decrements the reference count for
 *          the identifier, which normally closes the object, but if the
 *          reference count for the identifier has been incremented with
 *          H5Iinc_ref(), the object will only be closed when the reference
 *          count reaches zero with further calls to this function or the
 *          object identifier's \c close function.
 *
 *          If the object ID was created by a collective parallel call (such as
 *          H5Dcreate(), H5Gopen(), etc.), the reference count should be
 *          modified by all the processes which have copies of the ID.
 *          Generally, this means that group, dataset, attribute, file and named
 *          datatype IDs should be modified by all the processes and that all
 *          other types of IDs are safe to modify by individual processes.
 *
 *          This function is of particular value when an application
 *          maintains multiple copies of an object ID. The object ID can be
 *          incremented when a copy is made. Each copy of the ID can then be
 *          safely closed or decremented and the HDF5 object will be closed
 *          when the reference count for that object drops to zero.
 *
 * \since 1.6.3
 *
 */
H5_DLL int H5Idec_ref(hid_t id);
/**
 * \ingroup H5I
 *
 * \brief Retrieves the reference count for an object
 *
 * \obj_id{id}
 *
 * \return Returns a non-negative current reference count of the object
 *         identifier if successful; otherwise a negative value is returned.
 *
 * \details H5Iget_ref() retrieves the reference count of the object identified
 *          by \p id.
 *
 *          The reference count for an object identifier is attached to the
 *          information about an object in memory and has no relation to the
 *          number of links to an object on disk.
 *
 *          The function H5Iis_valid() is used to determine whether a specific
 *          object identifier is valid.
 *
 * \since 1.6.3
 *
 */
H5_DLL int H5Iget_ref(hid_t id);
/**
 * \ingroup H5IUD
 *
 * \brief Creates and returns a new ID type
 *
 * \param[in] hash_size Minimum hash table size (in entries) used to store IDs
 *                      for the new type (unused in 1.8.13 and later)
 * \param[in] reserved Number of reserved IDs for the new type
 * \param[in] free_func Function used to deallocate space for a single ID
 *
 * \return Returns the type identifier on success, negative on failure.
 *
 * \details H5Iregister_type() allocates space for a new ID type and returns an
 *          identifier for it.
 *
 *          The \p hash_size parameter indicates the minimum size of the hash
 *          table used to store IDs in the new type. This parameter is unused
 *          in 1.8.13 and later, when the implementation of ID storage changed.
 *
 *          The \p reserved parameter indicates the number of IDs in this new
 *          type to be reserved. Reserved IDs are valid IDs which are not
 *          associated with any storage within the library.
 *
 *          The \p free_func parameter is a function pointer to a function
 *          which returns an herr_t and accepts a \c void*. The purpose of this
 *          function is to deallocate memory for a single ID. It will be called
 *          by H5Iclear_type() and H5Idestroy_type() on each ID. This function
 *          is NOT called by H5Iremove_verify(). The \c void* will be the same
 *          pointer which was passed in to the H5Iregister() function. The \p
 *          free_func function should return 0 on success and -1 on failure.
 *
 * \since 1.8.0
 *
 */
H5_DLL H5I_type_t H5Iregister_type(size_t hash_size, unsigned reserved, H5I_free_t free_func);
/**
 * \ingroup H5IUD
 *
 * \brief Deletes all identifiers of the given type
 *
 * \param[in] type Identifier of identifier type which is to be cleared of identifiers
 * \param[in] force Whether or not to force deletion of all identifiers
 *
 * \return \herr_t
 *
 * \details H5Iclear_type() deletes all identifiers of the type identified by
 *          the argument \p type.
 *
 *          The identifier type's free function is first called on all of these
 *          identifiers to free their memory, then they are removed from the
 *          type.
 *
 *          If the \p force flag is set to false, only those identifiers whose
 *          reference counts are equal to 1 will be deleted, and all other
 *          identifiers will be entirely unchanged. If the force flag is true,
 *          all identifiers of this type will be deleted.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Iclear_type(H5I_type_t type, hbool_t force);
/**
 * \ingroup H5IUD
 *
 * \brief Removes an identifier type and all identifiers within that type
 *
 * \param[in] type Identifier of identifier type which is to be destroyed
 *
 * \return \herr_t
 *
 * \details H5Idestroy_type deletes an entire identifier type \p type. All
 *          identifiers of this type are destroyed and no new identifiers of
 *          this type can be registered.
 *
 *          The type's free function is called on all of the identifiers which
 *          are deleted by this function, freeing their memory. In addition,
 *          all memory used by this type's hash table is freed.
 *
 *          Since the H5I_type_t values of destroyed identifier types are
 *          reused when new types are registered, it is a good idea to set the
 *          variable holding the value of the destroyed type to #H5I_UNINIT.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Idestroy_type(H5I_type_t type);
/**
 * \ingroup H5IUD
 *
 * \brief Increments the reference count on an ID type
 *
 * \param[in] type The identifier of the type whose reference count is to be incremented
 *
 * \return Returns the current reference count on success, negative on failure.
 *
 * \details H5Iinc_type_ref() increments the reference count on an ID type. The
 *          reference count is used by the library to indicate when an ID type
 *          can be destroyed.
 *
 *          The type parameter is the identifier for the ID type whose
 *          reference count is to be incremented. This identifier must have
 *          been created by a call to H5Iregister_type().
 *
 * \since 1.8.0
 *
 */
H5_DLL int H5Iinc_type_ref(H5I_type_t type);
/**
 * \ingroup H5IUD
 *
 * \brief Decrements the reference count on an identifier type
 *
 * \param[in] type The identifier of the type whose reference count is to be decremented
 *
 * \return Returns the current reference count on success, negative on failure.
 *
 * \details H5Idec_type_ref() decrements the reference count on an identifier
 *          type. The reference count is used by the library to indicate when
 *          an identifier type can be destroyed. If the reference count reaches
 *          zero, this function will destroy it.
 *
 *          The type parameter is the identifier for the identifier type whose
 *          reference count is to be decremented. This identifier must have
 *          been created by a call to H5Iregister_type().
 *
 * \since 1.8.0
 *
 */
H5_DLL int H5Idec_type_ref(H5I_type_t type);
/**
 * \ingroup H5IUD
 *
 * \brief Retrieves the reference count on an ID type
 *
 * \param[in] type The identifier of the type whose reference count is to be retrieved
 *
 * \return Returns the current reference count on success, negative on failure.
 *
 * \details H5Iget_type_ref() retrieves the reference count on an ID type. The
 *          reference count is used by the library to indicate when an ID type
 *          can be destroyed.
 *
 *          The type parameter is the identifier for the ID type whose
 *          reference count is to be retrieved. This identifier must have been
 *          created by a call to H5Iregister_type().
 *
 * \since 1.8.0
 *
 */
H5_DLL int H5Iget_type_ref(H5I_type_t type);
/**
 * \ingroup H5IUD
 *
 * \brief Finds the memory referred to by an ID within the given ID type such
 *        that some criterion is satisfied
 *
 * \param[in] type The identifier of the type to be searched
 * \param[in] func The function defining the search criteria
 * \param[in] key A key for the search function
 *
 * \return Returns a pointer to the object which satisfies the search function
 *         on success, NULL on failure.
 *
 * \details H5Isearch() searches through a given ID type to find an object that
 *          satisfies the criteria defined by \p func. If such an object is
 *          found, the pointer to the memory containing this object is
 *          returned. Otherwise, NULL is returned. To do this, \p func is
 *          called on every member of type \p type. The first member to satisfy
 *          \p func is returned.
 *
 *          The \p type parameter is the identifier for the ID type which is to
 *          be searched. This identifier must have been created by a call to
 *          H5Iregister_type().
 *
 *          The parameter \p func is a function pointer to a function which
 *          takes three parameters. The first parameter is a \c void* and will
 *          be a pointer to the object to be tested. This is the same object
 *          that was placed in storage using H5Iregister(). The second
 *          parameter is a hid_t and is the ID of the object to be tested. The
 *          last parameter is a \c void*. This is the \p key parameter and can
 *          be used however the user finds helpful, or it can be ignored if it
 *          is not needed. \p func returns 0 if the object it is testing does
 *          not pass its criteria. A non-zero value should be returned if the
 *          object does pass its criteria. H5I_search_func_t is defined in
 *          H5Ipublic.h and is shown below.
 *          \snippet this H5I_search_func_t_snip
 *          The \p key parameter will be passed to the search function as a
 *          parameter. It can be used to further define the search at run-time.
 *
 * \since 1.8.0
 *
 */
H5_DLL void *H5Isearch(H5I_type_t type, H5I_search_func_t func, void *key);
/**
 * \ingroup H5IUD
 *
 * \brief Calls a callback for each member of the identifier type specified
 *
 * \param[in] type The identifier type
 * \param[in] op The callback function
 * \param[in,out] op_data The data for the callback function
 *
 * \return The last value returned by \p op
 *
 * \details H5Iiterate() calls the callback function \p op for each member of
 *          the identifier type \p type. The callback function type for \p op,
 *          H5I_iterate_func_t, is defined in H5Ipublic.h as:
 *          \snippet this H5I_iterate_func_t_snip
 *          \p op takes as parameters the identifier and a pass through of
 *          \p op_data, and returns an herr_t.
 *
 *          A positive return from op will cause the iteration to stop and
 *          H5Iiterate() will return the value returned by \p op. A negative
 *          return from \p op will cause the iteration to stop and H5Iiterate()
 *          will return failure. A zero return from \p op will allow iteration
 *          to continue, as long as there are other identifiers remaining in
 *          type.
 *
 * \warning  Adding or removing members of the identifier type during iteration
 *           will lead to undefined behavior.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Iiterate(H5I_type_t type, H5I_iterate_func_t op, void *op_data);
/**
 * \ingroup H5IUD
 *
 * \brief Returns the number of identifiers in a given identifier type
 *
 * \param[in] type The identifier type
 * \param[out] num_members Number of identifiers of the specified identifier type
 *
 * \return \herr_t
 *
 * \details H5Inmembers() returns the number of identifiers of the identifier
 *          type specified in \p type.
 *
 *          The number of identifiers is returned in \p num_members. If no
 *          identifiers of this type have been registered, the type does not
 *          exist, or it has been destroyed, \p num_members is returned with
 *          the value 0.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Inmembers(H5I_type_t type, hsize_t *num_members);
/**
 * \ingroup H5IUD
 *
 * \brief Determines whether an identifier type is registered
 *
 * \param[in] type Identifier type
 *
 * \return \htri_t
 *
 * \details H5Itype_exists() determines whether the given identifier type,
 *          \p type, is registered with the library.
 *
 * \since 1.8.0
 *
 */
H5_DLL htri_t H5Itype_exists(H5I_type_t type);
/**
 * \ingroup H5I
 *
 * \brief Determines whether an identifier is valid
 *
 * \obj_id{id}
 *
 * \return \htri_t
 *
 * \details H5Iis_valid() determines whether the identifier \p id is valid.
 *
 * \details Valid identifiers are those that have been obtained by an
 *          application and can still be used to access the original target.
 *          Examples of invalid identifiers include:
 *          \li Out-of-range values: negative, for example
 *          \li Previously-valid identifiers that have been released:
 *              for example, a dataset identifier for which the dataset has
 *              been closed
 *
 *          H5Iis_valid() can be used with any type of identifier: object
 *          identifier, property list identifier, attribute identifier, error
 *          message identifier, etc. When necessary, a call to H5Iget_type()
 *          can determine the type of object that the \p id identifies.
 *
 * \since 1.8.3
 *
 */
H5_DLL htri_t H5Iis_valid(hid_t id);

#ifdef __cplusplus
}
#endif
#endif /* H5Ipublic_H */
