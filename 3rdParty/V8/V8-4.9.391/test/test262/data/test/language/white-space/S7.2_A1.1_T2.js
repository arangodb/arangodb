// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: HORIZONTAL TAB (U+0009) between any two tokens is allowed
es5id: 7.2_A1.1_T2
description: Insert real HORIZONTAL TAB between tokens of var x=1
---*/

//CHECK#1
	var  x	=	1	;
if (x !== 1) {
  $ERROR('#1: 	var	x	=	1	; x === 1. Actual: ' + (x));
}

//CHECK#2
var result;
eval("	var\tx	=\t2	; result = x;");
if (result !== 2) {
  $ERROR('#2: 	var\\tx	=\\t1	; result = x; result === 2. Actual: ' + (result));
}
