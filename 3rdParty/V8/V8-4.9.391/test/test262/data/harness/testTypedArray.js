// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/**
 * Array containing every typed array constructor.
 */
var typedArrayConstructors = [
  Float64Array,
  Float32Array,
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array,
  Uint8ClampedArray,
];

/**
 * Callback for testing a typed array constructor.
 *
 * @callback typedArrayConstructorCallback
 * @param {Function} Constructor the constructor object to test with.
 */

/**
 * Calls the provided function for every typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 */
function testWithTypedArrayConstructors(f) {
  for (var i = 0; i < typedArrayConstructors.length; ++i) {
    var constructor = typedArrayConstructors[i];
    try {
      f(constructor);
    } catch (e) {
      e.message += " (Testing with " + constructor.name + ".)";
      throw e;
    }
  }
}
