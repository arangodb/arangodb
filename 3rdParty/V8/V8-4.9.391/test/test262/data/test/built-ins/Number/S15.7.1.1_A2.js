// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number() returns +0
es5id: 15.7.1.1_A2
description: Call Number() and check result
---*/

//CHECK#1
if( typeof Number() !== "number" ) {
  $ERROR('#1: typeof Number() should be "number", actual is "'+typeof Number()+'"');
}

//CHECK#2
if( Number() !== 0 ) {
  $ERROR('#2: Number() === 0, actual is '+Number());
} else if( 1/Number() !== Number.POSITIVE_INFINITY ) {
  $ERROR('#2: Number() === +0, actual is '+Number());
}
