// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The production CharacterClassEscape :: W evaluates by returning the set of all characters not
    included in the set returned by CharacterClassEscape :: w
es5id: 15.10.2.12_A4_T2
description: a - z
---*/

var regexp_W = /\W/;

//CHECK#0061-007A
var result = true; 
for (var alpha = 0x0061; alpha <= 0x007A; alpha++) {
  if (regexp_W.exec(String.fromCharCode(alpha)) !== null) {
    result = false;
  }
}

if (result !== true) {
  $ERROR('#1: a - z');
}
