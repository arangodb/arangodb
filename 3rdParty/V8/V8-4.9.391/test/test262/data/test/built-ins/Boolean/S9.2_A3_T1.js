// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Result of boolean conversion from boolean value is no conversion
es5id: 9.2_A3_T1
description: true and false convert to Boolean by explicit transformation
---*/

// CHECK#1 
if (Boolean(true) !== true) {
  $ERROR('#1: Boolean(true) === true. Actual: ' + (Boolean(true)));	
}

// CHECK#2
if (Boolean(false) !== false) {
  $ERROR('#2: Boolean(false) === false. Actual: ' + (Boolean(false)));
}
