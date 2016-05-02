// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If P's characters do not have the form Pattern, then throw a SyntaxError
    exception
es5id: 15.10.4.1_A9_T1
description: Pattern is "??"
---*/

//CHECK#1
try {
	$ERROR('#1.1: new RegExp("??") throw SyntaxError. Actual: ' + (new RegExp("??")));
} catch (e) {
	if ((e instanceof SyntaxError) !== true) {
		$ERROR('#1.2: new RegExp("??") throw SyntaxError. Actual: ' + (e));
	}
}
