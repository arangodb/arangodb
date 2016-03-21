// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The RegExp.prototype source property does not have the attribute
    DontDelete
es5id: 15.10.7.1_A9
description: Checking if deleting the source property succeeds
---*/

var __re = RegExp.prototype;

//CHECK#0
if (__re.hasOwnProperty('source') !== true) {
	$ERROR('#0: __re = RegExp.prototype; __re.hasOwnProperty(\'source\') === true');
}

//CHECK#1
if ((delete __re.source) !== true) {
	$ERROR('#1: __re = RegExp.prototype; (delete __re.source) === true');
}

//CHECK#2
if (__re.hasOwnProperty('source') !== false) {
	$ERROR('#2: __re = RegExp.prototype;delete __re.source === true; __re.hasOwnProperty(\'source\') === false');
}
