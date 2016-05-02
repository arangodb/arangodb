// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The RegExp.prototype.toString.length property does not have the attribute
    DontDelete
es5id: 15.10.6.4_A9
description: >
    Checking if deleting the RegExp.prototype.toString.length property
    fails
---*/

//CHECK#0
if ((RegExp.prototype.toString.hasOwnProperty('length') !== true)) {
	$ERROR('#0: RegExp.prototype.toString.hasOwnProperty(\'length\') === true');
}

//CHECK#1
if (delete RegExp.prototype.toString.length !== true) {
	$ERROR('#1: delete RegExp.prototype.toString.length === true');
}

//CHECK#2
if (RegExp.prototype.toString.hasOwnProperty('length') !== false) {
	$ERROR('#2: delete RegExp.prototype.toString.length; RegExp.prototype.toString.hasOwnProperty(\'length\') === false');
}
