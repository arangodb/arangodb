// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object.prototype.toString can't be used as a constructor
es5id: 15.2.4.2_A7
description: Checking if creating "new Object.prototype.toString" fails
---*/

var FACTORY = Object.prototype.toString;

assert.throws(TypeError, function() {
  new FACTORY;
});
