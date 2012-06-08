////////////////////////////////////////////////////////////////////////////////
/// @brief storage engine
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @mainpage
///
/// The Durham Storage Engine stores documents (aka objects or JSON documents)
/// in datafiles within the filesystem. These documents are mapped into the
/// process space using mmap. Instead of using JSON, BSON, or XML to store the
/// documents a very concise representation, namely JSON Shapes, is used.
/// This allows for verbose attributes name, while still being space efficient.
/// Access to attributes can be precompiled and is therefore incredible fast.
///
/// @section Interface
///
/// The Store Engine groups documents into collections. Each
/// collection can be accessed using queries. These queries can either
/// be expressed using a query language or JavaScript.
///
/// @section DataStructures Data Structures
///
/// The key data structures are @ref ShapedJson "JSON Shapes". JSON Shapes
/// allow the Durham Storage Engine to store documents in a very space
/// efficient way.
///
/// @section CollectionsDatafiles Collections and Datafiles
///
/// All data is stored in datafiles. The internal structure of a datafile is
/// described in @ref DurhamDatafiles "Datafiles". All datafiles of a set of
/// documents are grouped together in a @ref DurhamCollections "Collection".
////////////////////////////////////////////////////////////////////////////////
