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
 * This file contains public declarations for the H5PL module.
 */

#ifndef H5PLpublic_H
#define H5PLpublic_H

#include "H5public.h" /* Generic Functions                        */

/*******************/
/* Public Typedefs */
/*******************/

/** Special string to indicate no plugin loading \since 1.10.2 */
#define H5PL_NO_PLUGIN "::"

//! <!-- [H5PL_type_t_snip] -->
/**
 * Plugin type (bit-position) used by the plugin library
 */
typedef enum H5PL_type_t {
    H5PL_TYPE_ERROR  = -1, /**< Error                */
    H5PL_TYPE_FILTER = 0,  /**< Filter               */
    H5PL_TYPE_VOL    = 1,  /**< VOL connector        */
    H5PL_TYPE_VFD    = 2,  /**< VFD                  */
    H5PL_TYPE_NONE   = 3   /**< Sentinel: This must be last!   */
} H5PL_type_t;
//! <!-- [H5PL_type_t_snip] -->

/* Common dynamic plugin type flags used by the set/get_loading_state functions */
/** Flag for filter plugin \since 1.8.15 */
#define H5PL_FILTER_PLUGIN 0x0001
/** Flag for VOL plugin \since 1.12.0 */
#define H5PL_VOL_PLUGIN 0x0002
/** Flag for VFD plugin \since 1.14.0 */
#define H5PL_VFD_PLUGIN 0x0004
/** Flag for all plugin types \since 1.8.15 */
#define H5PL_ALL_PLUGIN 0xFFFF

#ifdef __cplusplus
extern "C" {
#endif

/* plugin state */
/**
 * \ingroup H5PL
 * \brief Controls the loadability of dynamic plugin types
 *
 * \param[in] plugin_control_mask The list of dynamic plugin types to enable or disable.\n
 *                                A plugin bit set to 0 (zero) prevents use of that dynamic plugin.\n
 *                                A plugin bit set to 1 (one) enables use of that dynamic plugin.\n
 *                                Setting \p plugin_control_mask to a negative value enables all dynamic
 *                                plugin types.\n
 *                                Setting \p plugin_control_mask to 0 (zero) disables all dynamic plugin\n
 *                                types.
 * \return \herr_t
 *
 * \details H5PLset_loading_state() uses one argument to enable or disable individual plugin types.
 *
 * \details The \p plugin_control_mask parameter is an encoded integer in which each bit controls a specific
 *          plugin type. Bit positions allocated to date are specified in \ref H5PL_type_t as follows:
 *          \snippet this H5PL_type_t_snip
 *
 *          A plugin bit set to 0 (zero) prevents the use of the dynamic plugin type corresponding to that bit
 *          position. A plugin bit set to 1 (one) allows the use of that dynamic plugin type.
 *
 *          All dynamic plugin types can be enabled by setting \p plugin_control_mask to a negative value. A
 *          value of 0 (zero) will disable all dynamic plugin types.
 *
 *          The loading of external dynamic plugins can be controlled during runtime with an environment
 *          variable, \c HDF5_PLUGIN_PRELOAD. H5PLset_loading_state() inspects the \c HDF5_PLUGIN_PRELOAD
 *          environment variable every time it is called. If the environment variable is set to the special
 *          \c :: string, all dynamic plugins are disabled.
 *
 * \warning The environment variable \c HDF5_PLUGIN_PRELOAD controls the loading of dynamic plugin types at
 *          runtime. If it is set to disable all plugin types, then it will disable them for \Emph{all}
 *          running programs that access the same variable instance.
 *
 * \since 1.8.15
 *
 */
H5_DLL herr_t H5PLset_loading_state(unsigned int plugin_control_mask);
/**
 * \ingroup H5PL
 * \brief Queries the loadability of dynamic plugin types
 *
 * \param[out] plugin_control_mask List of dynamic plugin types that are enabled or disabled.\n
 *                                 A plugin bit set to 0 (zero) indicates that the dynamic plugin type is
 *                                 disabled.\n
 *                                 A plugin bit set to 1 (one) indicates that the dynamic plugin type is
 *                                 enabled.\n
 *                                 If the value of \p plugin_control_mask is negative, all dynamic plugin
 *                                 types are enabled.\n
 *                                 If the value of \p plugin_control_mask is 0 (zero), all dynamic plugins
 *                                 are disabled.
 * \return \herr_t
 *
 * \details H5PLget_loading_state() retrieves the bitmask that controls whether a certain type of plugin
 *          (e.g.: filters, VOL drivers) will be loaded by the HDF5 library.
 *
 *          Bit positions allocated to date are specified in \ref H5PL_type_t as follows:
 *          \snippet this H5PL_type_t_snip
 *
 * \since 1.8.15
 *
 */
H5_DLL herr_t H5PLget_loading_state(unsigned int *plugin_control_mask /*out*/);
/**
 * \ingroup H5PL
 * \brief Inserts a plugin path at the end of the plugin search path list
 *
 * \param[in] search_path A plugin path
 * \return \herr_t
 *
 * \details H5PLappend() inserts a plugin path at the end of the plugin search path list.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5PLappend(const char *search_path);
/**
 * \ingroup H5PL
 * \brief Inserts a plugin path at the beginning of the plugin search path list
 *
 * \param[in] search_path A plugin path
 * \return \herr_t
 *
 * \details H5PLprepend() inserts a plugin path at the end of the plugin search path list.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5PLprepend(const char *search_path);
/**
 * \ingroup H5PL
 * \brief Replaces the path at the specified index in the plugin search path list
 *
 * \param[in] search_path A plugin path
 * \param[in] index Index
 * \return \herr_t
 *
 * \details H5PLreplace() replaces a plugin path at the specified index in the plugin search path list.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5PLreplace(const char *search_path, unsigned int index);
/**
 * \ingroup H5PL
 * \brief Inserts a path at the specified index in the plugin search path list
 *
 * \param[in] search_path A plugin path
 * \param[in] index Index
 * \return \herr_t
 *
 * \details H5PLinsert() inserts a plugin path at the specified index in the plugin search path list,
 *          moving other paths after \p index.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5PLinsert(const char *search_path, unsigned int index);
/**
 * \ingroup H5PL
 * \brief Removes a plugin path at a specified index from the plugin search path list
 *
 * \param[in] index Index
 * \return \herr_t
 *
 * \details H5PLremove() removes a plugin path at the specified \p index and compacts the plugin search path
 *          list.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5PLremove(unsigned int index);
/**
 * \ingroup H5PL
 * \brief Queries the plugin search path list at the specified index
 *
 * \param[in] index Index
 * \param[out] path_buf Pathname
 * \param[in] buf_size Size of \p path_buf
 * \return Returns the length of the path, a non-negative value, if successful; otherwise returns a negative
 *         value.
 *
 * \details H5PLget() queries the plugin path at a specified index. If \p path_buf is non-NULL then it writes
 *          up to \p buf_size bytes into that buffer and always returns the length of the path name.
 *
 *          If \p path_buf is NULL, this function will simply return the number of characters required to
 *          store the path name, ignoring \p path_buf and \p buf_size.
 *
 *          If an error occurs then the buffer pointed to by \p path_buf (NULL or non-NULL) is unchanged and
 *          the function returns a negative value. If a zero is returned for the name's length, then there is
 *          no path name associated with the index. and the \p path_buf buffer will be unchanged.
 *
 * \since 1.10.1
 *
 */
H5_DLL ssize_t H5PLget(unsigned int index, char *path_buf /*out*/, size_t buf_size);
/**
 * \ingroup H5PL
 * \brief Retrieves the number of stored plugin paths
 *
 * \param[out] num_paths Current length of the plugin search path list
 * \return \herr_t
 *
 * \details H5PLsize() retrieves the number of paths stored in the plugin search path list.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5PLsize(unsigned int *num_paths /*out*/);

#ifdef __cplusplus
}
#endif

#endif /* H5PLpublic_H */
