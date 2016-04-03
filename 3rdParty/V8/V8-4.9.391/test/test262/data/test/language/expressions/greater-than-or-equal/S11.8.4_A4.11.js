// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If y is a prefix of x, return true
es5id: 11.8.4_A4.11
description: x and y are string primitives
---*/

//CHECK#1
if (("x" >= "x") !== true) {
  $ERROR('#1: ("x" >= "x") === true');
}

//CHECK#2
if (("x" >= "") !== true) {
  $ERROR('#2: ("x" >= "") === true');
}

//CHECK#3
if (("abcd" >= "ab") !== true) {
  $ERROR('#3: ("abcd" >= ab") === true');
}

//CHECK#4
if (("abc\u0064" >= "abcd") !== true) {
  $ERROR('#4: ("abc\\u0064" >= abc") === true');
}

//CHECK#5
if (("x" + "y" >= "x") !== true) {
  $ERROR('#5: ("x" + "y" >= "x") === true');
}

//CHECK#6
var x = "x";
if ((x + 'y' >= x) !== true) {
  $ERROR('#6: var x = "x"; (x + "y" >= x) === true');
}

//CHECK#7
if (("a\u0000a" >= "a\u0000") !== true) {
  $ERROR('#7: ("a\\u0000a" >= "a\\u0000") === true');
}

//CHECK#8
if ((" x" >= "x") !== false) {
  $ERROR('#8: (" x" >= "x") === false');
}
