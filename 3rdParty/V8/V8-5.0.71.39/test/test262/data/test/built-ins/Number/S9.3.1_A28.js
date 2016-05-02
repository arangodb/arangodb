// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of HexDigit ::: c or of HexDigit ::: C is 12"
es5id: 9.3.1_A28
description: >
    Compare Number('0xC'), Number('0XC'), Number('0xc') and
    Number('0Xc') with 12
---*/

// CHECK#1
if (Number("0xc") !== 12)  {
  $ERROR('#1: Number("0xc") === 12. Actual: ' + (Number("0xc")));
}

// CHECK#2
if (+("0xC") !== 12)  {
  $ERROR('#2: +("0xC") === 12. Actual: ' + (+("0xC")));
}

// CHECK#3
if (Number("0Xc") !== 12)  {
  $ERROR('#3: Number("0Xc") === 12. Actual: ' + (Number("0Xc")));
}

// CHECK#4
if (Number("0XC") !== 12)  {
  $ERROR('#4: Number("0XC") === 12. Actual: ' + (Number("0XC")));
}
