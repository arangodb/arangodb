// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The RegExp.prototype multiline property does not have the attribute
    DontDelete
es5id: 15.10.7.4_A9
description: Checking if deleting the multiline property succeeds
---*/

var __re = RegExp.prototype;

//CHECK#0
if (__re.hasOwnProperty('multiline') !== true) {
  $ERROR('#0: __re = RegExp.prototype; __re.hasOwnProperty(\'multiline\') === true');
}

//CHECK#1
if ((delete __re.multiline) !== true) {
  $ERROR('#1: __re = RegExp.prototype; (delete __re.multiline) === true');
}

//CHECK#2
if (__re.hasOwnProperty('multiline') !== false) {
  $ERROR('#2: __re = RegExp.prototype;delete __re.multiline === true; __re.hasOwnProperty(\'multiline\') === false');
}
