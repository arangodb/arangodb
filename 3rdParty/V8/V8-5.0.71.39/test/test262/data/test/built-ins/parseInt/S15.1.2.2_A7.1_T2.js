// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If Z is empty, return NaN
es5id: 15.1.2.2_A7.1_T2
description: x is not a radix-R digit
---*/

//CHECK#1
if (isNaN(parseInt("$0x")) !== true) {
  $ERROR('#1: parseInt("$0x") === Not-a-Number. Actual: ' + (parseInt("$0x")));
}

//CHECK#2
if (isNaN(parseInt("$0X")) !== true) {
  $ERROR('#2: parseInt("$0X") === Not-a-Number. Actual: ' + (parseInt("$0X")));
}

//CHECK#3
if (isNaN(parseInt("$$$")) !== true) {
  $ERROR('#3: parseInt("$$$") === Not-a-Number. Actual: ' + (parseInt("$$$")));
}

//CHECK#4
if (isNaN(parseInt("")) !== true) {
  $ERROR('#4: parseInt("") === Not-a-Number. Actual: ' + (parseInt("")));
}

//CHECK#5
if (isNaN(parseInt(" ")) !== true) {
  $ERROR('#5: parseInt(" ") === Not-a-Number. Actual: ' + (parseInt(" ")));
}
