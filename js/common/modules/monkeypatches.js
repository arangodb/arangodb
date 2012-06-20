/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         plusplus: true */

////////////////////////////////////////////////////////////////////////////////
/// @brief Monkeypatches to built-in Classes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief remove last occurrence of element from an array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "removeLastOccurrenceOf", {
  value: function (element) {
    var index = this.lastIndexOf(element);
    return this.splice(index, 1);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief return the union with another array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "unite", {
  value: function (other_array) {
    return other_array.concat(this.filter(function (element) {
      return (other_array.indexOf(element) === -1);
    }));
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief return the intersection with another array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "intersect", {
  value: function (other_array) {
    return this.filter(function (element) {
      return (other_array.indexOf(element) > -1);
    });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow copy properties
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, "shallowCopy", {
  get: function () {
    var shallow = {},
      key;

    for (key in this) {
      if (this.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
        shallow[key] = this[key];
      }
    }

    return shallow;
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief property keys
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, "propertyKeys", {
  get: function () {
    var keys = [],
      key;

    for (key in this) {
      if (this.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
        keys.push(key);
      }
    }

    return keys;
  }
});

