// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The RegExp.prototype global property does not have the attribute
    DontDelete
es5id: 15.10.7.2_A9
description: Checking if deleting the global property succeeds
---*/

var __re = RegExp.prototype;

//CHECK#0
if (__re.hasOwnProperty('global') !== true) {
  $ERROR('#0: __re = RegExp.prototype; __re.hasOwnProperty(\'global\') === true');
}

//CHECK#1
if ((delete __re.global) !== true) {
  $ERROR('#1: __re = RegExp.prototype; (delete __re.global) === true');
}

//CHECK#2
if (__re.hasOwnProperty('global') !== false) {
  $ERROR('#2: __re = RegExp.prototype;delete __re.global === true; __re.hasOwnProperty(\'global\') === false');
}
