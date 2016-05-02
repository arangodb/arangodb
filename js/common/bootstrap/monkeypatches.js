/*eslint no-extend-native:0 */
/*eslint-disable */
(function () {
'use strict';
/*eslint-enable */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow copies properties
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, '_shallowCopy', {
  set(value) {
    if (this === Object.prototype) {
      throw new TypeError(`Can not override Object.prototype._shallowCopy!`);
    }
    Object.defineProperty(this, '_shallowCopy', {
      configurable: true,
      enumerable: true,
      writable: true,
      value
    });
  },
  get() {
    var self = this;
    return Object.prototype.propertyKeys.reduce(function (previous, key) {
      previous[key] = self[key];
      return previous;
    }, {});
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the property keys
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, 'propertyKeys', {
  get() {
    return Object.keys(this).filter(function (key) {
      return (key.charAt(0) !== '_' && key.charAt(0) !== '$');
    });
  },
  set(value) {
    if (this === Object.prototype) {
      throw new TypeError(`Can not override Object.prototype.propertyKeys!`);
    }
    Object.defineProperty(this, 'propertyKeys', {
      configurable: true,
      enumerable: true,
      writable: true,
      value
    });
  },
});


}());

