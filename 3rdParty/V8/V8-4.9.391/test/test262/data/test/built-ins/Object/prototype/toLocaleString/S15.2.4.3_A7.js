// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object.prototype.toLocaleString can't be used as a constructor
es5id: 15.2.4.3_A7
description: Checking if creating "new Object.prototype.toLocaleString" fails
---*/

var FACTORY = Object.prototype.toLocaleString;

assert.throws(TypeError, function() {
  new FACTORY;
});
