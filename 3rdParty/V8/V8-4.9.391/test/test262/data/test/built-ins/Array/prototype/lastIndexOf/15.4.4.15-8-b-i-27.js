// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-8-b-i-27
description: >
    Array.prototype.lastIndexOf applied to Arguments object which
    implements its own property get method (number of arguments is
    greater than number of parameters)
---*/

var func = function (a, b) {
  assert.sameValue(Array.prototype.lastIndexOf.call(arguments, arguments[0]), 2);
  assert.sameValue(Array.prototype.lastIndexOf.call(arguments, arguments[3]), 3);
  assert.sameValue(Array.prototype.lastIndexOf.call(arguments, arguments[4]), -1);
};

(function() {
  func(0, arguments, 0, Object.prototype);
})();
