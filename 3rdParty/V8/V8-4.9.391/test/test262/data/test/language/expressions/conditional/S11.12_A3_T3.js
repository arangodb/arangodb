// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If ToBoolean(x) is false, return z
es5id: 11.12_A3_T3
description: Type(y) and Type(z) are string primitives
---*/

//CHECK#1
if (("" ? "" : "1") !== "1") {
  $ERROR('#1: ("" ? "" : "1") === "1"');
}

//CHECK#2
var z = new String("1");
if (("" ? "1" : z) !== z) {
  $ERROR('#2: (var y = new String("1"); ("" ? "1" : z) === z');
}
