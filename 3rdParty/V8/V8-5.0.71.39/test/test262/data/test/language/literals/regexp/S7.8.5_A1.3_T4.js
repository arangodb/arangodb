// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "RegularExpressionFirstChar :: LineTerminator is incorrect"
es5id: 7.8.5_A1.3_T4
description: Carriage Return, with eval
---*/

//CHECK#1
try {
   eval("/\u000D/").source;
   $ERROR('#1.1: RegularExpressionFirstChar :: Carriage Return is incorrect. Actual: ' + (eval("/\u000D/").source)); 
}
catch (e) {
  if ((e instanceof SyntaxError) !== true) {
     $ERROR('#1.2: RegularExpressionFirstChar :: Carriage Return is incorrect. Actual: ' + (e));
  }
}
