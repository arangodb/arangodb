// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator remove leading StrWhiteSpaceChar
es5id: 15.1.2.2_A2_T3
description: "StrWhiteSpaceChar :: NBSB (U+00A0)"
---*/

//CHECK#1
if (parseInt("\u00A01") !== parseInt("1")) {
  $ERROR('#1: parseInt("\\u00A01") === parseInt("1"). Actual: ' + (parseInt("\u00A01")));
}

//CHECK#2
if (parseInt("\u00A0\u00A0-1") !== parseInt("-1")) {
  $ERROR('#2: parseInt("\\u00A0\\u00A0-1") === parseInt("-1"). Actual: ' + (parseInt("\u00A0\u00A0-1")));
}

//CHECK#3
if (isNaN(parseInt("\u00A0")) !== true) {
  $ERROR('#3: parseInt("\\u00A0") === Not-a-Number. Actual: ' + (parseInt("\u00A0")));
}
