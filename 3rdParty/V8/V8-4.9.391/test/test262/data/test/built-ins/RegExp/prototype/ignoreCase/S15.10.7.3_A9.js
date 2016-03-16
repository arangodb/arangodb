// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The RegExp.prototype ignoreCase property does not have the attribute
    DontDelete
es5id: 15.10.7.3_A9
description: Checking if deleting the ignoreCase property succeeds
---*/

var __re = RegExp.prototype;

//CHECK#0
if (__re.hasOwnProperty('ignoreCase') !== true) {
  $ERROR('#0: __re = RegExp.prototype; __re.hasOwnProperty(\'ignoreCase\') === true');
}

//CHECK#1
if ((delete __re.ignoreCase) !== true) {
  $ERROR('#1: __re = RegExp.prototype; (delete __re.ignoreCase) === true');
}

//CHECK#2
if (__re.hasOwnProperty('ignoreCase') !== false) {
  $ERROR('#2: __re = RegExp.prototype;delete __re.ignoreCase === true; __re.hasOwnProperty(\'ignoreCase\') === false');
}
