// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The valueOf function is not generic; it throws a TypeError exception if its this value is not a String object.
    Therefore, it cannot be transferred to other kinds of objects for use as a method
es5id: 15.5.4.3_A2_T1
description: Checking if creating variable String.prototype.valueOf fails
---*/

var __valueOf = String.prototype.valueOf;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (typeof __valueOf !== "function") {
  $ERROR('#1: __valueOf = String.prototype.valueOf; typeof __valueOf === "function". Actual: typeof __valueOf ==='+typeof __valueOf ); 
}

//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
try {
  var x = __valueOf();
  $ERROR('#2: "__valueOf = String.prototype.valueOf; var x = __valueOf()" lead to throwing exception');
} catch (e) {
  if (!(e instanceof TypeError)) {
    $ERROR('#2.1: Exception is instance of TypeError. Actual: exception is '+e);
  }
}
//
//////////////////////////////////////////////////////////////////////////////
