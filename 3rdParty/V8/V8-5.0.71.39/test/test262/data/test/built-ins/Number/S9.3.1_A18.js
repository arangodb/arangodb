// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of DecimalDigit ::: 2 or of HexDigit ::: 2 is 2"
es5id: 9.3.1_A18
description: Compare Number('0x2') and Number('0X2') with 2
---*/

// CHECK#1
if (+("2") !== 2)  {
  $ERROR('#1: +("2") === 2. Actual: ' + (+("2")));
}

// CHECK#2
if (Number("0x2") !== 2)  {
  $ERROR('#2: Number("0x2") === 2. Actual: ' + (Number("0x2")));
}

// CHECK#3
if (Number("0X2") !== 2)  {
  $ERROR('#3: Number("0X2") === 2. Actual: ' + (Number("0X2")));
}
