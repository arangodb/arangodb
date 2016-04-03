// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of SignedInteger ::: + DecimalDigits is the MV of DecimalDigits"
es5id: 9.3.1_A14
description: Compare Number('+1234567890') with +('1234567890')
---*/

// CHECK#1
if (Number("+1234567890") !== +("1234567890"))  {
  $ERROR('#1: Number("+1234567890") === +("1234567890")');
}
