// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    if argArray is neither an array nor an arguments object (see 10.1.8), a
    TypeError exception is thrown
es5id: 15.3.4.3_A6_T3
description: argArray is (object,"1,3,4")
---*/

var obj={};

//CHECK#1
try {
  Function().apply(obj,"1,3,4");
  $ERROR('#1: if argArray is neither an array nor an arguments object (see 10.1.8), a TypeError exception is thrown');
} catch (e) {
  if (!(e instanceof TypeError)) {
  	$ERROR('#1.1: if argArray is neither an array nor an arguments object (see 10.1.8), a TypeError exception is thrown');
  }
}
