// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator "instanceof" uses GetValue
es5id: 11.8.6_A2.1_T3
description: If GetBase(ShiftExpression) is null, throw ReferenceError
---*/

//CHECK#1
try {
  ({}) instanceof OBJECT;
  $ERROR('#1.1: ({}) instanceof OBJECT throw ReferenceError. Actual: ' + (({}) instanceof OBJECT));  
}
catch (e) {
  if ((e instanceof ReferenceError) !== true) {
    $ERROR('#1.2: ({}) instanceof OBJECT throw ReferenceError. Actual: ' + (e));  
  }
}
