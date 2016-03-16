// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "RegularExpressionFirstChar :: LineTerminator is incorrect"
es5id: 7.8.5_A1.3_T5
description: Line separator, with eval
---*/

//CHECK#1
try {
   eval("/\u2028/").source;
   $ERROR('#1.1: RegularExpressionFirstChar :: Line separator is incorrect. Actual: ' + (eval("/\u2028/").source)); 
}
catch (e) {
  if ((e instanceof SyntaxError) !== true) {
     $ERROR('#1.2: RegularExpressionFirstChar :: Line separator is incorrect. Actual: ' + (e));
  }
}
