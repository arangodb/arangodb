// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: the length property has the attributes { ReadOnly }
es5id: 15.3.5.1_A3_T2
description: >
    Checking if varying the length property of
    Function("arg1,arg2,arg3", null) fails
includes: [propertyHelper.js]
---*/

var f =  Function("arg1,arg2,arg3", null);

//CHECK#1
if (!(f.hasOwnProperty('length'))) {
  $ERROR('#1: the function has length property.');
}

var flength = f.length;

verifyNotWritable(f, "length", null, function(){});

//CHECK#2
if (f.length !== flength) {
  $ERROR('#2: the function.length property has the attributes ReadOnly');
}

//CHECK#3
try {
  f.length();
  $ERROR('#3: the function.length property has the attributes ReadOnly');
} catch (e) {
  if (e instanceof Test262Error) throw e;
}

//CHECK#4
if (f.length !== 3) {
  $ERROR('#4: the length property has the attributes { ReadOnly }');
}
