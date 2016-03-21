// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    RegularExpressionFirstChar :: NonTerminator but not * or \ or /,
    RegularExpressionChars :: [empty], RegularExpressionFlags :: [empty]
es5id: 7.8.5_A1.1_T1
description: Without eval
---*/

//CHECK#1
if (/1/.source !== "1") {
  $ERROR('#1: /1/');
}   

//CHECK#2
if (/a/.source !== "a") {
  $ERROR('#2: /a/');
}

//CHECK#3
if (/;/.source !== ";") {
  $ERROR('#3: /;/');
}

//CHECK#4
if (/ /.source !== " ") {
  $ERROR('#4: / /');
}

//CHECK#5
if (/\u0041/.source !== "\\u0041") {
  $ERROR('#5: /\\u0041/');
}
