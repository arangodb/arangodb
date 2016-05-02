// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Array.sort should not eat exceptions
es5id: 15.4.4.11_A5_T1
description: comparefn function throw "error"
---*/

//CHECK#1
var myComparefn = function(x,y) {
  throw "error";
}
var x = [1,0];
try {
  x.sort(myComparefn)
  $ERROR('#1.1: Array.sort should not eat exceptions');
} catch(e) {
  if (e !== "error") {
    $ERROR('#1.2: Array.sort should not eat exceptions');
  }
}
