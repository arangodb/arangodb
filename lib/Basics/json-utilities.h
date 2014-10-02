////////////////////////////////////////////////////////////////////////////////
/// @brief utility functions for json objects
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_JSON__UTILITIES_H
#define ARANGODB_BASICS_JSON__UTILITIES_H 1

#include "Basics/Common.h"

#include "Basics/json.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
///
/// the values are first compared by their types, and only by their values if
/// the types are the same
/// returns -1 if lhs is smaller than rhs, 0 if lhs == rhs, and 1 if rhs is
/// greater than lhs.
///
/// If useUTF8 is set to true, strings will be converted using proper UTF8,
/// otherwise, strcmp is used, so it should only be used to test for equality
/// in this case.
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson (TRI_json_t const*, 
                           TRI_json_t const*,
                           bool useUTF8 = true);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if two json values are the same
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckSameValueJson (TRI_json_t const*, 
                             TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a json value is contained in a json list
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckInListJson (TRI_json_t const*, 
                          TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the elements of a list that are between the specified bounds
///
/// lower and upper are the bounds values. if both lower and upper have a value,
/// then each list element is checked against the range (lower ... uppper).
/// if either lower or upper are null, then the comparison is done as either
/// (-inf ... upper) or (lower ... +inf).
///
/// using the boolean flags includeLower and includeUpper it can be specified
/// whether the bounds values are part of the range (true) or not (false)
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_BetweenListJson (TRI_json_t const*,
                                 TRI_json_t const*,
                                 bool,
                                 TRI_json_t const*,
                                 bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief uniquify a sorted json list into a new list
///
/// it is a prerequisite that the input list is already sorted.
/// otherwise the result is unpredictable.
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UniquifyListJson (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union of two sorted json lists into a new list
///
/// the result list can be made unique or non-unique. it is a prerequisite that
/// both input lists are already sorted. otherwise the result is unpredictable.
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UnionizeListsJson (TRI_json_t const*,
                                   TRI_json_t const*,
                                   bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief create the intersection of two sorted json lists into a new list
///
/// the result list can be made unique or non-unique. it is a prerequisite that
/// both input lists are already sorted. otherwise the result is unpredictable.
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_IntersectListsJson (TRI_json_t const*,
                                    TRI_json_t const*,
                                    bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a json list in place
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_SortListJson (TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a JSON struct has duplicate attribute names
////////////////////////////////////////////////////////////////////////////////

bool TRI_HasDuplicateKeyJson (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two JSON documents into one
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_MergeJson (TRI_memory_zone_t*,
                           TRI_json_t const*,
                           TRI_json_t const*,
                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashJson (TRI_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document depending on a list
/// of attributes.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashJsonByAttributes (TRI_json_t const* json,
                                   char const *attributes[],
                                   int nrAttributes,
                                   bool docComplete,
                                   int* error);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
