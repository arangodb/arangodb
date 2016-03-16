// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The production CharacterClassEscape :: w evaluates by returning the set of characters containing the sixty-three characters:
    a - z, A - Z, 0 - 9, _
es5id: 15.10.2.12_A3_T3
description: 0 - 9, _
---*/

var regexp_w = /\w/;

//CHECK#0030-0039
var result = true; 
for (var alpha = 0x0030; alpha <= 0x0039; alpha++) {
  var str = String.fromCharCode(alpha);
  var arr = regexp_w.exec(str); 
  if ((arr === null) || (arr[0] !== str)) {
    result = false;
  }
}

if (result !== true) {
  $ERROR('#1: 0 - 9');
}

//CHECK#005F
var arr = regexp_w.exec("_"); 
if ((arr === null) || (arr[0] !== "\u005F")) {
  $ERROR('#2: _');
}

//CHECK#0020
if (regexp_w.exec(" ") !== null) {
  $ERROR('#3:  ');
}
