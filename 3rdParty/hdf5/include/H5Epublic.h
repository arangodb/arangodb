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
 * This file contains public declarations for the H5E module.
 */
#ifndef H5Epublic_H
#define H5Epublic_H

#include <stdio.h> /* FILE arg of H5Eprint() */

#include "H5public.h"  /* Generic Functions                        */
#include "H5Ipublic.h" /* Identifiers                              */

/** Value for the default error stack \since 1.12.2 */
#define H5E_DEFAULT 0 /* (hid_t) */

/**
 * Different kinds of error information
 */
typedef enum H5E_type_t { H5E_MAJOR, H5E_MINOR } H5E_type_t;

/**
 * Information about an error; element of error stack
 */
typedef struct H5E_error2_t {
    hid_t cls_id;
    /**< Class ID                           */
    hid_t maj_num;
    /**< Major error ID                        */
    hid_t min_num;
    /**< Minor error number                    */
    unsigned line;
    /**< Line in file where error occurs    */
    const char *func_name;
    /**< Function in which error occurred   */
    const char *file_name;
    /**< File in which error occurred       */
    const char *desc;
    /**< Optional supplied description      */
} H5E_error2_t;

/* When this header is included from a private header, don't make calls to H5open() */
#undef H5OPEN
#ifndef H5private_H
#define H5OPEN H5open(),
#else /* H5private_H */
#define H5OPEN
#endif /* H5private_H */

/* HDF5 error class */
/* Extern "C" block needed to compile C++ filter plugins with some compilers */
#ifdef __cplusplus
extern "C" {
#endif
#define H5E_ERR_CLS (H5OPEN H5E_ERR_CLS_g)
H5_DLLVAR hid_t H5E_ERR_CLS_g;
#ifdef __cplusplus
}
#endif

/* Include the automatically generated public header information */
/* (This includes the list of major and minor error codes for the library) */
#include "H5Epubgen.h"

/*
 * One often needs to temporarily disable automatic error reporting when
 * trying something that's likely or expected to fail.  The code to try can
 * be nested between calls to H5Eget_auto() and H5Eset_auto(), but it's
 * easier just to use this macro like:
 *
 *     H5E_BEGIN_TRY {
 *        ...stuff here that's likely to fail...
 *      } H5E_END_TRY
 *
 * Warning: don't break, return, or longjmp() from the block of code or
 *        the error reporting won't be properly restored!
 *
 * These two macros still use the old API functions for backward compatibility
 * purpose.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS
#define H5E_BEGIN_TRY                                                                                        \
    {                                                                                                        \
        unsigned H5E_saved_is_v2;                                                                            \
        union {                                                                                              \
            H5E_auto1_t efunc1;                                                                              \
            H5E_auto2_t efunc2;                                                                              \
        } H5E_saved;                                                                                         \
        void *H5E_saved_edata;                                                                               \
                                                                                                             \
        (void)H5Eauto_is_v2(H5E_DEFAULT, &H5E_saved_is_v2);                                                  \
        if (H5E_saved_is_v2) {                                                                               \
            (void)H5Eget_auto2(H5E_DEFAULT, &H5E_saved.efunc2, &H5E_saved_edata);                            \
            (void)H5Eset_auto2(H5E_DEFAULT, NULL, NULL);                                                     \
        }                                                                                                    \
        else {                                                                                               \
            (void)H5Eget_auto1(&H5E_saved.efunc1, &H5E_saved_edata);                                         \
            (void)H5Eset_auto1(NULL, NULL);                                                                  \
        }

#define H5E_END_TRY                                                                                          \
    if (H5E_saved_is_v2)                                                                                     \
        (void)H5Eset_auto2(H5E_DEFAULT, H5E_saved.efunc2, H5E_saved_edata);                                  \
    else                                                                                                     \
        (void)H5Eset_auto1(H5E_saved.efunc1, H5E_saved_edata);                                               \
    }
#else /* H5_NO_DEPRECATED_SYMBOLS */
#define H5E_BEGIN_TRY                                                                                        \
    {                                                                                                        \
        H5E_auto2_t saved_efunc;                                                                             \
        void       *H5E_saved_edata;                                                                         \
                                                                                                             \
        (void)H5Eget_auto2(H5E_DEFAULT, &saved_efunc, &H5E_saved_edata);                                     \
        (void)H5Eset_auto2(H5E_DEFAULT, NULL, NULL);

#define H5E_END_TRY                                                                                          \
    (void)H5Eset_auto2(H5E_DEFAULT, saved_efunc, H5E_saved_edata);                                           \
    }
#endif /* H5_NO_DEPRECATED_SYMBOLS */

/*
 * Public API Convenience Macros for Error reporting - Documented
 */
/* Use the Standard C __FILE__ & __LINE__ macros instead of typing them in */
#define H5Epush_sim(func, cls, maj, min, str)                                                                \
    H5Epush2(H5E_DEFAULT, __FILE__, func, __LINE__, cls, maj, min, str)

/*
 * Public API Convenience Macros for Error reporting - Undocumented
 */
/* Use the Standard C __FILE__ & __LINE__ macros instead of typing them in */
/*  And return after pushing error onto stack */
#define H5Epush_ret(func, cls, maj, min, str, ret)                                                           \
    do {                                                                                                     \
        H5Epush2(H5E_DEFAULT, __FILE__, func, __LINE__, cls, maj, min, str);                                 \
        return (ret);                                                                                        \
    } while (0)

/* Use the Standard C __FILE__ & __LINE__ macros instead of typing them in
 * And goto a label after pushing error onto stack.
 */
#define H5Epush_goto(func, cls, maj, min, str, label)                                                        \
    do {                                                                                                     \
        H5Epush2(H5E_DEFAULT, __FILE__, func, __LINE__, cls, maj, min, str);                                 \
        goto label;                                                                                          \
    } while (0)

/**
 * Error stack traversal direction
 */
typedef enum H5E_direction_t {
    H5E_WALK_UPWARD   = 0, /**< begin w/ most specific error, end at API function */
    H5E_WALK_DOWNWARD = 1  /**< begin at API function, end w/ most specific error */
} H5E_direction_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Error stack traversal callback function pointers */
//! <!-- [H5E_walk2_t_snip] -->
/**
 * \brief Callback function for H5Ewalk2()
 *
 * \param[in] n Indexed error position in the stack
 * \param[in] err_desc Pointer to a data structure describing the error
 * \param[in] client_data Pointer to client data in the format expected by the
 *                        user-defined function
 * \return \herr_t
 *
 * \since 1.8.0
 *
 */
typedef herr_t (*H5E_walk2_t)(unsigned n, const H5E_error2_t *err_desc, void *client_data);
//! <!-- [H5E_walk2_t_snip] -->

//! <!-- [H5E_auto2_t_snip] -->
/**
 * \brief Callback function for H5Eset_auto2()
 *
 * \estack_id{estack}
 * \param[in] client_data Pointer to client data in the format expected by the
 *                        user-defined function
 * \return \herr_t
 *
 * \since 1.8.0
 *
 */
typedef herr_t (*H5E_auto2_t)(hid_t estack, void *client_data);
//! <!-- [H5E_auto2_t_snip] -->

/* Public API functions */
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Registers a client library or application program to the HDF5 error API
 *
 * \param[in] cls_name Name of the error class
 * \param[in] lib_name Name of the client library or application to which the error class belongs
 * \param[in] version Version of the client library or application to which the
              error class belongs. It can be \c NULL.
 * \return Returns a class identifier on success; otherwise returns H5I_INVALID_ID.
 *
 * \details H5Eregister_class() registers a client library or application
 *          program to the HDF5 error API so that the client library or
 *          application program can report errors together with the HDF5
 *          library. It receives an identifier for this error class for further
 *          error operations. The library name and version number will be
 *          printed out in the error message as a preamble.
 *
 * \since 1.8.0
 */
H5_DLL hid_t H5Eregister_class(const char *cls_name, const char *lib_name, const char *version);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Removes an error class
 *
 * \param[in] class_id Error class identifier.
 * \return \herr_t
 *
 * \details H5Eunregister_class() removes the error class specified by \p
 *          class_id. All the major and minor errors in this class will also be
 *          closed.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eunregister_class(hid_t class_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Closes an error message
 *
 * \param[in] err_id An error message identifier
 * \return \herr_t
 *
 * \details H5Eclose_msg() closes an error message identifier, which can be
 *          either a major or minor message.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eclose_msg(hid_t err_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Adds a major or minor error message to an error class
 *
 * \param[in] cls An error class identifier
 * \param[in] msg_type The type of the error message
 * \param[in] msg Error message
 * \return An error ID (success), H5I_INVALID_HID (failure)
 *
 * \details H5Ecreate_msg() adds an error message to an error class defined by
 *          client library or application program. The error message can be
 *          either major or minor as indicated by the parameter \p msg_type.
 *
 *          Use H5Eclose_msg() to close the message identifier returned by this
 *          function.
 *
 * \since 1.8.0
 */
H5_DLL hid_t H5Ecreate_msg(hid_t cls, H5E_type_t msg_type, const char *msg);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Creates a new, empty error stack
 *
 * \return \hid_ti{error stack}
 *
 * \details H5Ecreate_stack() creates a new empty error stack and returns the
 *          new stack's identifier. Use H5Eclose_stack() to close the error stack
 *          identifier returned by this function.
 *
 * \since 1.8.0
 */
H5_DLL hid_t H5Ecreate_stack(void);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Returns a copy of the current error stack
 *
 * \return \hid_ti{error stack}
 *
 * \details H5Eget_current_stack() copies the current error stack and returns an
 *          error stack identifier for the new copy.
 *
 * \since 1.8.0
 */
H5_DLL hid_t H5Eget_current_stack(void);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Appends one error stack to another, optionally closing the source
 *        stack.
 *
 * \estack_id{dst_stack_id}
 * \estack_id{src_stack_id}
 * \param[in] close_source_stack Flag to indicate whether to close the source stack
 * \return \herr_t
 *
 * \details H5Eappend_stack() appends the messages from error stack
 *          \p src_stack_id to the error stack \p dst_stack_id.
 *          If \p close_source_stack is \c true, the source error stack
 *          will be closed.
 *
 * \since 1.14.0
 */
H5_DLL herr_t H5Eappend_stack(hid_t dst_stack_id, hid_t src_stack_id, hbool_t close_source_stack);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief * Check if pushing errors on an error stack is paused
 *
 * \estack_id{stack_id}
 * \param[out] is_paused Flag whether stack is paused
 * \return \herr_t
 *
 * \details H5Eis_paused() can be used within HDF5 VOL connectors and other
 *          dynamically loaded components to check if the HDF5 library, or other
 *          component has paused pushing error on the default error stack or
 *          an application stack.
 *
 *          The library may pause pushing errors on the default error stack
 *          when performing "speculative" operations, such as testing for the
 *          existence of something that could be located at one of many
 *          locations. \p stack_id is the error stack to query, and the value
 *          pointed to by \p is_paused is set to TRUE/FALSE.
 *
 *          If an error occurs while attempting to query the status of \p stack_id,
 *          the value pointed to by \p is_paused is unchanged.
 *
 * \since 1.14.5
 */
H5_DLL herr_t H5Eis_paused(hid_t stack_id, hbool_t *is_paused);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief * Pause pushing errors on an error stack
 *
 * \estack_id{stack_id}
 * \return \herr_t
 *
 * \details H5Epause_stack() pauses pushing errors on an error stack.  Pushing
 *          an error on a paused error stack will be ignored (not fail).
 *
 *          H5Eresume_stack() is used to allow errors to be pushed on a stack.
 *          Calls to H5Epause_stack() and H5Eresume_stack() must be matched.
 *
 *          Calls to H5Epause_stack()/H5Eresume_stack() may be nested.
 *
 * \since 1.14.5
 */
H5_DLL herr_t H5Epause_stack(hid_t stack_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief * Resume pushing errors on an error stack
 *
 * \estack_id{stack_id}
 * \return \herr_t
 *
 * \details H5Eresume_stack() resumes pushing errors on an error stack.
 *
 *          Calls to H5Epause_stack() and H5Eresume_stack() must be matched.
 *
 *          Calls to H5Epause_stack()/H5Eresume_stack() may be nested.
 *
 * \since 1.14.5
 */
H5_DLL herr_t H5Eresume_stack(hid_t stack_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Closes an error stack handle
 *
 * \estack_id{stack_id}
 *
 * \return \herr_t
 *
 * \details H5Eclose_stack() closes the error stack handle \p stack_id
 *          and releases its resources. #H5E_DEFAULT cannot be closed.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eclose_stack(hid_t stack_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Retrieves error class name
 *
 * \param[in] class_id Error class identifier
 * \param[out] name Buffer for the error class name
 * \param[in] size The maximum number of characters of the class name to be returned
 *            by this function in \p name.
 * \return Returns non-negative value as on success; otherwise returns negative value.
 *
 * \details H5Eget_class_name() retrieves the name of the error class specified
 *          by the class identifier. If a non-NULL pointer is passed in for \p
 *          name and \p size is greater than zero, the class name of \p size
 *          long is returned. The length of the error class name is also
 *          returned.
 *
 *          \details_namelen{error class,H5Eget_class_name}
 *
 *          If zero is returned, it means the error class has no name.
 *
 * \since 1.8.0
 */
H5_DLL ssize_t H5Eget_class_name(hid_t class_id, char *name, size_t size);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Replaces the current error stack
 *
 * \estack_id{err_stack_id}
 *
 * \return \herr_t
 *
 * \details H5Eset_current_stack() replaces the content of the current error
 *          stack with a copy of the content of the error stack specified by
 *          \p err_stack_id, and it closes the error stack specified by
 *          \p err_stack_id.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eset_current_stack(hid_t err_stack_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Pushes a new error record onto an error stack
 *
 * \estack_id{err_stack}. If the identifier is #H5E_DEFAULT, the error record
 *                        will be pushed to the current stack.
 * \param[in] file Name of the file in which the error was detected
 * \param[in] func Name of the function in which the error was detected
 * \param[in] line Line number in the file where the error was detected
 * \param[in] cls_id Error class identifier
 * \param[in] maj_id Major error identifier
 * \param[in] min_id Minor error identifier
 * \param[in] msg Error description string
 * \return \herr_t
 *
 * \details H5Epush2() pushes a new error record onto the error stack specified
 *          by \p err_stack.\n
 *          The error record contains the error class identifier \p cls_id, the
 *          major and minor message identifiers \p maj_id and \p min_id, the
 *          function name \p func where the error was detected, the file name \p
 *          file and line number \p line in the file where the error was
 *          detected, and an error description \p msg.\n
 *          The major and minor errors must be in the same error class.\n
 *          The function name, filename, and error description strings must be
 *          statically allocated.\n
 *          \p msg can be a format control string with additional
 *          arguments. This design of appending additional arguments is similar
 *          to the system and C functions printf() and fprintf().
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Epush2(hid_t err_stack, const char *file, const char *func, unsigned line, hid_t cls_id,
                       hid_t maj_id, hid_t min_id, const char *msg, ...);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Deletes specified number of error messages from the error stack
 *
 * \estack_id{err_stack}
 * \param[in] count The number of error messages to be deleted from the top
 *                  of error stack
 * \return \herr_t
 *
 * \details H5Epop() deletes the number of error records specified in \p count
 *          from the top of the error stack specified by \p err_stack (including
 *          major, minor messages and description). The number of error messages
 *          to be deleted is specified by \p count.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Epop(hid_t err_stack, size_t count);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Prints the specified error stack in a default manner
 *
 * \estack_id{err_stack}
 * \param[in] stream File pointer, or \c NULL for \c stderr
 * \return \herr_t
 *
 * \details H5Eprint2() prints the error stack specified by \p err_stack on the
 *          specified stream, \p stream. Even if the error stack is empty, a
 *          one-line message of the following form will be printed:
 *          \code{.unparsed}
 *          HDF5-DIAG: Error detected in HDF5 library version: 1.5.62 thread 0.
 *          \endcode
 *
 *          A similar line will appear before the error messages of each error
 *          class stating the library name, library version number, and thread
 *          identifier.
 *
 *          If \p err_stack is #H5E_DEFAULT, the current error stack will be
 *          printed.
 *
 *          H5Eprint2() is a convenience function for H5Ewalk2() with a function
 *          that prints error messages. Users are encouraged to write their own
 *          more specific error handlers.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eprint2(hid_t err_stack, FILE *stream);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Walks the specified error stack, calling the specified function
 *
 * \estack_id{err_stack}
 * \param[in] direction Direction in which the error stack is to be walked
 * \param[in] func Function to be called for each error encountered
 * \param[in] client_data Data to be passed to \p func
 * \return \herr_t
 *
 * \details H5Ewalk2() walks the error stack specified by err_stack for the
 *          current thread and calls the function specified in \p func for each
 *          error along the way.
 *
 *          If the value of \p err_stack is #H5E_DEFAULT, then H5Ewalk2() walks
 *          the current error stack.
 *
 *          \p direction specifies whether the stack is walked from the inside
 *          out or the outside in. A value of #H5E_WALK_UPWARD means to begin
 *          with the most specific error and end at the API; a value of
 *          #H5E_WALK_DOWNWARD means to start at the API and end at the
 *          innermost function where the error was first detected.
 *
 *          \p func, a function conforming to the #H5E_walk2_t prototype, will
 *          be called for each error in the error stack. Its arguments will
 *          include an index number \c n (beginning at zero regardless of stack
 *          traversal direction), an error stack entry \c err_desc, and the \c
 *          client_data pointer passed to H5Eprint(). The #H5E_walk2_t prototype
 *          is as follows:
 *          \snippet this H5E_walk2_t_snip
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Ewalk2(hid_t err_stack, H5E_direction_t direction, H5E_walk2_t func, void *client_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Returns the settings for the automatic error stack traversal
 *        function and its data
 *
 * \estack_id
 * \param[out] func The function currently set to be called upon an error condition
 * \param[out] client_data Data currently set to be passed to the error function
 * \return \herr_t
 *
 * \details H5Eget_auto2() returns the settings for the automatic error stack
 *          traversal function, \p func, and its data, \p client_data, that are
 *          associated with the error stack specified by \p estack_id.
 *
 *          Either or both of the \p func and \p client_data arguments may be
 *          \c NULL, in which case the value is not returned.
 *
 *          The library initializes its default error stack traversal functions
 *          to H5Eprint1() and H5Eprint2(). A call to H5Eget_auto2() returns
 *          H5Eprint2() or the user-defined function passed in through
 *          H5Eset_auto2(). A call to H5Eget_auto1() returns H5Eprint1() or the
 *          user-defined function passed in through H5Eset_auto1(). However, if
 *          the application passes in a user-defined function through
 *          H5Eset_auto1(), it should call H5Eget_auto1() to query the traversal
 *          function. If the application passes in a user-defined function
 *          through H5Eset_auto2(), it should call H5Eget_auto2() to query the
 *          traversal function.
 *
 *          Mixing the new style and the old style functions will cause a
 *          failure. For example, if the application sets a user-defined
 *          old-style traversal function through H5Eset_auto1(), a call to
 *          H5Eget_auto2() will fail and will indicate that the application has
 *          mixed H5Eset_auto1() and H5Eget_auto2(). On the other hand, mixing
 *          H5Eset_auto2() and H5Eget_auto1() will also cause a failure. But if
 *          the traversal functions are the library's default H5Eprint1() or
 *          H5Eprint2(), mixing H5Eset_auto1() and H5Eget_auto2() or mixing
 *          H5Eset_auto2() and H5Eget_auto1() does not fail.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eget_auto2(hid_t estack_id, H5E_auto2_t *func, void **client_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Turns automatic error printing on or off
 *
 * \estack_id
 * \param[in] func Function to be called upon an error condition
 * \param[in] client_data Data passed to the error function
 * \return \herr_t
 *
 * \details H5Eset_auto2() turns on or off automatic printing of errors for the
 *          error stack specified with \p estack_id. An \p estack_id value of
 *          #H5E_DEFAULT indicates the current stack.
 *
 *          When automatic printing is turned on, by the use of a non-null \p func
 *          pointer, any API function which returns an error indication will
 *          first call \p func, passing it \p client_data as an argument.
 *
 *          \p func, a function compliant with the #H5E_auto2_t prototype, is
 *          defined in the H5Epublic.h source code file as:
 *          \snippet this H5E_auto2_t_snip
 *
 *          When the library is first initialized, the auto printing function is
 *          set to H5Eprint2() (cast appropriately) and \p client_data is the
 *          standard error stream pointer, \c stderr.
 *
 *          Automatic stack traversal is always in the #H5E_WALK_DOWNWARD
 *          direction.
 *
 *          Automatic error printing is turned off with a H5Eset_auto2() call
 *          with a \c NULL \p func pointer.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eset_auto2(hid_t estack_id, H5E_auto2_t func, void *client_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Clears the specified error stack or the error stack for the current thread
 *
 * \estack_id{err_stack}
 * \return \herr_t
 *
 * \details H5Eclear2() clears the error stack specified by \p err_stack, or, if
 *          \p err_stack is set to #H5E_DEFAULT, the error stack for the current
 *          thread.
 *
 *          \p err_stack is an error stack identifier, such as that returned by
 *          H5Eget_current_stack().
 *
 *          The current error stack is also cleared whenever an API function is
 *          called, with certain exceptions (for instance, H5Eprint1() or
 *          H5Eprint2()).
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eclear2(hid_t err_stack);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Determines the type of error stack
 *
 * \estack_id{err_stack}
 * \param[out] is_stack A flag indicating which error stack \c typedef the
 *                      specified error stack conforms to
 *
 * \return \herr_t
 *
 * \details H5Eauto_is_v2() determines whether the error auto reporting function
 *          for an error stack conforms to the #H5E_auto2_t \c typedef or the
 *          #H5E_auto1_t \c typedef.
 *
 *          The \p is_stack parameter is set to 1 if the error stack conforms to
 *          #H5E_auto2_t and 0 if it conforms to #H5E_auto1_t.
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Eauto_is_v2(hid_t err_stack, unsigned *is_stack);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Retrieves an error message
 *
 * \param[in] msg_id Error message identifier
 * \param[out] type The type of the error message. Valid values are #H5E_MAJOR
 *                  and #H5E_MINOR.
 * \param[out] msg Error message buffer
 * \param[in] size The length of error message to be returned by this function
 * \return Returns the size of the error message in bytes on success; otherwise
 *         returns a negative value.
 *
 * \details H5Eget_msg() retrieves the error message including its length and
 *          type. The error message is specified by \p msg_id. The user is
 *          responsible for passing in sufficient buffer space for the
 *          message. If \p msg is not NULL and \p size is greater than zero, the
 *          error message of \p size long is returned. The length of the message
 *          is also returned. If NULL is passed in as \p msg, only the length
 *          and type of the message is returned. If the return value is zero, it
 *          means there is no message.
 *
 * \since 1.8.0
 */
H5_DLL ssize_t H5Eget_msg(hid_t msg_id, H5E_type_t *type, char *msg, size_t size);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Retrieves the number of error messages in an error stack
 *
 * \estack_id{error_stack_id}
 * \return Returns number of error messages in an error stack on
 *         success; otherwise returns a negative value.
 *
 * \details H5Eget_num() retrieves the number of error records in the error
 *          stack specified by \p error_stack_id (including major, minor
 *          messages and description).
 *
 * \since 1.8.0
 */
H5_DLL ssize_t H5Eget_num(hid_t error_stack_id);

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/* Typedefs */

/* Alias major & minor error types to hid_t's, for compatibility with new
 *      error API in v1.8
 */
typedef hid_t H5E_major_t;
typedef hid_t H5E_minor_t;

/**
 * Information about an error element of error stack.
 */
typedef struct H5E_error1_t {
    H5E_major_t maj_num;   /**< major error number                 */
    H5E_minor_t min_num;   /**< minor error number                 */
    const char *func_name; /**< function in which error occurred   */
    const char *file_name; /**< file in which error occurred       */
    unsigned    line;      /**< line in file where error occurs    */
    const char *desc;      /**< optional supplied description      */
} H5E_error1_t;

/* Error stack traversal callback function pointers */
//! <!-- [H5E_walk1_t_snip] -->
/**
 * \brief Callback function for H5Ewalk1()
 *
 * \param[in] n Indexed error position in the stack
 * \param[in] err_desc Pointer to a data structure describing the error
 * \param[in] client_data Pointer to client data in the format expected by the
 *                        user-defined function
 * \return \herr_t
 *
 * \since 1.0.0
 *
 */
typedef herr_t (*H5E_walk1_t)(int n, H5E_error1_t *err_desc, void *client_data);
//! <!-- [H5E_walk1_t_snip] -->

//! <!-- [H5E_auto1_t_snip] -->
/**
 * \brief Callback function for H5Eset_auto1()
 *
 * \param[in] client_data Pointer to client data in the format expected by the
 *                        user-defined function
 * \return \herr_t
 *
 * \since 1.0.0
 *
 */
typedef herr_t (*H5E_auto1_t)(void *client_data);
//! <!-- [H5E_auto1_t_snip] -->

/* Function prototypes */
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Clears the error stack for the current thread
 *
 * \return \herr_t
 *
 * \deprecated 1.8.0 Function H5Eclear() renamed to H5Eclear1() and deprecated
 *                   in this release.
 *
 * \details H5Eclear1() clears the error stack for the current thread.\n
 *          The stack is also cleared whenever an API function is called, with
 *          certain exceptions (for instance, H5Eprint1()).
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Eclear1(void);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Returns the current settings for the automatic error stack traversal
 *        function and its data
 *
 * \param[out] func Current setting for the function to be called upon an error
 *                  condition
 * \param[out] client_data Current setting for the data passed to the error
 *                         function
 * \return \herr_t
 *
 * \deprecated 1.8.0 Function H5Eget_auto() renamed to H5Eget_auto1() and
 *                   deprecated in this release.
 *
 * \details H5Eget_auto1() returns the current settings for the automatic error
 *          stack traversal function, \p func, and its data,
 *          \p client_data. Either or both arguments may be \c NULL, in which case the
 *          value is not returned.
 *
 *          The library initializes its default error stack traversal functions
 *          to H5Eprint1() and H5Eprint2(). A call to H5Eget_auto2() returns
 *          H5Eprint2() or the user-defined function passed in through
 *          H5Eset_auto2(). A call to H5Eget_auto1() returns H5Eprint1() or the
 *          user-defined function passed in through H5Eset_auto1(). However, if
 *          the application passes in a user-defined function through
 *          H5Eset_auto1(), it should call H5Eget_auto1() to query the traversal
 *          function. If the application passes in a user-defined function
 *          through H5Eset_auto2(), it should call H5Eget_auto2() to query the
 *          traversal function.
 *
 *          Mixing the new style and the old style functions will cause a
 *          failure. For example, if the application sets a user-defined
 *          old-style traversal function through H5Eset_auto1(), a call to
 *          H5Eget_auto2() will fail and will indicate that the application has
 *          mixed H5Eset_auto1() and H5Eget_auto2(). On the other hand, mixing
 *          H5Eset_auto2() and H5Eget_auto1() will also cause a failure. But if
 *          the traversal functions are the library's default H5Eprint1() or
 *          H5Eprint2(), mixing H5Eset_auto1() and H5Eget_auto2() or mixing
 *          H5Eset_auto2() and H5Eget_auto1() does not fail.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Eget_auto1(H5E_auto1_t *func, void **client_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Pushes a new error record onto the error stack
 *
 * \param[in] file Name of the file in which the error was detected
 * \param[in] func Name of the function in which the error was detected
 * \param[in] line Line number in the file where the error was detected
 * \param[in] maj Major error identifier
 * \param[in] min Minor error identifier
 * \param[in] str Error description string
 * \return \herr_t
 *
 * \deprecated 1.8.0 Function H5Epush() renamed to H5Epush1() and
 *                   deprecated in this release.
 *
 * \details H5Epush1() pushes a new error record onto the error stack for the
 *          current thread.\n
 *          The error has major and minor numbers \p maj_num
 *          and \p min_num, the function \p func where the error was detected, the
 *          name of the file \p file where the error was detected, the line \p line
 *          within that file, and an error description string \p str.\n
 *          The function name, filename, and error description strings must be statically
 *          allocated.
 *
 * \since 1.4.0
 */
H5_DLL herr_t H5Epush1(const char *file, const char *func, unsigned line, H5E_major_t maj, H5E_minor_t min,
                       const char *str);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Prints the current error stack in a default manner
 *
 * \param[in] stream File pointer, or \c NULL for \c stderr
 * \return \herr_t
 *
 * \deprecated 1.8.0 Function H5Eprint() renamed to H5Eprint1() and
 *                   deprecated in this release.
 *
 * \details H5Eprint1() prints the error stack for the current thread
 *          on the specified stream, \p stream. Even if the error stack is empty, a
 *          one-line message of the following form will be printed:
 *          \code{.unparsed}
 *          HDF5-DIAG: Error detected in thread 0.
 *          \endcode
 *          H5Eprint1() is a convenience function for H5Ewalk1() with a function
 *          that prints error messages. Users are encouraged to write their own
 *          more specific error handlers.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Eprint1(FILE *stream);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Turns automatic error printing on or off
 *
 * \param[in] func Function to be called upon an error condition
 * \param[in] client_data Data passed to the error function
 * \return \herr_t
 *
 * \deprecated 1.8.0 Function H5Eset_auto() renamed to H5Eset_auto1() and
 *                   deprecated in this release.
 *
 * \details H5Eset_auto1() turns on or off automatic printing of errors. When
 *          turned on (non-null \p func pointer), any API function which returns
 *          an error indication will first call \p func, passing it \p
 *          client_data as an argument.
 *
 *          \p func, a function conforming to the #H5E_auto1_t prototype, is
 *          defined in the H5Epublic.h source code file as:
 *          \snippet this H5E_auto1_t_snip
 *
 *          When the library is first initialized, the auto printing function is
 *          set to H5Eprint1() (cast appropriately) and \p client_data is the
 *          standard error stream pointer, \c stderr.
 *
 *          Automatic stack traversal is always in the #H5E_WALK_DOWNWARD
 *          direction.
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Eset_auto1(H5E_auto1_t func, void *client_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Walks the current error stack, calling the specified function
 *
 * \param[in] direction Direction in which the error stack is to be walked
 * \param[in] func Function to be called for each error encountered
 * \param[in] client_data Data to be passed to \p func
 * \return \herr_t
 *
 * \deprecated 1.8.0 Function H5Ewalk() renamed to H5Ewalk1() and
 *                   deprecated in this release.
 *
 * \details H5Ewalk1() walks the error stack for the current thread and calls
 *          the function specified in \p func for each error along the way.
 *
 *          \p direction specifies whether the stack is walked from the inside
 *          out or the outside in. A value of #H5E_WALK_UPWARD means to begin
 *          with the most specific error and end at the API; a value of
 *          #H5E_WALK_DOWNWARD means to start at the API and end at the
 *          innermost function where the error was first detected.
 *
 *          \p func, a function conforming to the #H5E_walk1_t prototype, will
 *          be called for each error in the error stack. Its arguments will
 *          include an index number \c n (beginning at zero regardless of stack
 *          traversal direction), an error stack entry \c err_desc, and the \c
 *          client_data pointer passed to H5Eprint(). The #H5E_walk1_t prototype
 *          is as follows:
 *          \snippet this H5E_walk1_t_snip
 *
 * \since 1.0.0
 *
 */
H5_DLL herr_t H5Ewalk1(H5E_direction_t direction, H5E_walk1_t func, void *client_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Returns a character string describing an error specified by a major
 *        error number
 *
 * \param[in] maj Major error number
 * \return Pointer to the message (success), or NULL (failure)
 *
 * \deprecated 1.8.0 Function deprecated in this release.
 *
 * \details H5Eget_major() returns a constant
 *          character string that describes the error, given a major error number.
 *
 * \attention This function returns a dynamically allocated string (\c char
 *            array). An application calling this function must free the memory
 *            associated with the return value to prevent a memory leak.
 *
 * \since 1.0.0
 *
 */
H5_DLL char *H5Eget_major(H5E_major_t maj);
/**
 * --------------------------------------------------------------------------
 * \ingroup H5E
 *
 * \brief Returns a character string describing an error specified by a minor
 *        error number
 *
 * \param[in] min Minor error number
 * \return Pointer to the message (success), or NULL (failure)
 *
 * \deprecated 1.8.0 Function deprecated and return type changed in this release.
 *
 * \details H5Eget_minor() returns a constant
 *          character string that describes the error, given a minor error number.
 *
 * \attention In the Release 1.8.x series, H5Eget_minor() returns a string of
 *            dynamic allocated \c char array. An application calling this
 *            function from an HDF5 library of Release 1.8.0 or later must free
 *            the memory associated with the return value to prevent a memory
 *            leak. This is a change from the 1.6.x release series.
 *
 * \since 1.0.0
 *
 */
H5_DLL char *H5Eget_minor(H5E_minor_t min);
#endif /* H5_NO_DEPRECATED_SYMBOLS */

#ifdef __cplusplus
}
#endif

#endif /* end H5Epublic_H */
