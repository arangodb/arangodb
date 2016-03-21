// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: the length property does not have the attributes { DontDelete }
es5id: 15.3.5.1_A2_T2
description: >
    Checking if deleting the length property of
    Function("arg1,arg2,arg3","arg4,arg5", null) succeeds
---*/

var f =  Function("arg1,arg2,arg3","arg4,arg5", null);

//CHECK#1
if (!(f.hasOwnProperty('length'))) {
  $ERROR('#1: the function has length property.');
}

delete f.length;

//CHECK#2
if (f.hasOwnProperty('length')) {
  $ERROR('#2: the function.length property does not have the attributes DontDelete.');
}

//CHECK#3
if (f.length === 5) {
  $ERROR('#3: the length property does not have the attributes { DontDelete }');
}
