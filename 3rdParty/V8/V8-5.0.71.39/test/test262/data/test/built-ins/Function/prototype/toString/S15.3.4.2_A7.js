// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Function.prototype.toString can't be used as constructor
es5id: 15.3.4.2_A7
description: Checking if creating "new Function.prototype.toString" fails
---*/

var FACTORY = Function.prototype.toString;

assert.throws(TypeError, function() {
  new FACTORY;
});
