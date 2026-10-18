// C++ informative line for the emacs editor: -*- C++ -*-
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

#ifndef H5CppDoc_H
#define H5CppDoc_H

//-------------------------------------------------------------------------
// The following section will be used to generate the 'Mainpage'
// and the 'Examples' for the RM.
// ------------------------------------------------------------------------

/*! \mainpage notitle
 * <br />
 * \section intro_sec Introduction
 *
 * The C++ API provides C++ wrappers for the HDF5 C Library.
 *
 * It is assumed that the user has knowledge of the HDF5 file format and its
 * components.  For more information on the HDF5 C Library, please refer to
 * the HDF5 Software Documentation page.
 *
 * Because the HDF5 C Library maps very well to
 * the object oriented design approach, classes in the C++ API can
 * closely represent the interfaces of the C APIs as follows:
 *
 * \verbatim
    HDF5 C APIs                       C++ Classes
    -----------                       -----------
    Attribute Interface (H5A)         Attribute
    Datasets Interface (H5D)          DataSet
    Error Interface (H5E)             Exception
    File Interface (H5F)              H5File
    Group Interface (H5G)             Group
    Identifier Interface (H5I)        IdComponent
    Property List Interface (H5P)     PropList and subclasses
    Dataspace Interface (H5S)         DataSpace
    Datatype Interface (H5T)          DataType and subclasses
  \endverbatim
 *
 * This <a href="./C2Cppfunction_map.htm">
 * table </a> provides a map from the C APIs to the C++ member functions.
 * <br />
 * \section install_sec Installation
 *
 * The HDF5 C++ API is included with the HDF5 source code.
 *
 * Please refer to the release_docs/INSTALL file under the top directory
 * of the HDF5 source code for information about installing, building,
 * and testing the C++ API.
 *
 * <br />
 */

///        This example shows how to create datasets.
///\par
///\example     create.cpp

///\par
///        This example shows how to write datasets.
///\example     writedata.cpp

///\par
///        This example shows how to read datasets.
///\example     readdata.cpp

///\par
///        This example shows how to create a compound datatype,
///        write an array which has the compound datatype to the file,
///        and read back fields' subsets.
///\example     compound.cpp

///\par
///        This example shows how to work with extendible datasets.
///\example     extend_ds.cpp

///\par
///        This example shows how to read data from a chunked dataset.
///\example     chunks.cpp

///\par
///        This example shows how to work with groups.
///\example     h5group.cpp

#endif // H5CppDoc_H
