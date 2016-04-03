// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: the prototype property has the attributes { DontDelete }
es5id: 15.3.5.2_A1_T1
description: >
    Checking if deleting the prototype property of Function("", null)
    fails
includes: [propertyHelper.js]
---*/

var f = new Function("", null);

//CHECK#1
if (!(f.hasOwnProperty('prototype'))) {
  $ERROR('#1: the function has length property.');
}

var fproto = f.prototype;

verifyNotConfigurable(f, "prototype");

//CHECK#2
try {
  if ((delete f.prototype) !== false) {
    $ERROR('#2: the prototype property has the attributes { DontDelete }');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}

//CHECK#3
if (f.prototype !== fproto) {
  $ERROR('#3: the prototype property has the attributes { DontDelete }');
}
