// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The String.prototype.split.length property has the attribute ReadOnly
es5id: 15.5.4.14_A10
description: >
    Checking if varying the String.prototype.split.length property
    fails
includes: [propertyHelper.js]
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (!(String.prototype.split.hasOwnProperty('length'))) {
  $ERROR('#1: String.prototype.split.hasOwnProperty(\'length\') return true. Actual: '+String.prototype.split.hasOwnProperty('length'));
}
//
//////////////////////////////////////////////////////////////////////////////

var __obj = String.prototype.split.length;

verifyNotWritable(String.prototype.split, "length", null, function(){return "shifted";});

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (String.prototype.split.length !== __obj) {
  $ERROR('#2: __obj = String.prototype.split.length; String.prototype.split.length = function(){return "shifted";}; String.prototype.split.length === __obj. Actual: '+String.prototype.split.length );
}
//
//////////////////////////////////////////////////////////////////////////////
