// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "\"for(key in undefined)\" Statement is allowed"
es5id: 12.6.4_A1
description: Checking if execution of "for(key in undefined)" passes
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
try {
	for(__key in undefined){
	    var key=__key;
	};
} catch (e) {
	$ERROR('#1: "for(key in undefined){}" does not lead to throwing exception');
}
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (key!==undefined) {
	$ERROR('#2: key === undefined. Actual: key === '+key);
}
//
//////////////////////////////////////////////////////////////////////////////
