////////////////////////////////////////////////////////////////////////////////
/// @brief storage engine
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
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
