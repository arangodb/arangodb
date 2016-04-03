// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Error.prototype property has the attributes {ReadOnly}
es5id: 15.11.3.1_A3_T1
description: Checking if varying the Error.prototype property fails
includes: [propertyHelper.js]
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (!(Error.hasOwnProperty('prototype'))) {
  $ERROR('#1: Error.hasOwnProperty(\'prototype\') return true. Actual: '+Error.hasOwnProperty('prototype'));
}
//
//////////////////////////////////////////////////////////////////////////////

var __obj = Error.prototype;

verifyNotWritable(Error, "prototype", null, function(){return "shifted";});

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (Error.prototype !== __obj) {
  $ERROR('#2: __obj = Error.prototype; Error.prototype = function(){return "shifted";}; Error.prototype === __obj. Actual: '+Error.prototype );
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#3
try {
  Error.prototype();
  $ERROR('#3: "Error.prototype()" lead to throwing exception');
} catch (e) {
  if (e instanceof Test262Error) throw e;
}
//
//////////////////////////////////////////////////////////////////////////////
