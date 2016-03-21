// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Equivalent to the expression RegExp.prototype.exec(string) != null
es5id: 15.10.6.3_A1_T13
description: RegExp is /t[a-b|q-s]/ and tested string is true
---*/

var __string = true;
var __re = /t[a-b|q-s]/;

//CHECK#0
if (__re.test(__string) !== (__re.exec(__string) !== null)) {
	$ERROR('#0: var __string = true;__re = /t[a-b|q-s]/; __re.test(__string) === (__re.exec(__string) !== null)');
}
