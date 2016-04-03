// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
  description: >
      If the `this` value does not have all of the internal slots of an Array
      Iterator Instance (22.1.5.3), throw a TypeError exception.
  es6id: 22.1.5.2.1
 ---*/

var array = [0];
var iterator = array[Symbol.iterator]();
var object = Object.create(iterator);

assert.throws(TypeError, function() {
  object.next();
});
