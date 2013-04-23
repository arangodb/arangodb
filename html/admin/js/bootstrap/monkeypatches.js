/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, plusplus: true */

////////////////////////////////////////////////////////////////////////////////
/// @brief monkey-patches to built-in prototypes
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
/// @author Lucas Dohmen
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    monkey-patches
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes last occurrence of element from an array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "removeLastOccurrenceOf", {
  value: function (element) {
    return this.splice(this.lastIndexOf(element), 1);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the union with another array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "unite", {
  value: function (other_array) {
    return other_array.concat(this.filter(function (element) {
      return (other_array.indexOf(element) === -1);
    }));
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the intersection with another array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "intersect", {
  value: function (other_array) {
    return this.filter(function (element) {
      return (other_array.indexOf(element) > -1);
    });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow copies properties
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, "_shallowCopy", {
  get: function () {
    var that = this;

    return this.propertyKeys.reduce(function (previous, element) {
      previous[element] = that[element];
      return previous;
    }, {});
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the property keys
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, "propertyKeys", {
  get: function () {
    return Object.keys(this).filter(function (element) {
      return (element[0] !== '_' && element[0] !== '$');
    });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
