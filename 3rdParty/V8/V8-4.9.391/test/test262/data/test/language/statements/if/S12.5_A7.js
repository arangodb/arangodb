// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    In the "if" statement empty statement is allowed and is evaluated to
    "undefined"
es5id: 12.5_A7
description: Checking by using eval "eval("if(1);"))"
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
try {
	var __evaluated = eval("if(1);");
	if (__evaluated !== undefined) {
		$ERROR('#1: __evaluated === undefined. Actual:  __evaluated ==='+ __evaluated  );
	}

} catch (e) {
	$ERROR('#1.1: "__evaluated = eval("if(1);")" does not lead to throwing exception');

}
//
//////////////////////////////////////////////////////////////////////////////
