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

#ifndef H5IMpublic_H
#define H5IMpublic_H

#ifdef __cplusplus
extern "C" {
#endif

/** \page H5IM_UG HDF5 High Level Images
 * @todo Under Construction
 */

/**\defgroup H5IM HDF5 Images API (H5IM)
 *
 * <em>Creating and manipulating HDF5 datasets intended to be
 * interpreted as images (H5IM)</em>
 *
 * The specification for the Images API is presented in another
 * document: \ref IMG
 * This version of the API is primarily concerned with two dimensional raster
 * data similar to HDF4 Raster Images.
 * The HDF5 Images API uses the \ref H5LT.
 *
 * \note \Bold{Programming hints:}
 * \note To use any of these functions or subroutines,
 *       you must first include the relevant include file (C) or
 *       module (Fortran) in your application.
 * \note The following line includes the HDF5 Images package, H5IM,
 *       in C applications:
 *       \code #include "hdf5_hl.h" \endcode
 * \note This line includes the H5IM module in Fortran applications:
 *       \code use h5im \endcode
 *
 * - \ref H5IMget_image_info
 *    \n Gets information about an image dataset (dimensions,
 *       interlace mode and number of associated palettes).
 * - \ref H5IMget_npalettes
 *   \n Gets the number of palettes associated to an image.
 * - \ref H5IMget_palette
 *   \n Gets the palette dataset.
 * - \ref H5IMget_palette_info
 *   \n Gets information about a palette dataset (dimensions).
 * - \ref H5IMis_image
 *   \n Inquires if a dataset is an image
 * - \ref H5IMis_palette
 *   \n Inquires if a dataset is a palette.
 * - \ref H5IMlink_palette
 *   \n Attaches a palette to an image.
 * - \ref H5IMmake_image_8bit
 *   \n Creates and writes an image.
 * - \ref H5IMmake_image_24bit
 *   \n Creates and writes a true color image.
 * - \ref H5IMmake_palette
 *   \n Creates and writes a palette.
 * - \ref H5IMread_image
 *   \n Reads image data from disk.
 * - \ref H5IMunlink_palette
 *   \n Detaches a palette from an image.
 *
 */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Creates and writes an image.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to create
 * \param[in] width     The width of the image
 * \param[in] height    The height of the image
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5IMmake_image_8bit() creates and writes a dataset named
 *          \p dset_name attached to the file or group specified by the
 *          identifier \p loc_id. Attributes conforming to the HDF5 Image
 *          and Palette specification for an indexed image are attached to
 *          the dataset, thus identifying it as an image. The image data is
 *          of the type #H5T_NATIVE_UCHAR. An indexed image is an image in
 *          which each each pixel information storage is an index to a
 *          table palette.
 *
 */
H5_HLDLL herr_t H5IMmake_image_8bit(hid_t loc_id, const char *dset_name, hsize_t width, hsize_t height,
                                    const unsigned char *buffer);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Creates and writes a true color image.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to create
 * \param[in] width     The width of the image
 * \param[in] height    The height of the image
 * \param[in] interlace String defining the interlace mode
 * \param[in] buffer    Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5IMmake_image_24bit() creates and writes a dataset named
 *          \p dset_name attached to the file or group specified by the
 *          identifier \p loc_id. This function defines a true color image
 *          conforming to the HDF5 Image and Palette specification.
 *          The function assumes that the image data is of the type
 *          #H5T_NATIVE_UCHAR.
 *
 *          A true color image is an image where the pixel storage contains
 *          several color planes. In a 24 bit RGB color model, these planes
 *          are red, green and blue. In a true color image the stream of bytes
 *          can be stored in several different ways, thus defining the
 *          interlace (or interleaving) mode. The 2 most used types of interlace mode
 *          are interlace by pixel and interlace by plane. In the 24 bit RGB color
 *          model example, interlace by plane means all the red components for the
 *          entire dataset are stored first, followed by all the green components,
 *          and then by all the blue components. Interlace by pixel in this example
 *          means that for each pixel the sequence red, green, blue is defined.
 *          In this function, the interlace mode is defined in the parameter
 *          \p interlace, a string that can have the values INTERLACE_PIXEL
 *          or INTERLACE_PLANE.
 *
 */
H5_HLDLL herr_t H5IMmake_image_24bit(hid_t loc_id, const char *dset_name, hsize_t width, hsize_t height,
                                     const char *interlace, const unsigned char *buffer);

/**
 *-------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Gets information about an image dataset
 *        (dimensions, interlace mode and number of associated palettes).
 *
 * \fg_loc_id
 * \param[in] dset_name     The name of the dataset
 * \param[out] width        The width of the image
 * \param[out] height       The height of the image
 * \param[out] planes       The number of color planes of the image
 * \param[out] interlace    The interlace mode of the image
 * \param[out] npals        The number of palettes associated to the image
 *
 * \return \herr_t
 *
 * \details H5IMget_image_info() gets information about an image
 *          named \p dset_name attached to the file or group specified
 *          by the identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5IMget_image_info(hid_t loc_id, const char *dset_name, hsize_t *width, hsize_t *height,
                                   hsize_t *planes, char *interlace, hssize_t *npals);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Reads image data from disk.
 *
 * \fg_loc_id
 * \param[in] dset_name The name of the dataset to create
 * \param[out] buffer   Buffer with data to store the image
 *
 * \return \herr_t
 *
 * \details H5IMread_image() reads a dataset named \p dset_name
 *          attached to the file or group specified by the
 *          identifier \p loc_id.
 *
 */
H5_HLDLL herr_t H5IMread_image(hid_t loc_id, const char *dset_name, unsigned char *buffer);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Creates and writes a palette.
 *
 * \fg_loc_id
 * \param[in] pal_name  The name of the palette
 * \param[in] pal_dims  An array of the size of the palette dimensions
 * \param[in] pal_data  Buffer with data to be written to the dataset
 *
 * \return \herr_t
 *
 * \details H5IMmake_palette() creates and writes a dataset
 *          named \p pal_name. Attributes conforming to the HDF5 Image and
 *          Palette specification are attached to the dataset, thus
 *          identifying it as a palette. The palette data is of the
 *          type #H5T_NATIVE_UCHAR.
 *
 */
H5_HLDLL herr_t H5IMmake_palette(hid_t loc_id, const char *pal_name, const hsize_t *pal_dims,
                                 const unsigned char *pal_data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Attaches a palette to an image.
 *
 * \fg_loc_id
 * \param[in] image_name    The name of the dataset to attach the palette to
 * \param[in] pal_name      The name of the palette
 *
 * \return \herr_t
 *
 * \details H5IMlink_palette() attaches a palette named \p pal_name
 *          to an image specified by \p image_name. The image dataset
 *          may or not already have an attached palette. If it has,
 *          the array of palette references is extended to hold the reference
 *          to the new palette.
 *
 */
H5_HLDLL herr_t H5IMlink_palette(hid_t loc_id, const char *image_name, const char *pal_name);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Detaches a palette from an image.
 *
 * \fg_loc_id
 * \param[in] image_name    The name of the image dataset
 * \param[in] pal_name      The name of the palette
 *
 * \return \herr_t
 *
 * \details H5IMunlink_palette() detaches a palette from an image
 *          specified by \p image_name.
 *
 */
H5_HLDLL herr_t H5IMunlink_palette(hid_t loc_id, const char *image_name, const char *pal_name);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Gets the number of palettes associated to an image.
 *
 * \fg_loc_id
 * \param[in] image_name    The name of the image dataset
 * \param[out] npals        The number of palettes
 *
 * \return \herr_t
 *
 * \details H5IMget_npalettes() gets the number of palettes associated to
 *          an image specified by \p image_name.
 *
 */
H5_HLDLL herr_t H5IMget_npalettes(hid_t loc_id, const char *image_name, hssize_t *npals);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Gets information about a palette dataset (dimensions).
 *
 * \fg_loc_id
 * \param[in] image_name    The name of the image dataset
 * \param[in] pal_number    The zero based index that identifies
 *                          the palette
 * \param[out] pal_dims     The dimensions of the palette dataset
 *
 * \return \herr_t
 *
 * \details H5IMget_palette_info() gets the dimensions of the palette
 *          dataset identified by \p pal_number (a zero based index)
 *          associated to an image specified by \p image_name.
 *
 */
H5_HLDLL herr_t H5IMget_palette_info(hid_t loc_id, const char *image_name, int pal_number, hsize_t *pal_dims);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Gets the palette dataset.
 *
 * \fg_loc_id
 * \param[in] image_name    The name of the image dataset
 * \param[in] pal_number    The zero based index that identifies
 *                          the palette
 * \param[out] pal_data     The palette dataset
 *
 * \return \herr_t
 *
 * \details H5IMget_palette() gets the palette dataset identified
 *          by \p pal_number (a zero based index) associated to an
 *          image specified by \p image_name.
 *
 */
H5_HLDLL herr_t H5IMget_palette(hid_t loc_id, const char *image_name, int pal_number,
                                unsigned char *pal_data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Inquires if a dataset is an image.
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset
 *
 * \return \htri_t
 *
 * \details H5IMis_image() inquires if a dataset named \p dset_name,
 *          attached to the file or group specified by the identifier
 *          \p loc_id, is an image based on the HDF5 Image and Palette
 *          Specification.
 *
 */
H5_HLDLL herr_t H5IMis_image(hid_t loc_id, const char *dset_name);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5IM
 *
 * \brief Inquires if a dataset is a palette
 *
 * \fg_loc_id
 * \param[in] dset_name    The name of the dataset
 *
 * \return \htri_t
 *
 * \details H5IMis_palette() inquires if a dataset named \p dset_name,
 *          attached to the file or group specified by the
 *          identifier \p loc_id, is a palette based on the HDF5
 *          Image and Palette Specification.
 *
 */
H5_HLDLL herr_t H5IMis_palette(hid_t loc_id, const char *dset_name);

#ifdef __cplusplus
}
#endif

#endif
